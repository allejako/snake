#ifndef GAME_H
#define GAME_H

#include "common.h"
#include "board.h"
#include "snake.h"

/**
 * Core game state containing board, snake, game status, and score.
 */
typedef struct {
    Board board;      // Game board with food
    Snake snake;      // Snake state (position, length, direction)
    GameState state;  // Current game state (RUNNING or GAME_OVER)
    int score;        // Current score
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

#endif
