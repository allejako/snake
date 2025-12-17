#include "snake.h"

void snake_init(Snake *s, Vec2 start, Direction dir) {
    s->length = 2;
    s->dir = dir;
    s->segments[0] = start;

    switch (dir) {
        case DIR_UP:
            s->segments[1].x = start.x;
            s->segments[1].y = start.y + 1;
            break;
        case DIR_DOWN:
            s->segments[1].x = start.x;
            s->segments[1].y = start.y - 1;
            break;
        case DIR_LEFT:
            s->segments[1].x = start.x + 1;
            s->segments[1].y = start.y;
            break;
        case DIR_RIGHT:
            s->segments[1].x = start.x - 1;
            s->segments[1].y = start.y;
            break;
    }
}

Vec2 snake_head(const Snake *s) {
    return s->segments[0];
}

void snake_change_direction(Snake *s, Direction newDir) {
    // Prevent 180-degree turns
    if ((s->dir == DIR_UP && newDir == DIR_DOWN) ||
        (s->dir == DIR_DOWN && newDir == DIR_UP) ||
        (s->dir == DIR_LEFT && newDir == DIR_RIGHT) ||
        (s->dir == DIR_RIGHT && newDir == DIR_LEFT)) {
        return;
    }
    s->dir = newDir;
}

void snake_step_to(Snake *s, Vec2 newHead, int grow) {
    int newLength = s->length;
    if (grow && s->length < MAX_SNAKE_LEN) {
        newLength = s->length + 1;
    }

    // Move segments from back to front
    for (int i = newLength - 1; i > 0; --i) {
        if (i < s->length) {
            s->segments[i] = s->segments[i - 1];
        } else {
            // New segment is copied from the old tail
            s->segments[i] = s->segments[s->length - 1];
        }
    }

    s->segments[0] = newHead;
    s->length = newLength;
}

int snake_occupies(const Snake *s, Vec2 pos) {
    for (int i = 0; i < s->length; ++i) {
        if (s->segments[i].x == pos.x && s->segments[i].y == pos.y) {
            return 1;
        }
    }
    return 0;
}

int snake_occupies_excluding_tail(const Snake *s, Vec2 pos) {
    // Check all segments except the tail (last segment)
    // This is used for collision detection when the snake will move (not grow)
    for (int i = 0; i < s->length - 1; ++i) {
        if (s->segments[i].x == pos.x && s->segments[i].y == pos.y) {
            return 1;
        }
    }
    return 0;
}

int snake_remove_head(Snake *s) {
    if (s->length <= 0) {
        return 0;
    }

    // Shift all segments forward (removing the head)
    for (int i = 0; i < s->length - 1; ++i) {
        s->segments[i] = s->segments[i + 1];
    }

    s->length--;
    return 1;
}
