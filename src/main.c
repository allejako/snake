#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "constants.h"
#include "config.h"
#include "game.h"
#include "multiplayer_game.h"
#include "online_multiplayer.h"
#include "common.h"
#include "scoreboard.h"
#include "ui_sdl.h"
#include "input_buffer.h"
#include "settings.h"
#include "audio_sdl.h"
#include <SDL2/SDL_ttf.h>


// Legacy compatibility - will be replaced by config system
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
    APP_MULTIPLAYER_ONLINE_MENU,      // Host vs Join selection
    APP_MULTIPLAYER_HOST_SETUP,       // Private yes/no selection
    APP_MULTIPLAYER_JOIN_SELECT,      // Public vs Private join selection
    APP_MULTIPLAYER_LOBBY_BROWSER,    // Browse public lobbies
    APP_MULTIPLAYER_SESSION_INPUT,    // Enter session ID for joining (private)
    APP_MULTIPLAYER_ONLINE_LOBBY,     // Online lobby (waiting for players)
    APP_MULTIPLAYER_ONLINE_COUNTDOWN, // 3-2-1 countdown before game starts
    APP_MULTIPLAYER_ONLINE_GAME,      // Online gameplay
    APP_MULTIPLAYER_ONLINE_GAMEOVER,  // Online game over screen
    APP_SCOREBOARD,
    APP_QUIT,
    APP_OPTIONS_MENU,
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
    GameConfig *config;           // Game configuration (runtime adjustable)
    Scoreboard *sb;               // Scoreboard for high scores
    AppState *state;              // Current application state
    int *menu_selected;           // Main menu cursor position
    int *options_menu_selected;   // Options menu cursor position
    int *multiplayer_menu_selected; // Multiplayer menu cursor position
    int *keybind_current_player;  // Currently configuring player (0-3)
    int *keybind_current_action;  // Currently configuring action (0-4)
    int *sound_selected;          // Sound settings cursor position
    unsigned int *current_tick_ms;    // Runtime-variable tick speed
    Game *game;                   // Game state
    MultiplayerGame_s *mp_game;   // Multiplayer game state
    OnlineMultiplayerContext *online_ctx; // Online multiplayer context
    mpapi *mpapi_inst;            // MPAPI instance
    InputBuffer *input;           // Input buffer for gameplay
    char *player_name;            // Current player name
    int *paused;                  // Whether game is paused
    int *pause_selected;          // Pause menu cursor position (0-2)
    int *pause_in_options;        // Whether in pause options screen
    int *game_over_selected;      // Game over menu cursor position (0-1)
    unsigned int *last_tick;      // Last game update tick (ms)
    unsigned int *countdown_start; // Countdown start time (ms)
    unsigned int *gameover_start;  // Game over screen start time (ms)
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
            game_init(ctx->game, ctx->config->sp_board_width, ctx->config->sp_board_height);
            ctx->game->start_time = (unsigned int)SDL_GetTicks();
            ctx->game->combo_window_ms = TICK_MS * BASE_COMBO_WINDOW_TICKS; // Initialize combo window (tier 1)
            *ctx->paused = 0;
            *ctx->pending_save_this_round = 1;
            *ctx->last_tick = (unsigned int)SDL_GetTicks();
            input_buffer_clear(ctx->input);
            *ctx->state = APP_SINGLEPLAYER;
            break;

        case MENU_MULTIPLAYER:
            *ctx->state = APP_MULTIPLAYER_ONLINE_MENU;
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
            *ctx->keybind_current_action = 0;
            *ctx->state = APP_KEYBINDS_BINDING;
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
        *ctx->state = APP_OPTIONS_MENU;
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
            *ctx->state = APP_OPTIONS_MENU;
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
 * Handle online multiplayer menu - Host vs Join selection.
 */
static void handle_multiplayer_online_menu_state(AppContext *ctx)
{
    int quit = 0;
    UiMenuAction action = ui_sdl_poll_multiplayer_online_menu(ctx->ui, &quit);
    if (quit)
    {
        *ctx->state = APP_QUIT;
        return;
    }

    if (action == UI_MENU_UP)
    {
        *ctx->multiplayer_menu_selected = (*ctx->multiplayer_menu_selected - 1 + 3) % 3;
    }
    else if (action == UI_MENU_DOWN)
    {
        *ctx->multiplayer_menu_selected = (*ctx->multiplayer_menu_selected + 1) % 3;
    }
    else if (action == UI_MENU_SELECT)
    {
        switch (*ctx->multiplayer_menu_selected)
        {
        case 0: // Host Game
            *ctx->state = APP_MULTIPLAYER_HOST_SETUP;
            *ctx->multiplayer_menu_selected = 0; // Reset for host setup menu
            break;
        case 1: // Join Game
            *ctx->state = APP_MULTIPLAYER_JOIN_SELECT;
            *ctx->multiplayer_menu_selected = 0; // Reset for join select menu
            break;
        case 2: // Back
            *ctx->state = APP_MENU;
            break;
        }
    }
    else if (action == UI_MENU_BACK)
    {
        *ctx->state = APP_MENU;
    }

    ui_sdl_render_multiplayer_online_menu(ctx->ui, *ctx->multiplayer_menu_selected);
    SDL_Delay(MENU_FRAME_DELAY_MS);
}

/**
 * Handle host setup - Private yes/no selection.
 */
static void handle_multiplayer_host_setup_state(AppContext *ctx)
{
    int quit = 0;
    UiMenuAction action = ui_sdl_poll_host_setup(ctx->ui, &quit);
    if (quit)
    {
        *ctx->state = APP_QUIT;
        return;
    }

    if (action == UI_MENU_UP)
    {
        *ctx->multiplayer_menu_selected = (*ctx->multiplayer_menu_selected - 1 + 2) % 2;
    }
    else if (action == UI_MENU_DOWN)
    {
        *ctx->multiplayer_menu_selected = (*ctx->multiplayer_menu_selected + 1) % 2;
    }
    else if (action == UI_MENU_SELECT)
    {
        int is_private = (*ctx->multiplayer_menu_selected == 0) ? 1 : 0;

        // Initialize and host the game
        if (online_multiplayer_host(ctx->online_ctx, is_private, ctx->config->mp_board_width, ctx->config->mp_board_height) == MPAPI_OK)
        {
            *ctx->state = APP_MULTIPLAYER_ONLINE_LOBBY;
        }
        else
        {
            // Failed to host, go back
            *ctx->state = APP_MULTIPLAYER_ONLINE_MENU;
        }
    }
    else if (action == UI_MENU_BACK)
    {
        *ctx->state = APP_MULTIPLAYER_ONLINE_MENU;
    }

    ui_sdl_render_host_setup(ctx->ui, *ctx->multiplayer_menu_selected);
    SDL_Delay(MENU_FRAME_DELAY_MS);
}

/**
 * Handle join select - Public vs Private join selection.
 */
static void handle_multiplayer_join_select_state(AppContext *ctx)
{
    int quit = 0;
    UiMenuAction action = ui_sdl_poll_join_select(ctx->ui, &quit);
    if (quit)
    {
        *ctx->state = APP_QUIT;
        return;
    }

    if (action == UI_MENU_UP)
    {
        *ctx->multiplayer_menu_selected = (*ctx->multiplayer_menu_selected - 1 + 2) % 2;
    }
    else if (action == UI_MENU_DOWN)
    {
        *ctx->multiplayer_menu_selected = (*ctx->multiplayer_menu_selected + 1) % 2;
    }
    else if (action == UI_MENU_SELECT)
    {
        if (*ctx->multiplayer_menu_selected == 0)
        {
            // Public - go to lobby browser
            *ctx->state = APP_MULTIPLAYER_LOBBY_BROWSER;
            *ctx->multiplayer_menu_selected = 0; // Reset for lobby browser
        }
        else
        {
            // Private - go to session input
            *ctx->state = APP_MULTIPLAYER_SESSION_INPUT;
        }
    }
    else if (action == UI_MENU_BACK)
    {
        *ctx->state = APP_MULTIPLAYER_ONLINE_MENU;
    }

    ui_sdl_render_join_select(ctx->ui, *ctx->multiplayer_menu_selected);
    SDL_Delay(MENU_FRAME_DELAY_MS);
}

/**
 * Handle lobby browser - Browse and join public lobbies.
 */
static void handle_multiplayer_lobby_browser_state(AppContext *ctx)
{
    static json_t *cached_lobby_list = NULL;
    static int selected_lobby = 0;
    static int need_fetch = 1;

    int quit = 0;

    // Fetch lobby list only when entering state or refreshing
    if (need_fetch)
    {
        if (cached_lobby_list)
        {
            json_decref(cached_lobby_list);
            cached_lobby_list = NULL;
        }

        int rc = mpapi_list(ctx->online_ctx->api, &cached_lobby_list);

        if (rc != MPAPI_OK || !cached_lobby_list)
        {
            // Failed to fetch lobbies, go back
            need_fetch = 1;
            selected_lobby = 0;
            *ctx->state = APP_MULTIPLAYER_JOIN_SELECT;
            ui_sdl_render_error(ctx->ui, "Failed to fetch lobby list");
            SDL_Delay(2000);
            return;
        }

        need_fetch = 0;
        selected_lobby = 0;
    }

    size_t lobby_count = json_array_size(cached_lobby_list);

    UiMenuAction action = ui_sdl_poll_lobby_browser(ctx->ui, &quit);
    if (quit)
    {
        if (cached_lobby_list)
        {
            json_decref(cached_lobby_list);
            cached_lobby_list = NULL;
        }
        need_fetch = 1;
        *ctx->state = APP_QUIT;
        return;
    }

    if (action == UI_MENU_UP && lobby_count > 0)
    {
        selected_lobby = (selected_lobby - 1 + (int)lobby_count) % (int)lobby_count;
    }
    else if (action == UI_MENU_DOWN && lobby_count > 0)
    {
        selected_lobby = (selected_lobby + 1) % (int)lobby_count;
    }
    else if (action == UI_MENU_SELECT && lobby_count > 0)
    {
        // Get selected lobby's session ID
        json_t *lobby = json_array_get(cached_lobby_list, selected_lobby);
        json_t *session_json = json_object_get(lobby, "session");

        if (session_json && json_is_string(session_json))
        {
            const char *session_id = json_string_value(session_json);

            // Join the selected lobby
            if (online_multiplayer_join(ctx->online_ctx, session_id, ctx->config->mp_board_width, ctx->config->mp_board_height) == MPAPI_OK)
            {
                json_decref(cached_lobby_list);
                cached_lobby_list = NULL;
                need_fetch = 1;
                selected_lobby = 0;
                *ctx->state = APP_MULTIPLAYER_ONLINE_LOBBY;
                return;
            }
        }

        // Failed to join
        if (cached_lobby_list)
        {
            json_decref(cached_lobby_list);
            cached_lobby_list = NULL;
        }
        need_fetch = 1;
        selected_lobby = 0;
        *ctx->state = APP_MULTIPLAYER_JOIN_SELECT;
        ui_sdl_render_error(ctx->ui, "Failed to join lobby");
        SDL_Delay(2000);
        return;
    }
    else if (action == UI_MENU_BACK)
    {
        if (cached_lobby_list)
        {
            json_decref(cached_lobby_list);
            cached_lobby_list = NULL;
        }
        need_fetch = 1;
        selected_lobby = 0;
        *ctx->state = APP_MULTIPLAYER_JOIN_SELECT;
        return;
    }

    ui_sdl_render_lobby_browser(ctx->ui, cached_lobby_list, selected_lobby);
    SDL_Delay(MENU_FRAME_DELAY_MS);
}

/**
 * Handle session input - Enter session ID for joining.
 */
static void handle_multiplayer_session_input_state(AppContext *ctx)
{
    char session_id[8];
    if (ui_sdl_get_session_id(ctx->ui, session_id, sizeof(session_id)))
    {
        // User entered a session ID
        if (online_multiplayer_join(ctx->online_ctx, session_id, ctx->config->mp_board_width, ctx->config->mp_board_height) == MPAPI_OK)
        {
            *ctx->state = APP_MULTIPLAYER_ONLINE_LOBBY;
        }
        else
        {
            // Failed to join - check if we have an error message to display
            if (ctx->online_ctx->connection_lost)
            {
                // Display error message
                SDL_SetRenderDrawColor(ctx->ui->ren, 0, 0, 0, 255);
                SDL_RenderClear(ctx->ui->ren);
                text_draw_center(ctx->ui->ren, &ctx->ui->text, ctx->ui->w / 2, ctx->ui->h / 2 - 40, "Failed to Join");
                text_draw_center(ctx->ui->ren, &ctx->ui->text, ctx->ui->w / 2, ctx->ui->h / 2 + 10, ctx->online_ctx->error_message);
                text_draw_center(ctx->ui->ren, &ctx->ui->text, ctx->ui->w / 2, ctx->ui->h / 2 + 60, "Press any key to continue");
                SDL_RenderPresent(ctx->ui->ren);

                // Wait for keypress
                SDL_Event e;
                int waiting = 1;
                while (waiting)
                {
                    while (SDL_PollEvent(&e))
                    {
                        if (e.type == SDL_QUIT || e.type == SDL_KEYDOWN)
                        {
                            waiting = 0;
                        }
                    }
                    SDL_Delay(16);
                }

                // Reset connection lost flag
                ctx->online_ctx->connection_lost = 0;
            }

            // Go back to menu
            *ctx->state = APP_MULTIPLAYER_ONLINE_MENU;
        }
    }
    else
    {
        // User canceled
        *ctx->state = APP_MULTIPLAYER_ONLINE_MENU;
    }
}

/**
 * Handle online lobby - Waiting for players.
 */
static void handle_multiplayer_online_lobby_state(AppContext *ctx)
{
    int quit = 0;
    UiMenuAction action = ui_sdl_poll_online_lobby(ctx->ui, ctx->settings, &quit);
    if (quit)
    {
        *ctx->state = APP_QUIT;
        return;
    }

    // Check if game started (for clients receiving start command from host)
    if (ctx->online_ctx->state == ONLINE_STATE_COUNTDOWN)
    {
        *ctx->countdown_start = SDL_GetTicks();
        *ctx->state = APP_MULTIPLAYER_ONLINE_COUNTDOWN;
        return;
    }

    if (action == UI_MENU_BACK)
    {
        // Leave lobby - send disconnect notification and cleanup
        printf("DEBUG: Player leaving lobby, sending disconnect notification\n");
        fflush(stdout);

        // Send a disconnect notification via game message
        if (ctx->mpapi_inst) {
            json_t *disconnect_msg = json_object();
            json_object_set_new(disconnect_msg, "command", json_string("player_disconnect"));
            mpapi_game(ctx->mpapi_inst, disconnect_msg, NULL);
            json_decref(disconnect_msg);
        }

        // Give time for message to send before destroying
        SDL_Delay(100);

        if (ctx->online_ctx)
        {
            online_multiplayer_destroy(ctx->online_ctx);
            ctx->online_ctx = NULL;
        }
        if (ctx->mpapi_inst)
        {
            mpapi_destroy(ctx->mpapi_inst);
            ctx->mpapi_inst = NULL;
        }

        // Recreate for next session
        ctx->online_ctx = online_multiplayer_create();
        ctx->mpapi_inst = mpapi_create(ctx->config->server_host, ctx->config->server_port, "c609c6cf-ad69-4957-9aa4-6e7cac06a862");
        
        if (ctx->online_ctx && ctx->mpapi_inst)
        {
            ctx->online_ctx->api = ctx->mpapi_inst;
            ctx->online_ctx->game = ctx->mp_game;
        }

        *ctx->state = APP_MULTIPLAYER_ONLINE_MENU;
    }
    else if (action == UI_MENU_USE)
    {
        // Player pressed USE key to toggle ready status
        online_multiplayer_toggle_ready(ctx->online_ctx);
    }
    else if (action == UI_MENU_SELECT && ctx->online_ctx->game->is_host)
    {
        // Host pressed ENTER to start game - only allowed if all players are ready
        if (online_multiplayer_all_players_ready(ctx->online_ctx))
        {
            *ctx->current_tick_ms = TICK_MS; // Reset to default speed for multiplayer
            online_multiplayer_start_game(ctx->online_ctx);
            *ctx->countdown_start = SDL_GetTicks();
            *ctx->state = APP_MULTIPLAYER_ONLINE_COUNTDOWN;
        }
    }

    ui_sdl_render_online_lobby(ctx->ui, ctx->online_ctx);
    SDL_Delay(MENU_FRAME_DELAY_MS);
}

/**
 * Handle online countdown - 3-2-1 countdown before game starts.
 */
static void handle_multiplayer_online_countdown_state(AppContext *ctx)
{
    unsigned int current_time = SDL_GetTicks();
    unsigned int elapsed = current_time - *ctx->countdown_start;
    int countdown = 3 - (int)(elapsed / 1000);

    if (countdown < 0)
    {
        // Start game
        ctx->online_ctx->state = ONLINE_STATE_PLAYING;
        *ctx->state = APP_MULTIPLAYER_ONLINE_GAME;
        *ctx->last_tick = SDL_GetTicks();
        return;
    }

    ui_sdl_render_online_countdown(ctx->ui, ctx->online_ctx, countdown);
    SDL_Delay(MENU_FRAME_DELAY_MS);
}

/**
 * Handle online game - Main gameplay loop.
 */
static void handle_multiplayer_online_game_state(AppContext *ctx)
{
    int quit = 0;
    Direction input_dir = ui_sdl_poll_online_game_input(ctx->ui, ctx->settings, &quit);
    if (quit)
    {
        *ctx->state = APP_QUIT;
        return;
    }

    // Validate and send input for client
    if (input_dir != -1 && !ctx->online_ctx->game->is_host)
    {
        int local_idx = ctx->online_ctx->game->local_player_index;
        if (local_idx >= 0 && local_idx < MAX_PLAYERS)
        {
            // Validate input locally using the same logic as singleplayer
            Direction current_dir = ctx->online_ctx->game->players[local_idx].snake.dir;

            // Check if buffer has a pending direction
            Direction last_dir = current_dir;
            if (ctx->online_ctx->has_pending_input) {
                last_dir = ctx->online_ctx->pending_input;
            }

            // Validate: not same, not opposite
            int is_opposite = (last_dir == DIR_UP && input_dir == DIR_DOWN) ||
                             (last_dir == DIR_DOWN && input_dir == DIR_UP) ||
                             (last_dir == DIR_LEFT && input_dir == DIR_RIGHT) ||
                             (last_dir == DIR_RIGHT && input_dir == DIR_LEFT);

            if (input_dir != last_dir && !is_opposite)
            {
                // Valid input - store as pending and send to host
                ctx->online_ctx->pending_input = input_dir;
                ctx->online_ctx->has_pending_input = 1;
                online_multiplayer_client_send_input(ctx->online_ctx, input_dir);
            }
        }
    }

    unsigned int current_time = SDL_GetTicks();

    // Host updates game logic
    if (ctx->online_ctx->game->is_host)
    {
        // Process host's own input using input buffer (same as singleplayer)
        if (input_dir != -1)
        {
            int local_idx = ctx->online_ctx->game->local_player_index;
            if (local_idx >= 0 && local_idx < MAX_PLAYERS)
            {
                MultiplayerPlayer *local_player = &ctx->online_ctx->game->players[local_idx];
                // Use the input buffer (same as singleplayer!)
                input_buffer_push(&local_player->input, input_dir, local_player->snake.dir);
            }
        }

        if (current_time - *ctx->last_tick >= *ctx->current_tick_ms)
        {
            online_multiplayer_host_update(ctx->online_ctx, current_time);
            *ctx->last_tick = current_time;

            // Check game over
            if (multiplayer_game_is_over(ctx->online_ctx->game))
            {
                *ctx->gameover_start = (unsigned int)SDL_GetTicks();
                *ctx->state = APP_MULTIPLAYER_ONLINE_GAMEOVER;
                return;
            }
        }
    }

    // Play SFX for all players (detect state changes client-side)
    if (ctx->audio)
    {
        static int prev_scores[MAX_PLAYERS] = {0, 0, 0, 0};
        static int prev_snake_lengths[MAX_PLAYERS] = {0, 0, 0, 0};

        for (int i = 0; i < MAX_PLAYERS; i++)
        {
            MultiplayerPlayer *p = &ctx->online_ctx->game->players[i];

            if (!p->joined) {
                prev_scores[i] = 0;
                prev_snake_lengths[i] = 0;
                continue;
            }

            // Detect food eaten: score increased
            if (p->score > prev_scores[i])
            {
                int tier = game_get_combo_tier(p->combo_count);
                if (tier > 0)
                {
                    char sfx_name[32];
                    snprintf(sfx_name, sizeof(sfx_name), "combo%d", tier);
                    audio_sdl_play_sound(ctx->audio, sfx_name);
                }
            }

            // Detect death animation: snake losing segments while dying
            if (p->death_state == GAME_DYING && p->snake.length < prev_snake_lengths[i])
            {
                audio_sdl_play_sound(ctx->audio, "explosion");
            }

            // Update previous states
            prev_scores[i] = p->score;
            prev_snake_lengths[i] = p->snake.length;
        }
    }

    // Client: Check if game over was received from host
    if (!ctx->online_ctx->game->is_host)
    {
        if (ctx->online_ctx->state == ONLINE_STATE_GAME_OVER)
        {
            printf("DEBUG: Client detected ONLINE_STATE_GAME_OVER, transitioning to APP_MULTIPLAYER_ONLINE_GAMEOVER\n");
            fflush(stdout);
            *ctx->gameover_start = (unsigned int)SDL_GetTicks();
            *ctx->state = APP_MULTIPLAYER_ONLINE_GAMEOVER;
            return;
        }
    }

    ui_sdl_render_online_game(ctx->ui, ctx->online_ctx);
    SDL_Delay(GAME_FRAME_DELAY_MS);
}

/**
 * Handle online game over - Show final standings.
 * Auto-returns to lobby after 3 seconds.
 */
static void handle_multiplayer_online_gameover_state(AppContext *ctx)
{
    int quit = 0;
    ui_sdl_poll_online_gameover(ctx->ui, &quit);
    if (quit)
    {
        *ctx->state = APP_QUIT;
        return;
    }

    // Check if 3 seconds have passed (3000ms)
    unsigned int current_time = (unsigned int)SDL_GetTicks();
    unsigned int elapsed = current_time - *ctx->gameover_start;

    if (elapsed >= 3000)
    {
        // Return to online lobby
        online_multiplayer_reset_ready_states(ctx->online_ctx);
        ctx->online_ctx->state = ONLINE_STATE_LOBBY;
        *ctx->state = APP_MULTIPLAYER_ONLINE_LOBBY;
    }

    ui_sdl_render_online_gameover(ctx->ui, ctx->online_ctx);
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
            game_init(ctx->game, ctx->config->sp_board_width, ctx->config->sp_board_height);
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
            printf("Snake - Snake Game\n");
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

    // Load game configuration
    GameConfig game_config;
    if (config_load(&game_config, "data/game_config.ini") != 0)
    {
        fprintf(stderr, "Warning: Failed to load game config, using defaults\n");
        config_init_defaults(&game_config);
    }

    // Create UI with configured window dimensions
    UiSdl *ui = ui_sdl_create("Snake", game_config.window_width, game_config.window_height);
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

            // Load combo sound effects (7 tiers)
            const char *combo_files[] = {
                "assets/audio/Combo1.wav",
                "assets/audio/Combo2.wav",
                "assets/audio/Combo3.wav",
                "assets/audio/Combo4.wav",
                "assets/audio/Combo5.wav",
                "assets/audio/Combo6.wav",
                "assets/audio/Combo7.wav"
            };
            for (int i = 0; i < 7; i++)
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
    int keybind_current_player = 0;
    int keybind_current_action = 0;
    int sound_selected = 0;
    unsigned int current_tick_ms = TICK_MS;

    Game game;
    MultiplayerGame_s mp_game;
    OnlineMultiplayerContext *online_ctx = online_multiplayer_create();
    if (!online_ctx)
    {
        fprintf(stderr, "Failed to create online multiplayer context\n");
        return 1;
    }

    // Initialize mpapi instance and connect to online context
    // The identifier must be exactly 36 characters (UUID format)
    mpapi *mpapi_instance = mpapi_create(game_config.server_host, game_config.server_port, "c609c6cf-ad69-4957-9aa4-6e7cac06a862");
    if (!mpapi_instance)
    {
        fprintf(stderr, "Failed to create mpapi instance\n");
        online_multiplayer_destroy(online_ctx);
        return 1;
    }
    online_ctx->api = mpapi_instance;
    online_ctx->game = &mp_game;

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
    unsigned int gameover_start = 0;
    int pending_save_this_round = 0;

    // Initialize context struct
    AppContext ctx = {
        .ui = ui,
        .audio = audio,
        .settings = &settings,
        .config = &game_config,
        .sb = &sb,
        .state = &state,
        .menu_selected = &menu_selected,
        .options_menu_selected = &options_menu_selected,
        .multiplayer_menu_selected = &multiplayer_menu_selected,
        .keybind_current_player = &keybind_current_player,
        .keybind_current_action = &keybind_current_action,
        .sound_selected = &sound_selected,
        .current_tick_ms = &current_tick_ms,
        .game = &game,
        .mp_game = &mp_game,
        .online_ctx = online_ctx,
        .mpapi_inst = mpapi_instance,
        .input = &input,
        .player_name = player_name,
        .paused = &paused,
        .pause_selected = &pause_selected,
        .pause_in_options = &pause_in_options,
        .game_over_selected = &game_over_selected,
        .last_tick = &last_tick,
        .countdown_start = &countdown_start,
        .gameover_start = &gameover_start,
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
        case APP_KEYBINDS_BINDING:
            handle_keybinds_binding_state(&ctx);
            break;
        case APP_SOUND_SETTINGS:
            handle_sound_settings_state(&ctx);
            break;
        case APP_MULTIPLAYER_ONLINE_MENU:
            handle_multiplayer_online_menu_state(&ctx);
            break;
        case APP_MULTIPLAYER_HOST_SETUP:
            handle_multiplayer_host_setup_state(&ctx);
            break;
        case APP_MULTIPLAYER_JOIN_SELECT:
            handle_multiplayer_join_select_state(&ctx);
            break;
        case APP_MULTIPLAYER_LOBBY_BROWSER:
            handle_multiplayer_lobby_browser_state(&ctx);
            break;
        case APP_MULTIPLAYER_SESSION_INPUT:
            handle_multiplayer_session_input_state(&ctx);
            break;
        case APP_MULTIPLAYER_ONLINE_LOBBY:
            handle_multiplayer_online_lobby_state(&ctx);
            break;
        case APP_MULTIPLAYER_ONLINE_COUNTDOWN:
            handle_multiplayer_online_countdown_state(&ctx);
            break;
        case APP_MULTIPLAYER_ONLINE_GAME:
            handle_multiplayer_online_game_state(&ctx);
            break;
        case APP_MULTIPLAYER_ONLINE_GAMEOVER:
            handle_multiplayer_online_gameover_state(&ctx);
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

    // Cleanup online multiplayer context (use context pointers in case they were recreated)
    if (ctx.online_ctx)
    {
        online_multiplayer_destroy(ctx.online_ctx);
    }

    if (ctx.mpapi_inst)
    {
        mpapi_destroy(ctx.mpapi_inst);
    }

    if (audio)
    {
        audio_sdl_destroy(audio);
    }
    ui_sdl_destroy(ui);
    return 0;
}