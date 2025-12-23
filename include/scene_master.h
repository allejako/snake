#ifndef SCENE_MASTER_H
#define SCENE_MASTER_H

#include "ui_sdl.h"
#include "settings.h"
#include "audio_sdl.h"
#include "scoreboard.h"
#include "multiplayer.h"
#include "../src/mpapi/c_client/libs/mpapi.h"

/**
 * Scene types representing different screens/states in the game.
 */
typedef enum {
    SCENE_MENU,
    SCENE_SINGLEPLAYER,
    SCENE_GAME_OVER,
    SCENE_SCOREBOARD,
    SCENE_PAUSE_MENU,
    SCENE_OPTIONS_MENU,
    SCENE_SOUND_SETTINGS,
    SCENE_KEYBIND_PLAYER_SELECT,
    SCENE_KEYBIND_BINDING,
    SCENE_MULTIPLAYER_ONLINE_MENU,
    SCENE_MULTIPLAYER_LOBBY,
    SCENE_MULTIPLAYER_GAME,
    SCENE_QUIT
} SceneType;

/**
 * Scene lifecycle function pointers.
 * Each scene must implement these four callbacks.
 */
typedef struct {
    void (*init)(UiSdl *ui, Settings *settings, void *user_data);
    void (*update)(UiSdl *ui, Settings *settings);
    void (*render)(UiSdl *ui, Settings *settings);
    void (*cleanup)(void);
} Scene;

/**
 * Global context shared across all scenes.
 * Contains pointers to resources that scenes need to access.
 */
typedef struct {
    UiSdl *ui;
    Settings *settings;
    AudioSdl *audio;
    Scoreboard *scoreboard;
    mpapi *mp_api;
    Multiplayer *multiplayer;  // NULL when not in multiplayer mode
} SceneContext;

/**
 * Initialize the scene master system.
 * Must be called before using any other scene_master functions.
 *
 * @param ui UI renderer instance
 * @param settings Game settings
 * @param audio Audio system instance
 * @param scoreboard Scoreboard instance
 * @param mp_api MPAPI instance for multiplayer
 */
void scene_master_init(UiSdl *ui, Settings *settings, AudioSdl *audio,
                       Scoreboard *scoreboard, mpapi *mp_api);

/**
 * Transition to a new scene.
 * Calls cleanup() on current scene, then init() on new scene.
 *
 * @param type The scene to transition to
 * @param user_data Optional data to pass to the new scene's init()
 */
void scene_master_transition(SceneType type, void *user_data);

/**
 * Run the scene master main loop.
 * Blocks until SCENE_QUIT is reached.
 * Continuously calls update() and render() on the current scene.
 */
void scene_master_run(void);

/**
 * Get the current active scene type.
 *
 * @return Current scene type
 */
SceneType scene_master_get_current(void);

/**
 * Get the global scene context.
 * Allows scenes to access shared resources.
 *
 * @return Pointer to scene context
 */
SceneContext *scene_master_get_context(void);

/**
 * Shutdown the scene master system.
 * Calls cleanup() on current scene and frees resources.
 */
void scene_master_shutdown(void);

#endif
