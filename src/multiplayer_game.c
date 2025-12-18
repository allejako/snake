#include "multiplayer_game.h"
#include <stdlib.h>
#include <string.h>

// Starting positions for each player (in board coordinates)
static const Vec2 START_POSITIONS[MAX_PLAYERS] = {
    {2, 2},    // Player 1: top-left area
    {18, 2},   // Player 2: top-right area
    {2, 18},   // Player 3: bottom-left area
    {18, 18}   // Player 4: bottom-right area
};

static const Direction START_DIRECTIONS[MAX_PLAYERS] = {
    DIR_RIGHT,  // Player 1
    DIR_LEFT,   // Player 2
    DIR_RIGHT,  // Player 3
    DIR_LEFT    // Player 4
};

void multiplayer_game_init(MultiplayerGame_s *mg, int width, int height)
{
    board_init(&mg->board, width, height);
    mg->food_count = 0;
    mg->active_players = 0;
    mg->total_joined = 0;

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        mg->players[i].joined = 0;
        mg->players[i].alive = 0;
        mg->players[i].death_state = GAME_OVER;
        input_buffer_init(&mg->players[i].input);
    }
}

int multiplayer_game_join_player(MultiplayerGame_s *mg, int player_index)
{
    if (player_index < 0 || player_index >= MAX_PLAYERS)
        return 0;

    if (mg->players[player_index].joined)
        return 0;  // Already joined

    mg->players[player_index].joined = 1;
    mg->total_joined++;
    return 1;
}

int multiplayer_game_leave_player(MultiplayerGame_s *mg, int player_index)
{
    if (player_index < 0 || player_index >= MAX_PLAYERS)
        return 0;

    if (!mg->players[player_index].joined)
        return 0;  // Not joined

    mg->players[player_index].joined = 0;
    mg->total_joined--;
    return 1;
}

void multiplayer_game_start(MultiplayerGame_s *mg)
{
    mg->active_players = 0;
    mg->food_count = 0;

    // Initialize snakes for joined players
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (mg->players[i].joined)
        {
            snake_init(&mg->players[i].snake, START_POSITIONS[i], START_DIRECTIONS[i]);
            mg->players[i].alive = 1;
            mg->players[i].death_state = GAME_RUNNING;
            input_buffer_clear(&mg->players[i].input);
            mg->active_players++;
        }
    }

    // Spawn initial food (use first joined player's snake for placement)
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (mg->players[i].joined)
        {
            board_place_food(&mg->board, &mg->players[i].snake);
            break;
        }
    }
}

void multiplayer_game_add_food(MultiplayerGame_s *mg, Vec2 pos)
{
    if (mg->food_count < MAX_FOOD_ITEMS)
    {
        mg->food[mg->food_count++] = pos;
    }
}

static int is_position_occupied(const MultiplayerGame_s *mg, Vec2 pos)
{
    // Check if any snake occupies this position
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (mg->players[i].alive && snake_occupies(&mg->players[i].snake, pos))
        {
            return 1;
        }
    }

    // Check if food is at this position
    if (vec2_equal(mg->board.food, pos))
        return 1;

    for (int i = 0; i < mg->food_count; i++)
    {
        if (vec2_equal(mg->food[i], pos))
            return 1;
    }

    return 0;
}

static void spawn_food_avoiding_snakes(MultiplayerGame_s *mg, Vec2 *out_food)
{
    int max_attempts = 1000;
    for (int attempt = 0; attempt < max_attempts; attempt++)
    {
        Vec2 candidate = {
            rand() % mg->board.width,
            rand() % mg->board.height
        };

        if (!is_position_occupied(mg, candidate))
        {
            *out_food = candidate;
            return;
        }
    }

    // Fallback: just place anywhere
    out_food->x = rand() % mg->board.width;
    out_food->y = rand() % mg->board.height;
}

void multiplayer_game_update(MultiplayerGame_s *mg)
{
    // First pass: Calculate next positions and determine which snakes will eat food
    Vec2 next_positions[MAX_PLAYERS];
    int will_eat_food[MAX_PLAYERS];

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        will_eat_food[i] = 0;

        if (!mg->players[i].alive || mg->players[i].death_state != GAME_RUNNING)
            continue;

        Snake *snake = &mg->players[i].snake;
        Vec2 head = snake_head(snake);
        Vec2 next = head;

        switch (snake->dir)
        {
        case DIR_UP:    next.y--; break;
        case DIR_DOWN:  next.y++; break;
        case DIR_LEFT:  next.x--; break;
        case DIR_RIGHT: next.x++; break;
        }

        next_positions[i] = next;

        // Check if this snake will eat food
        if (vec2_equal(next, mg->board.food))
        {
            will_eat_food[i] = 1;
        }
        else
        {
            for (int f = 0; f < mg->food_count; f++)
            {
                if (vec2_equal(next, mg->food[f]))
                {
                    will_eat_food[i] = 1;
                    break;
                }
            }
        }
    }

    // Second pass: Check collisions for all snakes (don't move yet)
    int has_collision[MAX_PLAYERS];
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        has_collision[i] = 0;

        if (!mg->players[i].alive || mg->players[i].death_state != GAME_RUNNING)
            continue;

        Snake *snake = &mg->players[i].snake;
        Vec2 next = next_positions[i];

        // Check wall collision
        if (next.x < 0 || next.x >= mg->board.width ||
            next.y < 0 || next.y >= mg->board.height)
        {
            has_collision[i] = 1;
            continue;
        }

        // Check collision with any snake (including self)
        for (int j = 0; j < MAX_PLAYERS; j++)
        {
            if (!mg->players[j].alive)
                continue;

            if (i == j)
            {
                // Check against own body (excluding tail if not growing)
                if (snake_occupies_excluding_tail(snake, next))
                {
                    has_collision[i] = 1;
                    break;
                }
            }
            else
            {
                // Check against other snakes' bodies (BEFORE they move)
                // Exclude tail if that snake is NOT eating food (tail will move away)
                if (will_eat_food[j])
                {
                    // Other snake is eating food, tail won't move - check full body
                    if (snake_occupies(&mg->players[j].snake, next))
                    {
                        has_collision[i] = 1;
                        break;
                    }
                }
                else
                {
                    // Other snake is not eating food, tail will move - exclude it
                    if (snake_occupies_excluding_tail(&mg->players[j].snake, next))
                    {
                        has_collision[i] = 1;
                        break;
                    }
                }
            }
        }
    }

    // Third pass: Handle food consumption and move all snakes
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!mg->players[i].alive || mg->players[i].death_state != GAME_RUNNING)
            continue;

        MultiplayerPlayer *player = &mg->players[i];

        // Mark snakes with collisions as dying
        if (has_collision[i])
        {
            player->death_state = GAME_DYING;
            continue;
        }

        // Handle food consumption
        int ate_food = will_eat_food[i];

        if (ate_food)
        {
            Vec2 next = next_positions[i];

            // Check main board food
            if (vec2_equal(next, mg->board.food))
            {
                spawn_food_avoiding_snakes(mg, &mg->board.food);
            }
            else
            {
                // Check additional food items (from dead snakes)
                for (int f = 0; f < mg->food_count; f++)
                {
                    if (vec2_equal(next, mg->food[f]))
                    {
                        // Remove this food by replacing with last food
                        mg->food[f] = mg->food[mg->food_count - 1];
                        mg->food_count--;
                        break;
                    }
                }
            }
        }

        // Move snake
        snake_step_to(&player->snake, next_positions[i], ate_food);
    }
}

void multiplayer_game_change_direction(MultiplayerGame_s *mg, int player_index, Direction dir)
{
    if (player_index < 0 || player_index >= MAX_PLAYERS)
        return;

    if (!mg->players[player_index].alive)
        return;

    snake_change_direction(&mg->players[player_index].snake, dir);
}

int multiplayer_game_is_over(const MultiplayerGame_s *mg)
{
    return mg->active_players <= 1;
}

int multiplayer_game_update_death_animations(MultiplayerGame_s *mg)
{
    int any_dying = 0;

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (mg->players[i].death_state == GAME_DYING)
        {
            MultiplayerPlayer *player = &mg->players[i];
            Snake *snake = &player->snake;

            // Leave food on head position before removing it
            if (snake->length > 0)
            {
                Vec2 head = snake_head(snake);
                multiplayer_game_add_food(mg, head);
            }

            // Remove head segment
            if (!snake_remove_head(snake))
            {
                // Snake is fully removed, mark as dead
                player->death_state = GAME_OVER;
                player->alive = 0;
                mg->active_players--;
            }
            else
            {
                any_dying = 1;
            }
        }
    }

    return any_dying;
}
