#include "scene_master.h"
#include "ui_sdl.h"
#include "settings.h"
#include <stdlib.h>
#include <string.h>

/* Menu items */
typedef enum {
    MENU_SINGLEPLAYER = 0,
    MENU_MULTIPLAYER,
    MENU_OPTIONS,
    MENU_SCOREBOARD,
    MENU_QUIT,
    MENU_COUNT
} MenuItem;

/* Scene state */
typedef struct {
    int selected_index;
} MenuSceneData;

static MenuSceneData *state = NULL;

static void menu_scene_init(UiSdl *ui, Settings *settings, void *user_data)
{
    (void)ui;
    (void)settings;
    (void)user_data;

    state = (MenuSceneData *)malloc(sizeof(MenuSceneData));
    if (state) {
        state->selected_index = 0;
    }
}

static void menu_scene_update(UiSdl *ui, Settings *settings)
{
    if (!state) return;

    /* Poll input */
    int quit = 0;
    UiMenuAction action = ui_sdl_poll_menu(ui, settings, &quit);

    if (quit) {
        scene_master_transition(SCENE_QUIT, NULL);
        return;
    }

    /* Handle menu navigation */
    if (action == UI_MENU_UP) {
        state->selected_index = (state->selected_index - 1 + MENU_COUNT) % MENU_COUNT;
    }
    else if (action == UI_MENU_DOWN) {
        state->selected_index = (state->selected_index + 1) % MENU_COUNT;
    }
    else if (action == UI_MENU_SELECT) {
        /* Handle selection */
        switch (state->selected_index) {
            case MENU_SINGLEPLAYER:
                scene_master_transition(SCENE_SINGLEPLAYER, NULL);
                break;
            case MENU_MULTIPLAYER:
                scene_master_transition(SCENE_MULTIPLAYER_ONLINE_MENU, NULL);
                break;
            case MENU_OPTIONS:
                scene_master_transition(SCENE_OPTIONS_MENU, NULL);
                break;
            case MENU_SCOREBOARD:
                scene_master_transition(SCENE_SCOREBOARD, NULL);
                break;
            case MENU_QUIT:
                scene_master_transition(SCENE_QUIT, NULL);
                break;
        }
    }
}

static void menu_scene_render(UiSdl *ui, Settings *settings)
{
    if (!state) return;

    /* Render the menu using existing UI function */
    ui_sdl_render_menu(ui, settings, state->selected_index);
}

static void menu_scene_cleanup(void)
{
    if (state) {
        free(state);
        state = NULL;
    }
}

/* Register this scene with scene_master */
void scene_menu_register(void)
{
    Scene scene = {
        .init = menu_scene_init,
        .update = menu_scene_update,
        .render = menu_scene_render,
        .cleanup = menu_scene_cleanup
    };

    scene_master_register_scene(SCENE_MENU, scene);
}
