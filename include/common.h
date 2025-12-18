#ifndef COMMON_H
#define COMMON_H

typedef struct {
    int x;
    int y;
} Vec2;

typedef enum {
    DIR_UP,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
} Direction;

typedef enum {
    GAME_RUNNING,
    GAME_DYING,    // Death animation in progress
    GAME_OVER
} GameState;

// Helper function to compare Vec2 positions
static inline int vec2_equal(Vec2 a, Vec2 b)
{
    return a.x == b.x && a.y == b.y;
}

#endif
