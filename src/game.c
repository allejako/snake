#include "game.h"

#define POINTS_PER_FOOD 10

static void game_spawn_snake(Game *g) {
    Vec2 start;
    start.x = g->board.width / 2;
    start.y = g->board.height / 2;
    snake_init(&g->snake, start, DIR_RIGHT);
}

void game_init(Game *g, int width, int height) {
    board_init(&g->board, width, height);
    game_spawn_snake(g);
    g->state = GAME_RUNNING;
    g->score = 0;
    board_place_food(&g->board, &g->snake);
}

void game_reset(Game *g) {
    game_init(g, g->board.width, g->board.height);
}

void game_change_direction(Game *g, Direction dir) {
    snake_change_direction(&g->snake, dir);
}

void game_update(Game *g) {
    if (g->state != GAME_RUNNING) return;

    Vec2 head = snake_head(&g->snake);
    Vec2 newHead = head;

    switch (g->snake.dir) {
        case DIR_UP:    newHead.y -= 1; break;
        case DIR_DOWN:  newHead.y += 1; break;
        case DIR_LEFT:  newHead.x -= 1; break;
        case DIR_RIGHT: newHead.x += 1; break;
    }

    // Wall collision
    if (board_out_of_bounds(&g->board, newHead)) {
        g->state = GAME_DYING;
        return;
    }

    // Check if food will be eaten
    int grow = (newHead.x == g->board.food.x && newHead.y == g->board.food.y);

    // Self-collision check
    // When not growing, the tail will move away, so exclude it from collision check
    if (grow) {
        // Growing: check all segments including tail
        if (snake_occupies(&g->snake, newHead)) {
            g->state = GAME_DYING;
            return;
        }
    } else {
        // Not growing: exclude tail since it will move
        if (snake_occupies_excluding_tail(&g->snake, newHead)) {
            g->state = GAME_DYING;
            return;
        }
    }

    snake_step_to(&g->snake, newHead, grow);

    if (grow) {
        g->score += POINTS_PER_FOOD;
        board_place_food(&g->board, &g->snake);
    }
}

int game_update_death_animation(Game *g) {
    if (g->state != GAME_DYING) return 0;

    // Remove one segment from the head
    if (snake_remove_head(&g->snake)) {
        // Animation continues
        if (g->snake.length == 0) {
            // Animation complete
            g->state = GAME_OVER;
            return 0;
        }
        return 1;
    }

    // No segments left
    g->state = GAME_OVER;
    return 0;
}
