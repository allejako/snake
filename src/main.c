// Port == 9001, identifier == uuid

#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "common.h"
#include "scoreboard.h"
#include "ui_sdl.h"
#include "input_buffer.h"
#include "keybindings.h"
#include "audio_sdl.h"
#include <SDL2/SDL_ttf.h>

#define BOARD_WIDTH 40
#define BOARD_HEIGHT 30
#define TICK_MS 120
#define PAUSE_MENU_COUNT 3
#define WINDOW_WIDTH 960
#define WINDOW_HEIGHT 640
#define MENU_FRAME_DELAY_MS 16 // ~60 FPS for menus
#define GAME_FRAME_DELAY_MS 1 // Minimal delay for gameplay

typedef enum
{
    APP_MENU = 0,
    APP_SINGLEPLAYER,
    APP_MULTIPLAYER,
    APP_SCOREBOARD,
    APP_QUIT,
    APP_OPTIONS_MENU,
    APP_KEYBINDS_PLAYER_SELECT,
    APP_KEYBINDS_BINDING,
    APP_SOUND_SETTINGS,
    APP_GAME_MODE_SELECT,
    APP_SPEED_SELECT
} AppState;

typedef enum
{
    MENU_SINGLEPLAYER = 0,
    MENU_MULTIPLAYER,
    MENU_OPTIONS,
    MENU_SCOREBOARD,
    MENU_QUIT,
    MENU_COUNT
} MenuItem;

typedef enum
{
    OPTIONS_MENU_KEYBINDS = 0,
    OPTIONS_MENU_SOUND,
    OPTIONS_MENU_BACK,
    OPTIONS_MENU_COUNT
} OptionsMenuItem;

typedef enum
{
    KEYBIND_PLAYER_1 = 0,
    KEYBIND_PLAYER_2,
    KEYBIND_PLAYER_3,
    KEYBIND_PLAYER_4,
    KEYBIND_BACK,
    KEYBIND_PLAYER_COUNT
} KeybindPlayerMenuItem;

typedef enum
{
    SOUND_MENU_MUSIC_VOLUME = 0,
    SOUND_MENU_EFFECTS_VOLUME,
    SOUND_MENU_BACK,
    SOUND_MENU_COUNT
} SoundMenuItem;

typedef enum
{
    GAME_MODE_CLASSIC = 0,
    GAME_MODE_MODERN,
    GAME_MODE_VERSUS,
    GAME_MODE_COUNT
} GameMode;

typedef enum
{
    CLASSIC_SPEED_SLOW = 0,
    CLASSIC_SPEED_NORMAL,
    CLASSIC_SPEED_FAST,
    CLASSIC_SPEED_COUNT
} ClassicSpeed;

/**
 * Application context containing all state needed by state handlers.
 * This struct uses pointers to allow state handlers to modify values.
 */
typedef struct
{
    UiSdl *ui;                    // UI system handle
    AudioSdl *audio;              // Audio system handle
    Keybindings *keybindings;     // Keybindings configuration
    Scoreboard *sb;               // Scoreboard for high scores
    AppState *state;              // Current application state
    int *menu_selected;           // Main menu cursor position
    int *options_menu_selected;   // Options menu cursor position
    int *keybind_player_selected; // Keybind player select cursor
    int *keybind_current_player;  // Currently configuring player (0-3)
    int *keybind_current_action;  // Currently configuring action (0-4)
    int *sound_selected;          // Sound settings cursor position
    int *game_mode_selected;          // Game mode menu cursor
    int *speed_selected;              // Speed selection cursor
    GameMode *current_game_mode;      // Currently selected game mode
    ClassicSpeed *current_classic_speed; // Persistent speed selection
    unsigned int *current_tick_ms;    // Runtime-variable tick speed
    int *modern_mode_score_at_last_speed_update; // Track Modern mode speed changes
    Game *game;                   // Game state
    InputBuffer *input;           // Input buffer for gameplay
    char *player_name;            // Current player name
    int *paused;                  // Whether game is paused
    int *pause_selected;          // Pause menu cursor position (0-2)
    int *pause_in_options;        // Whether in pause options screen
    unsigned int *last_tick;      // Last game update tick (ms)
    int *pending_save_this_round; // Whether score should be saved on game over
} AppContext;

/**
 * Music monitoring disabled - no longer needed with Simple Audio
 * This was used to debug SDL_mixer stuttering issues
 */
static void ensure_music_playing(AppContext *ctx)
{
    (void)ctx; // Unused - music monitoring disabled
    // Music monitoring removed since Simple Audio handles playback reliably
}

/**
 * Handle main menu state - navigate menu and launch game modes.
 * Updates state to transition to selected mode (singleplayer, multiplayer, options, etc.)
 */
static void handle_menu_state(AppContext *ctx)
{
    // Ensure music is playing
    ensure_music_playing(ctx);

    int quit = 0;
    UiMenuAction action = ui_sdl_poll_menu(ctx->ui, ctx->keybindings, &quit);
    if (quit)
    {
        *ctx->state = APP_QUIT;
        return;
    }

    if (action == UI_MENU_UP)
    {
        *ctx->menu_selected = (*ctx->menu_selected - 1 + MENU_COUNT) % MENU_COUNT;
    }
    else if (action == UI_MENU_DOWN)
    {
        *ctx->menu_selected = (*ctx->menu_selected + 1) % MENU_COUNT;
    }
    else if (action == UI_MENU_SELECT)
    {
        switch (*ctx->menu_selected)
        {
        case MENU_SINGLEPLAYER:
            *ctx->game_mode_selected = 0;
            *ctx->state = APP_GAME_MODE_SELECT;
            break;

        case MENU_MULTIPLAYER:
            *ctx->state = APP_MULTIPLAYER;
            break;

        case MENU_OPTIONS:
            *ctx->state = APP_OPTIONS_MENU;
            *ctx->options_menu_selected = 0;
            break;

        case MENU_SCOREBOARD:
            *ctx->state = APP_SCOREBOARD;
            break;

        case MENU_QUIT:
            *ctx->state = APP_QUIT;
            break;
        }
    }

    ui_sdl_render_menu(ctx->ui, *ctx->menu_selected);
    SDL_Delay(MENU_FRAME_DELAY_MS);
}

/**
 * Handle game mode selection state - choose between Classic, Modern, and Versus.
 * Classic mode proceeds to speed selection, Modern starts immediately, Versus is placeholder.
 */
static void handle_game_mode_select_state(AppContext *ctx)
{
    int quit = 0;
    UiMenuAction action = ui_sdl_poll_game_mode_select(ctx->ui, ctx->keybindings, &quit);
    if (quit)
    {
        *ctx->state = APP_QUIT;
        return;
    }

    if (action == UI_MENU_UP)
    {
        *ctx->game_mode_selected = (*ctx->game_mode_selected - 1 + GAME_MODE_COUNT) % GAME_MODE_COUNT;
    }
    else if (action == UI_MENU_DOWN)
    {
        *ctx->game_mode_selected = (*ctx->game_mode_selected + 1) % GAME_MODE_COUNT;
    }
    else if (action == UI_MENU_SELECT)
    {
        switch (*ctx->game_mode_selected)
        {
        case GAME_MODE_CLASSIC:
            *ctx->current_game_mode = GAME_MODE_CLASSIC;
            *ctx->state = APP_SPEED_SELECT;
            *ctx->speed_selected = 0;
            break;

        case GAME_MODE_MODERN:
            *ctx->current_game_mode = GAME_MODE_MODERN;
            // Start Modern mode immediately - set tick speed to 80ms
            *ctx->current_tick_ms = 80;
            game_init(ctx->game, BOARD_WIDTH, BOARD_HEIGHT);
            input_buffer_clear(ctx->input);
            *ctx->last_tick = (unsigned int)SDL_GetTicks();
            *ctx->pending_save_this_round = 1;
            *ctx->modern_mode_score_at_last_speed_update = 0;
            *ctx->state = APP_SINGLEPLAYER;
            break;

        case GAME_MODE_VERSUS:
            // Placeholder - show coming soon
            *ctx->state = APP_MULTIPLAYER;
            break;
        }
    }
    else if (action == UI_MENU_BACK)
    {
        *ctx->state = APP_MENU;
    }

    ui_sdl_render_game_mode_select(ctx->ui, *ctx->game_mode_selected);
    SDL_Delay(MENU_FRAME_DELAY_MS);
}

/**
 * Handle speed selection state - choose game speed for Classic mode.
 * Speed selection persists across games.
 */
static void handle_speed_select_state(AppContext *ctx)
{
    int quit = 0;
    UiMenuAction action = ui_sdl_poll_speed_select(ctx->ui, ctx->keybindings, &quit);
    if (quit)
    {
        *ctx->state = APP_QUIT;
        return;
    }

    if (action == UI_MENU_UP)
    {
        *ctx->speed_selected = (*ctx->speed_selected - 1 + CLASSIC_SPEED_COUNT) % CLASSIC_SPEED_COUNT;
    }
    else if (action == UI_MENU_DOWN)
    {
        *ctx->speed_selected = (*ctx->speed_selected + 1) % CLASSIC_SPEED_COUNT;
    }
    else if (action == UI_MENU_SELECT)
    {
        // Update persistent speed selection
        *ctx->current_classic_speed = (ClassicSpeed)*ctx->speed_selected;

        // Convert ClassicSpeed to milliseconds
        switch (*ctx->current_classic_speed)
        {
        case CLASSIC_SPEED_SLOW:
            *ctx->current_tick_ms = 130;
            break;
        case CLASSIC_SPEED_NORMAL:
            *ctx->current_tick_ms = 100;
            break;
        case CLASSIC_SPEED_FAST:
            *ctx->current_tick_ms = 70;
            break;
        default:
            *ctx->current_tick_ms = 100;
        }

        // Initialize game with selected speed and go directly to gameplay
        game_init(ctx->game, BOARD_WIDTH, BOARD_HEIGHT);
        input_buffer_clear(ctx->input);
        *ctx->last_tick = (unsigned int)SDL_GetTicks();
        *ctx->pending_save_this_round = 1;
        *ctx->state = APP_SINGLEPLAYER;
    }
    else if (action == UI_MENU_BACK)
    {
        *ctx->state = APP_GAME_MODE_SELECT;
    }

    ui_sdl_render_speed_select(ctx->ui, *ctx->speed_selected);
    SDL_Delay(MENU_FRAME_DELAY_MS);
}

/**
 * Handle options menu state - navigate between Keybinds and Back.
 * Updates state to transition to keybind configuration or back to main menu.
 */
static void handle_options_menu_state(AppContext *ctx)
{
    int quit = 0;
    UiMenuAction action = ui_sdl_poll_options_menu(ctx->ui, ctx->keybindings, &quit);
    if (quit)
    {
        *ctx->state = APP_QUIT;
        return;
    }

    if (action == UI_MENU_UP)
    {
        *ctx->options_menu_selected = (*ctx->options_menu_selected - 1 + OPTIONS_MENU_COUNT) % OPTIONS_MENU_COUNT;
    }
    else if (action == UI_MENU_DOWN)
    {
        *ctx->options_menu_selected = (*ctx->options_menu_selected + 1) % OPTIONS_MENU_COUNT;
    }
    else if (action == UI_MENU_SELECT)
    {
        if (*ctx->options_menu_selected == OPTIONS_MENU_KEYBINDS)
        {
            *ctx->state = APP_KEYBINDS_PLAYER_SELECT;
            *ctx->keybind_player_selected = 0;
        }
        else if (*ctx->options_menu_selected == OPTIONS_MENU_SOUND)
        {
            *ctx->state = APP_SOUND_SETTINGS;
            *ctx->sound_selected = 0;
        }
        else if (*ctx->options_menu_selected == OPTIONS_MENU_BACK)
        {
            *ctx->state = APP_MENU;
        }
    }
    else if (action == UI_MENU_BACK)
    {
        *ctx->state = APP_MENU;
    }

    ui_sdl_render_options_menu(ctx->ui, *ctx->options_menu_selected);
    SDL_Delay(MENU_FRAME_DELAY_MS);
}

/**
 * Handle keybind player selection state - choose which player to configure.
 * Supports up to 4 players. Updates state to begin binding keys for selected player.
 */
static void handle_keybinds_player_select_state(AppContext *ctx)
{
    int quit = 0;
    UiMenuAction action = ui_sdl_poll_keybind_player_select(ctx->ui, ctx->keybindings, &quit);
    if (quit)
    {
        *ctx->state = APP_QUIT;
        return;
    }

    if (action == UI_MENU_UP)
    {
        *ctx->keybind_player_selected = (*ctx->keybind_player_selected - 1 + KEYBIND_PLAYER_COUNT) % KEYBIND_PLAYER_COUNT;
    }
    else if (action == UI_MENU_DOWN)
    {
        *ctx->keybind_player_selected = (*ctx->keybind_player_selected + 1) % KEYBIND_PLAYER_COUNT;
    }
    else if (action == UI_MENU_SELECT)
    {
        if (*ctx->keybind_player_selected >= KEYBIND_PLAYER_1 && *ctx->keybind_player_selected <= KEYBIND_PLAYER_4)
        {
            *ctx->keybind_current_player = *ctx->keybind_player_selected;
            *ctx->keybind_current_action = 0;
            *ctx->state = APP_KEYBINDS_BINDING;
        }
        else if (*ctx->keybind_player_selected == KEYBIND_BACK)
        {
            *ctx->state = APP_OPTIONS_MENU;
        }
    }
    else if (action == UI_MENU_BACK)
    {
        *ctx->state = APP_OPTIONS_MENU;
    }

    ui_sdl_render_keybind_player_select(ctx->ui, *ctx->keybind_player_selected);
    SDL_Delay(MENU_FRAME_DELAY_MS);
}

/**
 * Handle keybinding configuration state - sequential key binding UI.
 * Prompts user for each action (UP, DOWN, LEFT, RIGHT, USE) in sequence.
 * Uses auto-swap to resolve key conflicts. ESC cancels without saving.
 */
static void handle_keybinds_binding_state(AppContext *ctx)
{
    int cancel = 0;
    int quit = 0;

    SDL_Keycode pressed_key = ui_sdl_poll_keybind_input(ctx->ui, &cancel, &quit);

    if (quit)
    {
        *ctx->state = APP_QUIT;
        return;
    }

    if (cancel)
    {
        // User pressed ESC, don't save changes
        // Reload keybindings from file to discard changes
        keybindings_load(ctx->keybindings);
        *ctx->state = APP_KEYBINDS_PLAYER_SELECT;
        return;
    }

    if (pressed_key != 0)
    {
        // Valid key pressed
        KeybindAction action = (KeybindAction)*ctx->keybind_current_action;

        // Set binding with auto-swap
        keybindings_set_with_swap(ctx->keybindings, *ctx->keybind_current_player, action, pressed_key);

        // Move to next action
        (*ctx->keybind_current_action)++;

        if (*ctx->keybind_current_action >= KB_ACTION_COUNT)
        {
            // All actions bound, save and return
            keybindings_save(ctx->keybindings);
            *ctx->state = APP_KEYBINDS_PLAYER_SELECT;
            return;
        }
    }

    // Render current binding prompt
    KeybindAction current = (KeybindAction)*ctx->keybind_current_action;
    ui_sdl_render_keybind_prompt(ctx->ui, ctx->keybindings, *ctx->keybind_current_player, current);
    SDL_Delay(MENU_FRAME_DELAY_MS);
}

/**
 * Handle sound settings state - adjust music and effects volumes.
 * Left/Right arrows change volume, ESC returns to options menu.
 */
static void handle_sound_settings_state(AppContext *ctx)
{
    int quit = 0;
    UiMenuAction action = ui_sdl_poll_sound_settings(ctx->ui, ctx->keybindings, &quit);
    if (quit)
    {
        *ctx->state = APP_QUIT;
        return;
    }

    if (action == UI_MENU_UP)
    {
        *ctx->sound_selected = (*ctx->sound_selected - 1 + SOUND_MENU_COUNT) % SOUND_MENU_COUNT;
    }
    else if (action == UI_MENU_DOWN)
    {
        *ctx->sound_selected = (*ctx->sound_selected + 1) % SOUND_MENU_COUNT;
    }
    else if (action == UI_MENU_LEFT)
    {
        if (ctx->audio)
        {
            if (*ctx->sound_selected == SOUND_MENU_MUSIC_VOLUME)
            {
                int current = audio_sdl_get_music_volume(ctx->audio);
                audio_sdl_set_music_volume(ctx->audio, current - 5); // 5%
            }
            else if (*ctx->sound_selected == SOUND_MENU_EFFECTS_VOLUME)
            {
                int current = audio_sdl_get_effects_volume(ctx->audio);
                audio_sdl_set_effects_volume(ctx->audio, current - 5); // 5%
            }
        }
    }
    else if (action == UI_MENU_RIGHT)
    {
        if (ctx->audio)
        {
            if (*ctx->sound_selected == SOUND_MENU_MUSIC_VOLUME)
            {
                int current = audio_sdl_get_music_volume(ctx->audio);
                audio_sdl_set_music_volume(ctx->audio, current + 5); // 5%
            }
            else if (*ctx->sound_selected == SOUND_MENU_EFFECTS_VOLUME)
            {
                int current = audio_sdl_get_effects_volume(ctx->audio);
                audio_sdl_set_effects_volume(ctx->audio, current + 5); // 5%
            }
        }
    }
    else if (action == UI_MENU_SELECT || action == UI_MENU_BACK)
    {
        if (*ctx->sound_selected == SOUND_MENU_BACK || action == UI_MENU_BACK)
        {
            // Save settings before exiting
            if (ctx->audio)
            {
                audio_sdl_save_config(ctx->audio, "data/audio.ini");
            }
            *ctx->state = APP_OPTIONS_MENU;
        }
    }

    ui_sdl_render_sound_settings(ctx->ui, ctx->audio, *ctx->sound_selected);
    SDL_Delay(MENU_FRAME_DELAY_MS);
}

/**
 * Handle multiplayer placeholder state.
 * Currently displays a "Coming Soon" message. ESC returns to main menu.
 */
static void handle_multiplayer_state(AppContext *ctx)
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_QUIT)
        {
            *ctx->state = APP_QUIT;
            return;
        }
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
        {
            *ctx->state = APP_MENU;
        }
    }
    ui_sdl_render_multiplayer_placeholder(ctx->ui);
    SDL_Delay(MENU_FRAME_DELAY_MS);
}

/**
 * Handle scoreboard display state.
 * Shows high scores and waits for keypress, then returns to main menu.
 */
static void handle_scoreboard_state(AppContext *ctx)
{
    // Ensure music is playing (in case it stopped during game over)
    ensure_music_playing(ctx);

    scoreboard_sort(ctx->sb);
    ui_sdl_show_scoreboard(ctx->ui, ctx->sb);
    *ctx->state = APP_MENU;
}

/**
 * Handle singleplayer gameplay state.
 * Manages pause menu, game loop (input, tick, collision), and game over.
 * On game over, prompts for name and saves score to scoreboard.
 */
static void handle_singleplayer_state(AppContext *ctx)
{
    if (*ctx->paused)
    {
        // If we're in the pause-options screen, ESC returns to pause menu
        if (*ctx->pause_in_options)
        {
            SDL_Event e;
            while (SDL_PollEvent(&e))
            {
                if (e.type == SDL_QUIT)
                {
                    *ctx->state = APP_QUIT;
                    return;
                }
                if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
                {
                    *ctx->pause_in_options = 0;
                }
            }

            ui_sdl_render_pause_options(ctx->ui, ctx->game, ctx->player_name);
            SDL_Delay(MENU_FRAME_DELAY_MS);
            return;
        }

        int quit_app = 0;
        UiPauseAction pause_action = ui_sdl_poll_pause(ctx->ui, ctx->keybindings, &quit_app);
        if (quit_app)
        {
            *ctx->state = APP_QUIT;
            return;
        }

        if (pause_action == UI_PAUSE_UP)
        {
            *ctx->pause_selected = (*ctx->pause_selected - 1 + PAUSE_MENU_COUNT) % PAUSE_MENU_COUNT;
        }
        else if (pause_action == UI_PAUSE_DOWN)
        {
            *ctx->pause_selected = (*ctx->pause_selected + 1) % PAUSE_MENU_COUNT;
        }
        else if (pause_action == UI_PAUSE_ESCAPE)
        {
            *ctx->paused = 0;
            *ctx->last_tick = (unsigned int)SDL_GetTicks();
            if (ctx->audio)
            {
                audio_sdl_resume_music(ctx->audio);
            }
        }
        else if (pause_action == UI_PAUSE_SELECT)
        {
            if (*ctx->pause_selected == 0)
            {
                // Continue
                *ctx->paused = 0;
                *ctx->last_tick = (unsigned int)SDL_GetTicks();
                if (ctx->audio)
                {
                    audio_sdl_resume_music(ctx->audio);
                }
            }
            else if (*ctx->pause_selected == 1)
            {
                // Options
                *ctx->pause_in_options = 1;
            }
            else if (*ctx->pause_selected == 2)
            {
                // Quit -> back to main menu
                *ctx->paused = 0;
                *ctx->state = APP_MENU;
                return;
            }
        }

        ui_sdl_render_pause_menu(ctx->ui, ctx->game, ctx->player_name, *ctx->pause_selected);
        SDL_Delay(MENU_FRAME_DELAY_MS);
        return;
    }

    // Monitor music status during gameplay
    ensure_music_playing(ctx);

    ui_sdl_render(ctx->ui, ctx->game, ctx->player_name);
    SDL_Delay(GAME_FRAME_DELAY_MS);

    // Gameplay input + tick
    int out_has_dir = 0;
    Direction raw_dir = DIR_RIGHT;

    int pause = 0;
    int running = ui_sdl_poll(ctx->ui, ctx->keybindings, &out_has_dir, &raw_dir, &pause);
    if (!running)
    {
        *ctx->state = APP_QUIT;
        return;
    }

    // Only allow pausing during gameplay (not during death animation or game over)
    if (pause && ctx->game->state == GAME_RUNNING)
    {
        *ctx->paused = 1;
        *ctx->pause_selected = 0;
        *ctx->pause_in_options = 0;
        input_buffer_clear(ctx->input);
        if (ctx->audio)
        {
            audio_sdl_pause_music(ctx->audio);
        }
        return;
    }

    if (out_has_dir)
    {
        input_buffer_push(ctx->input, raw_dir, ctx->game->snake.dir);
    }

    unsigned int now = (unsigned int)SDL_GetTicks();
    if (ctx->game->state == GAME_RUNNING && (now - *ctx->last_tick) >= *ctx->current_tick_ms)
    {
        *ctx->last_tick = now;

        Direction next_dir;
        if (input_buffer_pop(ctx->input, &next_dir))
        {
            game_change_direction(ctx->game, next_dir);
        }
        game_update(ctx->game);

        // Modern mode: increase speed every 40 points
        if (*ctx->current_game_mode == GAME_MODE_MODERN)
        {
            int score_delta = ctx->game->score - *ctx->modern_mode_score_at_last_speed_update;
            int speed_increases = score_delta / 40;

            if (speed_increases > 0)
            {
                int new_tick = *ctx->current_tick_ms - (speed_increases * 10);
                if (new_tick < 40)
                    new_tick = 40; // Floor at 40ms
                *ctx->current_tick_ms = new_tick;
                *ctx->modern_mode_score_at_last_speed_update = ctx->game->score;
            }
        }
    }

    // Handle death animation
    if (ctx->game->state == GAME_DYING && (now - *ctx->last_tick) >= *ctx->current_tick_ms)
    {
        *ctx->last_tick = now;
        do
        {
            fprintf(stderr, "DEATH: triggering explosion sfx\n");
            audio_sdl_play_sound(ctx->audio, "explosion");        
        } while (game_update_death_animation(ctx->game) && !audio_sdl_is_sound_playing(ctx->audio, "explosion"));
    }

    // Save score on GAME_OVER
    if (ctx->game->state == GAME_OVER && *ctx->pending_save_this_round)
    {
        char namebuf[SB_MAX_NAME_LEN];
        if (ui_sdl_get_name(ctx->ui, namebuf, sizeof(namebuf)))
        {
            if (namebuf[0] != '\0')
            {
                strncpy(ctx->player_name, namebuf, SB_MAX_NAME_LEN - 1);
                ctx->player_name[SB_MAX_NAME_LEN - 1] = '\0';
            }
        }

        scoreboard_add(ctx->sb, ctx->player_name, ctx->game->score);
        scoreboard_sort(ctx->sb);
        scoreboard_save(ctx->sb);

        *ctx->pending_save_this_round = 0;
        *ctx->state = APP_SCOREBOARD;
    }
}

int main(int argc, char *argv[])
{
    srand((unsigned int)time(NULL));

    // Parse command-line arguments
    int enable_audio = 1; // Audio enabled by default
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--no-audio") == 0 || strcmp(argv[i], "-na") == 0)
        {
            enable_audio = 0;
            fprintf(stderr, "Audio disabled via command-line flag\n");
        }
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            printf("SnakeGPT - Snake Game\n");
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --no-audio, -na    Disable audio (useful for WSL2)\n");
            printf("  --help, -h         Show this help message\n");
            return 0;
        }
    }

    // Set SDL audio driver hint for better WSL2/Linux compatibility
    if (enable_audio)
    {
#ifdef __linux__
        SDL_setenv("SDL_AUDIODRIVER", "pulseaudio", 0);
#endif
    }

    UiSdl *ui = ui_sdl_create("Snake", WINDOW_WIDTH, WINDOW_HEIGHT);
    if (!ui)
        return 1;

    // Initialize audio system (only if enabled)
    AudioSdl *audio = NULL;
    if (enable_audio)
    {
        audio = audio_sdl_create();
        if (!audio)
        {
            fprintf(stderr, "Warning: Failed to initialize audio system\n");
        }
        else
        {
            // Load volume settings
            if (!audio_sdl_load_config(audio, "data/audio.ini"))
            {
                // Config doesn't exist, using defaults
            }

            if (audio_sdl_load_music(audio, "assets/music/background.wav"))
            {
                if (!audio_sdl_is_music_playing(audio))
                {
                    audio_sdl_play_music(audio, -1); // Play music if not already playing
                }
            }
            else
            {
                fprintf(stderr, "Warning: Failed to load background music\n");
            }

            // Load sound effects (using WAV for better compatibility)
            if (!audio_sdl_load_sound(audio, "assets/audio/hitmarker.wav", "explosion"))
            {
                fprintf(stderr, "Warning: Failed to load explosion sound effect\n");
            }
        }
    }
    else
    {
        fprintf(stderr, "Audio system disabled via command-line option\n");
    }

    // scoreboard persistent
    Scoreboard sb;
    scoreboard_init(&sb, "data/scoreboard.csv");
    scoreboard_load(&sb);
    scoreboard_sort(&sb);

    // Initialize keybindings
    Keybindings keybindings;
    keybindings_init(&keybindings, "data/keybindings.ini");
    if (!keybindings_load(&keybindings))
    {
        // File doesn't exist or error, use defaults
        keybindings_set_defaults(&keybindings);
        keybindings_save(&keybindings); // Create default file
    }

    AppState state = APP_MENU;
    int menu_selected = 0;
    int options_menu_selected = 0;
    int keybind_player_selected = 0;
    int keybind_current_player = 0;
    int keybind_current_action = 0;
    int sound_selected = 0;
    int game_mode_selected = 0;
    int speed_selected = 0;
    GameMode current_game_mode = GAME_MODE_CLASSIC;
    ClassicSpeed current_classic_speed = CLASSIC_SPEED_NORMAL;
    unsigned int current_tick_ms = 100;
    int modern_mode_score_at_last_speed_update = 0;

    Game game;
    InputBuffer input;
    input_buffer_init(&input);

    char player_name[SB_MAX_NAME_LEN] = "Player";
    int paused = 0;
    int pause_selected = 0;   // 0..2
    int pause_in_options = 0; // 0=pause menu, 1=options screen

    unsigned int last_tick = 0;
    int pending_save_this_round = 0;

    // Initialize context struct
    AppContext ctx = {
        .ui = ui,
        .audio = audio,
        .keybindings = &keybindings,
        .sb = &sb,
        .state = &state,
        .menu_selected = &menu_selected,
        .options_menu_selected = &options_menu_selected,
        .keybind_player_selected = &keybind_player_selected,
        .keybind_current_player = &keybind_current_player,
        .keybind_current_action = &keybind_current_action,
        .sound_selected = &sound_selected,
        .game_mode_selected = &game_mode_selected,
        .speed_selected = &speed_selected,
        .current_game_mode = &current_game_mode,
        .current_classic_speed = &current_classic_speed,
        .current_tick_ms = &current_tick_ms,
        .modern_mode_score_at_last_speed_update = &modern_mode_score_at_last_speed_update,
        .game = &game,
        .input = &input,
        .player_name = player_name,
        .paused = &paused,
        .pause_selected = &pause_selected,
        .pause_in_options = &pause_in_options,
        .last_tick = &last_tick,
        .pending_save_this_round = &pending_save_this_round};

    while (state != APP_QUIT)
    {
        switch (state)
        {
        case APP_MENU:
            handle_menu_state(&ctx);
            break;
        case APP_OPTIONS_MENU:
            handle_options_menu_state(&ctx);
            break;
        case APP_KEYBINDS_PLAYER_SELECT:
            handle_keybinds_player_select_state(&ctx);
            break;
        case APP_KEYBINDS_BINDING:
            handle_keybinds_binding_state(&ctx);
            break;
        case APP_SOUND_SETTINGS:
            handle_sound_settings_state(&ctx);
            break;
        case APP_GAME_MODE_SELECT:
            handle_game_mode_select_state(&ctx);
            break;
        case APP_SPEED_SELECT:
            handle_speed_select_state(&ctx);
            break;
        case APP_MULTIPLAYER:
            handle_multiplayer_state(&ctx);
            break;
        case APP_SCOREBOARD:
            handle_scoreboard_state(&ctx);
            break;
        case APP_SINGLEPLAYER:
            handle_singleplayer_state(&ctx);
            break;
        case APP_QUIT:
            break;
        }
    }

    scoreboard_free(&sb);
    if (audio)
    {
        audio_sdl_save_config(audio, "data/audio.ini");
        audio_sdl_destroy(audio);
    }
    ui_sdl_destroy(ui);
    return 0;
}