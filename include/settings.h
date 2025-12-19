#ifndef SETTINGS_H
#define SETTINGS_H

#include <SDL2/SDL.h>

#define SETTINGS_MAX_PROFILE_NAME 32
#define SETTINGS_MAX_PLAYERS 1
#define SETTINGS_ACTIONS_PER_PLAYER 5

typedef enum {
    SETTING_ACTION_UP = 0,
    SETTING_ACTION_DOWN,
    SETTING_ACTION_LEFT,
    SETTING_ACTION_RIGHT,
    SETTING_ACTION_USE,
    SETTING_ACTION_COUNT
} SettingAction;

typedef struct {
    char profile_name[SETTINGS_MAX_PROFILE_NAME];
    int music_volume;    // 0-100
    int effects_volume;  // 0-100
    SDL_Keycode keybindings[SETTINGS_MAX_PLAYERS][SETTINGS_ACTIONS_PER_PLAYER];
    char filename[256];
} Settings;

/**
 * Initialize settings structure with filename.
 */
void settings_init(Settings *s, const char *filename);

/**
 * Load settings from file. Returns 1 on success, 0 on failure.
 */
int settings_load(Settings *s);

/**
 * Save settings to file. Returns 1 on success, 0 on failure.
 */
int settings_save(const Settings *s);

/**
 * Set default values for all settings.
 */
void settings_set_defaults(Settings *s);

/**
 * Check if profile name is set (non-empty).
 */
int settings_has_profile(const Settings *s);

/**
 * Get keybinding for specific player and action.
 */
SDL_Keycode settings_get_key(const Settings *s, int player, SettingAction action);

/**
 * Set keybinding for specific player and action.
 */
void settings_set_key(Settings *s, int player, SettingAction action, SDL_Keycode key);

/**
 * Find which action (if any) a key is bound to for a player.
 * Returns action index (0-4) or -1 if not found.
 */
int settings_find_action(const Settings *s, int player, SDL_Keycode key);

/**
 * Get SDL key name for display.
 */
const char *settings_key_name(SDL_Keycode key);

/**
 * Swap keys if there's a conflict when binding a new key.
 */
void settings_set_key_with_swap(Settings *s, int player, SettingAction action, SDL_Keycode new_key);

/**
 * Get display name for an action.
 */
const char *settings_action_name(SettingAction action);

#endif
