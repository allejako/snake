#ifndef KEYBINDINGS_H
#define KEYBINDINGS_H

#include <SDL2/SDL.h>

#define KB_MAX_PLAYERS 4
#define KB_ACTION_COUNT 5

// Keybinding actions
typedef enum {
    KB_ACTION_UP = 0,
    KB_ACTION_DOWN,
    KB_ACTION_LEFT,
    KB_ACTION_RIGHT,
    KB_ACTION_USE
} KeybindAction;

// Player keybinding set
typedef struct {
    SDL_Keycode bindings[KB_ACTION_COUNT];
} PlayerKeybindings;

// Global keybindings state
typedef struct {
    PlayerKeybindings players[KB_MAX_PLAYERS];
    char filename[256];
} Keybindings;

// Core API
void keybindings_init(Keybindings *kb, const char *filename);
int keybindings_load(Keybindings *kb);
int keybindings_save(const Keybindings *kb);
void keybindings_set_defaults(Keybindings *kb);

// Binding management
void keybindings_set(Keybindings *kb, int player_index, KeybindAction action, SDL_Keycode key);
SDL_Keycode keybindings_get(const Keybindings *kb, int player_index, KeybindAction action);

// Auto-swap on conflict: if key is already bound to another action for this player, swap them
void keybindings_set_with_swap(Keybindings *kb, int player_index, KeybindAction action, SDL_Keycode key);

// Query: which action (if any) does this key trigger for this player?
// Returns -1 if not bound, otherwise KeybindAction
int keybindings_find_action(const Keybindings *kb, int player_index, SDL_Keycode key);

// Convert action to string for display
const char *keybindings_action_name(KeybindAction action);

// Convert SDL_Keycode to readable string
const char *keybindings_key_name(SDL_Keycode key);

#endif
