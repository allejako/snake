#include "scene_master.h"
#include "ui_sdl.h"
#include "settings.h"
#include <stdlib.h>

/* Options menu items */
typedef enum {
    OPTIONS_MENU_KEYBINDS = 0,
    OPTIONS_MENU_SOUND,
    OPTIONS_MENU_BACK,
    OPTIONS_MENU_COUNT
} OptionsMenuItem;

/* Scene state */
typedef struct {
    int selected_index;
} OptionsMenuSceneData;

static OptionsMenuSceneData *state = NULL;

#define MENU_FRAME_DELAY_MS 16

static void options_menu_scene_init(UiSdl *ui, Settings *settings, void *user_data)
{
    (void)ui;
    (void)settings;
    (void)user_data;

    state = (OptionsMenuSceneData *)malloc(sizeof(OptionsMenuSceneData));
    if (state) {
        state->selected_index = 0;
    }
}

static void options_menu_scene_update(UiSdl *ui, Settings *settings)
{
    if (!state) return;

    /* Poll input */
    int quit = 0;
    UiMenuAction action = ui_sdl_poll_options_menu(ui, settings, &quit);

    if (quit) {
        scene_master_transition(SCENE_QUIT, NULL);
        return;
    }

    /* Handle menu navigation */
    if (action == UI_MENU_UP) {
        state->selected_index = (state->selected_index - 1 + OPTIONS_MENU_COUNT) % OPTIONS_MENU_COUNT;
    }
    else if (action == UI_MENU_DOWN) {
        state->selected_index = (state->selected_index + 1) % OPTIONS_MENU_COUNT;
    }
    else if (action == UI_MENU_SELECT) {
        /* Handle selection */
        switch (state->selected_index) {
            case OPTIONS_MENU_KEYBINDS:
                scene_master_transition(SCENE_KEYBIND_BINDING, NULL);
                break;
            case OPTIONS_MENU_SOUND:
                scene_master_transition(SCENE_SOUND_SETTINGS, NULL);
                break;
            case OPTIONS_MENU_BACK:
                scene_master_transition(SCENE_MENU, NULL);
                break;
        }
    }
    else if (action == UI_MENU_BACK) {
        scene_master_transition(SCENE_MENU, NULL);
    }
}

static void options_menu_scene_render(UiSdl *ui, Settings *settings)
{
    if (!state) return;

    /* Render the options menu */
    ui_sdl_render_options_menu(ui, settings, state->selected_index);
    SDL_Delay(MENU_FRAME_DELAY_MS);
}

static void options_menu_scene_cleanup(void)
{
    if (state) {
        free(state);
        state = NULL;
    }
}

/* Register this scene with scene_master */
void scene_options_menu_register(void)
{
    Scene scene = {
        .init = options_menu_scene_init,
        .update = options_menu_scene_update,
        .render = options_menu_scene_render,
        .cleanup = options_menu_scene_cleanup
    };

    scene_master_register_scene(SCENE_OPTIONS_MENU, scene);
}
