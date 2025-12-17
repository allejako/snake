#ifndef TEXT_SDL_H
#define TEXT_SDL_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

typedef struct {
    TTF_Font *font;
} TextRenderer;

int  text_init(TextRenderer *tr, const char *font_path, int pt_size);
void text_shutdown(TextRenderer *tr);

void text_draw(SDL_Renderer *ren, TextRenderer *tr,
               int x, int y, const char *msg);

void text_draw_center(SDL_Renderer *ren, TextRenderer *tr,
                      int cx, int cy, const char *msg);

#endif
