#include "scene_master.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Scene state
static SceneContext context;
static SceneType current_scene_type = SCENE_QUIT;
static Scene *current_scene = NULL;
static int running = 0;

// Scene registry - maps SceneType to Scene implementation
// Will be populated as scenes are implemented
static Scene scene_registry[13];

void scene_master_init(UiSdl *ui, Settings *settings, AudioSdl *audio,
                       Scoreboard *scoreboard, mpapi *mp_api) {
    memset(&context, 0, sizeof(context));
    context.ui = ui;
    context.settings = settings;
    context.audio = audio;
    context.scoreboard = scoreboard;
    context.mp_api = mp_api;
    context.multiplayer = NULL;

    // Initialize scene registry with NULL scenes
    // Scenes will be registered as they are implemented
    memset(scene_registry, 0, sizeof(scene_registry));

    current_scene_type = SCENE_QUIT;
    current_scene = NULL;
    running = 0;

    printf("[SceneMaster] Initialized\n");
}

void scene_master_transition(SceneType type, void *user_data) {
    if (type < 0 || type >= 13) {
        fprintf(stderr, "[SceneMaster] ERROR: Invalid scene type %d\n", type);
        return;
    }

    // Cleanup current scene
    if (current_scene != NULL && current_scene->cleanup != NULL) {
        printf("[SceneMaster] Cleaning up scene %d\n", current_scene_type);
        current_scene->cleanup();
    }

    // Update current scene
    current_scene_type = type;
    current_scene = &scene_registry[type];

    // Initialize new scene
    if (current_scene != NULL && current_scene->init != NULL) {
        printf("[SceneMaster] Transitioning to scene %d\n", type);
        current_scene->init(context.ui, context.settings, user_data);
    } else if (type != SCENE_QUIT) {
        fprintf(stderr, "[SceneMaster] WARNING: Scene %d not implemented yet\n", type);
    }

    // If transitioning to SCENE_QUIT, stop the main loop
    if (type == SCENE_QUIT) {
        running = 0;
        printf("[SceneMaster] Quit requested\n");
    }
}

void scene_master_run(void) {
    if (current_scene == NULL) {
        fprintf(stderr, "[SceneMaster] ERROR: No scene active. Call scene_master_transition() first.\n");
        return;
    }

    running = 1;
    printf("[SceneMaster] Starting main loop\n");

    while (running && current_scene_type != SCENE_QUIT) {
        // Update scene logic
        if (current_scene != NULL && current_scene->update != NULL) {
            current_scene->update(context.ui, context.settings);
        }

        // Render scene
        if (current_scene != NULL && current_scene->render != NULL) {
            current_scene->render(context.ui, context.settings);
        }

        // Small delay to prevent 100% CPU usage
        SDL_Delay(10);

        // Update current_scene pointer in case transition happened during update
        if (current_scene_type >= 0 && current_scene_type < 13) {
            current_scene = &scene_registry[current_scene_type];
        }
    }

    printf("[SceneMaster] Main loop exited\n");
}

SceneType scene_master_get_current(void) {
    return current_scene_type;
}

SceneContext *scene_master_get_context(void) {
    return &context;
}

void scene_master_shutdown(void) {
    printf("[SceneMaster] Shutting down\n");

    // Cleanup current scene
    if (current_scene != NULL && current_scene->cleanup != NULL) {
        current_scene->cleanup();
    }

    // Clean up multiplayer if active
    if (context.multiplayer != NULL) {
        printf("[SceneMaster] Cleaning up multiplayer context\n");
        multiplayer_destroy(context.multiplayer);
        context.multiplayer = NULL;
    }

    current_scene = NULL;
    current_scene_type = SCENE_QUIT;
    running = 0;
}

/**
 * Register a scene implementation.
 * Called by scene implementation files during initialization.
 *
 * @param type Scene type to register
 * @param scene Scene implementation
 */
void scene_master_register_scene(SceneType type, Scene scene) {
    if (type < 0 || type >= 13) {
        fprintf(stderr, "[SceneMaster] ERROR: Cannot register invalid scene type %d\n", type);
        return;
    }

    scene_registry[type] = scene;
    printf("[SceneMaster] Registered scene %d\n", type);
}
