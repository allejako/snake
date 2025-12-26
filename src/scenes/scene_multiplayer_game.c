#include "scene_master.h"
#include "ui_sdl.h"
#include "settings.h"
#include "multiplayer.h"
#include "render_utils.h"
#include "ui_helpers.h"
#include "text_sdl.h"
#include "constants.h"
#include <stdlib.h>
#include <SDL2/SDL.h>

/* Scene state */
typedef struct {
    unsigned int last_tick;
    int countdown;  /* Countdown before game starts */
} MultiplayerGameSceneData;

static MultiplayerGameSceneData *state = NULL;

#define GAME_FRAME_DELAY_MS 16

/* Helper function to compute board layout */
static void compute_layout(UiSdl *ui, const Board *board, int *out_origin_x, int *out_origin_y)
{
    int board_cells_w = board->width + BOARD_BORDER_CELLS;
    int board_cells_h = board->height + BOARD_BORDER_CELLS;
    int padding = 4; // LAYOUT_PADDING_CELLS

    int cell_w = ui->w / (board_cells_w + padding);
    int cell_h = ui->h / (board_cells_h + padding);
    ui->cell = cell_w < cell_h ? cell_w : cell_h;
    if (ui->cell < MIN_CELL_SIZE)
        ui->cell = MIN_CELL_SIZE;
    if (ui->cell > MAX_CELL_SIZE)
        ui->cell = MAX_CELL_SIZE;

    ui->pad = ui->cell;

    int board_px_w = board_cells_w * ui->cell;
    int board_px_h = board_cells_h * ui->cell;

    *out_origin_x = (ui->w - board_px_w) / 2;
    *out_origin_y = (ui->h - board_px_h - TOP_OFFSET - BOTTOM_OFFSET) / 2 + TOP_OFFSET;
    if (*out_origin_x < ui->pad)
        *out_origin_x = ui->pad;
    if (*out_origin_y < ui->pad + TOP_OFFSET)
        *out_origin_y = ui->pad + TOP_OFFSET;
}

/* Static helper function to render online game */
static void render_online_game(UiSdl *ui, const Multiplayer *mp)
{
    // Compute board layout (centered with border) - same as singleplayer
    int ox, oy;
    compute_layout(ui, &mp->board, &ox, &oy);

    int board_w = mp->board.width;
    int board_h = mp->board.height;

    // Board bounds for mp HUD (in pixels)
    const int pad = 10;
    const int left = ox;
    const int top = oy;
    const int right = ox + (board_w + 2) * ui->cell;
    const int bottom = oy + (board_h + 2) * ui->cell;

    // Render board, food, and snakes using render_utils
    render_board(ui->ren, &mp->board, ui->cell, ox, oy);
    render_multiplayer_food(ui->ren, mp, ui->cell, ox, oy);
    render_all_snakes(ui->ren, mp->players, MAX_PLAYERS, ui->cell, ox, oy, &mp->board);

    // Board rect for HUD positioning
    SDL_Rect board_bg;
    board_bg.x = ox;
    board_bg.y = oy;
    board_bg.w = (board_w + 2) * ui->cell;
    board_bg.h = (board_h + 2) * ui->cell;

    // HUD - show player info
    if (ui->text_ok)
    {
        int hud_y = oy - 40;

        for (int p = 0; p < MAX_PLAYERS; p++)
        {
            if (!mp->players[p].joined)
                continue;

            // Corner anchors (x,y)
            int x = left, y = top; // default
            switch (p)
            {
            case 0: // P1: top-left
                x = pad;
                y = pad;
                break;
            case 1:              // P2: top-right
                x = ui->w - 200; // Reserve space for text
                y = pad;
                break;
            case 2: // P3: bottom-left
                x = pad;
                y = ui->h - 100;
                break;
            case 3: // P4: bottom-right
                x = ui->w - 200;
                y = ui->h - 100;
                break;
            }

            char hud1[64], hud2[64], hud3[64];

            snprintf(hud1, sizeof(hud1), "%s%s", mp->players[p].name,
                     mp->players[p].is_local_player ? " (YOU)" : "");
            snprintf(hud2, sizeof(hud2), "Score: %d | Wins: %d",
                     mp->players[p].score, mp->players[p].wins);

            if (mp->players[p].is_local_player)
            {
                snprintf(hud3, sizeof(hud3), "Lives: %d | Combo x%d",
                         mp->players[p].lives, mp->players[p].combo_count);
            }
            else
            {
                snprintf(hud3, sizeof(hud3), "Lives: %d", mp->players[p].lives);
            }

            text_draw(ui->ren, &ui->text, x, y, hud1);
            text_draw(ui->ren, &ui->text, x, y + 18, hud2);
            text_draw(ui->ren, &ui->text, x, y + 36, hud3);

            // Draw combo bar if player has an active combo
            if (mp->players[p].combo_count > 0 && mp->players[p].combo_expiry_time > 0)
            {
                unsigned int current_time = SDL_GetTicks();
                int bar_width = 150;
                int bar_height = 6;
                int bar_y = y + 54;

                // Calculate time remaining
                float time_remaining = 0.0f;
                if (current_time < mp->players[p].combo_expiry_time)
                {
                    time_remaining = (float)(mp->players[p].combo_expiry_time - current_time) / (float)mp->combo_window_ms;
                }

                // Clamp to [0, 1]
                if (time_remaining < 0.0f)
                    time_remaining = 0.0f;
                if (time_remaining > 1.0f)
                    time_remaining = 1.0f;

                int filled_width = (int)(bar_width * time_remaining);

                // Draw bar background (dark gray)
                SDL_Rect bar_bg = {x, bar_y, bar_width, bar_height};
                SDL_SetRenderDrawColor(ui->ren, 40, 40, 40, 255);
                SDL_RenderFillRect(ui->ren, &bar_bg);

                // Draw filled portion (based on player color from multiplayer_game.h)
                if (filled_width > 0)
                {
                    SDL_Rect bar_fill = {x, bar_y, filled_width, bar_height};
                    SDL_SetRenderDrawColor(ui->ren, PLAYER_COLORS[p].r, PLAYER_COLORS[p].g, PLAYER_COLORS[p].b, 255);
                    SDL_RenderFillRect(ui->ren, &bar_fill);
                }

                // Draw bar border (white)
                SDL_SetRenderDrawColor(ui->ren, 255, 255, 255, 255);
                SDL_RenderDrawRect(ui->ren, &bar_bg);
            }
        }

        // Instructions at bottom
        text_draw(ui->ren, &ui->text, ox, oy + board_bg.h + 8,
                  "Use keybinds to move | ESC: quit");
    }

    SDL_RenderPresent(ui->ren);
}

/* Helper functions for text rendering with colors (simplified) */
static void text_sdl_draw_centered(TextRenderer text, SDL_Renderer *ren, const char *msg,
                                   int x, int y, float scale, int r, int g, int b)
{
    (void)scale;
    (void)r;
    (void)g;
    (void)b; // Ignore for now
    text_draw_center(ren, &text, x, y, msg);
}

static void text_sdl_draw(TextRenderer text, SDL_Renderer *ren, const char *msg,
                          int x, int y, float scale, int r, int g, int b)
{
    (void)scale;
    (void)r;
    (void)g;
    (void)b; // Ignore for now
    text_draw(ren, &text, x, y, msg);
}

/* Static helper function to render online game over */
static void render_online_gameover(UiSdl *ui, const Multiplayer *mp)
{
    SDL_SetRenderDrawColor(ui->ren, 0, 0, 0, 255);
    SDL_RenderClear(ui->ren);

    text_sdl_draw_centered(ui->text, ui->ren, "GAME OVER", ui->w / 2, ui->h / 4, 2.0f, 255, 0, 0);

    // Find the winner (player who is alive or has highest score)
    int winner_idx = -1;
    int highest_score = -1;
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (mp->players[i].joined)
        {
            if (mp->players[i].alive ||
                (winner_idx == -1 && mp->players[i].score > highest_score))
            {
                winner_idx = i;
                highest_score = mp->players[i].score;
            }
        }
    }

    // Show final standings
    int y = ui->h / 3 + 40;
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (mp->players[i].joined)
        {
            char player_text[128];
            const char *name_suffix = mp->players[i].is_local_player ? " (YOU)" : "";
            int is_winner = (i == winner_idx);

            if (is_winner)
            {
                snprintf(player_text, sizeof(player_text), "%s%s: Score %d, Combo Best %d  << WINNER! >>",
                         mp->players[i].name, name_suffix,
                         mp->players[i].score, mp->players[i].combo_best);
                // Draw winner in bright gold/yellow
                text_sdl_draw(ui->text, ui->ren, player_text, ui->w / 2 - 250, y, 1.0f, 255, 215, 0);
            }
            else
            {
                snprintf(player_text, sizeof(player_text), "%s%s: Score %d, Combo Best %d",
                         mp->players[i].name, name_suffix,
                         mp->players[i].score, mp->players[i].combo_best);
                text_sdl_draw(ui->text, ui->ren, player_text, ui->w / 2 - 200, y, 0.9f,
                              PLAYER_COLORS[i].r, PLAYER_COLORS[i].g, PLAYER_COLORS[i].b);
            }
            y += 30;
        }
    }

    SDL_RenderPresent(ui->ren);
}

static void multiplayer_game_scene_init(UiSdl *ui, Settings *settings, void *user_data)
{
    (void)ui;
    (void)settings;
    (void)user_data;

    state = (MultiplayerGameSceneData *)malloc(sizeof(MultiplayerGameSceneData));
    if (state) {
        state->last_tick = SDL_GetTicks();
        state->countdown = 3;  /* 3-2-1 countdown */
    }
}

static void multiplayer_game_scene_update(UiSdl *ui, Settings *settings)
{
    if (!state) return;

    SceneContext *ctx = scene_master_get_context();
    if (!ctx || !ctx->multiplayer) {
        /* No multiplayer context, return to menu */
        scene_master_transition(SCENE_MENU, NULL);
        return;
    }

    /* Poll input */
    int quit = 0;
    Direction dir = ui_sdl_poll_online_game_input(ui, settings, &quit);

    if (quit) {
        scene_master_transition(SCENE_QUIT, NULL);
        return;
    }

    /* TODO: Handle countdown */
    if (state->countdown > 0) {
        /* Still in countdown */
        /* Decrement countdown every second */
        unsigned int now = SDL_GetTicks();
        if (now - state->last_tick >= 1000) {
            state->countdown--;
            state->last_tick = now;
        }
        return;
    }

    /* Game loop */
    /* TODO: Send input to multiplayer */
    /* TODO: Update multiplayer state */
    /* TODO: Check for game over */
    if (ctx->multiplayer->state == ONLINE_STATE_GAME_OVER) {
        /* Game over, transition to game over scene */
        /* For now, just go back to menu */
        scene_master_transition(SCENE_MENU, NULL);
        return;
    }
}

static void multiplayer_game_scene_render(UiSdl *ui, Settings *settings)
{
    (void)settings;
    if (!state) return;

    SceneContext *ctx = scene_master_get_context();
    if (!ctx || !ctx->multiplayer) return;

    if (state->countdown > 0) {
        /* Render countdown - use the one from lobby scene */
        int ox, oy;
        compute_layout(ui, &ctx->multiplayer->board, &ox, &oy);

        render_board(ui->ren, &ctx->multiplayer->board, ui->cell, ox, oy);
        render_all_snakes(ui->ren, ctx->multiplayer->players, MAX_PLAYERS, ui->cell, ox, oy, &ctx->multiplayer->board);

        SDL_SetRenderDrawBlendMode(ui->ren, SDL_BLENDMODE_BLEND);
        ui_draw_filled_rect_alpha(ui->ren, 0, 0, ui->w, ui->h, 0, 0, 0, 128);

        if (ui->text_ok)
        {
            char countdown_text[32];
            if (state->countdown > 0)
                snprintf(countdown_text, sizeof(countdown_text), "%d", state->countdown);
            else
                snprintf(countdown_text, sizeof(countdown_text), "GO!");
            text_draw_center(ui->ren, &ui->text, ui->w / 2, ui->h / 2, countdown_text);
        }
        SDL_RenderPresent(ui->ren);
    } else {
        /* Render game */
        render_online_game(ui, ctx->multiplayer);
    }

    SDL_Delay(GAME_FRAME_DELAY_MS);
}

static void multiplayer_game_scene_cleanup(void)
{
    if (state) {
        free(state);
        state = NULL;
    }
}

/* Register this scene with scene_master */
void scene_multiplayer_game_register(void)
{
    Scene scene = {
        .init = multiplayer_game_scene_init,
        .update = multiplayer_game_scene_update,
        .render = multiplayer_game_scene_render,
        .cleanup = multiplayer_game_scene_cleanup
    };

    scene_master_register_scene(SCENE_MULTIPLAYER_GAME, scene);
}
