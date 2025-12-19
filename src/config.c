#include "config.h"
#include "constants.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void config_init_defaults(GameConfig *cfg)
{
    // Network settings
    strncpy(cfg->server_host, DEFAULT_SERVER_HOST, sizeof(cfg->server_host) - 1);
    cfg->server_host[sizeof(cfg->server_host) - 1] = '\0';
    cfg->server_port = DEFAULT_SERVER_PORT;

    // Game board settings
    cfg->sp_board_width = SINGLEPLAYER_BOARD_WIDTH;
    cfg->sp_board_height = SINGLEPLAYER_BOARD_HEIGHT;
    cfg->mp_board_width = MULTIPLAYER_BOARD_WIDTH;
    cfg->mp_board_height = MULTIPLAYER_BOARD_HEIGHT;

    // Game timing
    cfg->tick_ms = TICK_MS;
    cfg->speed_floor_ms = (int)SPEED_FLOOR_MS;
    cfg->speed_curve_k = SPEED_CURVE_K;

    // Combo system
    cfg->combo_window_ticks = BASE_COMBO_WINDOW_TICKS;
    cfg->combo_decay = COMBO_WINDOW_DECAY;

    // Multiplayer settings
    cfg->initial_lives = INITIAL_LIVES;
    cfg->max_players = MAX_PLAYERS;

    // Display settings
    cfg->window_width = WINDOW_WIDTH;
    cfg->window_height = WINDOW_HEIGHT;
    cfg->max_cell_size = MAX_CELL_SIZE;
    cfg->min_cell_size = MIN_CELL_SIZE;
}

int config_load(GameConfig *cfg, const char *filename)
{
    FILE *f = fopen(filename, "r");

    // If file doesn't exist, create with defaults
    if (!f) {
        config_init_defaults(cfg);
        return config_save(cfg, filename);
    }

    // Initialize with defaults first
    config_init_defaults(cfg);

    char line[512];
    char section[64] = "";

    while (fgets(line, sizeof(line), f)) {
        // Remove newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        // Skip empty lines and comments
        if (line[0] == '\0' || line[0] == '#' || line[0] == ';') {
            continue;
        }

        // Section header [Section]
        if (line[0] == '[') {
            char *end = strchr(line, ']');
            if (end) {
                *end = '\0';
                strncpy(section, line + 1, sizeof(section) - 1);
                section[sizeof(section) - 1] = '\0';
            }
            continue;
        }

        // Key=Value pairs
        char *equals = strchr(line, '=');
        if (!equals) continue;

        *equals = '\0';
        char *key = line;
        char *value = equals + 1;

        // Trim leading whitespace from value
        while (*value == ' ' || *value == '\t') value++;

        // Parse based on section and key
        if (strcmp(section, "Network") == 0) {
            if (strcmp(key, "server_host") == 0) {
                strncpy(cfg->server_host, value, sizeof(cfg->server_host) - 1);
                cfg->server_host[sizeof(cfg->server_host) - 1] = '\0';
            } else if (strcmp(key, "server_port") == 0) {
                cfg->server_port = atoi(value);
            }
        } else if (strcmp(section, "Game") == 0) {
            if (strcmp(key, "sp_board_width") == 0) {
                cfg->sp_board_width = atoi(value);
            } else if (strcmp(key, "sp_board_height") == 0) {
                cfg->sp_board_height = atoi(value);
            } else if (strcmp(key, "mp_board_width") == 0) {
                cfg->mp_board_width = atoi(value);
            } else if (strcmp(key, "mp_board_height") == 0) {
                cfg->mp_board_height = atoi(value);
            } else if (strcmp(key, "tick_ms") == 0) {
                cfg->tick_ms = atoi(value);
            } else if (strcmp(key, "speed_floor_ms") == 0) {
                cfg->speed_floor_ms = atoi(value);
            } else if (strcmp(key, "speed_curve_k") == 0) {
                cfg->speed_curve_k = atof(value);
            } else if (strcmp(key, "combo_window_ticks") == 0) {
                cfg->combo_window_ticks = atoi(value);
            } else if (strcmp(key, "combo_decay") == 0) {
                cfg->combo_decay = atof(value);
            } else if (strcmp(key, "initial_lives") == 0) {
                cfg->initial_lives = atoi(value);
            } else if (strcmp(key, "max_players") == 0) {
                cfg->max_players = atoi(value);
            }
        } else if (strcmp(section, "Display") == 0) {
            if (strcmp(key, "window_width") == 0) {
                cfg->window_width = atoi(value);
            } else if (strcmp(key, "window_height") == 0) {
                cfg->window_height = atoi(value);
            } else if (strcmp(key, "max_cell_size") == 0) {
                cfg->max_cell_size = atoi(value);
            } else if (strcmp(key, "min_cell_size") == 0) {
                cfg->min_cell_size = atoi(value);
            }
        }
    }

    fclose(f);
    return 0;
}

int config_save(const GameConfig *cfg, const char *filename)
{
    FILE *f = fopen(filename, "w");
    if (!f) {
        return -1;
    }

    fprintf(f, "# Snake Game Configuration\n");
    fprintf(f, "# This file is auto-generated. Edit values to customize gameplay.\n\n");

    fprintf(f, "[Network]\n");
    fprintf(f, "server_host=%s\n", cfg->server_host);
    fprintf(f, "server_port=%d\n\n", cfg->server_port);

    fprintf(f, "[Game]\n");
    fprintf(f, "# Singleplayer board dimensions\n");
    fprintf(f, "sp_board_width=%d\n", cfg->sp_board_width);
    fprintf(f, "sp_board_height=%d\n\n", cfg->sp_board_height);

    fprintf(f, "# Multiplayer board dimensions\n");
    fprintf(f, "mp_board_width=%d\n", cfg->mp_board_width);
    fprintf(f, "mp_board_height=%d\n\n", cfg->mp_board_height);

    fprintf(f, "# Game speed settings\n");
    fprintf(f, "tick_ms=%d\n", cfg->tick_ms);
    fprintf(f, "speed_floor_ms=%d\n", cfg->speed_floor_ms);
    fprintf(f, "speed_curve_k=%.2f\n\n", cfg->speed_curve_k);

    fprintf(f, "# Combo system\n");
    fprintf(f, "combo_window_ticks=%d\n", cfg->combo_window_ticks);
    fprintf(f, "combo_decay=%.2f\n\n", cfg->combo_decay);

    fprintf(f, "# Multiplayer settings\n");
    fprintf(f, "initial_lives=%d\n", cfg->initial_lives);
    fprintf(f, "max_players=%d\n\n", cfg->max_players);

    fprintf(f, "[Display]\n");
    fprintf(f, "window_width=%d\n", cfg->window_width);
    fprintf(f, "window_height=%d\n", cfg->window_height);
    fprintf(f, "max_cell_size=%d\n", cfg->max_cell_size);
    fprintf(f, "min_cell_size=%d\n", cfg->min_cell_size);

    fclose(f);
    return 0;
}
