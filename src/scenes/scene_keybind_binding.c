#include "scene_master.h"
#include "ui_sdl.h"
#include "settings.h"
#include <stdlib.h>

/* Scene state */
typedef struct {
    int current_action;  /* Current action being bound (0 to SETTINGS_ACTION_COUNT-1) */
} KeybindBindingSceneData;

static KeybindBindingSceneData *state = NULL;

#define MENU_FRAME_DELAY_MS 16

static void keybind_binding_scene_init(UiSdl *ui, Settings *settings, void *user_data)
{
    (void)ui;
    (void)settings;
    (void)user_data;

    state = (KeybindBindingSceneData *)malloc(sizeof(KeybindBindingSceneData));
    if (state) {
        state->current_action = 0;  /* Start with first action */
    }
}

static void keybind_binding_scene_update(UiSdl *ui, Settings *settings)
{
    if (!state) return;

    int cancel = 0;
    int quit = 0;

    SDL_Keycode pressed_key = ui_sdl_poll_keybind_input(ui, &cancel, &quit);

    if (quit) {
        scene_master_transition(SCENE_QUIT, NULL);
        return;
    }

    if (cancel) {
        /* User pressed ESC, don't save changes */
        /* Reload keybindings from file to discard changes */
        settings_load(settings);
        scene_master_transition(SCENE_OPTIONS_MENU, NULL);
        return;
    }

    if (pressed_key != 0) {
        /* Valid key pressed */
        SettingAction action = (SettingAction)state->current_action;

        /* Set binding with auto-swap */
        settings_set_key_with_swap(settings, action, pressed_key);

        /* Move to next action */
        state->current_action++;

        if (state->current_action >= SETTINGS_ACTION_COUNT) {
            /* All actions bound, save and return */
            settings_save(settings);
            scene_master_transition(SCENE_OPTIONS_MENU, NULL);
            return;
        }
    }
}

static void keybind_binding_scene_render(UiSdl *ui, Settings *settings)
{
    if (!state) return;

    /* Render current binding prompt */
    SettingAction current = (SettingAction)state->current_action;
    ui_sdl_render_keybind_prompt(ui, settings, current);
    SDL_Delay(MENU_FRAME_DELAY_MS);
}

static void keybind_binding_scene_cleanup(void)
{
    if (state) {
        free(state);
        state = NULL;
    }
}

/* Register this scene with scene_master */
void scene_keybind_binding_register(void)
{
    Scene scene = {
        .init = keybind_binding_scene_init,
        .update = keybind_binding_scene_update,
        .render = keybind_binding_scene_render,
        .cleanup = keybind_binding_scene_cleanup
    };

    scene_master_register_scene(SCENE_KEYBIND_BINDING, scene);
}
