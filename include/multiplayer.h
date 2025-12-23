#ifndef MULTIPLAYER_H
#define MULTIPLAYER_H

#include "common.h"
#include "board.h"
#include "snake.h"
#include "input_buffer.h"
#include "../src/mpapi/c_client/libs/mpapi.h"
#include "../src/mpapi/c_client/libs/jansson/jansson.h"

#define MAX_PLAYERS 4
#define MAX_FOOD_ITEMS 32

/**
 * Online multiplayer state enum
 */
typedef enum {
    ONLINE_STATE_HOST_SETUP,     // Choosing private/public
    ONLINE_STATE_LOBBY,          // Waiting for players
    ONLINE_STATE_COUNTDOWN,      // 3-2-1 countdown
    ONLINE_STATE_PLAYING,        // Active game
    ONLINE_STATE_GAME_OVER,      // Game ended
    ONLINE_STATE_DISCONNECTED    // Connection lost
} OnlineState;

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
    int wins;                 // Number of games won in this session

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
 * Player colors for visual distinction.
 */
typedef struct {
    int r, g, b;
} PlayerColor;

/**
 * Unified multiplayer game state (merged from MultiplayerGame_s + OnlineMultiplayerContext).
 * Combines game logic and network state in a single structure.
 */
typedef struct {
    // ===== GAME STATE =====
    Board board;              // Shared game board
    MultiplayerPlayer players[MAX_PLAYERS];  // Player states
    Vec2 food[MAX_FOOD_ITEMS]; // Food positions
    int food_count;           // Number of active food items
    int active_players;       // Number of players currently alive
    int total_joined;         // Total number of players who joined

    // Network identity
    int is_host;              // 1 if local instance is host, 0 if client
    int local_player_index;   // Which player slot is local (-1 if spectating)
    unsigned int combo_window_ms; // Combo timer window (based on tick speed)

    // Session info
    char session_id[8];       // 6-char session ID + null terminator
    char host_client_id[64];  // Host's mpapi client ID

    // ===== NETWORK STATE =====
    mpapi *api;               // mpapi instance
    int listener_id;          // Event listener ID
    OnlineState state;        // Current online state
    int is_private;           // 1 for private, 0 for public

    // Error handling
    int connection_lost;      // 1 if connection lost
    char error_message[256];  // Error message to display

    // Client input management
    Direction pending_input;  // Client's next input direction
    int has_pending_input;    // 1 if input queued
    char our_client_id[64];   // Our mpapi client ID (for identifying ourselves)

    // Synchronized game timing
    unsigned int game_start_timestamp; // Synchronized timestamp when game should start
} Multiplayer;

// Player colors: Red, Blue, Green, Orange
extern const PlayerColor PLAYER_COLORS[MAX_PLAYERS];

// ===== LIFECYCLE =====
Multiplayer* multiplayer_create(void);
void multiplayer_destroy(Multiplayer *mp);

// ===== HOST OPERATIONS =====
int multiplayer_host(Multiplayer *mp, int is_private, int board_width,
                     int board_height, const char *player_name);
void multiplayer_host_update(Multiplayer *mp, unsigned int current_time);
void multiplayer_host_broadcast_state(Multiplayer *mp);

// ===== CLIENT OPERATIONS =====
int multiplayer_join(Multiplayer *mp, const char *session_id,
                     int board_width, int board_height, const char *player_name);
void multiplayer_client_send_input(Multiplayer *mp, Direction dir);

// ===== COMMON OPERATIONS =====
void multiplayer_start_game(Multiplayer *mp);
int multiplayer_get_local_player_index(Multiplayer *mp);
void multiplayer_toggle_ready(Multiplayer *mp);
int multiplayer_all_players_ready(Multiplayer *mp);
void multiplayer_reset_ready_states(Multiplayer *mp);

// ===== GAME LOGIC (exposed for main.c) =====
void multiplayer_game_init(Multiplayer *mp, int width, int height);
int multiplayer_game_join_player(Multiplayer *mp, int player_index);
int multiplayer_game_leave_player(Multiplayer *mp, int player_index);
void multiplayer_game_start(Multiplayer *mp);
void multiplayer_game_update(Multiplayer *mp, int is_host);
void multiplayer_game_change_direction(Multiplayer *mp, int player_index, Direction dir);
int multiplayer_game_is_over(const Multiplayer *mp);
int multiplayer_game_update_death_animations(Multiplayer *mp);
void multiplayer_game_add_food(Multiplayer *mp, Vec2 pos);

// ===== SERIALIZATION (internal) =====
json_t* multiplayer_serialize_state(Multiplayer *mp);
void multiplayer_deserialize_state(Multiplayer *mp, json_t *data);

#endif
