#include "scene_master.h"
#include "ui_sdl.h"
#include "scoreboard.h"
#include <stdlib.h>

/* Scene state - none needed for this simple scene */

static void scoreboard_scene_init(UiSdl *ui, Settings *settings, void *user_data)
{
    (void)ui;
    (void)settings;
    (void)user_data;
    /* No state to initialize */
}

static void scoreboard_scene_update(UiSdl *ui, Settings *settings)
{
    (void)ui;
    (void)settings;

    /* Scoreboard is a "blocking" scene - show it once then return to menu */
    /* The rendering will happen in render(), and then we immediately transition back */
    scene_master_transition(SCENE_MENU, NULL);
}

static void scoreboard_scene_render(UiSdl *ui, Settings *settings)
{
    (void)settings;

    /* Get scoreboard from context */
    SceneContext *ctx = scene_master_get_context();
    if (ctx && ctx->scoreboard) {
        scoreboard_sort(ctx->scoreboard);
        ui_sdl_show_scoreboard(ui, ctx->scoreboard);
    }
}

static void scoreboard_scene_cleanup(void)
{
    /* No state to cleanup */
}

/* Register this scene with scene_master */
void scene_scoreboard_register(void)
{
    Scene scene = {
        .init = scoreboard_scene_init,
        .update = scoreboard_scene_update,
        .render = scoreboard_scene_render,
        .cleanup = scoreboard_scene_cleanup
    };

    scene_master_register_scene(SCENE_SCOREBOARD, scene);
}
