#ifndef BOARD_H
#define BOARD_H

#include "common.h"
#include "snake.h"

typedef struct {
    int width;
    int height;
    Vec2 food;
} Board;

void board_init(Board *b, int width, int height);
int board_out_of_bounds(const Board *b, Vec2 pos);
void board_place_food(Board *b, const Snake *s);

#endif
