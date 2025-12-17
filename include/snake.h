#ifndef SNAKE_H
#define SNAKE_H

#include "common.h"

#define MAX_SNAKE_LEN 256

/**
 * Snake state: position segments, length, and current direction.
 * Segments are stored from head (index 0) to tail (index length-1).
 */
typedef struct {
    Vec2 segments[MAX_SNAKE_LEN];  // Position of each segment
    int length;                     // Current number of segments
    Direction dir;                  // Current movement direction
} Snake;

/**
 * Initialize snake at starting position with initial direction.
 * Snake starts with length 1 (just the head).
 */
void snake_init(Snake *s, Vec2 start, Direction dir);

/**
 * Get position of snake's head (first segment).
 */
Vec2 snake_head(const Snake *s);

/**
 * Change snake's direction. Prevents 180-degree turns.
 */
void snake_change_direction(Snake *s, Direction newDir);

/**
 * Move snake to new head position. If grow is true, tail remains in place.
 * If grow is false, all segments shift forward and tail moves.
 */
void snake_step_to(Snake *s, Vec2 newHead, int grow);

/**
 * Check if snake occupies a position (collision detection).
 * Returns 1 if any segment is at the given position.
 */
int snake_occupies(const Snake *s, Vec2 pos);

/**
 * Check if snake occupies a position, excluding the tail.
 * Used for collision detection when snake will move (not grow).
 */
int snake_occupies_excluding_tail(const Snake *s, Vec2 pos);

/**
 * Remove the head segment from the snake (for death animation).
 * Returns 1 if successful, 0 if snake has no segments left.
 */
int snake_remove_head(Snake *s);

#endif
