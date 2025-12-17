#include "text_sdl.h"
#include <string.h>

int text_init(TextRenderer *tr, const char *font_path, int pt_size) {
    tr->font = TTF_OpenFont(font_path, pt_size);
    return tr->font ? 1 : 0;
}

void text_shutdown(TextRenderer *tr) {
    if (tr->font) {
        TTF_CloseFont(tr->font);
        tr->font = NULL;
    }
}

static void draw_text_internal(SDL_Renderer *ren, TTF_Font *font, int x, int y, const char *msg) {
    if (!msg || !*msg) return;

    // White text
    SDL_Color c = {255, 255, 255, 255};

    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, msg, c);
    if (!surf) return;

    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
    if (!tex) {
        SDL_FreeSurface(surf);
        return;
    }

    SDL_Rect dst = { x, y, surf->w, surf->h };
    SDL_FreeSurface(surf);

    SDL_RenderCopy(ren, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
}

void text_draw(SDL_Renderer *ren, TextRenderer *tr, int x, int y, const char *msg) {
    if (!tr || !tr->font) return;
    draw_text_internal(ren, tr->font, x, y, msg);
}

void text_draw_center(SDL_Renderer *ren, TextRenderer *tr, int cx, int cy, const char *msg) {
    if (!tr || !tr->font || !msg) return;

    SDL_Color c = {255, 255, 255, 255};
    SDL_Surface *surf = TTF_RenderUTF8_Blended(tr->font, msg, c);
    if (!surf) return;

    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
    if (!tex) {
        SDL_FreeSurface(surf);
        return;
    }

    SDL_Rect dst;
    dst.w = surf->w;
    dst.h = surf->h;
    dst.x = cx - dst.w / 2;
    dst.y = cy - dst.h / 2;

    SDL_FreeSurface(surf);

    SDL_RenderCopy(ren, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
}
