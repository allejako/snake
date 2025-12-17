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
#include "keybindings.h"


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
    UI_MENU_BACK
} UiMenuAction;
void ui_sdl_render_menu(UiSdl *ui, int selected_index);
UiMenuAction ui_sdl_poll_menu(UiSdl *ui, const Keybindings *kb, int *out_quit);

typedef enum {
    UI_PAUSE_NONE = 0,
    UI_PAUSE_UP,
    UI_PAUSE_DOWN,
    UI_PAUSE_SELECT,
    UI_PAUSE_ESCAPE
} UiPauseAction;
void ui_sdl_render_pause_menu(UiSdl *ui, const Game *g, const char *player_name, int selected_index);
UiPauseAction ui_sdl_poll_pause(UiSdl *ui, const Keybindings *kb, int *out_quit);
void ui_sdl_render_pause_options(UiSdl *ui, const Game *g, const char *player_name);

UiSdl *ui_sdl_create(const char *title, int window_w, int window_h);
void ui_sdl_destroy(UiSdl *ui);
void ui_sdl_render_options(UiSdl *ui);
void ui_sdl_render_multiplayer_placeholder(UiSdl *ui);

int ui_sdl_poll(UiSdl *ui, const Keybindings *kb, int *out_has_dir, Direction *out_dir, int *out_pause);

// Render
void ui_sdl_draw_game(UiSdl *ui, const Game *g, const char *player_name);
void ui_sdl_render(UiSdl *ui, const Game *g, const char *player_name);

// "Game over" name input: minimal (no text rendering), uses SDL's text input
// Returns 1 if name was retrieved, 0 if user canceled
int ui_sdl_get_name(UiSdl *ui, char *out_name, int out_size);

void ui_sdl_show_scoreboard(UiSdl *ui, const Scoreboard *sb);

// Options menu
void ui_sdl_render_options_menu(UiSdl *ui, int selected_index);
UiMenuAction ui_sdl_poll_options_menu(UiSdl *ui, const Keybindings *kb, int *out_quit);

// Keybinds player select
void ui_sdl_render_keybind_player_select(UiSdl *ui, int selected_index);
UiMenuAction ui_sdl_poll_keybind_player_select(UiSdl *ui, const Keybindings *kb, int *out_quit);

// Keybind binding UI
void ui_sdl_render_keybind_prompt(UiSdl *ui, const Keybindings *kb, int player_index, KeybindAction action);
SDL_Keycode ui_sdl_poll_keybind_input(UiSdl *ui, int *out_cancel, int *out_quit);

// Sound settings - forward declare AudioSdl to avoid circular dependency
typedef struct AudioSdl AudioSdl;
void ui_sdl_render_sound_settings(UiSdl *ui, const AudioSdl *audio, int selected_index);
UiMenuAction ui_sdl_poll_sound_settings(UiSdl *ui, const Keybindings *kb, int *out_quit);

// Game mode selection menu
void ui_sdl_render_game_mode_select(UiSdl *ui, int selected_index);
UiMenuAction ui_sdl_poll_game_mode_select(UiSdl *ui, const Keybindings *kb, int *out_quit);

// Speed selection menu (for Classic mode)
void ui_sdl_render_speed_select(UiSdl *ui, int selected_index);
UiMenuAction ui_sdl_poll_speed_select(UiSdl *ui, const Keybindings *kb, int *out_quit);

#endif