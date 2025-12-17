#include "board.h"
#include <stdlib.h>

void board_init(Board *b, int width, int height) {
    b->width = width;
    b->height = height;
    b->food.x = width / 2;
    b->food.y = height / 2;
}

int board_out_of_bounds(const Board *b, Vec2 pos) {
    return pos.x < 0 || pos.x >= b->width || pos.y < 0 || pos.y >= b->height;
}

void board_place_food(Board *b, const Snake *s) {
    while (1) {
        Vec2 pos;
        pos.x = rand() % b->width;
        pos.y = rand() % b->height;

        if (!snake_occupies(s, pos)) {
            b->food = pos;
            break;
        }
    }
}
