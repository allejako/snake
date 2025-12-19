#include "online_multiplayer.h"
#include "game.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define COMBO_WINDOW_TICKS 30
#define INITIAL_LIVES 3
#define POINTS_PER_FOOD 10

// Forward declarations of internal functions
static void mpapi_event_callback(const char *event, int64_t messageId, const char *clientId, json_t *data, void *context);
static void handle_player_joined(OnlineMultiplayerContext *ctx, const char *clientId, json_t *data);
static void handle_player_left(OnlineMultiplayerContext *ctx, const char *clientId);
static void handle_client_input(OnlineMultiplayerContext *ctx, const char *clientId, json_t *data);
static void handle_game_state_update(OnlineMultiplayerContext *ctx, json_t *data);
static void respawn_player(MultiplayerGame_s *game, int player_idx);
static Vec2 find_safe_spawn_position(MultiplayerGame_s *game);
static void update_player_combo_timer(MultiplayerPlayer *p, unsigned int current_time, unsigned int combo_window_ms);
static json_t* serialize_player(MultiplayerPlayer *player);
static void deserialize_player(MultiplayerPlayer *player, json_t *data);

// Lifecycle functions

OnlineMultiplayerContext* online_multiplayer_create(void)
{
    OnlineMultiplayerContext *ctx = (OnlineMultiplayerContext*)malloc(sizeof(OnlineMultiplayerContext));
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(OnlineMultiplayerContext));
    ctx->api = NULL;
    ctx->listener_id = -1;
    ctx->game = NULL;
    ctx->state = ONLINE_STATE_HOST_SETUP;
    ctx->is_private = 0;
    ctx->connection_lost = 0;
    ctx->has_pending_input = 0;
    ctx->error_message[0] = '\0';

    return ctx;
}

void online_multiplayer_destroy(OnlineMultiplayerContext *ctx)
{
    if (!ctx) return;

    // Unregister listener if registered
    if (ctx->listener_id >= 0 && ctx->api) {
        mpapi_unlisten(ctx->api, ctx->listener_id);
    }

    free(ctx);
}

// Host operations

int online_multiplayer_host(OnlineMultiplayerContext *ctx, int is_private, int board_width, int board_height)
{
    if (!ctx || !ctx->api || !ctx->game) {
        return MPAPI_ERR_ARGUMENT;
    }

    ctx->is_private = is_private;

    // Create host data JSON
    json_t *host_data = json_object();
    json_object_set_new(host_data, "name", json_string("Snake Game"));
    json_object_set_new(host_data, "private", json_boolean(is_private));

    char *session_id = NULL;
    char *client_id = NULL;
    int rc = mpapi_host(ctx->api, host_data, &session_id, &client_id, NULL);

    json_decref(host_data);

    if (rc != MPAPI_OK) {
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Failed to host session: error %d", rc);
        return rc;
    }

    // Store session info
    if (session_id) {
        strncpy(ctx->game->session_id, session_id, sizeof(ctx->game->session_id) - 1);
        free(session_id);
    }

    if (client_id) {
        strncpy(ctx->game->host_client_id, client_id, sizeof(ctx->game->host_client_id) - 1);
        free(client_id);
    }

    // Initialize game as host
    multiplayer_game_init(ctx->game, board_width, board_height);
    ctx->game->is_host = 1;
    ctx->game->local_player_index = 0; // Host is player 0
    ctx->game->combo_window_ms = 95 * COMBO_WINDOW_TICKS; // Initial tick speed

    // Host auto-joins as player 0
    MultiplayerPlayer *p = &ctx->game->players[0];
    p->joined = 1;
    p->score = 0;
    p->fruits_eaten = 0;
    p->lives = INITIAL_LIVES;
    p->combo_count = 0;
    p->combo_expiry_time = 0;
    p->combo_best = 0;
    p->food_eaten_this_frame = 0;
    strncpy(p->client_id, ctx->game->host_client_id, sizeof(p->client_id) - 1);
    p->is_local_player = 1;
    ctx->game->total_joined = 1;

    return MPAPI_OK;
}

void online_multiplayer_host_update(OnlineMultiplayerContext *ctx, unsigned int current_time)
{
    if (!ctx || ctx->state != ONLINE_STATE_PLAYING || !ctx->game->is_host) {
        return;
    }

    MultiplayerGame_s *game = ctx->game;

    // 1. Process inputs from all players (already in buffers via callback)
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!game->players[i].alive) continue;

        Direction dir;
        if (input_buffer_pop(&game->players[i].input, &dir)) {
            snake_change_direction(&game->players[i].snake, dir);
        }
    }

    // 2. Update game (move snakes, check collisions)
    multiplayer_game_update(game);

    // 3. Handle death animations
    multiplayer_game_update_death_animations(game);

    // 4. Handle respawns
    for (int i = 0; i < MAX_PLAYERS; i++) {
        MultiplayerPlayer *p = &game->players[i];

        if (p->death_state == GAME_OVER && p->joined) {
            // Just died
            p->alive = 0;
            p->lives--;

            if (p->lives > 0) {
                // Respawn player
                respawn_player(game, i);
            } else {
                // Eliminated
                game->active_players--;
            }

            // Reset combo
            p->combo_count = 0;
            p->combo_expiry_time = 0;
        }
    }

    // 5. Update combo timers for all players
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game->players[i].alive) {
            update_player_combo_timer(&game->players[i], current_time, game->combo_window_ms);
        }
    }

    // 6. Broadcast state to all clients
    online_multiplayer_host_broadcast_state(ctx);

    // 7. Check win condition
    if (game->active_players <= 1) {
        ctx->state = ONLINE_STATE_GAME_OVER;
    }
}

void online_multiplayer_host_broadcast_state(OnlineMultiplayerContext *ctx)
{
    if (!ctx || !ctx->api) return;

    json_t *state = online_multiplayer_serialize_state(ctx->game);

    // Broadcast to all clients (destination = NULL)
    int rc = mpapi_game(ctx->api, state, NULL);

    if (rc != MPAPI_OK) {
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Failed to broadcast state: error %d", rc);
        ctx->connection_lost = 1;
    }

    json_decref(state);
}

// Client operations

int online_multiplayer_join(OnlineMultiplayerContext *ctx, const char *session_id)
{
    if (!ctx || !ctx->api || !ctx->game || !session_id) {
        return MPAPI_ERR_ARGUMENT;
    }

    json_t *join_data = json_object();
    json_object_set_new(join_data, "join", json_boolean(1));

    char *returned_session = NULL;
    char *client_id = NULL;
    int rc = mpapi_join(ctx->api, session_id, join_data, &returned_session, &client_id, NULL);

    json_decref(join_data);

    if (rc != MPAPI_OK) {
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Failed to join session: %s",
                 rc == MPAPI_ERR_REJECTED ? "Invalid session ID" : "Connection error");
        return rc;
    }

    // Store session info
    if (returned_session) {
        strncpy(ctx->game->session_id, returned_session, sizeof(ctx->game->session_id) - 1);
        free(returned_session);
    }

    // Initialize game as client
    ctx->game->is_host = 0;
    ctx->game->local_player_index = -1; // Will be set when we receive game state

    if (client_id) {
        free(client_id);
    }

    return MPAPI_OK;
}

void online_multiplayer_client_send_input(OnlineMultiplayerContext *ctx, Direction dir)
{
    if (!ctx || !ctx->api) return;

    // Create JSON: {"dir": "UP|DOWN|LEFT|RIGHT"}
    json_t *input = json_object();

    const char *dir_str = NULL;
    switch (dir) {
        case DIR_UP:    dir_str = "UP"; break;
        case DIR_DOWN:  dir_str = "DOWN"; break;
        case DIR_LEFT:  dir_str = "LEFT"; break;
        case DIR_RIGHT: dir_str = "RIGHT"; break;
        default: return;
    }

    json_object_set_new(input, "dir", json_string(dir_str));

    // Send to host (destination = NULL broadcasts to all, including host)
    int rc = mpapi_game(ctx->api, input, NULL);

    if (rc != MPAPI_OK) {
        ctx->connection_lost = 1;
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Connection lost: error %d", rc);
    }

    json_decref(input);
}

// Common operations

void online_multiplayer_start_game(OnlineMultiplayerContext *ctx)
{
    if (!ctx || !ctx->game) return;

    // Start the multiplayer game
    multiplayer_game_start(ctx->game);

    // Initialize lives for all players
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (ctx->game->players[i].joined) {
            ctx->game->players[i].lives = INITIAL_LIVES;
            ctx->game->players[i].score = 0;
            ctx->game->players[i].fruits_eaten = 0;
            ctx->game->players[i].combo_count = 0;
            ctx->game->players[i].combo_expiry_time = 0;
            ctx->game->players[i].combo_best = 0;
        }
    }

    ctx->state = ONLINE_STATE_COUNTDOWN;
}

int online_multiplayer_get_local_player_index(OnlineMultiplayerContext *ctx)
{
    if (!ctx || !ctx->game) return -1;
    return ctx->game->local_player_index;
}

// Event callback and handlers

static void mpapi_event_callback(const char *event, int64_t messageId,
                                 const char *clientId, json_t *data, void *context)
{
    (void)messageId; // Unused
    OnlineMultiplayerContext *ctx = (OnlineMultiplayerContext*)context;
    if (!ctx) return;

    if (strcmp(event, "joined") == 0) {
        handle_player_joined(ctx, clientId, data);
    }
    else if (strcmp(event, "leaved") == 0) {
        handle_player_left(ctx, clientId);
    }
    else if (strcmp(event, "game") == 0) {
        if (ctx->game->is_host) {
            handle_client_input(ctx, clientId, data);
        } else {
            handle_game_state_update(ctx, data);
        }
    }
    else if (strcmp(event, "closed") == 0) {
        ctx->connection_lost = 1;
        strcpy(ctx->error_message, "Session closed");
        ctx->state = ONLINE_STATE_DISCONNECTED;
    }
}

static void handle_player_joined(OnlineMultiplayerContext *ctx, const char *clientId, json_t *data)
{
    (void)data; // Unused
    if (!ctx->game->is_host) return; // Only host handles joins

    // Don't allow joins after game started
    if (ctx->state != ONLINE_STATE_LOBBY) return;

    // Find empty player slot
    int slot = -1;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!ctx->game->players[i].joined) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        // Game full
        return;
    }

    // Initialize player in slot
    MultiplayerPlayer *p = &ctx->game->players[slot];
    p->joined = 1;
    p->alive = 0; // Not alive until game starts
    strncpy(p->client_id, clientId, sizeof(p->client_id) - 1);
    p->is_local_player = 0;

    // Initialize stats
    p->score = 0;
    p->fruits_eaten = 0;
    p->lives = INITIAL_LIVES;
    p->combo_count = 0;
    p->combo_expiry_time = 0;
    p->combo_best = 0;
    p->food_eaten_this_frame = 0;

    ctx->game->total_joined++;

    // Broadcast updated lobby state
    online_multiplayer_host_broadcast_state(ctx);
}

static void handle_player_left(OnlineMultiplayerContext *ctx, const char *clientId)
{
    // Find player by client_id
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (ctx->game->players[i].joined &&
            strcmp(ctx->game->players[i].client_id, clientId) == 0) {

            // Mark as disconnected
            ctx->game->players[i].joined = 0;
            ctx->game->players[i].alive = 0;
            ctx->game->total_joined--;

            if (ctx->game->players[i].alive) {
                ctx->game->active_players--;
            }

            break;
        }
    }

    // If in game and too few players, end game
    if (ctx->state == ONLINE_STATE_PLAYING && ctx->game->active_players <= 1) {
        ctx->state = ONLINE_STATE_GAME_OVER;
    }

    // Check if host disconnected (for clients)
    if (!ctx->game->is_host &&
        strcmp(clientId, ctx->game->host_client_id) == 0) {
        ctx->connection_lost = 1;
        strcpy(ctx->error_message, "Host disconnected");
        ctx->state = ONLINE_STATE_DISCONNECTED;
    }
}

static void handle_client_input(OnlineMultiplayerContext *ctx, const char *clientId, json_t *data)
{
    // Find player by client_id
    int player_idx = -1;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (ctx->game->players[i].joined &&
            strcmp(ctx->game->players[i].client_id, clientId) == 0) {
            player_idx = i;
            break;
        }
    }

    if (player_idx == -1) return;

    // Parse direction from JSON
    json_t *dir_json = json_object_get(data, "dir");
    if (!json_is_string(dir_json)) return;

    const char *dir_str = json_string_value(dir_json);
    Direction dir;

    if (strcmp(dir_str, "UP") == 0) dir = DIR_UP;
    else if (strcmp(dir_str, "DOWN") == 0) dir = DIR_DOWN;
    else if (strcmp(dir_str, "LEFT") == 0) dir = DIR_LEFT;
    else if (strcmp(dir_str, "RIGHT") == 0) dir = DIR_RIGHT;
    else return;

    // Validate player is alive
    if (!ctx->game->players[player_idx].alive) return;

    // Add to player's input buffer
    Direction current_dir = ctx->game->players[player_idx].snake.dir;
    input_buffer_push(&ctx->game->players[player_idx].input, dir, current_dir);
}

static void handle_game_state_update(OnlineMultiplayerContext *ctx, json_t *data)
{
    // Deserialize and update local game state
    online_multiplayer_deserialize_state(ctx->game, data);

    // Find local player index if not set
    if (ctx->game->local_player_index == -1) {
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (ctx->game->players[i].is_local_player) {
                ctx->game->local_player_index = i;
                break;
            }
        }
    }
}

// Helper functions

static void respawn_player(MultiplayerGame_s *game, int player_idx)
{
    MultiplayerPlayer *p = &game->players[player_idx];

    // Find safe spawn position
    Vec2 spawn_pos = find_safe_spawn_position(game);

    // Reset snake to small size
    snake_init(&p->snake, spawn_pos, DIR_RIGHT);

    // Reset state
    p->alive = 1;
    p->death_state = GAME_RUNNING;
    input_buffer_clear(&p->input);

    game->active_players++;
}

static Vec2 find_safe_spawn_position(MultiplayerGame_s *game)
{
    // Try random positions until one is safe
    int max_attempts = 100;
    for (int attempt = 0; attempt < max_attempts; attempt++) {
        Vec2 candidate = {
            rand() % game->board.width,
            rand() % game->board.height
        };

        // Check if position is safe (not occupied)
        int safe = 1;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (game->players[i].alive &&
                snake_occupies(&game->players[i].snake, candidate)) {
                safe = 0;
                break;
            }
        }

        if (safe) return candidate;
    }

    // Fallback: just use random position
    Vec2 fallback = {rand() % game->board.width, rand() % game->board.height};
    return fallback;
}

static void update_player_combo_timer(MultiplayerPlayer *p, unsigned int current_time, unsigned int combo_window_ms)
{
    // If combo expiry time was just set to placeholder, update it
    if (p->combo_expiry_time == 1) {
        p->combo_expiry_time = current_time + combo_window_ms;
    }

    // Check expiration
    if (p->combo_count > 0 && current_time >= p->combo_expiry_time) {
        p->combo_count = 0;
        p->combo_expiry_time = 0;
    }
}

// JSON serialization

json_t* online_multiplayer_serialize_state(MultiplayerGame_s *game)
{
    json_t *root = json_object();

    // Board food position
    json_t *food = json_object();
    json_object_set_new(food, "x", json_integer(game->board.food.x));
    json_object_set_new(food, "y", json_integer(game->board.food.y));
    json_object_set_new(root, "food", food);

    // Additional food items (from dead snakes)
    json_t *extra_food = json_array();
    for (int i = 0; i < game->food_count; i++) {
        json_t *f = json_object();
        json_object_set_new(f, "x", json_integer(game->food[i].x));
        json_object_set_new(f, "y", json_integer(game->food[i].y));
        json_array_append_new(extra_food, f);
    }
    json_object_set_new(root, "extra_food", extra_food);

    // Players array
    json_t *players = json_array();
    for (int i = 0; i < MAX_PLAYERS; i++) {
        json_t *p = serialize_player(&game->players[i]);
        json_array_append_new(players, p);
    }
    json_object_set_new(root, "players", players);

    return root;
}

static json_t* serialize_player(MultiplayerPlayer *player)
{
    json_t *p = json_object();

    json_object_set_new(p, "joined", json_boolean(player->joined));
    json_object_set_new(p, "alive", json_boolean(player->alive));
    json_object_set_new(p, "death_state", json_integer(player->death_state));

    // Stats
    json_object_set_new(p, "score", json_integer(player->score));
    json_object_set_new(p, "lives", json_integer(player->lives));
    json_object_set_new(p, "combo", json_integer(player->combo_count));
    json_object_set_new(p, "combo_best", json_integer(player->combo_best));
    json_object_set_new(p, "fruits_eaten", json_integer(player->fruits_eaten));

    // Snake segments
    json_t *segments = json_array();
    for (int i = 0; i < player->snake.length; i++) {
        json_t *seg = json_array();
        json_array_append_new(seg, json_integer(player->snake.segments[i].x));
        json_array_append_new(seg, json_integer(player->snake.segments[i].y));
        json_array_append_new(segments, seg);
    }
    json_object_set_new(p, "segments", segments);

    // Direction
    json_object_set_new(p, "direction", json_integer(player->snake.dir));

    // Network identity
    json_object_set_new(p, "is_local", json_boolean(player->is_local_player));

    return p;
}

void online_multiplayer_deserialize_state(MultiplayerGame_s *game, json_t *data)
{
    // Food
    json_t *food = json_object_get(data, "food");
    if (food) {
        game->board.food.x = (int)json_integer_value(json_object_get(food, "x"));
        game->board.food.y = (int)json_integer_value(json_object_get(food, "y"));
    }

    // Extra food
    json_t *extra_food = json_object_get(data, "extra_food");
    if (json_is_array(extra_food)) {
        game->food_count = 0;
        size_t index;
        json_t *f;
        json_array_foreach(extra_food, index, f) {
            if (game->food_count < MAX_FOOD_ITEMS) {
                game->food[game->food_count].x = (int)json_integer_value(json_object_get(f, "x"));
                game->food[game->food_count].y = (int)json_integer_value(json_object_get(f, "y"));
                game->food_count++;
            }
        }
    }

    // Players
    json_t *players = json_object_get(data, "players");
    if (json_is_array(players)) {
        game->active_players = 0;
        game->total_joined = 0;
        for (int i = 0; i < MAX_PLAYERS && i < (int)json_array_size(players); i++) {
            deserialize_player(&game->players[i], json_array_get(players, i));
            if (game->players[i].joined) game->total_joined++;
            if (game->players[i].alive) game->active_players++;
        }
    }
}

static void deserialize_player(MultiplayerPlayer *player, json_t *data)
{
    player->joined = json_is_true(json_object_get(data, "joined"));
    player->alive = json_is_true(json_object_get(data, "alive"));
    player->death_state = (GameState)json_integer_value(json_object_get(data, "death_state"));

    player->score = (int)json_integer_value(json_object_get(data, "score"));
    player->lives = (int)json_integer_value(json_object_get(data, "lives"));
    player->combo_count = (int)json_integer_value(json_object_get(data, "combo"));
    player->combo_best = (int)json_integer_value(json_object_get(data, "combo_best"));
    player->fruits_eaten = (int)json_integer_value(json_object_get(data, "fruits_eaten"));

    // Snake
    json_t *segments = json_object_get(data, "segments");
    if (json_is_array(segments)) {
        player->snake.length = 0;
        size_t index;
        json_t *seg;
        json_array_foreach(segments, index, seg) {
            if (player->snake.length < MAX_SNAKE_LEN && json_array_size(seg) == 2) {
                player->snake.segments[player->snake.length].x =
                    (int)json_integer_value(json_array_get(seg, 0));
                player->snake.segments[player->snake.length].y =
                    (int)json_integer_value(json_array_get(seg, 1));
                player->snake.length++;
            }
        }
    }

    player->snake.dir = (Direction)json_integer_value(json_object_get(data, "direction"));
    player->is_local_player = json_is_true(json_object_get(data, "is_local"));
}
