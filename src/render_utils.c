#include "render_utils.h"
#include "constants.h"
#include "ui_helpers.h"
#include "text_sdl.h"
#include <stdio.h>

/* Helper macros for setting colors from constants */
#define SET_COLOR_BG_DARK(ren) SDL_SetRenderDrawColor(ren, COLOR_BG_DARK_R, COLOR_BG_DARK_G, COLOR_BG_DARK_B, 255)
#define SET_COLOR_BG_BOARD(ren) SDL_SetRenderDrawColor(ren, COLOR_BG_BOARD_R, COLOR_BG_BOARD_G, COLOR_BG_BOARD_B, 255)
#define SET_COLOR_BORDER(ren) SDL_SetRenderDrawColor(ren, COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B, 255)
#define SET_COLOR_FOOD(ren) SDL_SetRenderDrawColor(ren, COLOR_FOOD_R, COLOR_FOOD_G, COLOR_FOOD_B, 255)

/* Helper to compute board layout */
static void compute_layout(UiSdl *ui, const Board *board, int *out_origin_x, int *out_origin_y)
{
    int board_cells_w = board->width + BOARD_BORDER_CELLS;
    int board_cells_h = board->height + BOARD_BORDER_CELLS;

    int cell_w = ui->w / (board_cells_w + 4);
    int cell_h = ui->h / (board_cells_h + 4);
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

void render_board(SDL_Renderer *ren, const Board *board, int cell, int ox, int oy)
{
    /* Clear background */
    SET_COLOR_BG_DARK(ren);
    SDL_RenderClear(ren);

    /* Board area background */
    ui_draw_filled_rect(ren, ox, oy,
                        (board->width + 2) * cell, (board->height + 2) * cell,
                        COLOR_BG_BOARD_R, COLOR_BG_BOARD_G, COLOR_BG_BOARD_B);

    /* Border (1 cell thick) */
    ui_draw_filled_rect(ren, ox, oy, (board->width + 2) * cell, cell,
                        COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B);
    ui_draw_filled_rect(ren, ox, oy + (board->height + 1) * cell,
                        (board->width + 2) * cell, cell,
                        COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B);
    ui_draw_filled_rect(ren, ox, oy, cell, (board->height + 2) * cell,
                        COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B);
    ui_draw_filled_rect(ren, ox + (board->width + 1) * cell, oy,
                        cell, (board->height + 2) * cell,
                        COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B);
}

void render_snake(SDL_Renderer *ren, const Snake *snake, int r, int g, int b,
                  int cell, int ox, int oy, const Board *board)
{
    (void)board;  /* Unused parameter */

    for (int i = 0; i < snake->length; ++i)
    {
        Vec2 seg = snake->segments[i];
        int cell_x = ox + (1 + seg.x) * cell;
        int cell_y = oy + (1 + seg.y) * cell;

        if (i == 0) {
            /* Head */
            ui_draw_filled_rect_with_outline(ren, cell_x, cell_y, cell, cell, r, g, b);
        } else {
            /* Body - slightly darker */
            int body_r = (int)(r * 0.7);
            int body_g = (int)(g * 0.7);
            int body_b = (int)(b * 0.7);
            ui_draw_filled_rect_with_outline(ren, cell_x, cell_y, cell, cell,
                                            body_r, body_g, body_b);
        }
    }
}

void render_food(SDL_Renderer *ren, const Vec2 *food, int count, int cell, int ox, int oy)
{

    for (int i = 0; i < count; ++i)
    {
        ui_draw_filled_rect(ren, ox + (1 + food[i].x) * cell,
                            oy + (1 + food[i].y) * cell,
                            cell, cell, COLOR_FOOD_R, COLOR_FOOD_G, COLOR_FOOD_B);
    }
}

void render_all_snakes(SDL_Renderer *ren, const MultiplayerPlayer *players,
                       int player_count, int cell, int ox, int oy, const Board *board)
{
    /* Player colors from multiplayer.h */
    static const int PLAYER_COLORS[][3] = {
        {0, 255, 0},      /* Green */
        {255, 100, 100},  /* Red */
        {100, 100, 255},  /* Blue */
        {255, 255, 100}   /* Yellow */
    };

    for (int i = 0; i < player_count; ++i)
    {
        if (players[i].joined && (players[i].alive || players[i].death_state == GAME_DYING))
        {
            int r = PLAYER_COLORS[i % 4][0];
            int g = PLAYER_COLORS[i % 4][1];
            int b = PLAYER_COLORS[i % 4][2];
            render_snake(ren, &players[i].snake, r, g, b, cell, ox, oy, board);
        }
    }
}

void render_multiplayer_food(SDL_Renderer *ren, const Multiplayer *mp, int cell, int ox, int oy)
{
    render_food(ren, mp->food, mp->food_count, cell, ox, oy);
}

void render_hud_singleplayer(UiSdl *ui, int score, int fruits, unsigned int time_ms,
                              int combo_count, int combo_best)
{
    if (!ui->text_ok)
        return;

    char hud[128];

    /* Score */
    snprintf(hud, sizeof(hud), "Score: %d", score);
    text_draw(ui->ren, &ui->text, 10, 10, hud);

    /* Fruits */
    snprintf(hud, sizeof(hud), "Fruits: %d", fruits);
    text_draw(ui->ren, &ui->text, 10, 40, hud);

    /* Time */
    int seconds = time_ms / 1000;
    int minutes = seconds / 60;
    seconds = seconds % 60;
    snprintf(hud, sizeof(hud), "Time: %02d:%02d", minutes, seconds);
    text_draw(ui->ren, &ui->text, 10, 70, hud);

    /* Combo */
    if (combo_count > 0 || combo_best > 0) {
        snprintf(hud, sizeof(hud), "Combo: %d (Best: %d)", combo_count, combo_best);
        text_draw(ui->ren, &ui->text, 10, 100, hud);
    }
}

void render_hud_multiplayer(UiSdl *ui, const Multiplayer *mp, int local_index)
{
    if (!ui->text_ok)
        return;

    /* Render player cards for all players */
    int card_y = 10;
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        if (mp->players[i].joined) {
            render_player_card(ui, &mp->players[i], i, 10, card_y, i == local_index);
            card_y += 80;
        }
    }
}

void render_player_card(UiSdl *ui, const MultiplayerPlayer *player, int index,
                        int x, int y, int is_local)
{
    if (!ui->text_ok)
        return;

    char text[128];

    /* Player name */
    snprintf(text, sizeof(text), "%s%s",
             player->name, is_local ? " (You)" : "");
    text_draw(ui->ren, &ui->text, x, y, text);

    /* Lives */
    snprintf(text, sizeof(text), "Lives: %d", player->lives);
    text_draw(ui->ren, &ui->text, x, y + 20, text);

    /* Score */
    snprintf(text, sizeof(text), "Score: %d", player->score);
    text_draw(ui->ren, &ui->text, x, y + 40, text);

    /* Status */
    if (!player->alive && player->lives > 0) {
        text_draw(ui->ren, &ui->text, x, y + 60, "Respawning...");
    } else if (!player->alive) {
        text_draw(ui->ren, &ui->text, x, y + 60, "Out");
    }
}

void render_menu(UiSdl *ui, const char **items, int count, int selected)
{
    if (!ui->text_ok)
        return;

    /* Clear background */
    SDL_SetRenderDrawColor(ui->ren, COLOR_BG_MENU_R, COLOR_BG_MENU_G, COLOR_BG_MENU_B, 255);
    SDL_RenderClear(ui->ren);

    /* Draw menu items */
    int start_y = ui->h / 2 - (count * 40) / 2;
    for (int i = 0; i < count; ++i)
    {
        int y = start_y + i * 40;
        ui_draw_menu_item(ui->ren, &ui->text, ui->w / 2, y, items[i], i == selected);
    }

    SDL_RenderPresent(ui->ren);
}

void render_centered_text(UiSdl *ui, const char *text, int y, SDL_Color color)
{
    if (!ui->text_ok)
        return;

    ui_draw_text_centered(ui->ren, &ui->text, ui->w / 2, y, text);
}
