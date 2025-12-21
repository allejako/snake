#ifndef CONSTANTS_H
#define CONSTANTS_H

// =============================================================================
// GAME BOARD CONSTANTS
// =============================================================================
#define SINGLEPLAYER_BOARD_WIDTH 20
#define SINGLEPLAYER_BOARD_HEIGHT 20
#define MULTIPLAYER_BOARD_WIDTH 40
#define MULTIPLAYER_BOARD_HEIGHT 40
#define BOARD_BORDER_CELLS 2

// =============================================================================
// GAME TIMING CONSTANTS
// =============================================================================
#define TICK_MS 95                   // Default game tick duration in milliseconds
#define MENU_FRAME_DELAY_MS 16       // ~60 FPS for menus
#define GAME_FRAME_DELAY_MS 1        // Minimal delay for gameplay
#define GAMEOVER_DISPLAY_MS 2000     // How long to show game over screen

// Speed curve parameters (singleplayer)
#define SPEED_START_MS 95.0f         // Starting tick time
#define SPEED_FLOOR_MS 40.0f         // Minimum tick time (speed cap)
#define SPEED_CURVE_K 0.08f          // Exponential decay rate

// =============================================================================
// COMBO SYSTEM CONSTANTS
// =============================================================================
#define BASE_COMBO_WINDOW_TICKS 30   // Base window in ticks for tier 1
#define COMBO_WINDOW_INCREASE_PER_TIER 5  // Additional ticks added per tier level

// =============================================================================
// MULTIPLAYER CONSTANTS
// =============================================================================
#define MAX_PLAYERS 4
#define MAX_FOOD_ITEMS 32
#define INITIAL_LIVES 3
#define MULTIPLAYER_COMBO_WINDOW_TICKS 20

// =============================================================================
// WINDOW & DISPLAY CONSTANTS
// =============================================================================
#define WINDOW_WIDTH 960
#define WINDOW_HEIGHT 640
#define MIN_CELL_SIZE 8
#define MAX_CELL_SIZE 40
#define LAYOUT_PADDING_CELLS 2

// UI Layout offsets
#define TOP_OFFSET 60                // Extra space at top for combo bar and UI
#define BOTTOM_OFFSET 40             // Extra space at bottom
#define COMBO_BAR_HEIGHT 20
#define COMBO_TEXT_SPACING 5         // Space between text and bar
#define COMBO_TEXT_CENTER_OFFSET 50  // Approximate offset for centering text

// Multiplayer HUD
#define MP_HUD_PADDING 40            // Padding around board for player stats

// =============================================================================
// COLOR DEFINITIONS (RGB)
// =============================================================================

// Background colors
#define COLOR_BG_DARK_R 15
#define COLOR_BG_DARK_G 15
#define COLOR_BG_DARK_B 18

#define COLOR_BG_BOARD_R 25
#define COLOR_BG_BOARD_G 25
#define COLOR_BG_BOARD_B 30

#define COLOR_BG_MENU_R 10
#define COLOR_BG_MENU_G 10
#define COLOR_BG_MENU_B 12

// Border & UI elements
#define COLOR_BORDER_R 220
#define COLOR_BORDER_G 220
#define COLOR_BORDER_B 220

#define COLOR_TEXT_R 200
#define COLOR_TEXT_G 200
#define COLOR_TEXT_B 200

// Food color (orange)
#define COLOR_FOOD_R 240
#define COLOR_FOOD_G 180
#define COLOR_FOOD_B 40

// Snake colors (singleplayer - green)
#define COLOR_SNAKE_HEAD_R 60
#define COLOR_SNAKE_HEAD_G 220
#define COLOR_SNAKE_HEAD_B 120

#define COLOR_SNAKE_BODY_R 35
#define COLOR_SNAKE_BODY_G 160
#define COLOR_SNAKE_BODY_B 90

// Multiplayer player colors (Player 1-4)
// Player 1 - Green
#define COLOR_P1_HEAD_R 60
#define COLOR_P1_HEAD_G 220
#define COLOR_P1_HEAD_B 120
#define COLOR_P1_BODY_R 35
#define COLOR_P1_BODY_G 160
#define COLOR_P1_BODY_B 90

// Player 2 - Blue
#define COLOR_P2_HEAD_R 60
#define COLOR_P2_HEAD_G 160
#define COLOR_P2_HEAD_B 255
#define COLOR_P2_BODY_R 30
#define COLOR_P2_BODY_G 100
#define COLOR_P2_BODY_B 200

// Player 3 - Yellow
#define COLOR_P3_HEAD_R 255
#define COLOR_P3_HEAD_G 220
#define COLOR_P3_HEAD_B 60
#define COLOR_P3_BODY_R 200
#define COLOR_P3_BODY_G 170
#define COLOR_P3_BODY_B 30

// Player 4 - Magenta
#define COLOR_P4_HEAD_R 255
#define COLOR_P4_HEAD_G 60
#define COLOR_P4_HEAD_B 220
#define COLOR_P4_BODY_R 200
#define COLOR_P4_BODY_G 30
#define COLOR_P4_BODY_B 160

// Combo bar tier colors
#define COLOR_COMBO_BG_R 50
#define COLOR_COMBO_BG_G 50
#define COLOR_COMBO_BG_B 50

// Tier 7 - Ultimate (bright magenta)
#define COLOR_COMBO_T7_R 255
#define COLOR_COMBO_T7_G 50
#define COLOR_COMBO_T7_B 255

// Tier 6 - Purple
#define COLOR_COMBO_T6_R 200
#define COLOR_COMBO_T6_G 50
#define COLOR_COMBO_T6_B 255

// Tier 5 - Red
#define COLOR_COMBO_T5_R 255
#define COLOR_COMBO_T5_G 50
#define COLOR_COMBO_T5_B 50

// Tier 4 - Orange
#define COLOR_COMBO_T4_R 255
#define COLOR_COMBO_T4_G 150
#define COLOR_COMBO_T4_B 50

// Tier 3 - Yellow
#define COLOR_COMBO_T3_R 255
#define COLOR_COMBO_T3_G 220
#define COLOR_COMBO_T3_B 50

// Tier 2 - Light blue
#define COLOR_COMBO_T2_R 100
#define COLOR_COMBO_T2_G 200
#define COLOR_COMBO_T2_B 255

// Tier 1 - Light green
#define COLOR_COMBO_T1_R 150
#define COLOR_COMBO_T1_G 255
#define COLOR_COMBO_T1_B 150

// =============================================================================
// NETWORK CONSTANTS
// =============================================================================
#define DEFAULT_SERVER_HOST "kontoret.onvo.se" //kontoret.onvo.se/localhost
#define DEFAULT_SERVER_PORT 9001
#define NETWORK_TIMEOUT_MS 5000

// =============================================================================
// MISCELLANEOUS
// =============================================================================
// Note: MAIN_MENU_COUNT and OPTIONS_MENU_COUNT are defined as enums in main.c
#define PAUSE_MENU_COUNT 3          // Resume, Options, Quit
#define MAX_SNAKE_LEN 256
#define INPUT_BUFFER_SIZE 2
#define SCOREBOARD_TOP_N 5

#endif // CONSTANTS_H
