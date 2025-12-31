#include "scene_master.h"
#include "ui_sdl.h"
#include "settings.h"
#include "multiplayer.h"
#include "ui_helpers.h"
#include "text_sdl.h"
#include "constants.h"
#include "board.h"
#include "snake.h"
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>

#define MULTIPLAYER_TICK_MS 80
#define MAX_FOOD 10

/* Game state - HOST ONLY maintains this, clients render from received data */
typedef struct {
    Board board;
    Snake snakes[MAX_PLAYERS];
    Vec2 food[MAX_FOOD];
    int food_count;
    int scores[MAX_PLAYERS];
    int alive[MAX_PLAYERS];

    unsigned int last_tick;
    int countdown;
    unsigned int countdown_start;
    int game_started;
} MPGameState;

static MPGameState *state = NULL;

/* Serialize game state to JSON (HOST ONLY) */
static json_t* serialize_state(void)
{
    json_t *packet = json_object();
    json_object_set_new(packet, "type", json_string("game_state"));

    // Snakes
    json_t *snakes = json_array();
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (state->alive[i]) {
            json_t *snake = json_object();
            json_object_set_new(snake, "alive", json_integer(1));
            json_object_set_new(snake, "score", json_integer(state->scores[i]));

            json_t *segments = json_array();
            for (int j = 0; j < state->snakes[i].length; j++) {
                json_t *seg = json_object();
                json_object_set_new(seg, "x", json_integer(state->snakes[i].segments[j].x));
                json_object_set_new(seg, "y", json_integer(state->snakes[i].segments[j].y));
                json_array_append_new(segments, seg);
            }
            json_object_set_new(snake, "segments", segments);
            json_array_append_new(snakes, snake);
        } else {
            json_array_append_new(snakes, json_null());
        }
    }
    json_object_set_new(packet, "snakes", snakes);

    // Food
    json_t *food = json_array();
    for (int i = 0; i < state->food_count; i++) {
        json_t *f = json_object();
        json_object_set_new(f, "x", json_integer(state->food[i].x));
        json_object_set_new(f, "y", json_integer(state->food[i].y));
        json_array_append_new(food, f);
    }
    json_object_set_new(packet, "food", food);

    return packet;
}

/* Deserialize game state from JSON (CLIENT ONLY) */
static void deserialize_state(json_t *data)
{
    json_t *snakes = json_object_get(data, "snakes");
    if (snakes && json_is_array(snakes)) {
        for (size_t i = 0; i < json_array_size(snakes) && i < MAX_PLAYERS; i++) {
            json_t *snake = json_array_get(snakes, i);
            if (json_is_null(snake)) {
                state->alive[i] = 0;
            } else {
                state->alive[i] = json_integer_value(json_object_get(snake, "alive"));
                state->scores[i] = json_integer_value(json_object_get(snake, "score"));

                json_t *segments = json_object_get(snake, "segments");
                if (segments) {
                    state->snakes[i].length = json_array_size(segments);
                    for (size_t j = 0; j < json_array_size(segments) && j < MAX_SNAKE_LEN; j++) {
                        json_t *seg = json_array_get(segments, j);
                        state->snakes[i].segments[j].x = json_integer_value(json_object_get(seg, "x"));
                        state->snakes[i].segments[j].y = json_integer_value(json_object_get(seg, "y"));
                    }
                }
            }
        }
    }

    json_t *food = json_object_get(data, "food");
    if (food && json_is_array(food)) {
        state->food_count = json_array_size(food);
        for (int i = 0; i < state->food_count && i < MAX_FOOD; i++) {
            json_t *f = json_array_get(food, i);
            state->food[i].x = json_integer_value(json_object_get(f, "x"));
            state->food[i].y = json_integer_value(json_object_get(f, "y"));
        }
    }
}

/* Event handler */
static void event_handler(const char *event, int64_t messageId,
                          const char *clientId, json_t *data, void *context)
{
    (void)messageId;
    (void)context;

    SceneContext *ctx = scene_master_get_context();
    if (!ctx || !ctx->multiplayer || !state) return;

    if (strcmp(event, "game") == 0) {
        json_t *type = json_object_get(data, "type");
        if (!type) return;

        const char *type_str = json_string_value(type);

        // CLIENT: Receive state from host
        if (!ctx->multiplayer->is_host && strcmp(type_str, "game_state") == 0) {
            deserialize_state(data);
        }

        // HOST: Receive direction from client
        if (ctx->multiplayer->is_host && strcmp(type_str, "direction") == 0) {
            int player_idx = json_integer_value(json_object_get(data, "player"));
            int dir = json_integer_value(json_object_get(data, "dir"));

            if (player_idx >= 0 && player_idx < MAX_PLAYERS && state->alive[player_idx]) {
                state->snakes[player_idx].dir = (Direction)dir;
            }
        }
    }
}

/* HOST ONLY: Run game tick */
static void host_tick(Multiplayer *mp)
{
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!state->alive[i] || !mp->players[i].joined) continue;

        Vec2 head = snake_head(&state->snakes[i]);
        Vec2 new_head = head;

        switch (state->snakes[i].dir) {
            case DIR_UP:    new_head.y -= 1; break;
            case DIR_DOWN:  new_head.y += 1; break;
            case DIR_LEFT:  new_head.x -= 1; break;
            case DIR_RIGHT: new_head.x += 1; break;
        }

        // Wall collision
        if (board_out_of_bounds(&state->board, new_head)) {
            state->alive[i] = 0;
            continue;
        }

        // Food collision
        int grow = 0;
        for (int f = 0; f < state->food_count; f++) {
            if (new_head.x == state->food[f].x && new_head.y == state->food[f].y) {
                grow = 1;
                state->scores[i] += 10;
                state->food[f] = state->food[--state->food_count];

                // Spawn new food
                if (state->food_count < MAX_FOOD) {
                    Vec2 new_food = {rand() % state->board.width, rand() % state->board.height};
                    state->food[state->food_count++] = new_food;
                }
                break;
            }
        }

        // Self collision
        if (snake_occupies(&state->snakes[i], new_head)) {
            state->alive[i] = 0;
            continue;
        }

        // Move
        snake_step_to(&state->snakes[i], new_head, grow);
    }

    // Broadcast state
    json_t *packet = serialize_state();
    if (packet && mp->api) {
        mpapi_game(mp->api, packet, NULL);
        json_decref(packet);
    }
}

static void multiplayer_game_scene_init(UiSdl *ui, Settings *settings, void *user_data)
{
    (void)ui;
    (void)settings;
    (void)user_data;

    state = (MPGameState *)malloc(sizeof(MPGameState));
    memset(state, 0, sizeof(MPGameState));

    SceneContext *ctx = scene_master_get_context();
    if (!ctx || !ctx->multiplayer) return;

    // Register event handler
    if (ctx->multiplayer->api) {
        mpapi_listen(ctx->multiplayer->api, event_handler, NULL);
    }

    // HOST: Initialize game
    if (ctx->multiplayer->is_host) {
        board_init(&state->board, 40, 40);

        Vec2 positions[4] = {{10, 10}, {30, 10}, {10, 30}, {30, 30}};
        Direction dirs[4] = {DIR_RIGHT, DIR_LEFT, DIR_RIGHT, DIR_LEFT};

        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (ctx->multiplayer->players[i].joined) {
                snake_init(&state->snakes[i], positions[i], dirs[i]);
                state->alive[i] = 1;
                state->scores[i] = 0;
            }
        }

        // Spawn initial food
        state->food_count = 1;
        state->food[0] = (Vec2){rand() % 40, rand() % 40};
    } else {
        // CLIENT: Just init board for rendering
        board_init(&state->board, 40, 40);
    }

    state->countdown = 3;
    state->countdown_start = SDL_GetTicks();
    state->game_started = 0;
    state->last_tick = SDL_GetTicks();
}

static void multiplayer_game_scene_update(UiSdl *ui, Settings *settings)
{
    (void)ui;

    SceneContext *ctx = scene_master_get_context();
    if (!ctx || !ctx->multiplayer || !state) return;

    // Countdown
    if (state->countdown > 0) {
        unsigned int now = SDL_GetTicks();
        if (now - state->countdown_start >= 1000) {
            state->countdown--;
            state->countdown_start = now;
            if (state->countdown == 0) {
                state->game_started = 1;
                state->last_tick = now;
            }
        }
        return;
    }

    // Input
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            scene_master_transition(SCENE_QUIT, NULL);
            return;
        }
        if (e.type == SDL_KEYDOWN) {
            Direction dir = DIR_UP;
            int pressed = 0;

            if (e.key.keysym.sym == settings->keybindings[SETTING_ACTION_UP]) { dir = DIR_UP; pressed = 1; }
            else if (e.key.keysym.sym == settings->keybindings[SETTING_ACTION_DOWN]) { dir = DIR_DOWN; pressed = 1; }
            else if (e.key.keysym.sym == settings->keybindings[SETTING_ACTION_LEFT]) { dir = DIR_LEFT; pressed = 1; }
            else if (e.key.keysym.sym == settings->keybindings[SETTING_ACTION_RIGHT]) { dir = DIR_RIGHT; pressed = 1; }
            else if (e.key.keysym.sym == SDLK_ESCAPE) {
                scene_master_transition(SCENE_MULTIPLAYER_LOBBY, NULL);
                return;
            }

            if (pressed) {
                int idx = ctx->multiplayer->local_player_index;
                if (idx >= 0 && idx < MAX_PLAYERS) {
                    if (ctx->multiplayer->is_host) {
                        // Host: direct update
                        state->snakes[idx].dir = dir;
                    } else {
                        // Client: send to host
                        json_t *packet = json_object();
                        json_object_set_new(packet, "type", json_string("direction"));
                        json_object_set_new(packet, "player", json_integer(idx));
                        json_object_set_new(packet, "dir", json_integer(dir));
                        mpapi_game(ctx->multiplayer->api, packet, NULL);
                        json_decref(packet);
                    }
                }
            }
        }
    }

    // HOST: Game tick
    if (ctx->multiplayer->is_host && state->game_started) {
        unsigned int now = SDL_GetTicks();
        if (now - state->last_tick >= MULTIPLAYER_TICK_MS) {
            host_tick(ctx->multiplayer);
            state->last_tick = now;

            // Check game over
            int alive_count = 0;
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (state->alive[i]) alive_count++;
            }
            if (alive_count <= 1) {
                SDL_Delay(2000);
                scene_master_transition(SCENE_MULTIPLAYER_LOBBY, NULL);
            }
        }
    }
}

static void multiplayer_game_scene_render(UiSdl *ui, Settings *settings)
{
    (void)settings;

    SceneContext *ctx = scene_master_get_context();
    if (!ctx || !ctx->multiplayer || !state) return;

    SDL_SetRenderDrawColor(ui->ren, 20, 20, 30, 255);
    SDL_RenderClear(ui->ren);

    // Compute layout
    int cell = 16;
    int ox = 100;
    int oy = 100;

    // Render food
    for (int i = 0; i < state->food_count; i++) {
        int fx = ox + state->food[i].x * cell;
        int fy = oy + state->food[i].y * cell;
        SDL_Rect r = {fx, fy, cell, cell};
        SDL_SetRenderDrawColor(ui->ren, 255, 255, 0, 255);
        SDL_RenderFillRect(ui->ren, &r);
    }

    // Render snakes
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (state->alive[i]) {
            for (int j = 0; j < state->snakes[i].length; j++) {
                int sx = ox + state->snakes[i].segments[j].x * cell;
                int sy = oy + state->snakes[i].segments[j].y * cell;
                SDL_Rect r = {sx, sy, cell, cell};

                if (j == 0) {
                    SDL_SetRenderDrawColor(ui->ren, PLAYER_COLORS[i].r, PLAYER_COLORS[i].g, PLAYER_COLORS[i].b, 255);
                } else {
                    int cr = (int)(PLAYER_COLORS[i].r * 0.7);
                    int cg = (int)(PLAYER_COLORS[i].g * 0.7);
                    int cb = (int)(PLAYER_COLORS[i].b * 0.7);
                    SDL_SetRenderDrawColor(ui->ren, cr, cg, cb, 255);
                }
                SDL_RenderFillRect(ui->ren, &r);
            }
        }
    }

    // Scores
    if (ui->text_ok) {
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (ctx->multiplayer->players[i].joined) {
                char text[64];
                snprintf(text, sizeof(text), "%s: %d", ctx->multiplayer->players[i].name, state->scores[i]);
                text_draw(ui->ren, &ui->text, 10, 10 + i * 20, text);
            }
        }
    }

    // Countdown
    if (state->countdown > 0 && ui->text_ok) {
        char text[32];
        snprintf(text, sizeof(text), "%d", state->countdown);
        text_draw_center(ui->ren, &ui->text, ui->w / 2, ui->h / 2, text);
    }

    SDL_RenderPresent(ui->ren);
    SDL_Delay(16);
}

static void multiplayer_game_scene_cleanup(void)
{
    if (state) {
        free(state);
        state = NULL;
    }
}

void scene_multiplayer_game_register(void)
{
    Scene scene = {
        .init = multiplayer_game_scene_init,
        .update = multiplayer_game_scene_update,
        .render = multiplayer_game_scene_render,
        .cleanup = multiplayer_game_scene_cleanup
    };

    scene_master_register_scene(SCENE_MULTIPLAYER_GAME, scene);
}
