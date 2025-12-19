#ifndef UI_HELPERS_H
#define UI_HELPERS_H

#include <SDL2/SDL.h>
#include "text_sdl.h"

/**
 * Draw a filled rectangle with specified color.
 *
 * @param ren SDL renderer
 * @param x X position
 * @param y Y position
 * @param w Width
 * @param h Height
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 */
void ui_draw_filled_rect(SDL_Renderer *ren, int x, int y, int w, int h, int r, int g, int b);

/**
 * Draw a filled rectangle with specified color and alpha.
 */
void ui_draw_filled_rect_alpha(SDL_Renderer *ren, int x, int y, int w, int h, int r, int g, int b, int a);

/**
 * Draw centered text at specified position.
 *
 * @param ren SDL renderer
 * @param text Text rendering context
 * @param cx Center X position
 * @param y Y position
 * @param str Text string to draw
 */
void ui_draw_text_centered(SDL_Renderer *ren, TextRenderer *text, int cx, int y, const char *str);

/**
 * Handle menu navigation (up/down keys with wrapping).
 *
 * @param current_selection Current menu index
 * @param menu_count Total number of menu items
 * @param key_up 1 if up key pressed
 * @param key_down 1 if down key pressed
 * @return New menu index
 */
int ui_handle_menu_navigation(int current_selection, int menu_count, int key_up, int key_down);

/**
 * Draw a menu item (highlighted if selected).
 *
 * @param ren SDL renderer
 * @param text Text rendering context
 * @param cx Center X position
 * @param y Y position
 * @param label Menu item text
 * @param is_selected Whether this item is currently selected
 */
void ui_draw_menu_item(SDL_Renderer *ren, TextRenderer *text, int cx, int y, const char *label, int is_selected);

#endif // UI_HELPERS_H
