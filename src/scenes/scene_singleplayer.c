#include "scene_master.h"
#include "ui_sdl.h"
#include "settings.h"
#include "config.h"
#include "game.h"
#include "input_buffer.h"
#include "audio_sdl.h"
#include "constants.h"
#include <stdlib.h>
#include <math.h>
#include <SDL2/SDL.h>

/* Scene state */
typedef struct {
    Game game;
    InputBuffer input;
    char player_name[32];
    int debug_mode;
    unsigned int last_tick;
    unsigned int last_death_tick;
    unsigned int current_tick_ms;
    int paused;
    int pause_menu_selected;
    int pending_save_this_round;
} SingleplayerSceneData;

static SingleplayerSceneData *state = NULL;

/* Calculate tick time based on combo count (smooth exponential curve) */
static int tick_ms_for_combo(int combo_count)
{
    float t = SPEED_FLOOR_MS + (SPEED_START_MS - SPEED_FLOOR_MS) * expf(-SPEED_CURVE_K * (float)combo_count);
    return (int)(t + 0.5f);
}

static void singleplayer_scene_init(UiSdl *ui, Settings *settings, void *user_data)
{
    (void)ui;
    (void)user_data;

    state = (SingleplayerSceneData *)malloc(sizeof(SingleplayerSceneData));
    if (state) {
        /* Get config from scene context */
        SceneContext *ctx = scene_master_get_context();
        int board_w = (ctx && ctx->config) ? ctx->config->sp_board_width : 20;
        int board_h = (ctx && ctx->config) ? ctx->config->sp_board_height : 20;

        /* Initialize game */
        game_init(&state->game, board_w, board_h);
        state->game.start_time = SDL_GetTicks();
        state->game.combo_window_ms = TICK_MS * 5;  /* TODO: Get from config */

        /* Initialize input buffer */
        input_buffer_init(&state->input);

        /* Set player name from settings */
        if (settings->profile_name[0] != '\0') {
            snprintf(state->player_name, sizeof(state->player_name), "%s", settings->profile_name);
        } else {
            snprintf(state->player_name, sizeof(state->player_name), "Player");
        }

        state->debug_mode = 0;  /* TODO: Get from somewhere */
        state->last_tick = SDL_GetTicks();
        state->last_death_tick = 0;
        state->current_tick_ms = (unsigned int)SPEED_START_MS;
        state->paused = 0;
        state->pause_menu_selected = 0;
        state->pending_save_this_round = 1;

        /* Ensure music is playing if volume is set */
        if (ctx && ctx->audio) {
            /* If music volume is > 0, ensure music is playing */
            if (settings->music_volume > 0) {
                if (!audio_sdl_is_music_playing(ctx->audio)) {
                    /* Music stopped, try to play it */
                    if (audio_sdl_load_music(ctx->audio, "assets/music/background.wav")) {
                        audio_sdl_play_music(ctx->audio, -1);
                    }
                }
            }
        }
    }
}

static void singleplayer_scene_update(UiSdl *ui, Settings *settings)
{
    if (!state) return;

    /* Handle pause */
    if (state->paused) {
        int quit = 0;
        UiPauseAction action = ui_sdl_poll_pause(ui, settings, &quit);

        if (quit) {
            scene_master_transition(SCENE_QUIT, NULL);
            return;
        }

        /* Handle menu navigation */
        if (action == UI_PAUSE_UP) {
            state->pause_menu_selected = (state->pause_menu_selected - 1 + 3) % 3;
        } else if (action == UI_PAUSE_DOWN) {
            state->pause_menu_selected = (state->pause_menu_selected + 1) % 3;
        } else if (action == UI_PAUSE_SELECT) {
            /* Handle selection: 0=Resume, 1=Options, 2=Quit */
            if (state->pause_menu_selected == 0) {
                /* Resume */
                state->paused = 0;
                state->last_tick = SDL_GetTicks();
                SceneContext *ctx = scene_master_get_context();
                if (ctx && ctx->audio) {
                    audio_sdl_resume_music(ctx->audio);
                }
            } else if (state->pause_menu_selected == 1) {
                /* Options - TODO: implement without losing game state */
                /* For now, do nothing - options would lose game progress */
                /* Just resume instead */
                state->paused = 0;
                state->last_tick = SDL_GetTicks();
                SceneContext *ctx = scene_master_get_context();
                if (ctx && ctx->audio) {
                    audio_sdl_resume_music(ctx->audio);
                }
            } else if (state->pause_menu_selected == 2) {
                /* Quit to menu */
                scene_master_transition(SCENE_MENU, NULL);
            }
        } else if (action == UI_PAUSE_ESCAPE) {
            /* ESC = Resume */
            state->paused = 0;
            state->last_tick = SDL_GetTicks();
            SceneContext *ctx = scene_master_get_context();
            if (ctx && ctx->audio) {
                audio_sdl_resume_music(ctx->audio);
            }
        }
        return;
    }

    /* Poll input */
    int has_dir = 0;
    Direction dir = DIR_RIGHT;
    int pause = 0;
    int running = ui_sdl_poll(ui, settings, &has_dir, &dir, &pause);

    if (!running) {
        scene_master_transition(SCENE_QUIT, NULL);
        return;
    }

    if (pause && state->game.state == GAME_RUNNING) {
        state->paused = 1;
        SceneContext *ctx = scene_master_get_context();
        if (ctx && ctx->audio) {
            audio_sdl_pause_music(ctx->audio);
        }
        return;
    }

    /* Buffer input */
    if (has_dir) {
        input_buffer_push(&state->input, dir, state->game.snake.dir);
    }

    /* Update combo timer */
    unsigned int now = SDL_GetTicks();
    int prev_combo = state->game.combo_count;
    game_update_combo_timer(&state->game, now);

    /* If combo was lost, reset speed to starting speed */
    if (prev_combo > 0 && state->game.combo_count == 0) {
        state->current_tick_ms = tick_ms_for_combo(0);
    }

    /* Game tick */
    if (now - state->last_tick >= state->current_tick_ms) {
        /* Pop input from buffer */
        Direction buffered_dir;
        if (input_buffer_pop(&state->input, &buffered_dir)) {
            snake_change_direction(&state->game.snake, buffered_dir);
        }

        /* Update game */
        game_update(&state->game);
        state->last_tick = now;

        /* Update speed based on combo count */
        if (state->game.food_eaten_this_frame) {
            state->current_tick_ms = tick_ms_for_combo(state->game.combo_count);
        }

        /* Handle combo SFX and timer update if food was eaten */
        if (state->game.food_eaten_this_frame) {
            SceneContext *ctx = scene_master_get_context();

            /* Update combo window based on current game speed and tier */
            int tier = game_get_combo_tier(state->game.combo_count);
            int window_ticks = (ctx && ctx->config)
                ? ctx->config->combo_window_ticks + (tier - 1) * ctx->config->combo_window_increase_per_tier
                : 5 + (tier - 1) * 2;  /* Fallback defaults */
            state->game.combo_window_ms = state->current_tick_ms * window_ticks;

            /* Update combo expiry time */
            state->game.combo_expiry_time = now + state->game.combo_window_ms;

            /* Play combo SFX */
            if (ctx && ctx->audio) {
                char sfx_name[32];
                snprintf(sfx_name, sizeof(sfx_name), "combo%d", tier);
                audio_sdl_play_sound(ctx->audio, sfx_name);
            }
        }

    }

    /* Handle death animation (fixed 50ms per segment) */
    if (state->game.state == GAME_DYING) {
        /* Initialize death timer on first death frame */
        if (state->last_death_tick == 0) {
            state->last_death_tick = now;
        }

        /* Update animation every 50ms */
        if ((now - state->last_death_tick) >= 50) {
            state->last_death_tick = now;

            /* Remove one segment and play explosion sound */
            SceneContext *ctx = scene_master_get_context();
            if (ctx && ctx->audio) {
                audio_sdl_play_sound(ctx->audio, "explosion");
            }

            game_update_death_animation(&state->game);
        }
    } else {
        /* Reset death timer when not dying */
        state->last_death_tick = 0;
    }

    /* Transition to menu on GAME_OVER */
    if (state->game.state == GAME_OVER && state->pending_save_this_round) {
        /* Freeze time at death */
        state->game.death_time = SDL_GetTicks();

        /* TODO: Save score to scoreboard, transition to game over scene */
        /* For now, just go to menu */
        state->pending_save_this_round = 0;
        scene_master_transition(SCENE_MENU, NULL);
        return;
    }
}

static void singleplayer_scene_render(UiSdl *ui, Settings *settings)
{
    (void)settings;
    if (!state) return;

    unsigned int now = SDL_GetTicks();

    if (state->paused) {
        /* Render pause menu */
        ui_sdl_render_pause_menu(ui, &state->game, state->player_name,
                                  state->pause_menu_selected, state->debug_mode, now);
    } else {
        /* Render normal game */
        ui_sdl_render(ui, &state->game, state->player_name, state->debug_mode, now);
    }

    SDL_Delay(GAME_FRAME_DELAY_MS);
}

static void singleplayer_scene_cleanup(void)
{
    if (state) {
        /* Don't stop music - let it continue playing */

        free(state);
        state = NULL;
    }
}

/* Register this scene with scene_master */
void scene_singleplayer_register(void)
{
    Scene scene = {
        .init = singleplayer_scene_init,
        .update = singleplayer_scene_update,
        .render = singleplayer_scene_render,
        .cleanup = singleplayer_scene_cleanup
    };

    scene_master_register_scene(SCENE_SINGLEPLAYER, scene);
}
