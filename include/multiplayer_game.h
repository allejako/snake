#ifndef MULTIPLAYER_GAME_H
#define MULTIPLAYER_GAME_H

#include "common.h"
#include "board.h"
#include "snake.h"
#include "input_buffer.h"

#define MAX_PLAYERS 4
#define MAX_FOOD_ITEMS 32

/**
 * Player state in multiplayer game.
 */
typedef struct {
    Snake snake;              // Snake state
    InputBuffer input;        // Input buffer for this player (only used on host)
    int joined;               // 1 if player has joined, 0 otherwise
    int alive;                // 1 if snake is alive, 0 if dead
    GameState death_state;    // GAME_RUNNING, GAME_DYING, or GAME_OVER

    // Scoring and lives
    int score;                // Player's score
    int fruits_eaten;         // Number of fruits eaten
    int lives;                // Lives remaining (starts at 3)

    // Combo system (per-player)
    int combo_count;          // Current combo streak
    unsigned int combo_expiry_time; // When combo expires (milliseconds)
    int combo_best;           // Best combo achieved this game
    int food_eaten_this_frame; // Flag: 1 if food was eaten this update

    // Network identity
    char client_id[64];       // mpapi client ID (empty string for local player)
    char name[32];            // Player display name
    int is_local_player;      // 1 if this is the local player, 0 if remote

    // Lobby state
    int ready;                // 1 if player is ready to start, 0 otherwise
} MultiplayerPlayer;

/**
 * Multiplayer game state for online multiplayer.
 * Supports up to 4 players with separate snakes on shared board.
 */
typedef struct MultiplayerGame_s {
    Board board;              // Shared game board
    MultiplayerPlayer players[MAX_PLAYERS];  // Player states
    Vec2 food[MAX_FOOD_ITEMS]; // Food positions
    int food_count;           // Number of active food items
    int active_players;       // Number of players currently alive
    int total_joined;         // Total number of players who joined

    // Network state
    int is_host;              // 1 if local instance is host, 0 if client
    int local_player_index;   // Which player slot is local (-1 if spectating)
    unsigned int combo_window_ms; // Combo timer window (based on tick speed)

    // Session info
    char session_id[8];       // 6-char session ID + null terminator
    char host_client_id[64];  // Host's mpapi client ID
} MultiplayerGame_s;

/**
 * Player colors for visual distinction.
 */
typedef struct {
    int r, g, b;
} PlayerColor;

// Player colors: Red, Blue, Green, Orange
static const PlayerColor PLAYER_COLORS[MAX_PLAYERS] = {
    {240, 60, 60},   // Player 1: Red
    {60, 120, 240},  // Player 2: Blue
    {60, 220, 120},  // Player 3: Green
    {240, 160, 40}   // Player 4: Orange
};

/**
 * Initialize multiplayer game with specified board dimensions.
 */
void multiplayer_game_init(MultiplayerGame_s *mg, int width, int height);

/**
 * Add a player to the game (called when player presses USE in lobby).
 * Returns 1 if player joined successfully, 0 if already joined.
 */
int multiplayer_game_join_player(MultiplayerGame_s *mg, int player_index);

/**
 * Remove a player from the game (called when player presses USE again in lobby).
 * Returns 1 if player left successfully, 0 if not joined.
 */
int multiplayer_game_leave_player(MultiplayerGame_s *mg, int player_index);

/**
 * Start the game - initialize snakes at starting positions.
 */
void multiplayer_game_start(MultiplayerGame_s *mg);

/**
 * Update game state by one tick - move all snakes, check collisions, handle food.
 */
void multiplayer_game_update(MultiplayerGame_s *mg);

/**
 * Change direction for a specific player's snake.
 */
void multiplayer_game_change_direction(MultiplayerGame_s *mg, int player_index, Direction dir);

/**
 * Check if game is over (only 0 or 1 players alive).
 */
int multiplayer_game_is_over(const MultiplayerGame_s *mg);

/**
 * Update death animation for all dying snakes.
 * Returns 1 if any animations are still playing, 0 if all complete.
 */
int multiplayer_game_update_death_animations(MultiplayerGame_s *mg);

/**
 * Add food at a specific position.
 */
void multiplayer_game_add_food(MultiplayerGame_s *mg, Vec2 pos);

#endif
