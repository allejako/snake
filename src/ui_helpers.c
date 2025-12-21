#include "ui_helpers.h"
#include "text_sdl.h"
#include <string.h>

void ui_draw_filled_rect(SDL_Renderer *ren, int x, int y, int w, int h, int r, int g, int b)
{
    SDL_Rect rect = {x, y, w, h};
    SDL_SetRenderDrawColor(ren, r, g, b, 255);
    SDL_RenderFillRect(ren, &rect);
}

void ui_draw_filled_rect_alpha(SDL_Renderer *ren, int x, int y, int w, int h, int r, int g, int b, int a)
{
    SDL_Rect rect = {x, y, w, h};
    SDL_SetRenderDrawColor(ren, r, g, b, a);
    SDL_RenderFillRect(ren, &rect);
}

void ui_draw_filled_rect_with_outline(SDL_Renderer *ren, int x, int y, int w, int h, int r, int g, int b)
{
    SDL_Rect rect = {x, y, w, h};

    // Draw black outline
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderDrawRect(ren, &rect);

    // Draw filled rect (slightly inset to show outline)
    SDL_Rect inner_rect = {x + 1, y + 1, w - 2, h - 2};
    SDL_SetRenderDrawColor(ren, r, g, b, 255);
    SDL_RenderFillRect(ren, &inner_rect);
}

void ui_draw_text_centered(SDL_Renderer *ren, TextRenderer *text, int cx, int y, const char *str)
{
    text_draw_center(ren, text, cx, y, str);
}

int ui_handle_menu_navigation(int current_selection, int menu_count, int key_up, int key_down)
{
    if (key_up) {
        return (current_selection - 1 + menu_count) % menu_count;
    }
    if (key_down) {
        return (current_selection + 1) % menu_count;
    }
    return current_selection;
}

void ui_draw_menu_item(SDL_Renderer *ren, TextRenderer *text, int cx, int y, const char *label, int is_selected)
{
    if (is_selected) {
        // Draw selection indicator (you can customize this)
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "> %s <", label);
        text_draw_center(ren, text, cx, y, buffer);
    } else {
        text_draw_center(ren, text, cx, y, label);
    }
}
