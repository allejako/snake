#include "scene_master.h"
#include "ui_sdl.h"
#include "settings.h"
#include "multiplayer.h"
#include "config.h"
#include <stdlib.h>

/* Multiplayer menu items */
typedef enum {
    MULTIPLAYER_MENU_HOST = 0,
    MULTIPLAYER_MENU_JOIN,
    MULTIPLAYER_MENU_BACK,
    MULTIPLAYER_MENU_COUNT
} MultiplayerMenuItem;

/* Scene state */
typedef struct {
    int selected_index;
} MultiplayerOnlineMenuSceneData;

static MultiplayerOnlineMenuSceneData *state = NULL;

#define MENU_FRAME_DELAY_MS 16

static void multiplayer_online_menu_scene_init(UiSdl *ui, Settings *settings, void *user_data)
{
    (void)ui;
    (void)settings;
    (void)user_data;

    state = (MultiplayerOnlineMenuSceneData *)malloc(sizeof(MultiplayerOnlineMenuSceneData));
    if (state) {
        state->selected_index = 0;
    }
}

static void multiplayer_online_menu_scene_update(UiSdl *ui, Settings *settings)
{
    if (!state) return;

    SceneContext *ctx = scene_master_get_context();

    /* Poll input */
    int quit = 0;
    UiMenuAction action = ui_sdl_poll_multiplayer_online_menu(ui, &quit);

    if (quit) {
        scene_master_transition(SCENE_QUIT, NULL);
        return;
    }

    /* Handle menu navigation */
    if (action == UI_MENU_UP) {
        state->selected_index = (state->selected_index - 1 + MULTIPLAYER_MENU_COUNT) % MULTIPLAYER_MENU_COUNT;
    }
    else if (action == UI_MENU_DOWN) {
        state->selected_index = (state->selected_index + 1) % MULTIPLAYER_MENU_COUNT;
    }
    else if (action == UI_MENU_SELECT) {
        /* Handle selection */
        switch (state->selected_index) {
            case MULTIPLAYER_MENU_HOST:
                /* Host game (always private) */
                if (ctx && ctx->multiplayer) {
                    const char *player_name = (settings->profile_name[0] != '\0')
                        ? settings->profile_name : "Player";

                    /* TODO: Get config for board dimensions - hardcoded for now */
                    if (multiplayer_host(ctx->multiplayer, 1, 40, 30, player_name) == MPAPI_OK) {
                        scene_master_transition(SCENE_MULTIPLAYER_LOBBY, NULL);
                    }
                    /* If failed, stay in menu */
                }
                break;

            case MULTIPLAYER_MENU_JOIN:
                /* Go to session input scene */
                /* Note: We need to add SCENE_MULTIPLAYER_SESSION_INPUT */
                scene_master_transition(SCENE_MENU, NULL);  /* Placeholder for now */
                break;

            case MULTIPLAYER_MENU_BACK:
                scene_master_transition(SCENE_MENU, NULL);
                break;
        }
    }
    else if (action == UI_MENU_BACK) {
        scene_master_transition(SCENE_MENU, NULL);
    }
}

static void multiplayer_online_menu_scene_render(UiSdl *ui, Settings *settings)
{
    (void)settings;
    if (!state) return;

    /* Render the multiplayer online menu */
    ui_sdl_render_multiplayer_online_menu(ui, state->selected_index);
    SDL_Delay(MENU_FRAME_DELAY_MS);
}

static void multiplayer_online_menu_scene_cleanup(void)
{
    if (state) {
        free(state);
        state = NULL;
    }
}

/* Register this scene with scene_master */
void scene_multiplayer_online_menu_register(void)
{
    Scene scene = {
        .init = multiplayer_online_menu_scene_init,
        .update = multiplayer_online_menu_scene_update,
        .render = multiplayer_online_menu_scene_render,
        .cleanup = multiplayer_online_menu_scene_cleanup
    };

    scene_master_register_scene(SCENE_MULTIPLAYER_ONLINE_MENU, scene);
}
