#include "scene_master.h"
#include "ui_sdl.h"
#include "settings.h"
#include "audio_sdl.h"
#include <stdlib.h>

/* Sound menu items */
typedef enum {
    SOUND_MENU_MUSIC_VOLUME = 0,
    SOUND_MENU_EFFECTS_VOLUME,
    SOUND_MENU_BACK,
    SOUND_MENU_COUNT
} SoundMenuItem;

/* Scene state */
typedef struct {
    int selected_index;
} SoundSettingsSceneData;

static SoundSettingsSceneData *state = NULL;

#define MENU_FRAME_DELAY_MS 16

static void sound_settings_scene_init(UiSdl *ui, Settings *settings, void *user_data)
{
    (void)ui;
    (void)settings;
    (void)user_data;

    state = (SoundSettingsSceneData *)malloc(sizeof(SoundSettingsSceneData));
    if (state) {
        state->selected_index = 0;
    }
}

static void sound_settings_scene_update(UiSdl *ui, Settings *settings)
{
    if (!state) return;

    SceneContext *ctx = scene_master_get_context();

    /* Poll input */
    int quit = 0;
    UiMenuAction action = ui_sdl_poll_sound_settings(ui, settings, &quit);

    if (quit) {
        scene_master_transition(SCENE_QUIT, NULL);
        return;
    }

    /* Handle menu navigation */
    if (action == UI_MENU_UP) {
        state->selected_index = (state->selected_index - 1 + SOUND_MENU_COUNT) % SOUND_MENU_COUNT;
    }
    else if (action == UI_MENU_DOWN) {
        state->selected_index = (state->selected_index + 1) % SOUND_MENU_COUNT;
    }
    else if (action == UI_MENU_LEFT) {
        /* Decrease volume */
        if (ctx && ctx->audio) {
            if (state->selected_index == SOUND_MENU_MUSIC_VOLUME) {
                int current = audio_sdl_get_music_volume(ctx->audio);
                int new_volume = current - 5;
                audio_sdl_set_music_volume(ctx->audio, new_volume);
                settings->music_volume = audio_sdl_get_music_volume(ctx->audio);
            }
            else if (state->selected_index == SOUND_MENU_EFFECTS_VOLUME) {
                int current = audio_sdl_get_effects_volume(ctx->audio);
                int new_volume = current - 5;
                audio_sdl_set_effects_volume(ctx->audio, new_volume);
                settings->effects_volume = audio_sdl_get_effects_volume(ctx->audio);
            }
        }
    }
    else if (action == UI_MENU_RIGHT) {
        /* Increase volume */
        if (ctx && ctx->audio) {
            if (state->selected_index == SOUND_MENU_MUSIC_VOLUME) {
                int current = audio_sdl_get_music_volume(ctx->audio);
                int new_volume = current + 5;
                audio_sdl_set_music_volume(ctx->audio, new_volume);
                settings->music_volume = audio_sdl_get_music_volume(ctx->audio);
            }
            else if (state->selected_index == SOUND_MENU_EFFECTS_VOLUME) {
                int current = audio_sdl_get_effects_volume(ctx->audio);
                int new_volume = current + 5;
                audio_sdl_set_effects_volume(ctx->audio, new_volume);
                settings->effects_volume = audio_sdl_get_effects_volume(ctx->audio);
            }
        }
    }
    else if (action == UI_MENU_SELECT || action == UI_MENU_BACK) {
        if (state->selected_index == SOUND_MENU_BACK || action == UI_MENU_BACK) {
            scene_master_transition(SCENE_OPTIONS_MENU, NULL);
        }
    }
}

static void sound_settings_scene_render(UiSdl *ui, Settings *settings)
{
    if (!state) return;

    SceneContext *ctx = scene_master_get_context();

    /* Render the sound settings menu */
    ui_sdl_render_sound_settings(ui, settings, ctx ? ctx->audio : NULL, state->selected_index);
    SDL_Delay(MENU_FRAME_DELAY_MS);
}

static void sound_settings_scene_cleanup(void)
{
    if (state) {
        free(state);
        state = NULL;
    }
}

/* Register this scene with scene_master */
void scene_sound_settings_register(void)
{
    Scene scene = {
        .init = sound_settings_scene_init,
        .update = sound_settings_scene_update,
        .render = sound_settings_scene_render,
        .cleanup = sound_settings_scene_cleanup
    };

    scene_master_register_scene(SCENE_SOUND_SETTINGS, scene);
}
