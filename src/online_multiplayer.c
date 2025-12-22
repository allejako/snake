#include "online_multiplayer.h"
#include "game.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <SDL2/SDL.h>

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
    ctx->our_client_id[0] = '\0';

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

int online_multiplayer_host(OnlineMultiplayerContext *ctx, int is_private, int board_width, int board_height, const char *player_name)
{
    printf("DEBUG: online_multiplayer_host called\n");
    printf("DEBUG: ctx=%p, ctx->api=%p, ctx->game=%p\n", (void*)ctx, (void*)(ctx ? ctx->api : NULL), (void*)(ctx ? ctx->game : NULL));

    if (!ctx || !ctx->api || !ctx->game) {
        printf("DEBUG: NULL pointer check failed\n");
        return MPAPI_ERR_ARGUMENT;
    }

    printf("DEBUG: Setting is_private=%d\n", is_private);
    ctx->is_private = is_private;

    // Create host data JSON
    printf("DEBUG: Creating host data JSON\n");
    json_t *host_data = json_object();
    json_object_set_new(host_data, "name", json_string("Snake Game"));
    json_object_set_new(host_data, "private", json_boolean(is_private));

    char *session_id = NULL;
    char *client_id = NULL;
    printf("DEBUG: Calling mpapi_host\n");
    fflush(stdout);

    int rc = mpapi_host(ctx->api, host_data, &session_id, &client_id, NULL);

    printf("DEBUG: mpapi_host returned %d\n", rc);
    fflush(stdout);
    json_decref(host_data);

    if (rc != MPAPI_OK) {
        printf("DEBUG: mpapi_host failed with error %d\n", rc);
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Failed to host session: error %d", rc);
        return rc;
    }

    // Store session info
    printf("DEBUG: Storing session info\n");
    if (session_id) {
        strncpy(ctx->game->session_id, session_id, sizeof(ctx->game->session_id) - 1);
        free(session_id);
    }

    if (client_id) {
        strncpy(ctx->game->host_client_id, client_id, sizeof(ctx->game->host_client_id) - 1);
        strncpy(ctx->our_client_id, client_id, sizeof(ctx->our_client_id) - 1);
        ctx->our_client_id[sizeof(ctx->our_client_id) - 1] = '\0';
        free(client_id);
    }

    // Initialize game as host
    printf("DEBUG: Initializing multiplayer game\n");
    multiplayer_game_init(ctx->game, board_width, board_height);
    ctx->game->is_host = 1;
    ctx->game->local_player_index = 0; // Host is player 0
    ctx->game->combo_window_ms = 95 * COMBO_WINDOW_TICKS; // Initial tick speed

    // Host auto-joins as player 0
    MultiplayerPlayer *p = &ctx->game->players[0];
    p->joined = 1;
    p->alive = 0; // Not alive until game starts

    // Initialize snake (empty until game starts)
    p->snake.length = 0;
    p->snake.dir = DIR_RIGHT;
    p->death_state = GAME_RUNNING;

    // Initialize input buffer
    input_buffer_init(&p->input);

    p->score = 0;
    p->fruits_eaten = 0;
    p->lives = INITIAL_LIVES;
    p->combo_count = 0;
    p->combo_expiry_time = 0;
    p->combo_best = 0;
    p->food_eaten_this_frame = 0;
    strncpy(p->client_id, ctx->game->host_client_id, sizeof(p->client_id) - 1);
    strncpy(p->name, player_name ? player_name : "Player", sizeof(p->name) - 1);
    p->name[sizeof(p->name) - 1] = '\0';
    p->is_local_player = 1;
    ctx->game->total_joined = 1;

    // Register event listener
    printf("DEBUG: Registering event listener\n");
    fflush(stdout);
    ctx->listener_id = mpapi_listen(ctx->api, mpapi_event_callback, ctx);
    printf("DEBUG: mpapi_listen returned listener_id=%d\n", ctx->listener_id);
    fflush(stdout);
    if (ctx->listener_id < 0) {
        printf("DEBUG: Event listener registration FAILED\n");
        fflush(stdout);
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Failed to register event listener");
        return MPAPI_ERR_STATE;
    }
    printf("DEBUG: Event listener registered successfully\n");
    fflush(stdout);

    // Set state to lobby
    ctx->state = ONLINE_STATE_LOBBY;
    printf("DEBUG: State set to ONLINE_STATE_LOBBY\n");
    fflush(stdout);

    printf("DEBUG: online_multiplayer_host returning MPAPI_OK\n");
    fflush(stdout);
    return MPAPI_OK;
}

void online_multiplayer_host_update(OnlineMultiplayerContext *ctx, unsigned int current_time)
{
    if (!ctx || ctx->state != ONLINE_STATE_PLAYING || !ctx->game->is_host) {
        return;
    }

    MultiplayerGame_s *game = ctx->game;

    // 1. Process input for host's own player only (remote players send their positions)
    int host_player_idx = game->local_player_index;
    if (host_player_idx >= 0 && host_player_idx < MAX_PLAYERS && game->players[host_player_idx].alive) {
        Direction dir;
        if (input_buffer_pop(&game->players[host_player_idx].input, &dir)) {
            snake_change_direction(&game->players[host_player_idx].snake, dir);
        }
    }

    // 2. Update game (move snakes, check collisions, generate food)
    // Remote players' positions are already updated via handle_client_input
    multiplayer_game_update(game, 1);

    // 3. Handle death animations
    multiplayer_game_update_death_animations(game);

    // 4. Respawn is handled by each client in main.c (both host and join clients use same code)

    // 5. Update combo timers for all players
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game->players[i].alive) {
            update_player_combo_timer(&game->players[i], current_time, game->combo_window_ms);
        }
    }

    // 6. Broadcast state to all clients
    online_multiplayer_host_broadcast_state(ctx);

    // 7. Check win condition (count players with lives > 0)
    int players_with_lives = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game->players[i].joined && game->players[i].lives > 0) {
            players_with_lives++;
        }
    }

    if (players_with_lives <= 1) {
        ctx->state = ONLINE_STATE_GAME_OVER;

        // Find and award win to the winner
        int winner_idx = -1;
        int highest_score = -1;

        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (game->players[i].joined) {
                // Player with lives > 0 wins, or if no one has lives, highest score wins
                if (game->players[i].lives > 0) {
                    winner_idx = i;
                    break; // Winner found
                }
                else if (game->players[i].score > highest_score) {
                    winner_idx = i;
                    highest_score = game->players[i].score;
                }
            }
        }

        if (winner_idx >= 0) {
            game->players[winner_idx].wins++;
            printf("DEBUG: Player %d (%s) wins! Total wins: %d\n",
                   winner_idx, game->players[winner_idx].name, game->players[winner_idx].wins);
        }

        // Broadcast game over to all clients
        json_t *game_over_cmd = json_object();
        json_object_set_new(game_over_cmd, "command", json_string("game_over"));

        int rc = mpapi_game(ctx->api, game_over_cmd, NULL);
        if (rc != MPAPI_OK) {
            ctx->connection_lost = 1;
        }

        json_decref(game_over_cmd);
        printf("DEBUG: Broadcast game_over command\n");
        fflush(stdout);
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

int online_multiplayer_join(OnlineMultiplayerContext *ctx, const char *session_id, int board_width, int board_height, const char *player_name)
{
    printf("DEBUG: online_multiplayer_join called with session_id=%s\n", session_id);
    fflush(stdout);

    if (!ctx || !ctx->api || !ctx->game || !session_id) {
        printf("DEBUG: NULL pointer check failed\n");
        fflush(stdout);
        return MPAPI_ERR_ARGUMENT;
    }

    // Register event listener BEFORE joining to catch the "joined" event
    printf("DEBUG: Registering event listener BEFORE join\n");
    fflush(stdout);
    ctx->listener_id = mpapi_listen(ctx->api, mpapi_event_callback, ctx);
    printf("DEBUG: mpapi_listen returned listener_id=%d\n", ctx->listener_id);
    fflush(stdout);
    if (ctx->listener_id < 0) {
        printf("DEBUG: Event listener registration FAILED\n");
        fflush(stdout);
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Failed to register event listener");
        return MPAPI_ERR_STATE;
    }

    printf("DEBUG: Creating join data JSON\n");
    fflush(stdout);
    json_t *join_data = json_object();
    json_object_set_new(join_data, "join", json_boolean(1));
    json_object_set_new(join_data, "name", json_string(player_name ? player_name : "Player"));

    char *returned_session = NULL;
    char *client_id = NULL;
    json_t *join_response = NULL;
    printf("DEBUG: Calling mpapi_join\n");
    fflush(stdout);
    int rc = mpapi_join(ctx->api, session_id, join_data, &returned_session, &client_id, &join_response);

    printf("DEBUG: mpapi_join returned %d\n", rc);
    fflush(stdout);
    json_decref(join_data);

    if (rc != MPAPI_OK) {
        printf("DEBUG: mpapi_join failed with error %d\n", rc);
        fflush(stdout);

        // Clean up listener since join failed
        if (ctx->listener_id >= 0) {
            mpapi_unlisten(ctx->api, ctx->listener_id);
            ctx->listener_id = -1;
        }

        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Failed to join session: %s",
                 rc == MPAPI_ERR_REJECTED ? "Invalid session ID" : "Connection error");
        ctx->connection_lost = 1;
        ctx->state = ONLINE_STATE_DISCONNECTED;
        return rc;
    }

    // Store session info
    if (returned_session) {
        strncpy(ctx->game->session_id, returned_session, sizeof(ctx->game->session_id) - 1);
        free(returned_session);
    }

    // Store our client_id for later identification
    if (client_id) {
        strncpy(ctx->our_client_id, client_id, sizeof(ctx->our_client_id) - 1);
        ctx->our_client_id[sizeof(ctx->our_client_id) - 1] = '\0';
        printf("DEBUG: Our client_id: %s\n", ctx->our_client_id);
        fflush(stdout);
    }

    // Initialize game as client
    multiplayer_game_init(ctx->game, board_width, board_height);
    ctx->game->is_host = 0;
    ctx->game->local_player_index = -1;

    // Parse join response FIRST to get existing players and our correct slot
    printf("DEBUG: join_response=%p\n", (void*)join_response);
    fflush(stdout);
    if (join_response && json_is_object(join_response)) {
        printf("DEBUG: join_response is a valid JSON object\n");
        fflush(stdout);

        // Debug: print all keys in the response
        const char *key;
        json_t *value;
        printf("DEBUG: Keys in join_response: ");
        json_object_foreach(join_response, key, value) {
            printf("%s, ", key);
        }
        printf("\n");
        fflush(stdout);

        json_t *clients_array = json_object_get(join_response, "clients");
        printf("DEBUG: clients_array=%p\n", (void*)clients_array);
        fflush(stdout);
        if (json_is_array(clients_array)) {
            size_t num_clients = json_array_size(clients_array);
            printf("DEBUG: Join response has %zu clients\n", num_clients);
            fflush(stdout);

            // Add all existing clients (including ourselves) as players
            for (size_t i = 0; i < num_clients && i < MAX_PLAYERS; i++) {
                json_t *client_id_json = json_array_get(clients_array, i);
                if (json_is_string(client_id_json)) {
                    const char *cid = json_string_value(client_id_json);
                    MultiplayerPlayer *p = &ctx->game->players[i];
                    p->joined = 1;
                    p->alive = 0; // Not alive until game starts
                    strncpy(p->client_id, cid, sizeof(p->client_id) - 1);

                    // Check if this is the local player
                    if (client_id && strcmp(cid, client_id) == 0) {
                        p->is_local_player = 1;
                        ctx->game->local_player_index = i;
                        printf("DEBUG: Local player is at index %d\n", (int)i);
                        fflush(stdout);
                    } else {
                        p->is_local_player = 0;
                    }

                    // Initialize stats
                    p->score = 0;
                    p->fruits_eaten = 0;
                    p->lives = INITIAL_LIVES;
                    p->combo_count = 0;
                    p->combo_expiry_time = 0;
                    p->combo_best = 0;
                    p->food_eaten_this_frame = 0;

                    ctx->game->total_joined++;
                }
            }
        } else {
            printf("DEBUG: clients_array is not an array or not found\n");
            fflush(stdout);
        }
        json_decref(join_response);
    } else {
        printf("DEBUG: join_response is NULL or not a JSON object\n");
        fflush(stdout);
    }

    if (client_id) {
        free(client_id);
    }

    // Event listener already registered before join
    // Set state to lobby
    ctx->state = ONLINE_STATE_LOBBY;
    printf("DEBUG: State set to ONLINE_STATE_LOBBY\n");
    fflush(stdout);

    printf("DEBUG: online_multiplayer_join returning MPAPI_OK\n");
    fflush(stdout);
    return MPAPI_OK;
}

void online_multiplayer_client_send_input(OnlineMultiplayerContext *ctx, Direction dir)
{
    if (!ctx || !ctx->api) return;

    // Buffer input using the player's InputBuffer system
    // This validates 180-degree turns and queues up to 2 inputs
    int local_idx = ctx->game->local_player_index;
    if (local_idx < 0 || local_idx >= MAX_PLAYERS) return;

    MultiplayerPlayer *local_player = &ctx->game->players[local_idx];
    if (!local_player->alive || local_player->death_state != GAME_RUNNING) return;

    input_buffer_push(&local_player->input, dir, local_player->snake.dir);

    // Create JSON: {"dir": "UP|DOWN|LEFT|RIGHT", "segments": [...], "direction": dir}
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

    // Include current snake position for reconciliation
    json_t *segments = json_array();
    for (int i = 0; i < local_player->snake.length; i++) {
        json_array_append_new(segments, json_integer(local_player->snake.segments[i].x));
        json_array_append_new(segments, json_integer(local_player->snake.segments[i].y));
    }
    json_object_set_new(input, "segments", segments);
    json_object_set_new(input, "direction", json_integer(local_player->snake.dir));

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
    printf("DEBUG: online_multiplayer_start_game called\n");
    fflush(stdout);

    if (!ctx || !ctx->game) {
        printf("DEBUG: start_game - NULL check failed\n");
        fflush(stdout);
        return;
    }

    printf("DEBUG: Calling multiplayer_game_start\n");
    fflush(stdout);

    // Start the multiplayer game
    multiplayer_game_start(ctx->game);

    printf("DEBUG: multiplayer_game_start completed, initializing lives\n");
    fflush(stdout);

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

    printf("DEBUG: Setting state to COUNTDOWN\n");
    fflush(stdout);

    ctx->state = ONLINE_STATE_COUNTDOWN;

    // Broadcast game start to all clients (only host should call this)
    if (ctx->game->is_host) {
        printf("DEBUG: Broadcasting start_game command\n");
        fflush(stdout);

        // Host calculates their own start timestamp (3 seconds from now)
        unsigned int current_time = (unsigned int)SDL_GetTicks();
        ctx->game_start_timestamp = current_time + 3000;

        json_t *start_cmd = json_object();
        json_object_set_new(start_cmd, "command", json_string("start_game"));
        // Send RELATIVE delay (3000ms) instead of absolute timestamp
        json_object_set_new(start_cmd, "countdown_ms", json_integer(3000));

        int rc = mpapi_game(ctx->api, start_cmd, NULL);
        if (rc != MPAPI_OK) {
            printf("DEBUG: Broadcast failed with error %d\n", rc);
            fflush(stdout);
            ctx->connection_lost = 1;
        }

        json_decref(start_cmd);

        printf("DEBUG: Broadcast start_game command with 3000ms countdown\n");
        fflush(stdout);
    }

    printf("DEBUG: online_multiplayer_start_game completed successfully\n");
    fflush(stdout);
}

int online_multiplayer_get_local_player_index(OnlineMultiplayerContext *ctx)
{
    if (!ctx || !ctx->game) return -1;
    return ctx->game->local_player_index;
}

void online_multiplayer_toggle_ready(OnlineMultiplayerContext *ctx)
{
    if (!ctx || !ctx->game || ctx->state != ONLINE_STATE_LOBBY) return;

    int local_idx = ctx->game->local_player_index;
    if (local_idx < 0 || local_idx >= MAX_PLAYERS) return;

    printf("DEBUG: toggle_ready called, local_player_index=%d, is_host=%d\n", local_idx, ctx->game->is_host);
    fflush(stdout);

    // Toggle ready status
    ctx->game->players[local_idx].ready = !ctx->game->players[local_idx].ready;

    printf("DEBUG: Player %d ready status now: %d\n", local_idx, ctx->game->players[local_idx].ready);
    fflush(stdout);

    // Spawn/despawn snake based on ready status
    if (ctx->game->players[local_idx].ready) {
        // Spawn snake when readying up
        static const Vec2 START_POSITIONS[MAX_PLAYERS] = {
            {5, 5},    // Player 1: top-left area
            {34, 5},   // Player 2: top-right area
            {5, 34},   // Player 3: bottom-left area
            {34, 34}   // Player 4: bottom-right area
        };
        static const Direction START_DIRECTIONS[MAX_PLAYERS] = {
            DIR_RIGHT,  // Player 1
            DIR_LEFT,   // Player 2
            DIR_RIGHT,  // Player 3
            DIR_LEFT    // Player 4
        };

        snake_init(&ctx->game->players[local_idx].snake, START_POSITIONS[local_idx], START_DIRECTIONS[local_idx]);
        ctx->game->players[local_idx].alive = 0; // Not alive until game starts
        ctx->game->players[local_idx].death_state = GAME_RUNNING;
    } else {
        // Despawn snake when unreadying
        ctx->game->players[local_idx].snake.length = 0;
        ctx->game->players[local_idx].alive = 0;
    }

    // Broadcast ready status change
    json_t *ready_cmd = json_object();
    json_object_set_new(ready_cmd, "command", json_string("toggle_ready"));
    json_object_set_new(ready_cmd, "player_index", json_integer(local_idx));
    json_object_set_new(ready_cmd, "ready", json_boolean(ctx->game->players[local_idx].ready));

    int rc = mpapi_game(ctx->api, ready_cmd, NULL);
    if (rc != MPAPI_OK) {
        ctx->connection_lost = 1;
    }

    json_decref(ready_cmd);
}

int online_multiplayer_all_players_ready(OnlineMultiplayerContext *ctx)
{
    if (!ctx || !ctx->game) return 0;

    int joined_count = 0;
    int ready_count = 0;

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (ctx->game->players[i].joined) {
            joined_count++;
            printf("DEBUG: Player %d (%s) joined=1, ready=%d\n",
                   i, ctx->game->players[i].name, ctx->game->players[i].ready);
            if (ctx->game->players[i].ready) {
                ready_count++;
            }
        }
    }

    printf("DEBUG: all_players_ready check: joined=%d, ready=%d, result=%d\n",
           joined_count, ready_count, (joined_count > 0 && joined_count == ready_count));
    fflush(stdout);

    // Need at least 1 player, and all joined players must be ready
    return joined_count > 0 && joined_count == ready_count;
}

void online_multiplayer_reset_ready_states(OnlineMultiplayerContext *ctx)
{
    if (!ctx || !ctx->game) return;

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (ctx->game->players[i].joined) {
            ctx->game->players[i].ready = 0;
        }
    }

    printf("DEBUG: Reset all ready states\n");
    fflush(stdout);
}

// Event callback and handlers

static void mpapi_event_callback(const char *event, int64_t messageId,
                                 const char *clientId, json_t *data, void *context)
{
    (void)messageId; // Unused
    OnlineMultiplayerContext *ctx = (OnlineMultiplayerContext*)context;
    if (!ctx) return;

    printf("DEBUG: Received event '%s' from clientId '%s'\n", event, clientId ? clientId : "NULL");
    fflush(stdout);

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
    printf("DEBUG: handle_player_joined called for clientId=%s, is_host=%d\n", clientId, ctx->game->is_host);
    fflush(stdout);

    // Don't allow joins after game started
    if (ctx->state != ONLINE_STATE_LOBBY) return;

    // Check if this player is already in our list
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (ctx->game->players[i].joined && strcmp(ctx->game->players[i].client_id, clientId) == 0) {
            printf("DEBUG: Player %s already in slot %d\n", clientId, i);
            fflush(stdout);
            return; // Already added
        }
    }

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

    // Extract player name from join data
    const char *player_name = "Player";
    if (data && json_is_object(data)) {
        json_t *name_json = json_object_get(data, "name");
        if (name_json && json_is_string(name_json)) {
            player_name = json_string_value(name_json);
        }
    }
    strncpy(p->name, player_name, sizeof(p->name) - 1);
    p->name[sizeof(p->name) - 1] = '\0';

    p->is_local_player = 0;

    // Initialize snake (empty until game starts)
    p->snake.length = 0;
    p->snake.dir = DIR_RIGHT;
    p->death_state = GAME_RUNNING;

    // Initialize input buffer
    input_buffer_init(&p->input);

    // Initialize stats
    p->score = 0;
    p->fruits_eaten = 0;
    p->lives = INITIAL_LIVES;
    p->wins = 0;
    p->combo_count = 0;
    p->combo_expiry_time = 0;
    p->combo_best = 0;
    p->food_eaten_this_frame = 0;
    p->ready = 0;

    ctx->game->total_joined++;

    printf("DEBUG: Added player %s to slot %d (total_joined=%d)\n", clientId, slot, ctx->game->total_joined);
    fflush(stdout);

    // Broadcast updated lobby state
    online_multiplayer_host_broadcast_state(ctx);
}

static void handle_player_left(OnlineMultiplayerContext *ctx, const char *clientId)
{
    printf("DEBUG: Player left: %s\n", clientId);
    fflush(stdout);

    // Find player by client_id
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (ctx->game->players[i].joined &&
            strcmp(ctx->game->players[i].client_id, clientId) == 0) {

            printf("DEBUG: Removing player from slot %d\n", i);
            fflush(stdout);

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

    // If in game and too few players with lives, end game
    if (ctx->state == ONLINE_STATE_PLAYING) {
        int players_with_lives = 0;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (ctx->game->players[i].joined && ctx->game->players[i].lives > 0) {
                players_with_lives++;
            }
        }
        if (players_with_lives <= 1) {
            ctx->state = ONLINE_STATE_GAME_OVER;
        }
    }

    // Check if host disconnected (for clients)
    if (!ctx->game->is_host &&
        strcmp(clientId, ctx->game->host_client_id) == 0) {
        ctx->connection_lost = 1;
        strcpy(ctx->error_message, "Host disconnected");
        ctx->state = ONLINE_STATE_DISCONNECTED;
        return;
    }

    // Broadcast updated state to remaining clients (host only)
    if (ctx->game->is_host && (ctx->state == ONLINE_STATE_LOBBY || ctx->state == ONLINE_STATE_PLAYING)) {
        printf("DEBUG: Broadcasting updated state after player left\n");
        fflush(stdout);
        online_multiplayer_host_broadcast_state(ctx);
    }
}

static void handle_client_input(OnlineMultiplayerContext *ctx, const char *clientId, json_t *data)
{
    // Check for commands first
    json_t *command = json_object_get(data, "command");
    if (command && json_is_string(command)) {
        const char *cmd = json_string_value(command);

        if (strcmp(cmd, "player_disconnect") == 0) {
            // Handle player disconnect - same as leaved event
            printf("DEBUG: Received player_disconnect command from %s\n", clientId);
            fflush(stdout);
            handle_player_left(ctx, clientId);
            return;
        }
        else if (strcmp(cmd, "toggle_ready") == 0) {
            // Handle ready status change from client
            json_t *player_idx_json = json_object_get(data, "player_index");
            json_t *ready_json = json_object_get(data, "ready");

            if (player_idx_json && ready_json) {
                int player_idx = json_integer_value(player_idx_json);
                int ready = json_boolean_value(ready_json);

                if (player_idx >= 0 && player_idx < MAX_PLAYERS) {
                    ctx->game->players[player_idx].ready = ready;
                    printf("DEBUG: Host received toggle_ready for player %d, ready=%d\n", player_idx, ready);
                    fflush(stdout);

                    // Spawn/despawn snake based on ready status
                    if (ready) {
                        static const Vec2 START_POSITIONS[MAX_PLAYERS] = {
                            {5, 5},    {34, 5},   {5, 34},   {34, 34}
                        };
                        static const Direction START_DIRECTIONS[MAX_PLAYERS] = {
                            DIR_RIGHT,  DIR_LEFT,   DIR_RIGHT,  DIR_LEFT
                        };
                        snake_init(&ctx->game->players[player_idx].snake, START_POSITIONS[player_idx], START_DIRECTIONS[player_idx]);
                        ctx->game->players[player_idx].alive = 0;
                        ctx->game->players[player_idx].death_state = GAME_RUNNING;
                    } else {
                        ctx->game->players[player_idx].snake.length = 0;
                        ctx->game->players[player_idx].alive = 0;
                    }

                    // Broadcast updated ready status to all clients
                    printf("DEBUG: Broadcasting updated ready status\n");
                    fflush(stdout);
                    online_multiplayer_host_broadcast_state(ctx);
                }
            }
            return;
        }
    }

    // Check for death notification (client handled own death)
    json_t *player_died = json_object_get(data, "player_died");
    if (player_died && json_is_true(player_died)) {
        // Find player by client_id
        int player_idx = -1;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (ctx->game->players[i].joined &&
                strcmp(ctx->game->players[i].client_id, clientId) == 0) {
                player_idx = i;
                break;
            }
        }

        if (player_idx >= 0) {
            // Update player's lives from client's report (client is authoritative)
            json_t *lives_json = json_object_get(data, "lives");
            if (lives_json) {
                int client_lives = (int)json_integer_value(lives_json);
                ctx->game->players[player_idx].lives = client_lives;
                printf("DEBUG: Client %d died, lives now: %d\n", player_idx, client_lives);
                fflush(stdout);
            }

            // CRITICAL: Update death_state immediately to prevent food eating during death animation
            ctx->game->players[player_idx].death_state = GAME_DYING;

            // Broadcast death event to other clients
            json_t *broadcast = json_object();
            json_object_set_new(broadcast, "command", json_string("player_died"));
            json_object_set_new(broadcast, "player_index", json_integer(player_idx));
            mpapi_game(ctx->api, broadcast, NULL);
            json_decref(broadcast);
        }
        return;
    }

    // Check for food eaten notification (client ate food)
    json_t *food_eaten = json_object_get(data, "food_eaten");
    if (food_eaten && json_is_true(food_eaten)) {
        json_t *food_x = json_object_get(data, "food_x");
        json_t *food_y = json_object_get(data, "food_y");

        if (food_x && food_y) {
            Vec2 food_pos = {
                (int)json_integer_value(food_x),
                (int)json_integer_value(food_y)
            };

            // Remove the food that was eaten and generate new food
            if (vec2_equal(ctx->game->board.food, food_pos)) {
                // Regenerate main food using first joined player's snake
                for (int i = 0; i < MAX_PLAYERS; i++) {
                    if (ctx->game->players[i].joined) {
                        board_place_food(&ctx->game->board, &ctx->game->players[i].snake);
                        break;
                    }
                }
            } else {
                // Check additional food items
                for (int f = 0; f < ctx->game->food_count; f++) {
                    if (vec2_equal(ctx->game->food[f], food_pos)) {
                        // Remove this food by replacing with last food
                        ctx->game->food[f] = ctx->game->food[ctx->game->food_count - 1];
                        ctx->game->food_count--;
                        break;
                    }
                }
            }
        }
        return;
    }

    // Check for respawn notification (client respawned itself)
    json_t *player_respawned = json_object_get(data, "player_respawned");
    if (player_respawned && json_is_true(player_respawned)) {
        // Find player by client_id
        int player_idx = -1;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (ctx->game->players[i].joined &&
                strcmp(ctx->game->players[i].client_id, clientId) == 0) {
                player_idx = i;
                break;
            }
        }

        if (player_idx >= 0) {
            json_t *spawn_x = json_object_get(data, "spawn_x");
            json_t *spawn_y = json_object_get(data, "spawn_y");

            // Host broadcasts respawn event to other clients
            json_t *broadcast = json_object();
            json_object_set_new(broadcast, "command", json_string("player_respawned"));
            json_object_set_new(broadcast, "player_index", json_integer(player_idx));
            if (spawn_x && spawn_y) {
                json_object_set_new(broadcast, "spawn_x", json_integer(json_integer_value(spawn_x)));
                json_object_set_new(broadcast, "spawn_y", json_integer(json_integer_value(spawn_y)));
            }
            mpapi_game(ctx->api, broadcast, NULL);
            json_decref(broadcast);
            printf("DEBUG: Client %d respawned, broadcasting to other clients\n", player_idx);
            fflush(stdout);
        }
        return;
    }

    // Check for food added notification (client added food during death animation)
    json_t *food_added = json_object_get(data, "food_added");
    if (food_added && json_is_true(food_added)) {
        json_t *food_x = json_object_get(data, "food_x");
        json_t *food_y = json_object_get(data, "food_y");

        if (food_x && food_y) {
            Vec2 food_pos = {
                (int)json_integer_value(food_x),
                (int)json_integer_value(food_y)
            };

            // Add food to host's game state
            multiplayer_game_add_food(ctx->game, food_pos);

            // Broadcast food to all clients
            json_t *broadcast = json_object();
            json_object_set_new(broadcast, "command", json_string("food_added"));
            json_object_set_new(broadcast, "food_x", json_integer(food_pos.x));
            json_object_set_new(broadcast, "food_y", json_integer(food_pos.y));
            mpapi_game(ctx->api, broadcast, NULL);
            json_decref(broadcast);
        }
        return;
    }

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

    // Parse direction from JSON (optional for position-only updates)
    json_t *dir_json = json_object_get(data, "dir");
    if (dir_json && json_is_string(dir_json)) {
        const char *dir_str = json_string_value(dir_json);
        Direction dir;

        if (strcmp(dir_str, "UP") == 0) dir = DIR_UP;
        else if (strcmp(dir_str, "DOWN") == 0) dir = DIR_DOWN;
        else if (strcmp(dir_str, "LEFT") == 0) dir = DIR_LEFT;
        else if (strcmp(dir_str, "RIGHT") == 0) dir = DIR_RIGHT;
        else goto skip_input;

        // Validate player is alive
        if (ctx->game->players[player_idx].alive) {
            // Add to player's input buffer
            Direction current_dir = ctx->game->players[player_idx].snake.dir;
            input_buffer_push(&ctx->game->players[player_idx].input, dir, current_dir);
        }
    }

skip_input:

    // Client authoritative: directly apply reported position
    // Each client handles their own wall/self-collisions locally (no network sync needed)
    // Host only detects and broadcasts inter-player collisions
    json_t *segments = json_object_get(data, "segments");
    json_t *direction = json_object_get(data, "direction");

    MultiplayerPlayer *player = &ctx->game->players[player_idx];

    // Apply position update from client (accept in all states for smooth rendering)
    if (segments && json_is_array(segments)) {
        size_t arr_len = json_array_size(segments);
        player->snake.length = 0;

        // Directly apply client's position
        for (size_t i = 0; i + 1 < arr_len && player->snake.length < MAX_SNAKE_LEN; i += 2) {
            player->snake.segments[player->snake.length].x =
                (int)json_integer_value(json_array_get(segments, i));
            player->snake.segments[player->snake.length].y =
                (int)json_integer_value(json_array_get(segments, i + 1));
            player->snake.length++;
        }
    }

    // Update direction
    if (direction && json_is_integer(direction)) {
        player->snake.dir = (Direction)json_integer_value(direction);
    }

    // Update death_state so host knows to skip food eating for dying clients
    json_t *death_state_json = json_object_get(data, "death_state");
    if (death_state_json && json_is_integer(death_state_json)) {
        player->death_state = (GameState)json_integer_value(death_state_json);
    }

    // Update alive flag so host can correctly determine winner
    json_t *alive_json = json_object_get(data, "alive");
    if (alive_json && json_is_boolean(alive_json)) {
        player->alive = json_is_true(alive_json);
    }
}

static void handle_game_state_update(OnlineMultiplayerContext *ctx, json_t *data)
{
    // Check for commands first
    json_t *command = json_object_get(data, "command");
    if (command && json_is_string(command)) {
        const char *cmd = json_string_value(command);
        if (strcmp(cmd, "player_disconnect") == 0) {
            // Player disconnect is handled via leaved event or host broadcast
            // Just ignore this message on client side
            printf("DEBUG: Client ignoring player_disconnect command (handled via state update)\n");
            fflush(stdout);
            return;
        }
        else if (strcmp(cmd, "start_game") == 0) {
            printf("DEBUG: Received start_game command, transitioning to COUNTDOWN\n");
            fflush(stdout);

            // Read relative countdown delay from host
            json_t *countdown_json = json_object_get(data, "countdown_ms");
            unsigned int countdown_ms = 3000; // Default to 3 seconds
            if (countdown_json && json_is_integer(countdown_json)) {
                countdown_ms = (unsigned int)json_integer_value(countdown_json);
            }

            // Calculate our own absolute timestamp based on OUR clock
            unsigned int current_time = (unsigned int)SDL_GetTicks();
            ctx->game_start_timestamp = current_time + countdown_ms;
            printf("DEBUG: Client calculated game_start_timestamp: %u (current: %u, countdown: %u)\n",
                   ctx->game_start_timestamp, current_time, countdown_ms);
            fflush(stdout);

            ctx->state = ONLINE_STATE_COUNTDOWN;
            multiplayer_game_start(ctx->game);

            // Client-authoritative: Reset lives for all players when game starts
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

            return; // Don't deserialize, this is just a command
        }
        else if (strcmp(cmd, "game_over") == 0) {
            printf("DEBUG: Received game_over command, transitioning to GAME_OVER\n");
            fflush(stdout);
            ctx->state = ONLINE_STATE_GAME_OVER;
            return; // Don't deserialize, this is just a command
        }
        else if (strcmp(cmd, "toggle_ready") == 0) {
            // Handle ready status change from other players
            // (We already updated our own ready status locally, so skip if it's us)
            json_t *player_idx_json = json_object_get(data, "player_index");
            json_t *ready_json = json_object_get(data, "ready");

            if (player_idx_json && ready_json) {
                int player_idx = json_integer_value(player_idx_json);
                int ready = json_boolean_value(ready_json);

                // Don't update if this is our own toggle message coming back
                if (player_idx >= 0 && player_idx < MAX_PLAYERS &&
                    player_idx != ctx->game->local_player_index) {
                    ctx->game->players[player_idx].ready = ready;
                    printf("DEBUG: Received toggle_ready for player %d (not us), ready=%d\n", player_idx, ready);
                    fflush(stdout);

                    // Spawn/despawn snake based on ready status
                    if (ready) {
                        static const Vec2 START_POSITIONS[MAX_PLAYERS] = {
                            {5, 5},    {34, 5},   {5, 34},   {34, 34}
                        };
                        static const Direction START_DIRECTIONS[MAX_PLAYERS] = {
                            DIR_RIGHT,  DIR_LEFT,   DIR_RIGHT,  DIR_LEFT
                        };
                        snake_init(&ctx->game->players[player_idx].snake, START_POSITIONS[player_idx], START_DIRECTIONS[player_idx]);
                        ctx->game->players[player_idx].alive = 0;
                        ctx->game->players[player_idx].death_state = GAME_RUNNING;
                    } else {
                        ctx->game->players[player_idx].snake.length = 0;
                        ctx->game->players[player_idx].alive = 0;
                    }
                }
            }
            return; // Don't deserialize
        }
        else if (strcmp(cmd, "food_added") == 0) {
            // Client receives food added by another player's death animation
            json_t *food_x = json_object_get(data, "food_x");
            json_t *food_y = json_object_get(data, "food_y");

            if (food_x && food_y) {
                Vec2 food_pos = {
                    (int)json_integer_value(food_x),
                    (int)json_integer_value(food_y)
                };
                multiplayer_game_add_food(ctx->game, food_pos);
            }
            return; // Don't deserialize
        }
    }

    // Deserialize and update local game state
    online_multiplayer_deserialize_state(ctx->game, data);

    // Clear pending input if snake has turned to match it
    if (ctx->has_pending_input && ctx->game->local_player_index >= 0) {
        int local_idx = ctx->game->local_player_index;
        if (ctx->game->players[local_idx].snake.dir == ctx->pending_input) {
            ctx->has_pending_input = 0;
        }
    }

    // Find local player index if not set - match by client_id
    if (ctx->game->local_player_index == -1 && ctx->our_client_id[0] != '\0') {
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (ctx->game->players[i].joined &&
                strcmp(ctx->game->players[i].client_id, ctx->our_client_id) == 0) {
                ctx->game->local_player_index = i;
                ctx->game->players[i].is_local_player = 1;
                printf("DEBUG: Found local player at index %d via client_id match\n", i);
                fflush(stdout);
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

    // Note: Don't increment active_players - player was never removed from active count
}

static Vec2 find_safe_spawn_position(MultiplayerGame_s *game)
{
    // Try random positions until one is safe
    int max_attempts = 100;
    for (int attempt = 0; attempt < max_attempts; attempt++) {
        // Spawn at least 3 cells away from edges to avoid immediate wall collision
        int margin = 3;
        Vec2 candidate = {
            margin + (rand() % (game->board.width - 2 * margin)),
            margin + (rand() % (game->board.height - 2 * margin))
        };

        // Check if position and surrounding area are safe
        int safe = 1;

        // Check 3x3 area around spawn point to ensure some clearance
        for (int dx = -1; dx <= 1 && safe; dx++) {
            for (int dy = -1; dy <= 1 && safe; dy++) {
                Vec2 check = {candidate.x + dx, candidate.y + dy};

                // Check if any snake occupies this position
                for (int i = 0; i < MAX_PLAYERS; i++) {
                    // Check any snake with segments (including dying snakes)
                    if (game->players[i].snake.length > 0 &&
                        snake_occupies(&game->players[i].snake, check)) {
                        safe = 0;
                        break;
                    }
                }

                // Also avoid food positions in the area
                if (vec2_equal(check, game->board.food)) {
                    safe = 0;
                }
            }
        }

        if (safe) return candidate;
    }

    // Fallback: center of board
    Vec2 fallback = {game->board.width / 2, game->board.height / 2};
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

    // Lives (needed for respawn sync)
    json_object_set_new(p, "lives", json_integer(player->lives));

    // Food event flag (for client-side stat tracking) - sent as integer for compactness
    json_object_set_new(p, "ate", json_integer(player->food_eaten_this_frame));

    // Score and stats
    json_object_set_new(p, "score", json_integer(player->score));
    json_object_set_new(p, "fruits_eaten", json_integer(player->fruits_eaten));
    json_object_set_new(p, "combo_count", json_integer(player->combo_count));
    json_object_set_new(p, "combo_expiry_time", json_integer(player->combo_expiry_time));
    json_object_set_new(p, "combo_best", json_integer(player->combo_best));
    json_object_set_new(p, "wins", json_integer(player->wins));

    // Snake segments - COMPACT FORMAT: flat array [x1,y1,x2,y2,...]
    json_t *segments = json_array();
    for (int i = 0; i < player->snake.length; i++) {
        json_array_append_new(segments, json_integer(player->snake.segments[i].x));
        json_array_append_new(segments, json_integer(player->snake.segments[i].y));
    }
    json_object_set_new(p, "segments", segments);

    // Direction
    json_object_set_new(p, "direction", json_integer(player->snake.dir));

    // Network identity (client_id only, is_local is preserved locally)
    json_object_set_new(p, "client_id", json_string(player->client_id));
    json_object_set_new(p, "name", json_string(player->name));

    // Lobby state
    json_object_set_new(p, "ready", json_boolean(player->ready));

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
    // Preserve is_local_player flag - should not be overwritten by network data
    int was_local = player->is_local_player;
    GameState old_death_state = player->death_state;
    int old_alive = player->alive;
    int old_lives = player->lives;

    player->joined = json_is_true(json_object_get(data, "joined"));

    GameState net_death_state = (GameState)json_integer_value(json_object_get(data, "death_state"));
    int net_alive = json_is_true(json_object_get(data, "alive"));

    // Client-authoritative: Local player completely ignores network death_state, alive, and lives
    // Client handles own death, death animation, lives, and respawn
    if (was_local) {
        // Keep local death_state - client is fully authoritative
        player->death_state = old_death_state;
        // Keep local alive flag - client manages own death animation
        player->alive = old_alive;
        // Keep local lives count - client manages own lives
        player->lives = old_lives;
    } else {
        // Remote player: trust network death_state, alive, and lives
        player->death_state = net_death_state;
        player->alive = net_alive;
        player->lives = (int)json_integer_value(json_object_get(data, "lives"));
    }

    // Score and stats - now synced directly from host instead of recalculating
    player->score = (int)json_integer_value(json_object_get(data, "score"));
    player->fruits_eaten = (int)json_integer_value(json_object_get(data, "fruits_eaten"));
    player->combo_count = (int)json_integer_value(json_object_get(data, "combo_count"));
    player->combo_expiry_time = (unsigned int)json_integer_value(json_object_get(data, "combo_expiry_time"));
    player->combo_best = (int)json_integer_value(json_object_get(data, "combo_best"));
    player->wins = (int)json_integer_value(json_object_get(data, "wins"));

    // Food event flag (still used for sound effects on client)
    player->food_eaten_this_frame = (int)json_integer_value(json_object_get(data, "ate"));

    // Client-authoritative: Local player NEVER accepts position updates from network
    // Local player manages own movement, death animation, and respawn
    // Remote players: directly apply position from network (no prediction)
    if (!was_local) {
        json_t *segments = json_object_get(data, "segments");
        if (json_is_array(segments)) {
            size_t arr_len = json_array_size(segments);
            player->snake.length = 0;

            // Directly apply position from network
            for (size_t i = 0; i + 1 < arr_len && player->snake.length < MAX_SNAKE_LEN; i += 2) {
                player->snake.segments[player->snake.length].x =
                    (int)json_integer_value(json_array_get(segments, i));
                player->snake.segments[player->snake.length].y =
                    (int)json_integer_value(json_array_get(segments, i + 1));
                player->snake.length++;
            }
        }

        // Update direction from network
        player->snake.dir = (Direction)json_integer_value(json_object_get(data, "direction"));
    }
    // Local player keeps own simulated position (client authoritative)

    // Network identity
    json_t *client_id_json = json_object_get(data, "client_id");
    if (client_id_json && json_is_string(client_id_json)) {
        const char *cid = json_string_value(client_id_json);
        strncpy(player->client_id, cid, sizeof(player->client_id) - 1);
        player->client_id[sizeof(player->client_id) - 1] = '\0';
    }

    json_t *name_json = json_object_get(data, "name");
    if (name_json && json_is_string(name_json)) {
        const char *name = json_string_value(name_json);
        strncpy(player->name, name, sizeof(player->name) - 1);
        player->name[sizeof(player->name) - 1] = '\0';
    }

    // Restore is_local_player - don't let network data override this
    player->is_local_player = was_local;

    // Lobby state
    player->ready = json_is_true(json_object_get(data, "ready"));
}
