#include "input_buffer.h"

static int is_opposite(Direction a, Direction b) {
    return (a == DIR_UP    && b == DIR_DOWN) ||
           (a == DIR_DOWN  && b == DIR_UP)   ||
           (a == DIR_LEFT  && b == DIR_RIGHT)||
           (a == DIR_RIGHT && b == DIR_LEFT);
}

void input_buffer_init(InputBuffer *ib) {
    ib->count = 0;
}

void input_buffer_clear(InputBuffer *ib) {
    ib->count = 0;
}

int input_buffer_push(InputBuffer *ib, Direction dir, Direction current_dir) {
    if (ib->count >= INPUT_BUFFER_SIZE)
        return 0;

    Direction last =
        (ib->count > 0) ? ib->buf[ib->count - 1] : current_dir;

    // Ignore same direction
    if (dir == last)
        return 0;

    // Block 180-degree turns
    if (is_opposite(dir, last))
        return 0;

    ib->buf[ib->count++] = dir;
    return 1;
}

int input_buffer_pop(InputBuffer *ib, Direction *out_dir) {
    if (ib->count <= 0)
        return 0;

    *out_dir = ib->buf[0];

    // shift
    for (int i = 1; i < ib->count; ++i) {
        ib->buf[i - 1] = ib->buf[i];
    }
    ib->count--;

    return 1;
}
