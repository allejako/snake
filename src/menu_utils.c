#include "menu_utils.h"
#include "ui_helpers.h"
#include "constants.h"
#include <string.h>
#include <stdio.h>

void menu_draw_text_centered(SDL_Renderer *ren, TextRenderer *text, int x, int y, const char *str)
{
    ui_draw_text_centered(ren, text, x, y, str);
}

void menu_render_standard(SDL_Renderer *ren, TextRenderer *text, const char *title,
                          const char **items, int count, int selected_index,
                          int window_w, int window_h)
{
    /* Clear background with menu color */
    SDL_SetRenderDrawColor(ren, COLOR_BG_MENU_R, COLOR_BG_MENU_G, COLOR_BG_MENU_B, 255);
    SDL_RenderClear(ren);

    int center_x = window_w / 2;
    int start_y = window_h / 2 - (count * 16);

    /* Draw title if provided */
    if (title) {
        ui_draw_text_centered(ren, text, center_x, start_y - 60, title);
    }

    /* Draw menu items */
    for (int i = 0; i < count; ++i) {
        ui_draw_menu_item(ren, text, center_x, start_y + i * 32, items[i], i == selected_index);
    }

    SDL_RenderPresent(ren);
}

void menu_render_message(SDL_Renderer *ren, TextRenderer *text, const char *message,
                         int window_w, int window_h)
{
    /* Clear background with menu color */
    SDL_SetRenderDrawColor(ren, COLOR_BG_MENU_R, COLOR_BG_MENU_G, COLOR_BG_MENU_B, 255);
    SDL_RenderClear(ren);

    int center_x = window_w / 2;
    int center_y = window_h / 2;

    /* Parse multi-line messages */
    char buffer[1024];
    strncpy(buffer, message, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    int line_count = 0;
    char *lines[32];
    char *token = strtok(buffer, "\n");
    while (token && line_count < 32) {
        lines[line_count++] = token;
        token = strtok(NULL, "\n");
    }

    /* Center the block of lines */
    int start_y = center_y - (line_count * 15);
    for (int i = 0; i < line_count; ++i) {
        ui_draw_text_centered(ren, text, center_x, start_y + i * 30, lines[i]);
    }

    SDL_RenderPresent(ren);
}

int menu_poll_input(const Settings *settings, int *selected_index, int item_count,
                    int *out_select, int *out_back, int *out_quit)
{
    *out_select = 0;
    *out_back = 0;
    *out_quit = 0;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            *out_quit = 1;
            return 0;
        }

        if (e.type == SDL_KEYDOWN) {
            SDL_Keycode key = e.key.keysym.sym;
            SettingAction action = settings_find_action(settings, key);

            if (action == SETTING_ACTION_UP) {
                *selected_index = (*selected_index - 1 + item_count) % item_count;
            } else if (action == SETTING_ACTION_DOWN) {
                *selected_index = (*selected_index + 1) % item_count;
            } else if (key == SDLK_RETURN || key == SDLK_SPACE) {
                *out_select = 1;
            } else if (key == SDLK_ESCAPE) {
                *out_back = 1;
            }
        }
    }

    return 1;
}

void menu_render_list(SDL_Renderer *ren, TextRenderer *text, const char *title,
                      const char **items, int count, int selected_index,
                      int window_w, int window_h, int max_visible)
{
    /* Clear background with menu color */
    SDL_SetRenderDrawColor(ren, COLOR_BG_MENU_R, COLOR_BG_MENU_G, COLOR_BG_MENU_B, 255);
    SDL_RenderClear(ren);

    int center_x = window_w / 2;
    int start_y = 100;

    /* Draw title */
    if (title) {
        ui_draw_text_centered(ren, text, center_x, start_y, title);
        start_y += 60;
    }

    /* Calculate scroll offset */
    int scroll_offset = 0;
    if (count > max_visible && selected_index >= max_visible / 2) {
        scroll_offset = selected_index - max_visible / 2;
        if (scroll_offset + max_visible > count) {
            scroll_offset = count - max_visible;
        }
    }

    /* Draw visible items */
    int visible_count = count < max_visible ? count : max_visible;
    for (int i = 0; i < visible_count; ++i) {
        int item_idx = i + scroll_offset;
        ui_draw_menu_item(ren, text, center_x, start_y + i * 32,
                         items[item_idx], item_idx == selected_index);
    }

    /* Draw scroll indicators if needed */
    if (count > max_visible) {
        if (scroll_offset > 0) {
            ui_draw_text_centered(ren, text, center_x, start_y - 20, "^^^");
        }
        if (scroll_offset + max_visible < count) {
            ui_draw_text_centered(ren, text, center_x, start_y + visible_count * 32 + 10, "vvv");
        }
    }

    SDL_RenderPresent(ren);
}
