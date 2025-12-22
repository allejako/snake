#include "multiplayer_game.h"
#include "game.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Starting positions for each player (in board coordinates)
static const Vec2 START_POSITIONS[MAX_PLAYERS] = {
    {5, 5},    // Player 1: top-left area
    {34, 5},   // Player 2: top-right area
    {5, 34},   // Player 3: bottom-left area
    {34, 34}   // Player 4: bottom-right area
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
        mg->players[i].score = 0;
        mg->players[i].lives = 0;
        mg->players[i].wins = 0;
        mg->players[i].fruits_eaten = 0;
        mg->players[i].combo_count = 0;
        mg->players[i].combo_expiry_time = 0;
        mg->players[i].combo_best = 0;
        mg->players[i].food_eaten_this_frame = 0;
        mg->players[i].is_local_player = 0;
        mg->players[i].client_id[0] = '\0';
        mg->players[i].name[0] = '\0';
        mg->players[i].snake.length = 0;
        mg->players[i].snake.dir = DIR_RIGHT;
        mg->players[i].ready = 0;
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
    printf("DEBUG: multiplayer_game_start - entry\n");
    fflush(stdout);

    mg->active_players = 0;
    mg->food_count = 0;

    printf("DEBUG: multiplayer_game_start - initializing snakes\n");
    fflush(stdout);

    // Initialize snakes for joined players
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (mg->players[i].joined)
        {
            printf("DEBUG: Initializing player %d snake\n", i);
            fflush(stdout);

            snake_init(&mg->players[i].snake, START_POSITIONS[i], START_DIRECTIONS[i]);
            mg->players[i].alive = 1;
            mg->players[i].death_state = GAME_RUNNING;
            input_buffer_clear(&mg->players[i].input);
            mg->active_players++;

            // Reset game stats for new round
            mg->players[i].score = 0;
            mg->players[i].fruits_eaten = 0;
            mg->players[i].combo_count = 0;
            mg->players[i].combo_expiry_time = 0;
            mg->players[i].combo_best = 0;
            mg->players[i].food_eaten_this_frame = 0;
        }
    }

    printf("DEBUG: multiplayer_game_start - spawning food\n");
    fflush(stdout);

    // Spawn initial food (use first joined player's snake for placement)
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (mg->players[i].joined)
        {
            board_place_food(&mg->board, &mg->players[i].snake);
            break;
        }
    }

    printf("DEBUG: multiplayer_game_start - completed\n");
    fflush(stdout);
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

void multiplayer_game_update(MultiplayerGame_s *mg, int is_host)
{
    // Reset food eaten flags from previous frame
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        mg->players[i].food_eaten_this_frame = 0;
    }

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

        // Pure Client Authoritative: Remote players' positions are already updated from network
        // Only calculate next position for local player (who will move in this tick)
        if (mg->players[i].is_local_player)
        {
            // Local player: Calculate where we WILL move
            switch (snake->dir)
            {
            case DIR_UP:    next.y--; break;
            case DIR_DOWN:  next.y++; break;
            case DIR_LEFT:  next.x--; break;
            case DIR_RIGHT: next.x++; break;
            }
        }
        // else: Remote player - use current position (already moved on their client)

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

    // DEBUG: Print alive players
    static int debug_counter = 0;
    if (debug_counter++ % 50 == 0) {  // Print every 50 ticks
        printf("DEBUG: Alive players: ");
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (mg->players[i].alive) {
                printf("P%d(len=%d) ", i, mg->players[i].snake.length);
            }
        }
        printf("\n");
        fflush(stdout);
    }

    // Initialize collision flags
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        has_collision[i] = 0;
    }

    // Check head-to-head collisions FIRST (before other checks)
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!mg->players[i].alive || mg->players[i].death_state != GAME_RUNNING)
            continue;

        for (int j = i + 1; j < MAX_PLAYERS; j++)
        {
            if (!mg->players[j].alive || mg->players[j].death_state != GAME_RUNNING)
                continue;

            if (vec2_equal(next_positions[i], next_positions[j]))
            {
                // Both snakes trying to move to same position - both die
                has_collision[i] = 1;
                has_collision[j] = 1;
                printf("DEBUG: Head-to-head collision between P%d and P%d at (%d,%d)\n",
                       i, j, next_positions[i].x, next_positions[i].y);
                fflush(stdout);
            }
        }
    }

    // Now check other collisions for each player
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!mg->players[i].alive || mg->players[i].death_state != GAME_RUNNING)
            continue;

        Snake *snake = &mg->players[i].snake;
        Vec2 next = next_positions[i];

        // Pure Client Authoritative: Only local player checks wall/self collisions
        // Remote players already validated these on their client
        if (mg->players[i].is_local_player)
        {
            // Check wall collision for local player only
            if (next.x < 0 || next.x >= mg->board.width ||
                next.y < 0 || next.y >= mg->board.height)
            {
                has_collision[i] = 1;
                continue;
            }
        }

        // Check collision with any snake
        for (int j = 0; j < MAX_PLAYERS; j++)
        {
            if (!mg->players[j].alive)
                continue;

            if (i == j)
            {
                // Only local player checks self-collision (remote already validated)
                if (mg->players[i].is_local_player)
                {
                    // Check against own body (excluding tail if not growing)
                    if (snake_occupies_excluding_tail(snake, next))
                    {
                        has_collision[i] = 1;
                        printf("DEBUG: Player %d collision with own body at (%d,%d)\n", i, next.x, next.y);
                        fflush(stdout);
                        break;
                    }
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
                        printf("DEBUG: Player %d collision with player %d at (%d,%d) - other eating food\n", i, j, next.x, next.y);
                        fflush(stdout);
                        break;
                    }
                }
                else
                {
                    // Other snake is not eating food, tail will move - exclude it
                    if (snake_occupies_excluding_tail(&mg->players[j].snake, next))
                    {
                        has_collision[i] = 1;
                        printf("DEBUG: Player %d collision with player %d at (%d,%d)\n", i, j, next.x, next.y);
                        fflush(stdout);
                        break;
                    }
                }
            }
        }
    }

    // Third pass: Handle food consumption and move snakes
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!mg->players[i].alive || mg->players[i].death_state != GAME_RUNNING)
            continue;

        MultiplayerPlayer *player = &mg->players[i];

        // Mark snakes with collisions as dying (applies to all players)
        if (has_collision[i])
        {
            player->death_state = GAME_DYING;
            // Clear input buffer to prevent queued inputs from being processed
            input_buffer_clear(&player->input);
            continue;
        }

        // Handle food consumption ONLY if no collision (don't eat food when dying)
        // Re-check food eating here to ensure we don't eat if we just collided
        int ate_food = will_eat_food[i] && !has_collision[i];

        if (ate_food)
        {
            Vec2 next = next_positions[i];

            // Update combo system
            if (player->combo_count > 0 && player->combo_expiry_time > 0)
            {
                player->combo_count++;
            }
            else
            {
                player->combo_count = 1;
            }

            if (player->combo_count > player->combo_best)
            {
                player->combo_best = player->combo_count;
            }

            // Calculate score with multiplier
            int multiplier = game_get_combo_multiplier(player->combo_count);
            player->score += 10 * multiplier;
            player->fruits_eaten++;
            player->food_eaten_this_frame = 1;
            player->combo_expiry_time = 1;  // Placeholder, updated by host timer

            // Only host generates new food - clients wait for food position from network
            if (is_host)
            {
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
        }

        // Pure Client Authoritative: Only move local player's snake
        // Remote players' positions are updated directly from network (via handle_client_input)
        if (player->is_local_player)
        {
            snake_step_to(&player->snake, next_positions[i], ate_food);
        }
        // else: Remote player - position already updated from network, don't move here
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
    // Count players with lives remaining
    int players_with_lives = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (mg->players[i].joined && mg->players[i].lives > 0) {
            players_with_lives++;
        }
    }
    return players_with_lives <= 1;
}

int multiplayer_game_update_death_animations(MultiplayerGame_s *mg)
{
    int any_dying = 0;

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        // Only process death animation for LOCAL player (each client manages their own)
        // Remote players' death animations are handled on their own clients
        if (mg->players[i].death_state == GAME_DYING && mg->players[i].is_local_player)
        {
            MultiplayerPlayer *player = &mg->players[i];
            Snake *snake = &player->snake;

            // Leave food on head position before removing it
            if (snake->length > 0)
            {
                Vec2 head = snake_head(snake);
                // Only host adds food directly (clients send notifications handled elsewhere)
                if (mg->is_host) {
                    multiplayer_game_add_food(mg, head);
                }
                // Note: Client food notifications are sent in main.c death animation handler
            }

            // Remove head segment
            if (!snake_remove_head(snake))
            {
                // Snake is fully removed, mark as dead
                player->death_state = GAME_OVER;
                player->alive = 0;
                // Note: active_players is managed by online_multiplayer.c based on lives system
            }
            else
            {
                any_dying = 1;
            }
        }
    }

    return any_dying;
}
