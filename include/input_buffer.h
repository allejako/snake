#ifndef INPUT_BUFFER_H
#define INPUT_BUFFER_H

#include "common.h"

#define INPUT_BUFFER_SIZE 2

/**
 * Input buffer for queuing direction changes between game ticks.
 * Prevents missed inputs when player presses keys faster than game updates.
 * Capacity of 2 allows for one queued turn (e.g., right then down in quick succession).
 */
typedef struct {
    Direction buf[INPUT_BUFFER_SIZE];  // Buffered direction inputs
    int count;                          // Number of buffered inputs (0-2)
} InputBuffer;

/**
 * Initialize input buffer to empty state.
 */
void input_buffer_init(InputBuffer *ib);

/**
 * Clear all buffered inputs.
 */
void input_buffer_clear(InputBuffer *ib);

/**
 * Push a direction input into the buffer.
 * Ignores duplicate directions and 180-degree turns.
 * Returns 1 if input was buffered, 0 if rejected or buffer full.
 */
int  input_buffer_push(InputBuffer *ib, Direction dir, Direction current_dir);

/**
 * Pop the next direction from the buffer (FIFO).
 * Returns 1 and sets out_dir if buffer not empty, 0 otherwise.
 */
int  input_buffer_pop(InputBuffer *ib, Direction *out_dir);

#endif
