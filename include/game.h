#ifndef GAME_H
#define GAME_H

#include "common.h"
#include "board.h"
#include "snake.h"

/**
 * Core game state containing board, snake, game status, and score.
 */
typedef struct {
    Board board;           // Game board with food
    Snake snake;           // Snake state (position, length, direction)
    GameState state;       // Current game state (RUNNING or GAME_OVER)
    int score;             // Current score
    int fruits_eaten;      // Number of fruits eaten
    unsigned int start_time; // Game start time (milliseconds)
    unsigned int death_time; // Game death time (milliseconds)

    // Combo system
    int combo_count;       // Current combo streak (0 when inactive)
    unsigned int combo_expiry_time; // When combo expires (milliseconds)
    int combo_window_ms;   // Time window for combo in milliseconds
    int combo_best;        // Best combo achieved this game
    int food_eaten_this_frame; // Flag: 1 if food was eaten this update
} Game;

/**
 * Initialize a new game with specified board dimensions.
 * Places snake at center and spawns initial food.
 */
void game_init(Game *g, int width, int height);

/**
 * Reset game to initial state, keeping same board dimensions.
 */
void game_reset(Game *g);

/**
 * Update game state by one tick - move snake, check collisions, handle food.
 * Sets state to GAME_OVER if collision detected.
 */
void game_update(Game *g);

/**
 * Change snake direction (input handling).
 * Direction change takes effect on next update.
 */
void game_change_direction(Game *g, Direction dir);

/**
 * Update death animation - removes one segment per tick.
 * Returns 1 if animation continues, 0 if complete.
 */
int game_update_death_animation(Game *g);

/**
 * Update combo timer - checks if combo has expired.
 * Call this each frame with current time.
 */
void game_update_combo_timer(Game *g, unsigned int current_time);

/**
 * Get combo tier (1-5) based on current combo count.
 * Used for SFX and UI.
 */
int game_get_combo_tier(int combo_count);

/**
 * Get score multiplier based on combo count.
 */
int game_get_combo_multiplier(int combo_count);

#endif
