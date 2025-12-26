#ifndef MENU_UTILS_H
#define MENU_UTILS_H

#include <SDL2/SDL.h>
#include "text_sdl.h"
#include "settings.h"

/**
 * Render a standard menu with title and selectable items.
 * Highlights the currently selected item.
 *
 * @param ren SDL renderer
 * @param text Text rendering context
 * @param title Menu title (NULL for no title)
 * @param items Array of menu item strings
 * @param count Number of menu items
 * @param selected_index Index of currently selected item
 * @param window_w Window width (for centering)
 * @param window_h Window height (for centering)
 */
void menu_render_standard(SDL_Renderer *ren, TextRenderer *text, const char *title,
                          const char **items, int count, int selected_index,
                          int window_w, int window_h);

/**
 * Render a text-centered message (used for prompts, errors, etc.).
 *
 * @param ren SDL renderer
 * @param text Text rendering context
 * @param message Message to display (can contain multiple lines separated by \n)
 * @param window_w Window width
 * @param window_h Window height
 */
void menu_render_message(SDL_Renderer *ren, TextRenderer *text, const char *message,
                         int window_w, int window_h);

/**
 * Poll standard menu input (up, down, select, back).
 *
 * @param settings Settings for keybinding
 * @param selected_index Pointer to current selection (will be updated)
 * @param item_count Number of menu items
 * @param out_select Set to 1 if select pressed
 * @param out_back Set to 1 if back/escape pressed
 * @param out_quit Set to 1 if quit event received
 * @return 1 if should continue, 0 if should quit
 */
int menu_poll_input(const Settings *settings, int *selected_index, int item_count,
                    int *out_select, int *out_back, int *out_quit);

/**
 * Render a simple scrollable list with selection.
 *
 * @param ren SDL renderer
 * @param text Text rendering context
 * @param title List title
 * @param items Array of strings to display
 * @param count Number of items
 * @param selected_index Currently selected item
 * @param window_w Window width
 * @param window_h Window height
 * @param max_visible Maximum items to show at once (for scrolling)
 */
void menu_render_list(SDL_Renderer *ren, TextRenderer *text, const char *title,
                      const char **items, int count, int selected_index,
                      int window_w, int window_h, int max_visible);

/**
 * Centered text drawing helper.
 *
 * @param ren SDL renderer
 * @param text Text rendering context
 * @param x X position (center point)
 * @param y Y position (top)
 * @param str Text to draw
 */
void menu_draw_text_centered(SDL_Renderer *ren, TextRenderer *text, int x, int y, const char *str);

#endif
