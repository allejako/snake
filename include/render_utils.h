#ifndef RENDER_UTILS_H
#define RENDER_UTILS_H

#include <SDL2/SDL.h>
#include "ui_sdl.h"
#include "board.h"
#include "snake.h"
#include "multiplayer.h"

/**
 * Render the game board (grid).
 *
 * @param ren SDL renderer
 * @param board Board to render
 * @param cell Pixel size per cell
 * @param ox X origin (centered position)
 * @param oy Y origin (centered position)
 */
void render_board(SDL_Renderer *ren, const Board *board, int cell, int ox, int oy);

/**
 * Render a single snake.
 *
 * @param ren SDL renderer
 * @param snake Snake to render
 * @param r Red color component (0-255)
 * @param g Green color component (0-255)
 * @param b Blue color component (0-255)
 * @param cell Pixel size per cell
 * @param ox X origin (centered position)
 * @param oy Y origin (centered position)
 * @param board Board for bounds checking
 */
void render_snake(SDL_Renderer *ren, const Snake *snake, int r, int g, int b,
                  int cell, int ox, int oy, const Board *board);

/**
 * Render food items.
 *
 * @param ren SDL renderer
 * @param food Array of food positions
 * @param count Number of food items
 * @param cell Pixel size per cell
 * @param ox X origin (centered position)
 * @param oy Y origin (centered position)
 */
void render_food(SDL_Renderer *ren, const Vec2 *food, int count, int cell, int ox, int oy);

/**
 * Render all snakes in a multiplayer game.
 * Uses player colors from PLAYER_COLORS array.
 *
 * @param ren SDL renderer
 * @param players Array of multiplayer players
 * @param player_count Number of players
 * @param cell Pixel size per cell
 * @param ox X origin (centered position)
 * @param oy Y origin (centered position)
 * @param board Board for bounds checking
 */
void render_all_snakes(SDL_Renderer *ren, const MultiplayerPlayer *players,
                       int player_count, int cell, int ox, int oy, const Board *board);

/**
 * Render food in a multiplayer game.
 *
 * @param ren SDL renderer
 * @param mp Multiplayer game state
 * @param cell Pixel size per cell
 * @param ox X origin (centered position)
 * @param oy Y origin (centered position)
 */
void render_multiplayer_food(SDL_Renderer *ren, const Multiplayer *mp, int cell, int ox, int oy);

/**
 * Render HUD for singleplayer mode.
 * Displays score, fruits, time, and combo information.
 *
 * @param ui UI renderer with text capabilities
 * @param score Current score
 * @param fruits Fruits eaten
 * @param time_ms Game time in milliseconds
 * @param combo_count Current combo count
 * @param combo_best Best combo achieved
 */
void render_hud_singleplayer(UiSdl *ui, int score, int fruits, unsigned int time_ms,
                              int combo_count, int combo_best);

/**
 * Render HUD for multiplayer mode.
 * Displays player cards with lives, scores, and status.
 *
 * @param ui UI renderer with text capabilities
 * @param mp Multiplayer game state
 * @param local_index Index of local player
 */
void render_hud_multiplayer(UiSdl *ui, const Multiplayer *mp, int local_index);

/**
 * Render a player card (multiplayer HUD element).
 * Shows player name, lives, score, and status.
 *
 * @param ui UI renderer with text capabilities
 * @param player Player to render card for
 * @param index Player index (for color coding)
 * @param x X position of card
 * @param y Y position of card
 * @param is_local 1 if this is the local player, 0 otherwise
 */
void render_player_card(UiSdl *ui, const MultiplayerPlayer *player, int index,
                        int x, int y, int is_local);

/**
 * Render a menu with selectable items.
 * Highlights the currently selected item.
 *
 * @param ui UI renderer with text capabilities
 * @param items Array of menu item strings
 * @param count Number of items
 * @param selected Index of selected item
 */
void render_menu(UiSdl *ui, const char **items, int count, int selected);

/**
 * Render centered text at a specific Y position.
 *
 * @param ui UI renderer with text capabilities
 * @param text Text to render
 * @param y Y position (text will be centered horizontally)
 * @param color Text color
 */
void render_centered_text(UiSdl *ui, const char *text, int y, SDL_Color color);

#endif
