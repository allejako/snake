#include "keybindings.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// Default bindings for all 4 players
static const SDL_Keycode DEFAULT_BINDINGS[KB_MAX_PLAYERS][KB_ACTION_COUNT] = {
    // Player 1: WASD + E
    {SDLK_w, SDLK_s, SDLK_a, SDLK_d, SDLK_e},
    // Player 2: Arrows + Right Shift
    {SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT, SDLK_RSHIFT},
    // Player 3: IJKL + U
    {SDLK_i, SDLK_k, SDLK_j, SDLK_l, SDLK_u},
    // Player 4: TFGH + R
    {SDLK_t, SDLK_g, SDLK_f, SDLK_h, SDLK_r}
};

// Helper: convert action enum to string
static const char *action_to_string(KeybindAction action) {
    switch (action) {
        case KB_ACTION_UP:    return "move_up";
        case KB_ACTION_DOWN:  return "move_down";
        case KB_ACTION_LEFT:  return "move_left";
        case KB_ACTION_RIGHT: return "move_right";
        case KB_ACTION_USE:   return "use";
        default:              return NULL;
    }
}

// Helper: convert string to action enum
static int string_to_action(const char *str) {
    if (strcmp(str, "move_up") == 0)    return KB_ACTION_UP;
    if (strcmp(str, "move_down") == 0)  return KB_ACTION_DOWN;
    if (strcmp(str, "move_left") == 0)  return KB_ACTION_LEFT;
    if (strcmp(str, "move_right") == 0) return KB_ACTION_RIGHT;
    if (strcmp(str, "use") == 0)        return KB_ACTION_USE;
    return -1;
}

// Helper: trim whitespace from string
static void trim_whitespace(char *str) {
    // Trim leading whitespace
    char *start = str;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    // Move trimmed string to beginning
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }

    // Trim trailing whitespace
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
}

void keybindings_init(Keybindings *kb, const char *filename) {
    strncpy(kb->filename, filename, sizeof(kb->filename) - 1);
    kb->filename[sizeof(kb->filename) - 1] = '\0';
    keybindings_set_defaults(kb);
}

void keybindings_set_defaults(Keybindings *kb) {
    for (int p = 0; p < KB_MAX_PLAYERS; p++) {
        for (int a = 0; a < KB_ACTION_COUNT; a++) {
            kb->players[p].bindings[a] = DEFAULT_BINDINGS[p][a];
        }
    }
}

int keybindings_load(Keybindings *kb) {
    FILE *f = fopen(kb->filename, "r");
    if (!f) {
        return 0;  // File doesn't exist, use defaults
    }

    int current_player = -1;
    char line[256];

    while (fgets(line, sizeof(line), f)) {
        // Remove newline
        line[strcspn(line, "\r\n")] = '\0';

        // Skip empty lines and comments
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        // Check for section header [PlayerN]
        if (line[0] == '[') {
            if (strstr(line, "[Player1]")) current_player = 0;
            else if (strstr(line, "[Player2]")) current_player = 1;
            else if (strstr(line, "[Player3]")) current_player = 2;
            else if (strstr(line, "[Player4]")) current_player = 3;
            continue;
        }

        // Parse key=value
        if (current_player >= 0 && current_player < KB_MAX_PLAYERS) {
            char *eq = strchr(line, '=');
            if (!eq) continue;

            *eq = '\0';
            char *action_str = line;
            char *key_str = eq + 1;

            // Trim whitespace
            trim_whitespace(action_str);
            trim_whitespace(key_str);

            // Convert strings
            int action = string_to_action(action_str);
            if (action < 0) continue;

            SDL_Keycode key = SDL_GetKeyFromName(key_str);
            if (key == SDLK_UNKNOWN) continue;

            kb->players[current_player].bindings[action] = key;
        }
    }

    fclose(f);
    return 1;
}

int keybindings_save(const Keybindings *kb) {
    FILE *f = fopen(kb->filename, "w");
    if (!f) {
        return 0;
    }

    for (int p = 0; p < KB_MAX_PLAYERS; p++) {
        fprintf(f, "[Player%d]\n", p + 1);

        for (int a = 0; a < KB_ACTION_COUNT; a++) {
            const char *action_name = action_to_string((KeybindAction)a);
            const char *key_name = SDL_GetKeyName(kb->players[p].bindings[a]);

            if (action_name && key_name) {
                fprintf(f, "%s=%s\n", action_name, key_name);
            }
        }

        fprintf(f, "\n");
    }

    fclose(f);
    return 1;
}

void keybindings_set(Keybindings *kb, int player_index, KeybindAction action, SDL_Keycode key) {
    if (player_index < 0 || player_index >= KB_MAX_PLAYERS) return;
    if (action < 0 || action >= KB_ACTION_COUNT) return;

    kb->players[player_index].bindings[action] = key;
}

SDL_Keycode keybindings_get(const Keybindings *kb, int player_index, KeybindAction action) {
    if (player_index < 0 || player_index >= KB_MAX_PLAYERS) return SDLK_UNKNOWN;
    if (action < 0 || action >= KB_ACTION_COUNT) return SDLK_UNKNOWN;

    return kb->players[player_index].bindings[action];
}

void keybindings_set_with_swap(Keybindings *kb, int player_index, KeybindAction action, SDL_Keycode key) {
    if (player_index < 0 || player_index >= KB_MAX_PLAYERS) return;
    if (action < 0 || action >= KB_ACTION_COUNT) return;

    // Get the old key for this action
    SDL_Keycode old_key = kb->players[player_index].bindings[action];

    // Find if the new key is already bound to another action
    for (int i = 0; i < KB_ACTION_COUNT; i++) {
        if (i == action) continue;  // Skip the action we're setting

        if (kb->players[player_index].bindings[i] == key) {
            // Conflict found! Swap the keys
            kb->players[player_index].bindings[i] = old_key;
            break;
        }
    }

    // Set the new binding
    kb->players[player_index].bindings[action] = key;
}

int keybindings_find_action(const Keybindings *kb, int player_index, SDL_Keycode key) {
    if (player_index < 0 || player_index >= KB_MAX_PLAYERS) return -1;

    for (int a = 0; a < KB_ACTION_COUNT; a++) {
        if (kb->players[player_index].bindings[a] == key) {
            return a;
        }
    }

    return -1;
}

const char *keybindings_action_name(KeybindAction action) {
    switch (action) {
        case KB_ACTION_UP:    return "UP";
        case KB_ACTION_DOWN:  return "DOWN";
        case KB_ACTION_LEFT:  return "LEFT";
        case KB_ACTION_RIGHT: return "RIGHT";
        case KB_ACTION_USE:   return "USE";
        default:              return "UNKNOWN";
    }
}

const char *keybindings_key_name(SDL_Keycode key) {
    return SDL_GetKeyName(key);
}
