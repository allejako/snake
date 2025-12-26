#ifndef SCENE_H
#define SCENE_H

#include <SDL2/SDL.h>

/* Forward declarations */
typedef struct SceneContext SceneContext;
typedef struct UiSdl UiSdl;
typedef struct Settings Settings;

/* Scene lifecycle callbacks */
typedef void (*SceneInitFunc)(UiSdl *ui, Settings *settings, void *user_data);
typedef void (*SceneUpdateFunc)(UiSdl *ui, Settings *settings);
typedef void (*SceneRenderFunc)(UiSdl *ui, Settings *settings);
typedef void (*SceneCleanupFunc)(void);

/* Scene definition */
typedef struct {
    SceneInitFunc init;      /* Called once when entering scene */
    SceneUpdateFunc update;  /* Handle input and logic (every frame) */
    SceneRenderFunc render;  /* Draw to screen (every frame) */
    SceneCleanupFunc cleanup; /* Free resources (once on exit) */
} Scene;

/* Scene IDs (matching current APP_* states) */
typedef enum {
    SCENE_MENU = 0,
    SCENE_SINGLEPLAYER,
    SCENE_GAMEOVER,
    SCENE_SCOREBOARD,
    SCENE_PAUSE_MENU,
    SCENE_OPTIONS_MENU,
    SCENE_SOUND_SETTINGS,
    SCENE_KEYBIND_PLAYER_SELECT,
    SCENE_KEYBIND_BINDING,
    SCENE_MULTIPLAYER_ONLINE_MENU,
    SCENE_MULTIPLAYER_LOBBY,
    SCENE_MULTIPLAYER_GAME,
    SCENE_QUIT,
    SCENE_COUNT
} SceneType;

#endif /* SCENE_H */
