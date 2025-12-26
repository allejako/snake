#include "scene_master.h"
#include "ui_sdl.h"
#include "settings.h"
#include "game.h"
#include "audio_sdl.h"
#include <stdlib.h>

/* Pause menu items */
typedef enum {
    PAUSE_MENU_CONTINUE = 0,
    PAUSE_MENU_OPTIONS,
    PAUSE_MENU_QUIT,
    PAUSE_MENU_COUNT
} PauseMenuItem;

/* Scene state */
typedef struct {
    int selected_index;
    /* TODO: Need reference to game state to resume */
} PauseMenuSceneData;

static PauseMenuSceneData *state = NULL;

#define MENU_FRAME_DELAY_MS 16

static void pause_menu_scene_init(UiSdl *ui, Settings *settings, void *user_data)
{
    (void)ui;
    (void)settings;
    (void)user_data;

    state = (PauseMenuSceneData *)malloc(sizeof(PauseMenuSceneData));
    if (state) {
        state->selected_index = 0;
    }

    /* Pause audio */
    SceneContext *ctx = scene_master_get_context();
    if (ctx && ctx->audio) {
        audio_sdl_pause_music(ctx->audio);
    }
}

static void pause_menu_scene_update(UiSdl *ui, Settings *settings)
{
    if (!state) return;

    SceneContext *ctx = scene_master_get_context();

    /* Poll input */
    int quit = 0;
    UiPauseAction action = ui_sdl_poll_pause(ui, settings, &quit);

    if (quit) {
        scene_master_transition(SCENE_QUIT, NULL);
        return;
    }

    /* Handle menu navigation */
    if (action == UI_PAUSE_UP) {
        state->selected_index = (state->selected_index - 1 + PAUSE_MENU_COUNT) % PAUSE_MENU_COUNT;
    }
    else if (action == UI_PAUSE_DOWN) {
        state->selected_index = (state->selected_index + 1) % PAUSE_MENU_COUNT;
    }
    else if (action == UI_PAUSE_ESCAPE || (action == UI_PAUSE_SELECT && state->selected_index == PAUSE_MENU_CONTINUE)) {
        /* Resume game */
        if (ctx && ctx->audio) {
            audio_sdl_resume_music(ctx->audio);
        }
        /* TODO: Return to singleplayer scene */
        scene_master_transition(SCENE_SINGLEPLAYER, NULL);
    }
    else if (action == UI_PAUSE_SELECT) {
        switch (state->selected_index) {
            case PAUSE_MENU_OPTIONS:
                /* Go to options (simplified - just go back for now) */
                scene_master_transition(SCENE_OPTIONS_MENU, NULL);
                break;
            case PAUSE_MENU_QUIT:
                /* Quit to menu */
                if (ctx && ctx->audio) {
                    audio_sdl_resume_music(ctx->audio);
                }
                scene_master_transition(SCENE_MENU, NULL);
                break;
        }
    }
}

static void pause_menu_scene_render(UiSdl *ui, Settings *settings)
{
    (void)settings;
    if (!state) return;

    /* TODO: Need game reference to render pause menu properly */
    /* For now, just render a simple pause menu */
    /* ui_sdl_render_pause_menu(ui, game, player_name, state->selected_index, debug_mode, current_tick_ms); */
    SDL_Delay(MENU_FRAME_DELAY_MS);
}

static void pause_menu_scene_cleanup(void)
{
    if (state) {
        free(state);
        state = NULL;
    }
}

/* Register this scene with scene_master */
void scene_pause_menu_register(void)
{
    Scene scene = {
        .init = pause_menu_scene_init,
        .update = pause_menu_scene_update,
        .render = pause_menu_scene_render,
        .cleanup = pause_menu_scene_cleanup
    };

    scene_master_register_scene(SCENE_PAUSE_MENU, scene);
}
