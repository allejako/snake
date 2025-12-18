// Port == 9001, identifier == uuid

#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "game.h"
#include "multiplayer_game.h"
#include "common.h"
#include "scoreboard.h"
#include "ui_sdl.h"
#include "input_buffer.h"
#include "settings.h"
#include "audio_sdl.h"
#include <SDL2/SDL_ttf.h>

#define BOARD_WIDTH 20
#define BOARD_HEIGHT 20
#define TICK_MS 95
#define PAUSE_MENU_COUNT 3
#define WINDOW_WIDTH 960
#define WINDOW_HEIGHT 640
#define MENU_FRAME_DELAY_MS 16 // ~60 FPS for menus
#define GAME_FRAME_DELAY_MS 1 // Minimal delay for gameplay

// Speed curve parameters
#define SPEED_START_MS 95.0f      // Starting tick time
#define SPEED_FLOOR_MS 40.0f      // Minimum tick time (speed cap)
#define SPEED_CURVE_K 0.12f       // Exponential decay rate (tuned for food count)

// Combo system
#define BASE_COMBO_WINDOW_TICKS 20     // Base window in ticks for tier 1
#define COMBO_WINDOW_INCREASE_PER_TIER 5  // Additional ticks per tier level

// Calculate tick time based on number of foods eaten (smooth exponential curve)
static int tick_ms_for_foods(int foods)
{
    float t = SPEED_FLOOR_MS + (SPEED_START_MS - SPEED_FLOOR_MS) * expf(-SPEED_CURVE_K * (float)foods);
    return (int)(t + 0.5f);
}

typedef enum
{
    APP_MENU = 0,
    APP_SINGLEPLAYER,
    APP_GAME_OVER,
    APP_MULTIPLAYER_MENU,
    APP_MULTIPLAYER_LOBBY,
    APP_MULTIPLAYER_COUNTDOWN,
    APP_MULTIPLAYER_GAME,
    APP_MULTIPLAYER_ONLINE,
    APP_SCOREBOARD,
    APP_QUIT,
    APP_OPTIONS_MENU,
    APP_KEYBINDS_PLAYER_SELECT,
    APP_KEYBINDS_BINDING,
    APP_SOUND_SETTINGS
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
    MULTIPLAYER_MENU_LOCAL = 0,
    MULTIPLAYER_MENU_ONLINE,
    MULTIPLAYER_MENU_BACK,
    MULTIPLAYER_MENU_COUNT
} MultiplayerMenuItem;


/**
 * Application context containing all state needed by state handlers.
 * This struct uses pointers to allow state handlers to modify values.
 */
typedef struct
{
    UiSdl *ui;                    // UI system handle
    AudioSdl *audio;              // Audio system handle
    Settings *settings;           // Unified settings (profile, audio, keybindings)
    Scoreboard *sb;               // Scoreboard for high scores
    AppState *state;              // Current application state
    int *menu_selected;           // Main menu cursor position
    int *options_menu_selected;   // Options menu cursor position
    int *multiplayer_menu_selected; // Multiplayer menu cursor position
    int *keybind_player_selected; // Keybind player select cursor
    int *keybind_current_player;  // Currently configuring player (0-3)
    int *keybind_current_action;  // Currently configuring action (0-4)
    int *sound_selected;          // Sound settings cursor position
    unsigned int *current_tick_ms;    // Runtime-variable tick speed
    Game *game;                   // Game state
    MultiplayerGame_s *mp_game;   // Multiplayer game state
    InputBuffer *input;           // Input buffer for gameplay
    char *player_name;            // Current player name
    int *paused;                  // Whether game is paused
    int *pause_selected;          // Pause menu cursor position (0-2)
    int *pause_in_options;        // Whether in pause options screen
    int *game_over_selected;      // Game over menu cursor position (0-1)
    unsigned int *last_tick;      // Last game update tick (ms)
    unsigned int *countdown_start; // Countdown start time (ms)
    int *pending_save_this_round; // Whether score should be saved on game over
    int debug_mode;               // Debug mode flag (shows game speed)
} AppContext;

/**
 * Handle main menu state - navigate menu and launch game modes.
 * Updates state to transition to selected mode (singleplayer, multiplayer, options, etc.)
 */
static void handle_menu_state(AppContext *ctx)
{

    int quit = 0;
    UiMenuAction action = ui_sdl_poll_menu(ctx->ui, ctx->settings, &quit);
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
            *ctx->current_tick_ms = TICK_MS;
            game_init(ctx->game, BOARD_WIDTH, BOARD_HEIGHT);
            ctx->game->start_time = (unsigned int)SDL_GetTicks();
            ctx->game->combo_window_ms = TICK_MS * BASE_COMBO_WINDOW_TICKS; // Initialize combo window (tier 1)
            *ctx->paused = 0;
            *ctx->pending_save_this_round = 1;
            *ctx->last_tick = (unsigned int)SDL_GetTicks();
            input_buffer_clear(ctx->input);
            *ctx->state = APP_SINGLEPLAYER;
            break;

        case MENU_MULTIPLAYER:
            *ctx->state = APP_MULTIPLAYER_MENU;
            *ctx->multiplayer_menu_selected = 0;
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

    ui_sdl_render_menu(ctx->ui, ctx->settings, *ctx->menu_selected);
    SDL_Delay(MENU_FRAME_DELAY_MS);
}

/**
 * Handle options menu state - navigate between Keybinds and Back.
 * Updates state to transition to keybind configuration or back to main menu.
 */
static void handle_options_menu_state(AppContext *ctx)
{
    int quit = 0;
    UiMenuAction action = ui_sdl_poll_options_menu(ctx->ui, ctx->settings, &quit);
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

    ui_sdl_render_options_menu(ctx->ui, ctx->settings, *ctx->options_menu_selected);
    SDL_Delay(MENU_FRAME_DELAY_MS);
}

/**
 * Handle keybind player selection state - choose which player to configure.
 * Supports up to 4 players. Updates state to begin binding keys for selected player.
 */
static void handle_keybinds_player_select_state(AppContext *ctx)
{
    int quit = 0;
    UiMenuAction action = ui_sdl_poll_keybind_player_select(ctx->ui, ctx->settings, &quit);
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

    ui_sdl_render_keybind_player_select(ctx->ui, ctx->settings, *ctx->keybind_player_selected);
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
        settings_load(ctx->settings);
        *ctx->state = APP_KEYBINDS_PLAYER_SELECT;
        return;
    }

    if (pressed_key != 0)
    {
        // Valid key pressed
        SettingAction action = (SettingAction)*ctx->keybind_current_action;

        // Set binding with auto-swap
        settings_set_key_with_swap(ctx->settings, *ctx->keybind_current_player, action, pressed_key);

        // Move to next action
        (*ctx->keybind_current_action)++;

        if (*ctx->keybind_current_action >= SETTING_ACTION_COUNT)
        {
            // All actions bound, save and return
            settings_save(ctx->settings);
            *ctx->state = APP_KEYBINDS_PLAYER_SELECT;
            return;
        }
    }

    // Render current binding prompt
    SettingAction current = (SettingAction)*ctx->keybind_current_action;
    ui_sdl_render_keybind_prompt(ctx->ui, ctx->settings, *ctx->keybind_current_player, current);
    SDL_Delay(MENU_FRAME_DELAY_MS);
}

/**
 * Handle sound settings state - adjust music and effects volumes.
 * Left/Right arrows change volume, ESC returns to options menu.
 */
static void handle_sound_settings_state(AppContext *ctx)
{
    int quit = 0;
    UiMenuAction action = ui_sdl_poll_sound_settings(ctx->ui, ctx->settings, &quit);
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
                int new_volume = current - 5;
                audio_sdl_set_music_volume(ctx->audio, new_volume);
                ctx->settings->music_volume = audio_sdl_get_music_volume(ctx->audio);
            }
            else if (*ctx->sound_selected == SOUND_MENU_EFFECTS_VOLUME)
            {
                int current = audio_sdl_get_effects_volume(ctx->audio);
                int new_volume = current - 5;
                audio_sdl_set_effects_volume(ctx->audio, new_volume);
                ctx->settings->effects_volume = audio_sdl_get_effects_volume(ctx->audio);
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
                int new_volume = current + 5;
                audio_sdl_set_music_volume(ctx->audio, new_volume);
                ctx->settings->music_volume = audio_sdl_get_music_volume(ctx->audio);
            }
            else if (*ctx->sound_selected == SOUND_MENU_EFFECTS_VOLUME)
            {
                int current = audio_sdl_get_effects_volume(ctx->audio);
                int new_volume = current + 5;
                audio_sdl_set_effects_volume(ctx->audio, new_volume);
                ctx->settings->effects_volume = audio_sdl_get_effects_volume(ctx->audio);
            }
        }
    }
    else if (action == UI_MENU_SELECT || action == UI_MENU_BACK)
    {
        if (*ctx->sound_selected == SOUND_MENU_BACK || action == UI_MENU_BACK)
        {
            // Save settings before exiting
            settings_save(ctx->settings);
            *ctx->state = APP_OPTIONS_MENU;
        }
    }

    ui_sdl_render_sound_settings(ctx->ui, ctx->settings, ctx->audio, *ctx->sound_selected);
    SDL_Delay(MENU_FRAME_DELAY_MS);
}

/**
 * Handle multiplayer menu state - navigate between Local and Online.
 * Updates state to transition to local/online multiplayer or back to main menu.
 */
static void handle_multiplayer_menu_state(AppContext *ctx)
{
    int quit = 0;
    UiMenuAction action = ui_sdl_poll_multiplayer_menu(ctx->ui, ctx->settings, &quit);
    if (quit)
    {
        *ctx->state = APP_QUIT;
        return;
    }

    if (action == UI_MENU_UP)
    {
        *ctx->multiplayer_menu_selected = (*ctx->multiplayer_menu_selected - 1 + MULTIPLAYER_MENU_COUNT) % MULTIPLAYER_MENU_COUNT;
    }
    else if (action == UI_MENU_DOWN)
    {
        *ctx->multiplayer_menu_selected = (*ctx->multiplayer_menu_selected + 1) % MULTIPLAYER_MENU_COUNT;
    }
    else if (action == UI_MENU_SELECT)
    {
        if (*ctx->multiplayer_menu_selected == MULTIPLAYER_MENU_LOCAL)
        {
            // Start directly in lobby with default speed
            *ctx->current_tick_ms = TICK_MS;
            multiplayer_game_init(ctx->mp_game, BOARD_WIDTH, BOARD_HEIGHT);
            *ctx->state = APP_MULTIPLAYER_LOBBY;
        }
        else if (*ctx->multiplayer_menu_selected == MULTIPLAYER_MENU_ONLINE)
        {
            *ctx->state = APP_MULTIPLAYER_ONLINE;
        }
        else if (*ctx->multiplayer_menu_selected == MULTIPLAYER_MENU_BACK)
        {
            *ctx->state = APP_MENU;
        }
    }
    else if (action == UI_MENU_BACK)
    {
        *ctx->state = APP_MENU;
    }

    ui_sdl_render_multiplayer_menu(ctx->ui, ctx->settings, *ctx->multiplayer_menu_selected);
    SDL_Delay(MENU_FRAME_DELAY_MS);
}

/**
 * Handle multiplayer lobby state - players join by pressing USE key.
 */
static void handle_multiplayer_lobby_state(AppContext *ctx)
{
    int quit = 0;
    int players_pressed[MAX_PLAYERS] = {0, 0, 0, 0};
    int start_pressed = 0;

    int running = ui_sdl_poll_multiplayer_lobby(ctx->ui, ctx->settings, &quit, players_pressed, &start_pressed);

    if (!running)
    {
        *ctx->state = APP_QUIT;
        return;
    }

    if (quit)
    {
        // ESC pressed - go back to multiplayer menu
        *ctx->state = APP_MULTIPLAYER_MENU;
        return;
    }

    // Handle USE key presses for join/leave
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (players_pressed[i])
        {
            if (ctx->mp_game->players[i].joined)
            {
                multiplayer_game_leave_player(ctx->mp_game, i);
            }
            else
            {
                multiplayer_game_join_player(ctx->mp_game, i);
            }
        }
    }

    // Handle start game (ENTER) if 2+ players
    if (start_pressed && ctx->mp_game->total_joined >= 2)
    {
        multiplayer_game_start(ctx->mp_game);
        *ctx->countdown_start = (unsigned int)SDL_GetTicks();
        *ctx->state = APP_MULTIPLAYER_COUNTDOWN;
    }

    ui_sdl_render_multiplayer_lobby(ctx->ui, ctx->settings, ctx->mp_game);
    SDL_Delay(MENU_FRAME_DELAY_MS);
}

/**
 * Handle multiplayer countdown state - 3 second countdown before game starts.
 */
static void handle_multiplayer_countdown_state(AppContext *ctx)
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_QUIT)
        {
            *ctx->state = APP_QUIT;
            return;
        }
    }

    unsigned int now = (unsigned int)SDL_GetTicks();
    unsigned int elapsed = now - *ctx->countdown_start;
    int countdown = 3 - (int)(elapsed / 1000);

    if (countdown < 0)
    {
        // Countdown finished, start game
        *ctx->last_tick = (unsigned int)SDL_GetTicks();
        *ctx->state = APP_MULTIPLAYER_GAME;
        return;
    }

    ui_sdl_render_multiplayer_countdown(ctx->ui, ctx->mp_game, countdown);
    SDL_Delay(MENU_FRAME_DELAY_MS);
}

/**
 * Handle multiplayer gameplay state - multiple snakes on one board.
 */
static void handle_multiplayer_game_state(AppContext *ctx)
{
    ui_sdl_render_multiplayer_game(ctx->ui, ctx->mp_game);
    SDL_Delay(GAME_FRAME_DELAY_MS);

    // Poll input from all players
    int running = ui_sdl_poll_multiplayer_game(ctx->ui, ctx->settings, ctx->mp_game);
    if (!running)
    {
        *ctx->state = APP_QUIT;
        return;
    }

    unsigned int now = (unsigned int)SDL_GetTicks();

    // Check for tick update
    if ((now - *ctx->last_tick) >= *ctx->current_tick_ms)
    {
        *ctx->last_tick = now;

        // Check if any snakes are dying (for animation)
        int any_dying = 0;
        for (int i = 0; i < MAX_PLAYERS; i++)
        {
            if (ctx->mp_game->players[i].death_state == GAME_DYING)
            {
                any_dying = 1;
                break;
            }
        }

        // Handle death animations for dying snakes
        if (any_dying)
        {
            // Count segments before animation update
            int segments_before = 0;
            for (int i = 0; i < MAX_PLAYERS; i++)
            {
                if (ctx->mp_game->players[i].death_state == GAME_DYING)
                {
                    segments_before += ctx->mp_game->players[i].snake.length;
                }
            }

            // Update death animations (removes segments)
            multiplayer_game_update_death_animations(ctx->mp_game);

            // Count segments after animation update
            int segments_after = 0;
            for (int i = 0; i < MAX_PLAYERS; i++)
            {
                if (ctx->mp_game->players[i].death_state == GAME_DYING)
                {
                    segments_after += ctx->mp_game->players[i].snake.length;
                }
            }

            // Play explosion sound once for each segment removed
            int segments_removed = segments_before - segments_after;
            if (ctx->audio && segments_removed > 0)
            {
                // Play sound (limit to once per tick to avoid excessive overlap)
                audio_sdl_play_sound(ctx->audio, "explosion");
            }
        }

        // Process input buffers for all living players
        for (int i = 0; i < MAX_PLAYERS; i++)
        {
            if (ctx->mp_game->players[i].alive && ctx->mp_game->players[i].death_state == GAME_RUNNING)
            {
                Direction next_dir;
                if (input_buffer_pop(&ctx->mp_game->players[i].input, &next_dir))
                {
                    multiplayer_game_change_direction(ctx->mp_game, i, next_dir);
                }
            }
        }

        // Update game state for all living snakes
        multiplayer_game_update(ctx->mp_game);
    }

    // Check if game is over
    if (multiplayer_game_is_over(ctx->mp_game))
    {
        // Game over - return to lobby
        *ctx->state = APP_MULTIPLAYER_LOBBY;
        multiplayer_game_init(ctx->mp_game, BOARD_WIDTH, BOARD_HEIGHT);
    }
}

/**
 * Handle online multiplayer placeholder state.
 * Currently displays a "Coming Soon" message. ESC returns to multiplayer menu.
 */
static void handle_multiplayer_online_state(AppContext *ctx)
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
            *ctx->state = APP_MULTIPLAYER_MENU;
        }
    }
    ui_sdl_render_multiplayer_online_placeholder(ctx->ui);
    SDL_Delay(MENU_FRAME_DELAY_MS);
}

/**
 * Handle game over screen state.
 * Shows stats and presents Try Again / Quit options.
 */
static void handle_game_over_state(AppContext *ctx)
{
    // Calculate time survived (frozen at death)
    int time_seconds = (int)((ctx->game->death_time - ctx->game->start_time) / 1000);

    // Render game over screen
    ui_sdl_render_game_over(ctx->ui, ctx->game->score, ctx->game->fruits_eaten, time_seconds, *ctx->game_over_selected);
    SDL_Delay(MENU_FRAME_DELAY_MS);

    // Poll for input
    int quit = 0;
    UiMenuAction action = ui_sdl_poll_game_over(ctx->ui, ctx->settings, &quit);
    if (quit)
    {
        *ctx->state = APP_QUIT;
        return;
    }

    if (action == UI_MENU_UP)
    {
        *ctx->game_over_selected = (*ctx->game_over_selected - 1 + 2) % 2;
    }
    else if (action == UI_MENU_DOWN)
    {
        *ctx->game_over_selected = (*ctx->game_over_selected + 1) % 2;
    }
    else if (action == UI_MENU_SELECT)
    {
        if (*ctx->game_over_selected == 0)
        {
            // Try again - restart game
            *ctx->current_tick_ms = TICK_MS; // Reset to normal speed
            game_init(ctx->game, BOARD_WIDTH, BOARD_HEIGHT);
            ctx->game->start_time = (unsigned int)SDL_GetTicks();
            ctx->game->combo_window_ms = TICK_MS * BASE_COMBO_WINDOW_TICKS; // Reset combo window (tier 1)
            *ctx->paused = 0;
            *ctx->pending_save_this_round = 1;
            *ctx->last_tick = (unsigned int)SDL_GetTicks();
            input_buffer_clear(ctx->input);
            *ctx->state = APP_SINGLEPLAYER;
        }
        else
        {
            // Quit - return to main menu
            *ctx->state = APP_MENU;
        }
    }
}

/**
 * Handle scoreboard display state.
 * Shows high scores and waits for keypress, then returns to main menu.
 */
static void handle_scoreboard_state(AppContext *ctx)
{
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

            ui_sdl_render_pause_options(ctx->ui, ctx->game, ctx->player_name, ctx->debug_mode, *ctx->current_tick_ms);
            SDL_Delay(MENU_FRAME_DELAY_MS);
            return;
        }

        int quit_app = 0;
        UiPauseAction pause_action = ui_sdl_poll_pause(ctx->ui, ctx->settings, &quit_app);
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
                if (ctx->audio)
                {
                    audio_sdl_resume_music(ctx->audio);
                }
                *ctx->state = APP_MENU;
                return;
            }
        }

        ui_sdl_render_pause_menu(ctx->ui, ctx->game, ctx->player_name, *ctx->pause_selected, ctx->debug_mode, *ctx->current_tick_ms);
        SDL_Delay(MENU_FRAME_DELAY_MS);
        return;
    }


    ui_sdl_render(ctx->ui, ctx->game, ctx->player_name, ctx->debug_mode, *ctx->current_tick_ms);
    SDL_Delay(GAME_FRAME_DELAY_MS);

    // Gameplay input + tick
    int out_has_dir = 0;
    Direction raw_dir = DIR_RIGHT;

    int pause = 0;
    int running = ui_sdl_poll(ctx->ui, ctx->settings, &out_has_dir, &raw_dir, &pause);
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

    // Update combo timer (every frame)
    if (ctx->game->state == GAME_RUNNING)
    {
        game_update_combo_timer(ctx->game, now);
    }

    if (ctx->game->state == GAME_RUNNING && (now - *ctx->last_tick) >= *ctx->current_tick_ms)
    {
        *ctx->last_tick = now;

        Direction next_dir;
        if (input_buffer_pop(ctx->input, &next_dir))
        {
            game_change_direction(ctx->game, next_dir);
        }
        game_update(ctx->game);

        // Handle combo SFX and timer update if food was eaten
        if (ctx->game->food_eaten_this_frame)
        {
            // Update combo window based on current game speed and tier
            // Higher tiers get more time to maintain combo
            int tier = game_get_combo_tier(ctx->game->combo_count);
            int window_ticks = BASE_COMBO_WINDOW_TICKS + (tier - 1) * COMBO_WINDOW_INCREASE_PER_TIER;
            ctx->game->combo_window_ms = *ctx->current_tick_ms * window_ticks;

            // Update combo expiry time
            ctx->game->combo_expiry_time = now + ctx->game->combo_window_ms;

            // Play combo SFX
            if (ctx->audio)
            {
                char sfx_name[32];
                snprintf(sfx_name, sizeof(sfx_name), "combo%d", tier);
                audio_sdl_play_sound(ctx->audio, sfx_name);
            }
        }

        // Update speed based on foods eaten (smooth exponential curve)
        if (ctx->game->food_eaten_this_frame)
        {
            *ctx->current_tick_ms = tick_ms_for_foods(ctx->game->fruits_eaten);
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

    // Transition to game over screen on GAME_OVER
    if (ctx->game->state == GAME_OVER && *ctx->pending_save_this_round)
    {
        // Freeze time at death
        ctx->game->death_time = (unsigned int)SDL_GetTicks();

        // Check if score qualifies for top 5 and save
        int qualifies = scoreboard_qualifies_for_top_n(ctx->sb, ctx->game->score, 5);
        if (qualifies)
        {
            scoreboard_add(ctx->sb, ctx->player_name, ctx->game->score);
            scoreboard_sort(ctx->sb);
            scoreboard_trim_to_top_n(ctx->sb, 5);
            scoreboard_save(ctx->sb);
        }

        *ctx->pending_save_this_round = 0;
        *ctx->game_over_selected = 0; // Default to "Try again"
        *ctx->state = APP_GAME_OVER;
    }
}

int main(int argc, char *argv[])
{
    srand((unsigned int)time(NULL));

    // Parse command-line arguments
    int enable_audio = 1; // Audio enabled by default
    int debug_mode = 0;   // Debug mode disabled by default
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--no-audio") == 0 || strcmp(argv[i], "-na") == 0)
        {
            enable_audio = 0;
            fprintf(stderr, "Audio disabled via command-line flag\n");
        }
        else if (strcmp(argv[i], "--debug") == 0 || strcmp(argv[i], "-d") == 0)
        {
            debug_mode = 1;
            fprintf(stderr, "Debug mode enabled\n");
        }
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            printf("SnakeGPT - Snake Game\n");
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --no-audio, -na    Disable audio (useful for WSL2)\n");
            printf("  --debug, -d        Enable debug mode (shows game speed)\n");
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

    // Initialize settings first
    Settings settings;
    settings_init(&settings, "data/settings.ini");
    if (!settings_load(&settings))
    {
        // File doesn't exist or error, use defaults
        settings_set_defaults(&settings);
        settings_save(&settings); // Create default file
    }

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
            // Apply volume settings from unified settings
            audio_sdl_set_music_volume(audio, settings.music_volume);
            audio_sdl_set_effects_volume(audio, settings.effects_volume);

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

            // Load combo sound effects (5 tiers)
            const char *combo_files[] = {
                "assets/audio/combo1.wav",
                "assets/audio/combo2.wav",
                "assets/audio/combo3.wav",
                "assets/audio/combo4.wav",
                "assets/audio/combo5.wav"
            };
            for (int i = 0; i < 5; i++)
            {
                char name[16];
                snprintf(name, sizeof(name), "combo%d", i + 1);
                if (!audio_sdl_load_sound(audio, combo_files[i], name))
                {
                    fprintf(stderr, "Warning: Failed to load %s\n", combo_files[i]);
                }
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

    // Prompt for profile name if not set
    if (!settings_has_profile(&settings))
    {
        char namebuf[SETTINGS_MAX_PROFILE_NAME];
        if (ui_sdl_get_name(ui, namebuf, sizeof(namebuf), 0))
        {
            if (namebuf[0] != '\0')
            {
                strncpy(settings.profile_name, namebuf, SETTINGS_MAX_PROFILE_NAME - 1);
                settings.profile_name[SETTINGS_MAX_PROFILE_NAME - 1] = '\0';
                settings_save(&settings);
            }
        }
    }

    AppState state = APP_MENU;
    int menu_selected = 0;
    int options_menu_selected = 0;
    int multiplayer_menu_selected = 0;
    int keybind_player_selected = 0;
    int keybind_current_player = 0;
    int keybind_current_action = 0;
    int sound_selected = 0;
    unsigned int current_tick_ms = TICK_MS;

    Game game;
    MultiplayerGame_s mp_game;
    InputBuffer input;
    input_buffer_init(&input);

    // Use profile name from settings
    char player_name[SB_MAX_NAME_LEN];
    strncpy(player_name, settings.profile_name, SB_MAX_NAME_LEN - 1);
    player_name[SB_MAX_NAME_LEN - 1] = '\0';
    int paused = 0;
    int pause_selected = 0;   // 0..2
    int pause_in_options = 0; // 0=pause menu, 1=options screen
    int game_over_selected = 0; // 0..1 (Try again, Quit)

    unsigned int last_tick = 0;
    unsigned int countdown_start = 0;
    int pending_save_this_round = 0;

    // Initialize context struct
    AppContext ctx = {
        .ui = ui,
        .audio = audio,
        .settings = &settings,
        .sb = &sb,
        .state = &state,
        .menu_selected = &menu_selected,
        .options_menu_selected = &options_menu_selected,
        .multiplayer_menu_selected = &multiplayer_menu_selected,
        .keybind_player_selected = &keybind_player_selected,
        .keybind_current_player = &keybind_current_player,
        .keybind_current_action = &keybind_current_action,
        .sound_selected = &sound_selected,
        .current_tick_ms = &current_tick_ms,
        .game = &game,
        .mp_game = &mp_game,
        .input = &input,
        .player_name = player_name,
        .paused = &paused,
        .pause_selected = &pause_selected,
        .pause_in_options = &pause_in_options,
        .game_over_selected = &game_over_selected,
        .last_tick = &last_tick,
        .countdown_start = &countdown_start,
        .pending_save_this_round = &pending_save_this_round,
        .debug_mode = debug_mode};

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
        case APP_MULTIPLAYER_MENU:
            handle_multiplayer_menu_state(&ctx);
            break;
        case APP_MULTIPLAYER_LOBBY:
            handle_multiplayer_lobby_state(&ctx);
            break;
        case APP_MULTIPLAYER_COUNTDOWN:
            handle_multiplayer_countdown_state(&ctx);
            break;
        case APP_MULTIPLAYER_GAME:
            handle_multiplayer_game_state(&ctx);
            break;
        case APP_MULTIPLAYER_ONLINE:
            handle_multiplayer_online_state(&ctx);
            break;
        case APP_SCOREBOARD:
            handle_scoreboard_state(&ctx);
            break;
        case APP_SINGLEPLAYER:
            handle_singleplayer_state(&ctx);
            break;
        case APP_GAME_OVER:
            handle_game_over_state(&ctx);
            break;
        case APP_QUIT:
            break;
        }
    }

    scoreboard_free(&sb);

    // Save settings before cleanup
    settings_save(&settings);

    if (audio)
    {
        audio_sdl_destroy(audio);
    }
    ui_sdl_destroy(ui);
    return 0;
}