#ifndef ONLINE_MULTIPLAYER_H
#define ONLINE_MULTIPLAYER_H

#include "multiplayer_game.h"
#include "../src/mpapi/c_client/libs/mpapi.h"
#include "../src/mpapi/c_client/libs/jansson/jansson.h"

typedef enum {
    ONLINE_STATE_HOST_SETUP,     // Choosing private/public
    ONLINE_STATE_LOBBY,          // Waiting for players
    ONLINE_STATE_COUNTDOWN,      // 3-2-1 countdown
    ONLINE_STATE_PLAYING,        // Active game
    ONLINE_STATE_GAME_OVER,      // Game ended
    ONLINE_STATE_DISCONNECTED    // Connection lost
} OnlineState;

typedef struct {
    mpapi *api;                  // mpapi instance
    int listener_id;             // Event listener ID
    MultiplayerGame_s *game;     // Game state
    OnlineState state;           // Current online state
    int is_private;              // 1 for private, 0 for public

    // Error handling
    int connection_lost;         // 1 if connection lost
    char error_message[256];     // Error message to display

    // Client-specific
    Direction pending_input;     // Client's next input direction
    int has_pending_input;       // 1 if input queued
    char our_client_id[64];      // Our mpapi client ID (for identifying ourselves)
} OnlineMultiplayerContext;

// Lifecycle
OnlineMultiplayerContext* online_multiplayer_create(void);
void online_multiplayer_destroy(OnlineMultiplayerContext *ctx);

// Host operations
int online_multiplayer_host(OnlineMultiplayerContext *ctx, int is_private, int board_width, int board_height, const char *player_name);
void online_multiplayer_host_update(OnlineMultiplayerContext *ctx, unsigned int current_time);
void online_multiplayer_host_broadcast_state(OnlineMultiplayerContext *ctx);

// Client operations
int online_multiplayer_join(OnlineMultiplayerContext *ctx, const char *session_id, int board_width, int board_height, const char *player_name);
void online_multiplayer_client_send_input(OnlineMultiplayerContext *ctx, Direction dir);

// Common operations
void online_multiplayer_start_game(OnlineMultiplayerContext *ctx);
int online_multiplayer_get_local_player_index(OnlineMultiplayerContext *ctx);
void online_multiplayer_toggle_ready(OnlineMultiplayerContext *ctx);
int online_multiplayer_all_players_ready(OnlineMultiplayerContext *ctx);
void online_multiplayer_reset_ready_states(OnlineMultiplayerContext *ctx);

// JSON serialization
json_t* online_multiplayer_serialize_state(MultiplayerGame_s *game);
void online_multiplayer_deserialize_state(MultiplayerGame_s *game, json_t *data);

#endif
