#ifndef ONLINE_MULTIPLAYER_STUB_H
#define ONLINE_MULTIPLAYER_STUB_H

// Stub definitions for Windows builds (online multiplayer not supported)

#ifdef WINDOWS_BUILD

#include "multiplayer_game.h"
#include "ui_sdl.h"
#include "settings.h"
#include "game.h"

// Forward declare json_t to avoid needing jansson
typedef void json_t;

// Stub types
typedef struct {
    void *api;
    int listener_id;
    MultiplayerGame_s *game;
    int state;
    int is_private;
    int connection_lost;
    char error_message[256];
    Direction pending_input;
    int has_pending_input;
} OnlineMultiplayerContext;

typedef struct {
    void *dummy;
} mpapi;

// Stub enums
typedef enum {
    ONLINE_STATE_HOST_SETUP,
    ONLINE_STATE_LOBBY,
    ONLINE_STATE_COUNTDOWN,
    ONLINE_STATE_PLAYING,
    ONLINE_STATE_GAME_OVER,
    ONLINE_STATE_DISCONNECTED
} OnlineState;

typedef enum {
    MPAPI_OK = 0,
    MPAPI_ERROR = -1
} mpapi_result;

// Stub backend functions
static inline OnlineMultiplayerContext* online_multiplayer_create(void) { return NULL; }
static inline void online_multiplayer_destroy(OnlineMultiplayerContext *ctx) { (void)ctx; }
static inline int online_multiplayer_host(OnlineMultiplayerContext *ctx, int is_private, int w, int h, const char *name) { (void)ctx; (void)is_private; (void)w; (void)h; (void)name; return MPAPI_ERROR; }
static inline int online_multiplayer_join(OnlineMultiplayerContext *ctx, const char *session_id, int w, int h, const char *name) { (void)ctx; (void)session_id; (void)w; (void)h; (void)name; return MPAPI_ERROR; }
static inline mpapi* mpapi_create(const char *host, int port, const char *uuid) { (void)host; (void)port; (void)uuid; return NULL; }
static inline void mpapi_destroy(mpapi *api) { (void)api; }
static inline int mpapi_game(mpapi *api, const char *msg, void *data) { (void)api; (void)msg; (void)data; return MPAPI_ERROR; }

// Stub UI functions
static inline void ui_sdl_render_multiplayer_online_menu(UiSdl *ui, int selected_index) { (void)ui; (void)selected_index; }
static inline UiMenuAction ui_sdl_poll_multiplayer_online_menu(UiSdl *ui, int *out_quit) { (void)ui; (void)out_quit; return UI_MENU_BACK; }
static inline void ui_sdl_render_host_setup(UiSdl *ui, int selected_index) { (void)ui; (void)selected_index; }
static inline UiMenuAction ui_sdl_poll_host_setup(UiSdl *ui, int *out_quit) { (void)ui; (void)out_quit; return UI_MENU_BACK; }
static inline void ui_sdl_render_join_select(UiSdl *ui, int selected_index) { (void)ui; (void)selected_index; }
static inline UiMenuAction ui_sdl_poll_join_select(UiSdl *ui, int *out_quit) { (void)ui; (void)out_quit; return UI_MENU_BACK; }
static inline void ui_sdl_render_lobby_browser(UiSdl *ui, json_t *lobby_list, int selected_index) { (void)ui; (void)lobby_list; (void)selected_index; }
static inline UiMenuAction ui_sdl_poll_lobby_browser(UiSdl *ui, int *out_quit) { (void)ui; (void)out_quit; return UI_MENU_BACK; }
static inline void ui_sdl_render_error(UiSdl *ui, const char *message) { (void)ui; (void)message; }
static inline int ui_sdl_get_session_id(UiSdl *ui, char *out_session_id, int out_size) { (void)ui; (void)out_session_id; (void)out_size; return 0; }
static inline void ui_sdl_render_online_lobby(UiSdl *ui, const OnlineMultiplayerContext *ctx) { (void)ui; (void)ctx; }
static inline UiMenuAction ui_sdl_poll_online_lobby(UiSdl *ui, const Settings *settings, int *out_quit) { (void)ui; (void)settings; (void)out_quit; return UI_MENU_BACK; }
static inline void ui_sdl_render_online_countdown(UiSdl *ui, const OnlineMultiplayerContext *ctx, int countdown) { (void)ui; (void)ctx; (void)countdown; }
static inline void ui_sdl_render_online_game(UiSdl *ui, const OnlineMultiplayerContext *ctx) { (void)ui; (void)ctx; }
static inline Direction ui_sdl_poll_online_game_input(UiSdl *ui, const Settings *settings, int *out_quit) { (void)ui; (void)settings; (void)out_quit; return DIR_NONE; }
static inline void ui_sdl_render_online_gameover(UiSdl *ui, const OnlineMultiplayerContext *ctx) { (void)ui; (void)ctx; }
static inline UiMenuAction ui_sdl_poll_online_gameover(UiSdl *ui, int *out_quit) { (void)ui; (void)out_quit; return UI_MENU_BACK; }

#endif // WINDOWS_BUILD

#endif // ONLINE_MULTIPLAYER_STUB_H
