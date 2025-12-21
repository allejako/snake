#ifndef CONFIG_H
#define CONFIG_H

/**
 * Runtime configuration for the game.
 * These values can be loaded from game_config.ini and modified at runtime.
 */
typedef struct {
    // Network settings
    char server_host[256];
    int server_port;

    // Game board settings
    int sp_board_width;      // Singleplayer board width
    int sp_board_height;     // Singleplayer board height
    int mp_board_width;      // Multiplayer board width
    int mp_board_height;     // Multiplayer board height

    // Game timing
    int tick_ms;             // Default tick duration
    int speed_floor_ms;      // Minimum tick duration (max speed)
    float speed_curve_k;     // Speed curve exponential decay

    // Combo system
    int combo_window_ticks;  // Base combo window in ticks
    int combo_window_increase_per_tier; // Additional ticks added per tier level

    // Multiplayer settings
    int initial_lives;       // Starting lives per player
    int max_players;         // Maximum players allowed

    // Display settings
    int window_width;
    int window_height;
    int max_cell_size;
    int min_cell_size;

} GameConfig;

/**
 * Load configuration from file.
 * If file doesn't exist, creates it with default values.
 * Returns 0 on success, -1 on error.
 */
int config_load(GameConfig *cfg, const char *filename);

/**
 * Save configuration to file.
 * Returns 0 on success, -1 on error.
 */
int config_save(const GameConfig *cfg, const char *filename);

/**
 * Initialize configuration with default values.
 */
void config_init_defaults(GameConfig *cfg);

#endif // CONFIG_H
