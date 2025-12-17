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

#endif
