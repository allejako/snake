#ifndef UI_SDL_H
#define UI_SDL_H
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "game.h"
#include "scoreboard.h"
#include "text_sdl.h"
#include "settings.h"


typedef struct
{
    SDL_Window *win;
    SDL_Renderer *ren;
    int w, h;
    TextRenderer text;
    int text_ok;

    // rendering scale
    int cell; // pixel size per cell
    int pad;  // padding around board
} UiSdl;

typedef enum {
    UI_MENU_NONE = 0,
    UI_MENU_UP,
    UI_MENU_DOWN,
    UI_MENU_LEFT,
    UI_MENU_RIGHT,
    UI_MENU_SELECT,
    UI_MENU_BACK,
    UI_MENU_USE
} UiMenuAction;
void ui_sdl_render_menu(UiSdl *ui, const Settings *settings, int selected_index);
UiMenuAction ui_sdl_poll_menu(UiSdl *ui, const Settings *settings, int *out_quit);

typedef enum {
    UI_PAUSE_NONE = 0,
    UI_PAUSE_UP,
    UI_PAUSE_DOWN,
    UI_PAUSE_SELECT,
    UI_PAUSE_ESCAPE
} UiPauseAction;
void ui_sdl_render_pause_menu(UiSdl *ui, const Game *g, const char *player_name, int selected_index, int debug_mode, unsigned int current_tick_ms);
UiPauseAction ui_sdl_poll_pause(UiSdl *ui, const Settings *settings, int *out_quit);
void ui_sdl_render_pause_options(UiSdl *ui, const Game *g, const char *player_name, int debug_mode, unsigned int current_tick_ms);

UiSdl *ui_sdl_create(const char *title, int window_w, int window_h);
void ui_sdl_destroy(UiSdl *ui);
void ui_sdl_render_options(UiSdl *ui);

// Forward declaration of MultiplayerGame from multiplayer_game.h
// Include multiplayer_game.h in your source file to use these functions
typedef struct MultiplayerGame_s MultiplayerGame_s;

int ui_sdl_poll(UiSdl *ui, const Settings *settings, int *out_has_dir, Direction *out_dir, int *out_pause);

// Render
void ui_sdl_draw_game(UiSdl *ui, const Game *g, const char *player_name, int debug_mode, unsigned int current_tick_ms);
void ui_sdl_render(UiSdl *ui, const Game *g, const char *player_name, int debug_mode, unsigned int current_tick_ms);

// Name input: minimal (no text rendering), uses SDL's text input
// Returns 1 if name was retrieved, 0 if user canceled
// If show_game_over is 1, displays "GAME OVER" title; otherwise just shows the prompt
int ui_sdl_get_name(UiSdl *ui, char *out_name, int out_size, int show_game_over);

void ui_sdl_show_scoreboard(UiSdl *ui, const Scoreboard *sb);

// Options menu
void ui_sdl_render_options_menu(UiSdl *ui, const Settings *settings, int selected_index);
UiMenuAction ui_sdl_poll_options_menu(UiSdl *ui, const Settings *settings, int *out_quit);

// Keybinds player select
void ui_sdl_render_keybind_player_select(UiSdl *ui, const Settings *settings, int selected_index);
UiMenuAction ui_sdl_poll_keybind_player_select(UiSdl *ui, const Settings *settings, int *out_quit);

// Keybind binding UI
void ui_sdl_render_keybind_prompt(UiSdl *ui, const Settings *settings, int player_index, SettingAction action);
SDL_Keycode ui_sdl_poll_keybind_input(UiSdl *ui, int *out_cancel, int *out_quit);

// Sound settings - forward declare AudioSdl to avoid circular dependency
typedef struct AudioSdl AudioSdl;
void ui_sdl_render_sound_settings(UiSdl *ui, const Settings *settings, const AudioSdl *audio, int selected_index);
UiMenuAction ui_sdl_poll_sound_settings(UiSdl *ui, const Settings *settings, int *out_quit);

// Game over screen
#include "scoreboard.h"
void ui_sdl_render_game_over(UiSdl *ui, int score, int fruits, int time_seconds, int combo_best, const Scoreboard *sb, int selected_index);
UiMenuAction ui_sdl_poll_game_over(UiSdl *ui, const Settings *settings, int *out_quit);

// Online multiplayer - include the header for OnlineMultiplayerContext
#include "online_multiplayer.h"

// Online multiplayer menu (Host/Join selection)
void ui_sdl_render_multiplayer_online_menu(UiSdl *ui, int selected_index);
UiMenuAction ui_sdl_poll_multiplayer_online_menu(UiSdl *ui, int *out_quit);

// Host setup (Private yes/no selection)
void ui_sdl_render_host_setup(UiSdl *ui, int selected_index);
UiMenuAction ui_sdl_poll_host_setup(UiSdl *ui, int *out_quit);

// Join select (Public vs Private join selection)
void ui_sdl_render_join_select(UiSdl *ui, int selected_index);
UiMenuAction ui_sdl_poll_join_select(UiSdl *ui, int *out_quit);

// Lobby browser (Browse public lobbies)
void ui_sdl_render_lobby_browser(UiSdl *ui, json_t *lobby_list, int selected_index);
UiMenuAction ui_sdl_poll_lobby_browser(UiSdl *ui, int *out_quit);

// Error display
void ui_sdl_render_error(UiSdl *ui, const char *message);

// Session input (Enter session ID for joining)
int ui_sdl_get_session_id(UiSdl *ui, char *out_session_id, int out_size);

// Online lobby (Waiting for players)
void ui_sdl_render_online_lobby(UiSdl *ui, const OnlineMultiplayerContext *ctx);
UiMenuAction ui_sdl_poll_online_lobby(UiSdl *ui, const Settings *settings, int *out_quit);

// Online countdown (3-2-1 countdown)
void ui_sdl_render_online_countdown(UiSdl *ui, const OnlineMultiplayerContext *ctx, int countdown);

// Online game (Main gameplay)
void ui_sdl_render_online_game(UiSdl *ui, const OnlineMultiplayerContext *ctx);
Direction ui_sdl_poll_online_game_input(UiSdl *ui, const Settings *settings, int *out_quit);

// Online game over (Final standings)
void ui_sdl_render_online_gameover(UiSdl *ui, const OnlineMultiplayerContext *ctx);
UiMenuAction ui_sdl_poll_online_gameover(UiSdl *ui, int *out_quit);

#endif