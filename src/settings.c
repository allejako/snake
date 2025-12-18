#include "settings.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

// Default keybindings for all 4 players
static const SDL_Keycode DEFAULT_BINDINGS[SETTINGS_MAX_PLAYERS][SETTINGS_ACTIONS_PER_PLAYER] = {
    // Player 1
    {SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT, SDLK_SPACE},
    // Player 2
    {SDLK_w, SDLK_s, SDLK_a, SDLK_d, SDLK_e},
    // Player 3
    {SDLK_i, SDLK_k, SDLK_j, SDLK_l, SDLK_u},
    // Player 4
    {SDLK_t, SDLK_g, SDLK_f, SDLK_h, SDLK_r}
};

// Helper: convert action enum to string
static const char *action_to_string(SettingAction action) {
    switch (action) {
        case SETTING_ACTION_UP:    return "move_up";
        case SETTING_ACTION_DOWN:  return "move_down";
        case SETTING_ACTION_LEFT:  return "move_left";
        case SETTING_ACTION_RIGHT: return "move_right";
        case SETTING_ACTION_USE:   return "use";
        default:                   return NULL;
    }
}

// Helper: convert string to action enum
static int string_to_action(const char *str) {
    if (strcmp(str, "move_up") == 0)    return SETTING_ACTION_UP;
    if (strcmp(str, "move_down") == 0)  return SETTING_ACTION_DOWN;
    if (strcmp(str, "move_left") == 0)  return SETTING_ACTION_LEFT;
    if (strcmp(str, "move_right") == 0) return SETTING_ACTION_RIGHT;
    if (strcmp(str, "use") == 0)        return SETTING_ACTION_USE;
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

void settings_init(Settings *s, const char *filename) {
    strncpy(s->filename, filename, sizeof(s->filename) - 1);
    s->filename[sizeof(s->filename) - 1] = '\0';
    settings_set_defaults(s);
}

void settings_set_defaults(Settings *s) {
    s->profile_name[0] = '\0';  // Empty profile name
    s->music_volume = 50;
    s->effects_volume = 50;

    // Set default keybindings
    for (int p = 0; p < SETTINGS_MAX_PLAYERS; p++) {
        for (int a = 0; a < SETTINGS_ACTIONS_PER_PLAYER; a++) {
            s->keybindings[p][a] = DEFAULT_BINDINGS[p][a];
        }
    }
}

int settings_load(Settings *s) {
    FILE *f = fopen(s->filename, "r");
    if (!f) {
        return 0;  // File doesn't exist, use defaults
    }

    int current_player = -1;
    char line[256];
    int in_general_section = 0;

    while (fgets(line, sizeof(line), f)) {
        // Remove newline
        line[strcspn(line, "\r\n")] = '\0';

        // Skip empty lines and comments
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        // Check for section headers
        if (line[0] == '[') {
            if (strstr(line, "[General]")) {
                in_general_section = 1;
                current_player = -1;
            } else if (strstr(line, "[Audio]")) {
                in_general_section = 0;
                current_player = -2;  // Special marker for audio section
            } else if (strstr(line, "[Player1]")) {
                in_general_section = 0;
                current_player = 0;
            } else if (strstr(line, "[Player2]")) {
                in_general_section = 0;
                current_player = 1;
            } else if (strstr(line, "[Player3]")) {
                in_general_section = 0;
                current_player = 2;
            } else if (strstr(line, "[Player4]")) {
                in_general_section = 0;
                current_player = 3;
            }
            continue;
        }

        // Parse key=value
        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = line;
        char *value = eq + 1;

        // Trim whitespace
        trim_whitespace(key);
        trim_whitespace(value);

        // Handle different sections
        if (in_general_section) {
            if (strcmp(key, "profile_name") == 0) {
                strncpy(s->profile_name, value, SETTINGS_MAX_PROFILE_NAME - 1);
                s->profile_name[SETTINGS_MAX_PROFILE_NAME - 1] = '\0';
            }
        } else if (current_player == -2) {
            // Audio section
            if (strcmp(key, "music_volume") == 0) {
                s->music_volume = atoi(value);
                if (s->music_volume < 0) s->music_volume = 0;
                if (s->music_volume > 100) s->music_volume = 100;
            } else if (strcmp(key, "effects_volume") == 0) {
                s->effects_volume = atoi(value);
                if (s->effects_volume < 0) s->effects_volume = 0;
                if (s->effects_volume > 100) s->effects_volume = 100;
            }
        } else if (current_player >= 0 && current_player < SETTINGS_MAX_PLAYERS) {
            // Keybindings section
            int action = string_to_action(key);
            if (action < 0) continue;

            SDL_Keycode keycode = SDL_GetKeyFromName(value);
            if (keycode == SDLK_UNKNOWN) continue;

            s->keybindings[current_player][action] = keycode;
        }
    }

    fclose(f);
    return 1;
}

int settings_save(const Settings *s) {
    // Create data directory if it doesn't exist
    #ifdef _WIN32
        mkdir("data");
    #else
        mkdir("data", 0755);
    #endif

    FILE *f = fopen(s->filename, "w");
    if (!f) {
        return 0;
    }

    // General section
    fprintf(f, "[General]\n");
    fprintf(f, "profile_name=%s\n", s->profile_name);
    fprintf(f, "\n");

    // Audio section
    fprintf(f, "[Audio]\n");
    fprintf(f, "music_volume=%d\n", s->music_volume);
    fprintf(f, "effects_volume=%d\n", s->effects_volume);
    fprintf(f, "\n");

    // Keybindings sections
    for (int p = 0; p < SETTINGS_MAX_PLAYERS; p++) {
        fprintf(f, "[Player%d]\n", p + 1);

        for (int a = 0; a < SETTINGS_ACTIONS_PER_PLAYER; a++) {
            const char *action_name = action_to_string((SettingAction)a);
            const char *key_name = SDL_GetKeyName(s->keybindings[p][a]);

            if (action_name && key_name) {
                fprintf(f, "%s=%s\n", action_name, key_name);
            }
        }

        fprintf(f, "\n");
    }

    fclose(f);
    return 1;
}

int settings_has_profile(const Settings *s) {
    return s->profile_name[0] != '\0';
}

SDL_Keycode settings_get_key(const Settings *s, int player, SettingAction action) {
    if (player < 0 || player >= SETTINGS_MAX_PLAYERS) return SDLK_UNKNOWN;
    if (action < 0 || action >= SETTING_ACTION_COUNT) return SDLK_UNKNOWN;

    return s->keybindings[player][action];
}

void settings_set_key(Settings *s, int player, SettingAction action, SDL_Keycode key) {
    if (player < 0 || player >= SETTINGS_MAX_PLAYERS) return;
    if (action < 0 || action >= SETTING_ACTION_COUNT) return;

    s->keybindings[player][action] = key;
}

int settings_find_action(const Settings *s, int player, SDL_Keycode key) {
    if (player < 0 || player >= SETTINGS_MAX_PLAYERS) return -1;

    for (int a = 0; a < SETTING_ACTION_COUNT; a++) {
        if (s->keybindings[player][a] == key) {
            return a;
        }
    }

    return -1;
}

const char *settings_key_name(SDL_Keycode key) {
    return SDL_GetKeyName(key);
}

void settings_set_key_with_swap(Settings *s, int player, SettingAction action, SDL_Keycode new_key) {
    if (player < 0 || player >= SETTINGS_MAX_PLAYERS) return;
    if (action < 0 || action >= SETTING_ACTION_COUNT) return;

    // Get the old key for this action
    SDL_Keycode old_key = s->keybindings[player][action];

    // Find if the new key is already bound to another action
    for (int i = 0; i < SETTING_ACTION_COUNT; i++) {
        if (i == (int)action) continue;  // Skip the action we're setting

        if (s->keybindings[player][i] == new_key) {
            // Conflict found! Swap the keys
            s->keybindings[player][i] = old_key;
            break;
        }
    }

    // Set the new binding
    s->keybindings[player][action] = new_key;
}

const char *settings_action_name(SettingAction action) {
    switch (action) {
        case SETTING_ACTION_UP:    return "UP";
        case SETTING_ACTION_DOWN:  return "DOWN";
        case SETTING_ACTION_LEFT:  return "LEFT";
        case SETTING_ACTION_RIGHT: return "RIGHT";
        case SETTING_ACTION_USE:   return "USE";
        default:                   return "UNKNOWN";
    }
}
