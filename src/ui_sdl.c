#include "ui_sdl.h"
#include "constants.h"
#include "ui_helpers.h"
#include "scoreboard.h"
#include "game.h"

#define LAYOUT_PADDING_CELLS 4  // Local override for layout padding
#define DEFAULT_FONT_SIZE 18

// Helper macros for setting colors from constants
#define SET_COLOR_BG_DARK(ren) SDL_SetRenderDrawColor(ren, COLOR_BG_DARK_R, COLOR_BG_DARK_G, COLOR_BG_DARK_B, 255)
#define SET_COLOR_BG_BOARD(ren) SDL_SetRenderDrawColor(ren, COLOR_BG_BOARD_R, COLOR_BG_BOARD_G, COLOR_BG_BOARD_B, 255)
#define SET_COLOR_BG_MENU(ren) SDL_SetRenderDrawColor(ren, COLOR_BG_MENU_R, COLOR_BG_MENU_G, COLOR_BG_MENU_B, 255)
#define SET_COLOR_BORDER(ren) SDL_SetRenderDrawColor(ren, COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B, 255)
#define SET_COLOR_FOOD(ren) SDL_SetRenderDrawColor(ren, COLOR_FOOD_R, COLOR_FOOD_G, COLOR_FOOD_B, 255)
#define SET_COLOR_SNAKE_HEAD(ren) SDL_SetRenderDrawColor(ren, COLOR_SNAKE_HEAD_R, COLOR_SNAKE_HEAD_G, COLOR_SNAKE_HEAD_B, 255)
#define SET_COLOR_SNAKE_BODY(ren) SDL_SetRenderDrawColor(ren, COLOR_SNAKE_BODY_R, COLOR_SNAKE_BODY_G, COLOR_SNAKE_BODY_B, 255)
static void compute_layout(UiSdl *ui, const Board *board, int *out_origin_x, int *out_origin_y)
{
    // Draw a board with 1-cell border, but in pixels
    int board_cells_w = board->width + BOARD_BORDER_CELLS;
    int board_cells_h = board->height + BOARD_BORDER_CELLS;

    // Cell size: try to maximize without clipping (simple heuristic)
    int cell_w = ui->w / (board_cells_w + LAYOUT_PADDING_CELLS);
    int cell_h = ui->h / (board_cells_h + LAYOUT_PADDING_CELLS);
    ui->cell = cell_w < cell_h ? cell_w : cell_h;
    if (ui->cell < MIN_CELL_SIZE)
        ui->cell = MIN_CELL_SIZE;
    if (ui->cell > MAX_CELL_SIZE)
        ui->cell = MAX_CELL_SIZE;

    ui->pad = ui->cell; // 1 cell padding around board

    int board_px_w = board_cells_w * ui->cell;
    int board_px_h = board_cells_h * ui->cell;

    *out_origin_x = (ui->w - board_px_w) / 2;
    *out_origin_y = (ui->h - board_px_h - TOP_OFFSET - BOTTOM_OFFSET) / 2 + TOP_OFFSET;
    if (*out_origin_x < ui->pad)
        *out_origin_x = ui->pad;
    if (*out_origin_y < ui->pad + TOP_OFFSET)
        *out_origin_y = ui->pad + TOP_OFFSET;
}

static SDL_Rect cell_rect(UiSdl *ui, int origin_x, int origin_y, int cx, int cy)
{
    SDL_Rect r;
    r.x = origin_x + cx * ui->cell;
    r.y = origin_y + cy * ui->cell;
    r.w = ui->cell;
    r.h = ui->cell;
    return r;
}

// ---- public API ----

UiSdl *ui_sdl_create(const char *title, int window_w, int window_h)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return NULL;
    }
    if (TTF_Init() != 0)
    {
        fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
        // Can return NULL or continue without text
    }

    UiSdl *ui = (UiSdl *)calloc(1, sizeof(UiSdl));
    if (!ui)
        return NULL;

    ui->w = window_w;
    ui->h = window_h;

    ui->win = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        window_w,
        window_h,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!ui->win)
    {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        free(ui);
        SDL_Quit();
        return NULL;
    }

    SDL_SetWindowTitle(ui->win, "Snake");

    ui->ren = SDL_CreateRenderer(ui->win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ui->ren)
    {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(ui->win);
        free(ui);
        SDL_Quit();
        return NULL;
    }
    ui->text_ok = 0;
    if (text_init(&ui->text, "assets/fonts/BBHBogle-Regular.ttf", DEFAULT_FONT_SIZE))
    {
        ui->text_ok = 1;
    }
    else
    {
        fprintf(stderr, "Could not load font: %s\n", TTF_GetError());
    }

    return ui;
}

void ui_sdl_destroy(UiSdl *ui)
{
    if (!ui)
        return;
    if (ui->ren)
        SDL_DestroyRenderer(ui->ren);
    if (ui->win)
        SDL_DestroyWindow(ui->win);
    if (ui->text_ok)
        text_shutdown(&ui->text);
    TTF_Quit();
    free(ui);
    SDL_Quit();
}

int ui_sdl_poll(UiSdl *ui, const Settings *settings, int *out_has_dir, Direction *out_dir, int *out_pause)
{
    *out_has_dir = 0;
    *out_pause = 0;

    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_QUIT)
            return 0;

        if (e.type == SDL_KEYDOWN)
        {
            SDL_Keycode key = e.key.keysym.sym;

            // Check fixed keys first
            if (key == SDLK_ESCAPE)
            {
                *out_pause = 1;
                break;
            }

            // Check dynamic directional bindings for Player 1
            int action = settings_find_action(settings, 0, key);
            if (action >= 0)
            {
                *out_has_dir = 1;
                switch (action)
                {
                case SETTING_ACTION_UP:
                    *out_dir = DIR_UP;
                    break;
                case SETTING_ACTION_DOWN:
                    *out_dir = DIR_DOWN;
                    break;
                case SETTING_ACTION_LEFT:
                    *out_dir = DIR_LEFT;
                    break;
                case SETTING_ACTION_RIGHT:
                    *out_dir = DIR_RIGHT;
                    break;
                case SETTING_ACTION_USE:
                    // Future functionality
                    break;
                }
            }
        }
    }

    SDL_GetWindowSize(ui->win, &ui->w, &ui->h);
    return 1;
}
void ui_sdl_draw_game(UiSdl *ui, const Game *g, const char *player_name, int debug_mode, unsigned int current_tick_ms)
{
    int ox, oy;
    compute_layout(ui, &g->board, &ox, &oy);

    // bakgrund
    SET_COLOR_BG_DARK(ui->ren);
    SDL_RenderClear(ui->ren);

    // board area (bakgrund)
    SDL_Rect board_bg = cell_rect(ui, ox, oy, 0, 0);
    board_bg.w = (g->board.width + 2) * ui->cell;
    board_bg.h = (g->board.height + 2) * ui->cell;

    ui_draw_filled_rect(ui->ren, board_bg.x, board_bg.y, board_bg.w, board_bg.h,
                        COLOR_BG_BOARD_R, COLOR_BG_BOARD_G, COLOR_BG_BOARD_B);

    // ram (1-cell tjock)
    // top
    ui_draw_filled_rect(ui->ren, ox, oy, (g->board.width + 2) * ui->cell, ui->cell,
                        COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B);

    // bottom
    ui_draw_filled_rect(ui->ren, ox, oy + (g->board.height + 1) * ui->cell,
                        (g->board.width + 2) * ui->cell, ui->cell,
                        COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B);

    // left
    ui_draw_filled_rect(ui->ren, ox, oy, ui->cell, (g->board.height + 2) * ui->cell,
                        COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B);

    // right
    ui_draw_filled_rect(ui->ren, ox + (g->board.width + 1) * ui->cell, oy,
                        ui->cell, (g->board.height + 2) * ui->cell,
                        COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B);

    // Food (in board coords -> +1 for border)
    ui_draw_filled_rect(ui->ren, ox + (1 + g->board.food.x) * ui->cell,
                        oy + (1 + g->board.food.y) * ui->cell,
                        ui->cell, ui->cell, COLOR_FOOD_R, COLOR_FOOD_G, COLOR_FOOD_B);

    // Snake segments
    for (int i = 0; i < g->snake.length; ++i)
    {
        Vec2 seg = g->snake.segments[i];
        int cell_x = ox + (1 + seg.x) * ui->cell;
        int cell_y = oy + (1 + seg.y) * ui->cell;

        if (i == 0)
            ui_draw_filled_rect_with_outline(ui->ren, cell_x, cell_y, ui->cell, ui->cell,
                                COLOR_SNAKE_HEAD_R, COLOR_SNAKE_HEAD_G, COLOR_SNAKE_HEAD_B);
        else
            ui_draw_filled_rect_with_outline(ui->ren, cell_x, cell_y, ui->cell, ui->cell,
                                COLOR_SNAKE_BODY_R, COLOR_SNAKE_BODY_G, COLOR_SNAKE_BODY_B);
    }

    if (ui->text_ok)
    {
        char hud[128];
        snprintf(hud, sizeof(hud), "Score: %d", g->score);
        text_draw(ui->ren, &ui->text, ox, oy - 28, hud);

        // Debug info - show game speed (bottom right corner)
        if (debug_mode)
        {
            char debug_speed[64];
            char debug_combo[64];
            int debug_y = ui->h - 50;  // Position from bottom
            int debug_x = ui->w - 250; // Position from right

            snprintf(debug_speed, sizeof(debug_speed), "Speed: %ums/tick", current_tick_ms);
            text_draw(ui->ren, &ui->text, debug_x, debug_y, debug_speed);

            // Calculate current combo window ticks
            int combo_window_ticks = g->combo_window_ms / current_tick_ms;
            snprintf(debug_combo, sizeof(debug_combo), "Combo Window: %d ticks", combo_window_ticks);
            text_draw(ui->ren, &ui->text, debug_x, debug_y + 22, debug_combo);
        }

        // Combo display
        if (g->combo_count > 0)
        {
            unsigned int now = (unsigned int)SDL_GetTicks();

            // Calculate bar width to match board + border size
            int bar_width = (g->board.width + 2) * ui->cell;
            int board_center_x = ox + bar_width / 2;

            // Position the bar centered between top of screen and top of game border
            int bar_x = ox;
            int bar_y = (oy / 2) - (COMBO_BAR_HEIGHT / 2);

            // Draw combo timer bar (if active)
            if (now < g->combo_expiry_time)
            {
                float time_remaining = (float)(g->combo_expiry_time - now);
                float time_total = (float)g->combo_window_ms;
                float fill_ratio = time_remaining / time_total;

                // Background (empty bar)
                ui_draw_filled_rect(ui->ren, bar_x, bar_y, bar_width, COMBO_BAR_HEIGHT,
                                    COLOR_COMBO_BG_R, COLOR_COMBO_BG_G, COLOR_COMBO_BG_B);

                // Filled portion (timer) - color based on tier
                int tier = game_get_combo_tier(g->combo_count);
                int fill_width = (int)(bar_width * fill_ratio);

                if (tier >= 7)
                    ui_draw_filled_rect(ui->ren, bar_x, bar_y, fill_width, COMBO_BAR_HEIGHT,
                                        COLOR_COMBO_T7_R, COLOR_COMBO_T7_G, COLOR_COMBO_T7_B);
                else if (tier >= 6)
                    ui_draw_filled_rect(ui->ren, bar_x, bar_y, fill_width, COMBO_BAR_HEIGHT,
                                        COLOR_COMBO_T6_R, COLOR_COMBO_T6_G, COLOR_COMBO_T6_B);
                else if (tier >= 5)
                    ui_draw_filled_rect(ui->ren, bar_x, bar_y, fill_width, COMBO_BAR_HEIGHT,
                                        COLOR_COMBO_T5_R, COLOR_COMBO_T5_G, COLOR_COMBO_T5_B);
                else if (tier >= 4)
                    ui_draw_filled_rect(ui->ren, bar_x, bar_y, fill_width, COMBO_BAR_HEIGHT,
                                        COLOR_COMBO_T4_R, COLOR_COMBO_T4_G, COLOR_COMBO_T4_B);
                else if (tier >= 3)
                    ui_draw_filled_rect(ui->ren, bar_x, bar_y, fill_width, COMBO_BAR_HEIGHT,
                                        COLOR_COMBO_T3_R, COLOR_COMBO_T3_G, COLOR_COMBO_T3_B);
                else if (tier >= 2)
                    ui_draw_filled_rect(ui->ren, bar_x, bar_y, fill_width, COMBO_BAR_HEIGHT,
                                        COLOR_COMBO_T2_R, COLOR_COMBO_T2_G, COLOR_COMBO_T2_B);
                else
                    ui_draw_filled_rect(ui->ren, bar_x, bar_y, fill_width, COMBO_BAR_HEIGHT,
                                        COLOR_COMBO_T1_R, COLOR_COMBO_T1_G, COLOR_COMBO_T1_B);

                // Border
                SDL_Rect bg_rect = {bar_x, bar_y, bar_width, COMBO_BAR_HEIGHT};
                SDL_SetRenderDrawColor(ui->ren, COLOR_TEXT_R, COLOR_TEXT_G, COLOR_TEXT_B, 255);
                SDL_RenderDrawRect(ui->ren, &bg_rect);
            }

            // Draw combo text centered above the bar
            char combo_text[32];
            snprintf(combo_text, sizeof(combo_text), "COMBO x%d", g->combo_count);
            int combo_text_x = board_center_x - COMBO_TEXT_CENTER_OFFSET;
            int combo_text_y = bar_y - DEFAULT_FONT_SIZE - COMBO_TEXT_SPACING;
            text_draw(ui->ren, &ui->text, combo_text_x, combo_text_y, combo_text);

            // Draw multiplier text centered below the bar
            int multiplier = game_get_combo_multiplier(g->combo_count);
            char mult_text[32];
            snprintf(mult_text, sizeof(mult_text), "%dx Mult", multiplier);
            int mult_text_x = board_center_x - COMBO_TEXT_CENTER_OFFSET;
            int mult_text_y = bar_y + COMBO_BAR_HEIGHT + COMBO_TEXT_SPACING;
            text_draw(ui->ren, &ui->text, mult_text_x, mult_text_y, mult_text);
        }

        text_draw(ui->ren, &ui->text, ox, oy + (g->board.height + 2) * ui->cell + 8,
                  "Use keybinds to move | ESC: pause");
    }

    // GAME OVER overlay
    if (g->state == GAME_OVER && ui->text_ok)
    {
        int cx = board_bg.x + board_bg.w / 2;
        int cy = board_bg.y + board_bg.h / 2;

        text_draw_center(ui->ren, &ui->text, cx, cy - 20, "GAME OVER");
        text_draw_center(ui->ren, &ui->text, cx, cy + 15, "ESC: Back to menu");
    }
}
void ui_sdl_render(UiSdl *ui, const Game *g, const char *player_name, int debug_mode, unsigned int current_tick_ms)
{
    ui_sdl_draw_game(ui, g, player_name, debug_mode, current_tick_ms);
    SDL_RenderPresent(ui->ren);
}

int ui_sdl_get_name(UiSdl *ui, char *out_name, int out_size, int show_game_over)
{
    if (!ui || !ui->text_ok || !out_name || out_size <= 1)
        return 0;

    out_name[0] = '\0';
    int len = 0;

    SDL_StartTextInput();

    int running = 1;
    int accepted = 0;

    while (running)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
            {
                running = 0;
                accepted = 0;
                break;
            }

            if (e.type == SDL_KEYDOWN)
            {
                switch (e.key.keysym.sym)
                {
                case SDLK_ESCAPE:
                    running = 0;
                    accepted = 0;
                    break;

                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    running = 0;
                    accepted = 1;
                    break;

                case SDLK_BACKSPACE:
                    if (len > 0)
                    {
                        len--;
                        out_name[len] = '\0';
                    }
                    break;

                default:
                    break;
                }
            }

            if (e.type == SDL_TEXTINPUT)
            {
                const char *txt = e.text.text;
                int add = (int)strlen(txt);
                if (len + add < out_size - 1)
                {
                    memcpy(out_name + len, txt, add);
                    len += add;
                    out_name[len] = '\0';
                }
            }
        }

        /* -------- Rendering -------- */

        SET_COLOR_BG_MENU(ui->ren);
        SDL_RenderClear(ui->ren);

        int cx = ui->w / 2;
        int cy = ui->h / 2;

        // Dark overlay box
        SDL_Rect box;
        box.w = ui->w / 2;
        box.h = ui->h / 4;
        box.x = cx - box.w / 2;
        box.y = cy - box.h / 2;

        ui_draw_filled_rect(ui->ren, box.x, box.y, box.w, box.h,
                            COLOR_BG_BOARD_R, COLOR_BG_BOARD_G, COLOR_BG_BOARD_B);

        SDL_SetRenderDrawColor(ui->ren, COLOR_TEXT_R, COLOR_TEXT_G, COLOR_TEXT_B, 255);
        SDL_RenderDrawRect(ui->ren, &box);

        // Text
        if (show_game_over)
        {
            ui_draw_text_centered(ui->ren, &ui->text, cx, box.y + 30, "GAME OVER");
            ui_draw_text_centered(ui->ren, &ui->text, cx, box.y + 70, "Enter your name:");
        }
        else
        {
            ui_draw_text_centered(ui->ren, &ui->text, cx, box.y + 50, "Enter your name:");
        }

        // Visa input (eller placeholder)
        char display[128];
        if (len > 0)
        {
            snprintf(display, sizeof(display), "%s_", out_name);
        }
        else
        {
            snprintf(display, sizeof(display), "_");
        }

        ui_draw_text_centered(ui->ren, &ui->text, cx, box.y + 110, display);

        ui_draw_text_centered(ui->ren, &ui->text, cx, box.y + box.h - 30,
                              "Enter = OK    Esc = Cancel");

        SDL_RenderPresent(ui->ren);

        SDL_Delay(16); // ~60 FPS
    }

    SDL_StopTextInput();
    return accepted;
}

void ui_sdl_show_scoreboard(UiSdl *ui, const Scoreboard *sb)
{
    int center_x = ui->w / 2;
    int segment_y = ui->h / 40;
    int offset_y = 3 * segment_y;
    int running = 1;

    while (running)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
                return;
            if (e.type == SDL_KEYDOWN)
            {
                SDL_Keycode key = e.key.keysym.sym;
                // ESC or Enter to exit
                if (key == SDLK_ESCAPE || key == SDLK_RETURN || key == SDLK_KP_ENTER)
                {
                    running = 0;
                }
            }
        }

        SET_COLOR_BG_MENU(ui->ren);
        SDL_RenderClear(ui->ren);

        if (ui->text_ok)
        {
            // Title
            ui_draw_text_centered(ui->ren, &ui->text, center_x, segment_y, "HIGH SCORES");

            // Instructions
            ui_draw_text_centered(ui->ren, &ui->text, center_x, ui->h - 40, "ESC = Back");

            // Display all entries (up to 5)
            int display_count = sb->count < 5 ? sb->count : 5;
            for (int i = 0; i < display_count; ++i)
            {
                char row[128];
                snprintf(row, sizeof(row), "%2d) %-20s  %d", i + 1,
                         sb->entries[i].name, sb->entries[i].score);
                ui_draw_text_centered(ui->ren, &ui->text, center_x, offset_y + i * segment_y, row);
            }

            // Show message if no entries
            if (display_count == 0)
            {
                ui_draw_text_centered(ui->ren, &ui->text, center_x, offset_y, "No scores yet");
            }
        }

        SDL_RenderPresent(ui->ren);
        SDL_Delay(16);
    }
}

UiMenuAction ui_sdl_poll_menu(UiSdl *ui, const Settings *settings, int *out_quit)
{
    *out_quit = 0;

    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_QUIT)
        {
            *out_quit = 1;
            return UI_MENU_NONE;
        }
        if (e.type == SDL_KEYDOWN)
        {
            SDL_Keycode key = e.key.keysym.sym;

            // Fixed keys
            if (key == SDLK_ESCAPE)
            {
                *out_quit = 1;
                return UI_MENU_NONE;
            }
            if (key == SDLK_RETURN || key == SDLK_KP_ENTER)
            {
                return UI_MENU_SELECT;
            }

            // Dynamic navigation using Player 1's bindings
            int action = settings_find_action(settings, 0, key);
            if (action == SETTING_ACTION_UP)
                return UI_MENU_UP;
            if (action == SETTING_ACTION_DOWN)
                return UI_MENU_DOWN;
        }
    }

    SDL_GetWindowSize(ui->win, &ui->w, &ui->h);
    return UI_MENU_NONE;
}

void ui_sdl_render_menu(UiSdl *ui, const Settings *settings, int selected_index)
{
    SET_COLOR_BG_MENU(ui->ren);
    SDL_RenderClear(ui->ren);

    if (ui->text_ok)
    {
        int center_x = ui->w / 2;
        int y = ui->h / 2 - 120;

        ui_draw_text_centered(ui->ren, &ui->text, center_x, y, "SNAKE");
        y += 60;

        const char *items[] = {
            "Singleplayer",
            "Multiplayer",
            "Options",
            "Scoreboard",
            "Quit"};

        for (int i = 0; i < 5; ++i)
        {
            ui_draw_menu_item(ui->ren, &ui->text, center_x, y + i * 32, items[i], i == selected_index);
        }

        // Build instruction string with actual keybindings
        const char *up = settings_key_name(settings_get_key(settings, 0, SETTING_ACTION_UP));
        const char *down = settings_key_name(settings_get_key(settings, 0, SETTING_ACTION_DOWN));
        char instructions[128];
        snprintf(instructions, sizeof(instructions), "%s/%s + ENTER | ESC = Quit", up, down);
        ui_draw_text_centered(ui->ren, &ui->text, center_x, ui->h - 40, instructions);
    }

    SDL_RenderPresent(ui->ren);
}

void ui_sdl_render_options(UiSdl *ui)
{
    SET_COLOR_BG_MENU(ui->ren);
    SDL_RenderClear(ui->ren);

    if (ui->text_ok)
    {
        int cx = ui->w / 2;
        ui_draw_text_centered(ui->ren, &ui->text, cx, ui->h / 2 - 20, "OPTIONS");
        ui_draw_text_centered(ui->ren, &ui->text, cx, ui->h / 2 + 20, "(empty for now)");
        ui_draw_text_centered(ui->ren, &ui->text, cx, ui->h - 40, "ESC = Back to menu");
    }

    SDL_RenderPresent(ui->ren);
}

UiPauseAction ui_sdl_poll_pause(UiSdl *ui, const Settings *settings, int *out_quit)
{
    *out_quit = 0;

    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_QUIT)
        {
            *out_quit = 1;
            return UI_PAUSE_NONE;
        }
        if (e.type == SDL_KEYDOWN)
        {
            SDL_Keycode key = e.key.keysym.sym;

            // Fixed keys
            if (key == SDLK_ESCAPE)
            {
                // ESC in pause menu = quick continue
                return UI_PAUSE_ESCAPE;
            }
            if (key == SDLK_RETURN || key == SDLK_KP_ENTER)
            {
                return UI_PAUSE_SELECT;
            }

            // Dynamic navigation using Player 1's bindings
            int action = settings_find_action(settings, 0, key);
            if (action == SETTING_ACTION_UP)
                return UI_PAUSE_UP;
            if (action == SETTING_ACTION_DOWN)
                return UI_PAUSE_DOWN;
        }
    }

    SDL_GetWindowSize(ui->win, &ui->w, &ui->h);
    return UI_PAUSE_NONE;
}

void ui_sdl_render_pause_menu(UiSdl *ui, const Game *g, const char *player_name, int selected_index, int debug_mode, unsigned int current_tick_ms)
{
    // Render the game frame behind the pause overlay
    ui_sdl_draw_game(ui, g, player_name, debug_mode, current_tick_ms);

    // Draw a semi-transparent overlay
    SDL_SetRenderDrawBlendMode(ui->ren, SDL_BLENDMODE_BLEND);
    ui_draw_filled_rect_alpha(ui->ren, 0, 0, ui->w, ui->h, 0, 0, 0, 160);

    // Menu box
    SDL_Rect box;
    box.w = ui->w / 2;
    box.h = ui->h / 3;
    box.x = (ui->w - box.w) / 2;
    box.y = (ui->h - box.h) / 2;

    ui_draw_filled_rect(ui->ren, box.x, box.y, box.w, box.h,
                        COLOR_BG_BOARD_R, COLOR_BG_BOARD_G, COLOR_BG_BOARD_B);

    SDL_SetRenderDrawColor(ui->ren, COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B, 255);
    SDL_RenderDrawRect(ui->ren, &box);

    if (ui->text_ok)
    {
        const char *items[] = {"Continue", "Options", "Quit"};
        int cx = ui->w / 2;
        int yseg = box.h / 5;
        ui_draw_text_centered(ui->ren, &ui->text, cx, box.y - yseg, "PAUSED");

        for (int i = 0; i < 3; ++i)
        {
            ui_draw_menu_item(ui->ren, &ui->text, cx, box.y + yseg + i * yseg, items[i], i == selected_index);
        }

        ui_draw_text_centered(ui->ren, &ui->text, cx, box.y + 4 * yseg, "UP/DOWN + ENTER");
    }

    SDL_RenderPresent(ui->ren);
}

void ui_sdl_render_pause_options(UiSdl *ui, const Game *g, const char *player_name, int debug_mode, unsigned int current_tick_ms)
{
    // render the game behind
    ui_sdl_draw_game(ui, g, player_name, debug_mode, current_tick_ms);

    SDL_SetRenderDrawBlendMode(ui->ren, SDL_BLENDMODE_BLEND);
    ui_draw_filled_rect_alpha(ui->ren, 0, 0, ui->w, ui->h, 0, 0, 0, 170);

    SDL_Rect box;
    box.w = ui->w / 2;
    box.h = ui->h / 3;
    box.x = (ui->w - box.w) / 2;
    box.y = (ui->h - box.h) / 2;

    ui_draw_filled_rect(ui->ren, box.x, box.y, box.w, box.h,
                        COLOR_BG_BOARD_R, COLOR_BG_BOARD_G, COLOR_BG_BOARD_B);
    SDL_SetRenderDrawColor(ui->ren, COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B, 255);
    SDL_RenderDrawRect(ui->ren, &box);

    if (ui->text_ok)
    {
        int cx = ui->w / 2;
        int yseg = box.h / 5;
        ui_draw_text_centered(ui->ren, &ui->text, cx, box.y - yseg, "PAUSED");
        ui_draw_text_centered(ui->ren, &ui->text, cx, box.y + 1 * yseg, "OPTIONS");
        ui_draw_text_centered(ui->ren, &ui->text, cx, box.y + 2 * yseg, "(empty for now)");
        ui_draw_text_centered(ui->ren, &ui->text, cx, box.y + 4 * yseg, "ESC = Back");
    }

    SDL_RenderPresent(ui->ren);
}

// ==== New Keybindings UI Functions ====

void ui_sdl_render_options_menu(UiSdl *ui, const Settings *settings, int selected_index)
{
    SET_COLOR_BG_MENU(ui->ren);
    SDL_RenderClear(ui->ren);

    if (ui->text_ok)
    {
        int cx = ui->w / 2;
        int y = ui->h / 2 - 80;

        ui_draw_text_centered(ui->ren, &ui->text, cx, y, "OPTIONS");
        y += 60;

        const char *items[] = {"Keybinds", "Sound", "Back"};

        for (int i = 0; i < 3; ++i)
        {
            ui_draw_menu_item(ui->ren, &ui->text, cx, y + i * 32, items[i], i == selected_index);
        }

        // Build instruction string with actual keybindings
        const char *up = settings_key_name(settings_get_key(settings, 0, SETTING_ACTION_UP));
        const char *down = settings_key_name(settings_get_key(settings, 0, SETTING_ACTION_DOWN));
        char instructions[128];
        snprintf(instructions, sizeof(instructions), "%s/%s + ENTER | ESC = Back", up, down);
        ui_draw_text_centered(ui->ren, &ui->text, cx, ui->h - 40, instructions);
    }

    SDL_RenderPresent(ui->ren);
}

UiMenuAction ui_sdl_poll_options_menu(UiSdl *ui, const Settings *settings, int *out_quit)
{
    *out_quit = 0;

    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_QUIT)
        {
            *out_quit = 1;
            return UI_MENU_NONE;
        }
        if (e.type == SDL_KEYDOWN)
        {
            SDL_Keycode key = e.key.keysym.sym;

            // Fixed keys
            if (key == SDLK_ESCAPE)
            {
                return UI_MENU_BACK;
            }
            if (key == SDLK_RETURN || key == SDLK_KP_ENTER)
            {
                return UI_MENU_SELECT;
            }

            // Dynamic navigation using Player 1's bindings
            int action = settings_find_action(settings, 0, key);
            if (action == SETTING_ACTION_UP)
                return UI_MENU_UP;
            if (action == SETTING_ACTION_DOWN)
                return UI_MENU_DOWN;
        }
    }

    SDL_GetWindowSize(ui->win, &ui->w, &ui->h);
    return UI_MENU_NONE;
}

void ui_sdl_render_keybind_player_select(UiSdl *ui, const Settings *settings, int selected_index)
{
    SET_COLOR_BG_MENU(ui->ren);
    SDL_RenderClear(ui->ren);

    if (ui->text_ok)
    {
        int cx = ui->w / 2;
        int y = ui->h / 2 - 100;

        ui_draw_text_centered(ui->ren, &ui->text, cx, y, "CONFIGURE KEYBINDS");
        y += 60;

        const char *items[] = {"Player 1", "Player 2", "Player 3", "Player 4", "Back"};

        for (int i = 0; i < 5; ++i)
        {
            ui_draw_menu_item(ui->ren, &ui->text, cx, y + i * 32, items[i], i == selected_index);
        }

        // Build instruction string with actual keybindings
        const char *up = settings_key_name(settings_get_key(settings, 0, SETTING_ACTION_UP));
        const char *down = settings_key_name(settings_get_key(settings, 0, SETTING_ACTION_DOWN));
        char instructions[128];
        snprintf(instructions, sizeof(instructions), "%s/%s + ENTER | ESC = Back", up, down);
        ui_draw_text_centered(ui->ren, &ui->text, cx, ui->h - 40, instructions);
    }

    SDL_RenderPresent(ui->ren);
}

UiMenuAction ui_sdl_poll_keybind_player_select(UiSdl *ui, const Settings *settings, int *out_quit)
{
    *out_quit = 0;

    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_QUIT)
        {
            *out_quit = 1;
            return UI_MENU_NONE;
        }
        if (e.type == SDL_KEYDOWN)
        {
            SDL_Keycode key = e.key.keysym.sym;

            // Fixed keys
            if (key == SDLK_ESCAPE)
            {
                return UI_MENU_BACK;
            }
            if (key == SDLK_RETURN || key == SDLK_KP_ENTER)
            {
                return UI_MENU_SELECT;
            }

            // Dynamic navigation using Player 1's bindings
            int action = settings_find_action(settings, 0, key);
            if (action == SETTING_ACTION_UP)
                return UI_MENU_UP;
            if (action == SETTING_ACTION_DOWN)
                return UI_MENU_DOWN;
        }
    }

    SDL_GetWindowSize(ui->win, &ui->w, &ui->h);
    return UI_MENU_NONE;
}

void ui_sdl_render_keybind_prompt(UiSdl *ui, const Settings *settings, int player_index, SettingAction action)
{
    SET_COLOR_BG_MENU(ui->ren);
    SDL_RenderClear(ui->ren);

    if (!ui->text_ok)
    {
        SDL_RenderPresent(ui->ren);
        return;
    }

    int cx = ui->w / 2;
    int cy = ui->h / 2;

    // Title
    char title[64];
    snprintf(title, sizeof(title), "PLAYER %d KEYBINDS", player_index + 1);
    ui_draw_text_centered(ui->ren, &ui->text, cx, 40, title);

    // Progress
    char progress[32];
    snprintf(progress, sizeof(progress), "Binding %d/5", action + 1);
    ui_draw_text_centered(ui->ren, &ui->text, cx, 80, progress);

    // Main prompt
    char prompt[64];
    snprintf(prompt, sizeof(prompt), "Bind %s", settings_action_name(action));
    ui_draw_text_centered(ui->ren, &ui->text, cx, cy - 40, prompt);

    ui_draw_text_centered(ui->ren, &ui->text, cx, cy, "Press any key...");

    // Current binding
    SDL_Keycode current_binding = settings_get_key(settings, player_index, action);
    const char *current_key_name = settings_key_name(current_binding);
    char current[64];
    snprintf(current, sizeof(current), "Current: %s", current_key_name);
    ui_draw_text_centered(ui->ren, &ui->text, cx, cy + 40, current);

    // Show already bound keys
    int y_offset = cy + 100;
    if (action > 0)
    {
        ui_draw_text_centered(ui->ren, &ui->text, cx, y_offset, "Already bound:");
        y_offset += 30;

        for (int i = 0; i < action; i++)
        {
            SDL_Keycode bound = settings_get_key(settings, player_index, (SettingAction)i);
            char entry[64];
            snprintf(entry, sizeof(entry), "%s: %s",
                     settings_action_name((SettingAction)i),
                     settings_key_name(bound));
            ui_draw_text_centered(ui->ren, &ui->text, cx, y_offset, entry);
            y_offset += 24;
        }
    }

    // Cancel hint
    ui_draw_text_centered(ui->ren, &ui->text, cx, ui->h - 40, "ESC = Cancel");

    SDL_RenderPresent(ui->ren);
}

SDL_Keycode ui_sdl_poll_keybind_input(UiSdl *ui, int *out_cancel, int *out_quit)
{
    *out_cancel = 0;
    *out_quit = 0;

    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_QUIT)
        {
            *out_quit = 1;
            return 0;
        }

        if (e.type == SDL_KEYDOWN)
        {
            SDL_Keycode key = e.key.keysym.sym;

            // ESC cancels
            if (key == SDLK_ESCAPE)
            {
                *out_cancel = 1;
                return 0;
            }

            // Reject reserved keys
            if (key == SDLK_RETURN || key == SDLK_KP_ENTER)
            {
                // Reserved for menu select, ignore
                continue;
            }

            // Accept any other key
            return key;
        }
    }

    SDL_GetWindowSize(ui->win, &ui->w, &ui->h);
    return 0;
}

// ==== Sound Settings UI Functions ====

#include "audio_sdl.h"

void ui_sdl_render_sound_settings(UiSdl *ui, const Settings *settings, const AudioSdl *audio, int selected_index)
{
    SDL_SetRenderDrawColor(ui->ren, 10, 10, 12, 255);
    SDL_RenderClear(ui->ren);

    if (ui->text_ok)
    {
        int cx = ui->w / 2;
        int y = ui->h / 2 - 100;

        text_draw_center(ui->ren, &ui->text, cx, y, "SOUND SETTINGS");
        y += 60;

        // Get current volumes (already in 0-100 scale)
        int music_vol = audio ? audio_sdl_get_music_volume(audio) : 0;
        int effects_vol = audio ? audio_sdl_get_effects_volume(audio) : 0;

        const char *items[] = {"Music Volume", "Effects Volume", "Back"};

        for (int i = 0; i < 3; ++i)
        {
            char line[128];
            if (i == 0)
            {
                // Music volume with bar
                if (i == selected_index)
                {
                    snprintf(line, sizeof(line), "> Music Volume: [%-10s] %3d%% <",
                             "##########" + (10 - (music_vol / 10)), music_vol);
                }
                else
                {
                    snprintf(line, sizeof(line), "  Music Volume: [%-10s] %3d%%  ",
                             "##########" + (10 - (music_vol / 10)), music_vol);
                }
            }
            else if (i == 1)
            {
                // Effects volume with bar
                if (i == selected_index)
                {
                    snprintf(line, sizeof(line), "> Effects Volume: [%-10s] %3d%% <",
                             "##########" + (10 - (effects_vol / 10)), effects_vol);
                }
                else
                {
                    snprintf(line, sizeof(line), "  Effects Volume: [%-10s] %3d%%  ",
                             "##########" + (10 - (effects_vol / 10)), effects_vol);
                }
            }
            else
            {
                // Back
                if (i == selected_index)
                {
                    snprintf(line, sizeof(line), "> %s <", items[i]);
                }
                else
                {
                    snprintf(line, sizeof(line), "  %s  ", items[i]);
                }
            }
            text_draw_center(ui->ren, &ui->text, cx, y + i * 40, line);
        }

        // Build instruction strings with actual keybindings
        const char *up = settings_key_name(settings_get_key(settings, 0, SETTING_ACTION_UP));
        const char *down = settings_key_name(settings_get_key(settings, 0, SETTING_ACTION_DOWN));
        const char *left = settings_key_name(settings_get_key(settings, 0, SETTING_ACTION_LEFT));
        const char *right = settings_key_name(settings_get_key(settings, 0, SETTING_ACTION_RIGHT));

        char instructions1[128];
        char instructions2[64];
        snprintf(instructions1, sizeof(instructions1), "%s/%s = Navigate | %s/%s = Adjust", up, down, left, right);
        snprintf(instructions2, sizeof(instructions2), "ENTER/ESC = Back");

        text_draw_center(ui->ren, &ui->text, cx, ui->h - 60, instructions1);
        text_draw_center(ui->ren, &ui->text, cx, ui->h - 30, instructions2);
    }

    SDL_RenderPresent(ui->ren);
}

UiMenuAction ui_sdl_poll_sound_settings(UiSdl *ui, const Settings *settings, int *out_quit)
{
    *out_quit = 0;

    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_QUIT)
        {
            *out_quit = 1;
            return UI_MENU_NONE;
        }
        if (e.type == SDL_KEYDOWN)
        {
            SDL_Keycode key = e.key.keysym.sym;

            // Fixed keys
            if (key == SDLK_ESCAPE)
            {
                return UI_MENU_BACK;
            }
            if (key == SDLK_RETURN || key == SDLK_KP_ENTER)
            {
                return UI_MENU_SELECT;
            }

            // Dynamic navigation using Player 1's bindings
            int action = settings_find_action(settings, 0, key);
            if (action == SETTING_ACTION_UP)
                return UI_MENU_UP;
            if (action == SETTING_ACTION_DOWN)
                return UI_MENU_DOWN;
            if (action == SETTING_ACTION_LEFT)
                return UI_MENU_LEFT;
            if (action == SETTING_ACTION_RIGHT)
                return UI_MENU_RIGHT;
        }
    }

    SDL_GetWindowSize(ui->win, &ui->w, &ui->h);
    return UI_MENU_NONE;
}

void ui_sdl_render_game_over(UiSdl *ui, int score, int fruits, int time_seconds, int combo_best, const Scoreboard *sb, int selected_index)
{
    SET_COLOR_BG_MENU(ui->ren);
    SDL_RenderClear(ui->ren);

    if (ui->text_ok)
    {
        // Center - Stats and Menu
        int cx = ui->w / 2;
        int y = ui->h / 2 - 150;

        // Title
        ui_draw_text_centered(ui->ren, &ui->text, cx, y, "GAME OVER");
        y += 60;

        // Stats
        char line[128];
        snprintf(line, sizeof(line), "Score: %d", score);
        ui_draw_text_centered(ui->ren, &ui->text, cx, y, line);
        y += 32;

        snprintf(line, sizeof(line), "Fruits eaten: %d", fruits);
        ui_draw_text_centered(ui->ren, &ui->text, cx, y, line);
        y += 32;

        int minutes = time_seconds / 60;
        int seconds = time_seconds % 60;
        snprintf(line, sizeof(line), "Time survived: %d:%02d", minutes, seconds);
        ui_draw_text_centered(ui->ren, &ui->text, cx, y, line);
        y += 32;

        // Combo stats
        int combo_tier = game_get_combo_tier(combo_best);
        snprintf(line, sizeof(line), "Best combo: %dx (Tier %d)", combo_best, combo_tier);
        ui_draw_text_centered(ui->ren, &ui->text, cx, y, line);
        y += 60;

        // Menu options
        const char *items[] = {"Try again", "Quit"};
        for (int i = 0; i < 2; ++i)
        {
            ui_draw_menu_item(ui->ren, &ui->text, cx, y + i * 32, items[i], i == selected_index);
        }

        // Right side - Scoreboard
        int right_x = (ui->w * 3) / 4;
        int sb_y = ui->h / 2 - 150;

        ui_draw_text_centered(ui->ren, &ui->text, right_x, sb_y, "HIGH SCORES");
        sb_y += 50;

        // Display top 5 scores
        int display_count = sb->count < 5 ? sb->count : 5;
        for (int i = 0; i < display_count; ++i)
        {
            char row[128];
            snprintf(row, sizeof(row), "%d) %s - %d", i + 1,
                     sb->entries[i].name, sb->entries[i].score);
            ui_draw_text_centered(ui->ren, &ui->text, right_x, sb_y + i * 32, row);
        }

        // Show message if no entries
        if (display_count == 0)
        {
            ui_draw_text_centered(ui->ren, &ui->text, right_x, sb_y, "No scores yet");
        }
    }

    SDL_RenderPresent(ui->ren);
}

UiMenuAction ui_sdl_poll_game_over(UiSdl *ui, const Settings *settings, int *out_quit)
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_QUIT)
        {
            *out_quit = 1;
            return UI_MENU_NONE;
        }
        if (e.type == SDL_KEYDOWN)
        {
            SDL_Keycode key = e.key.keysym.sym;

            // Enter to select
            if (key == SDLK_RETURN || key == SDLK_KP_ENTER)
            {
                return UI_MENU_SELECT;
            }

            // Dynamic navigation using Player 1's bindings
            int action = settings_find_action(settings, 0, key);
            if (action == SETTING_ACTION_UP)
                return UI_MENU_UP;
            if (action == SETTING_ACTION_DOWN)
                return UI_MENU_DOWN;
        }
    }

    SDL_GetWindowSize(ui->win, &ui->w, &ui->h);
    return UI_MENU_NONE;
}

// ============================================================================
// Online Multiplayer UI Functions
// ============================================================================

#include "online_multiplayer.h"

// Helper functions for text rendering with colors (simplified)
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

void ui_sdl_render_multiplayer_online_menu(UiSdl *ui, int selected_index)
{
    SDL_SetRenderDrawColor(ui->ren, 0, 0, 0, 255);
    SDL_RenderClear(ui->ren);

    const char *title = "Online Multiplayer";
    const char *options[] = {"Host Game", "Join Game", "Back"};
    int option_count = 3;

    // Draw title
    text_sdl_draw_centered(ui->text, ui->ren, title, ui->w / 2, ui->h / 4, 1.5f, 255, 255, 255);

    // Draw options
    for (int i = 0; i < option_count; i++)
    {
        int is_selected = (i == selected_index);
        int y = ui->h / 2 + i * 40;

        // Draw selection indicator
        if (is_selected)
        {
            char indicator[128];
            snprintf(indicator, sizeof(indicator), "> %s <", options[i]);
            text_sdl_draw_centered(ui->text, ui->ren, indicator, ui->w / 2, y, 1.0f, 255, 255, 0);
        }
        else
        {
            text_sdl_draw_centered(ui->text, ui->ren, options[i], ui->w / 2, y, 1.0f, 180, 180, 180);
        }
    }

    SDL_RenderPresent(ui->ren);
}

UiMenuAction ui_sdl_poll_multiplayer_online_menu(UiSdl *ui, int *out_quit)
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_QUIT)
        {
            *out_quit = 1;
            return UI_MENU_NONE;
        }
        if (e.type == SDL_KEYDOWN)
        {
            SDL_Keycode key = e.key.keysym.sym;
            if (key == SDLK_ESCAPE)
                return UI_MENU_BACK;
            if (key == SDLK_UP || key == SDLK_w)
                return UI_MENU_UP;
            if (key == SDLK_DOWN || key == SDLK_s)
                return UI_MENU_DOWN;
            if (key == SDLK_RETURN || key == SDLK_KP_ENTER)
                return UI_MENU_SELECT;
        }
    }
    return UI_MENU_NONE;
}

void ui_sdl_render_host_setup(UiSdl *ui, int selected_index)
{
    SDL_SetRenderDrawColor(ui->ren, 0, 0, 0, 255);
    SDL_RenderClear(ui->ren);

    const char *title = "Host Game";
    const char *prompt = "Private Game?";
    const char *options[] = {"Yes", "No"};
    int option_count = 2;

    text_sdl_draw_centered(ui->text, ui->ren, title, ui->w / 2, ui->h / 4, 1.5f, 255, 255, 255);
    text_sdl_draw_centered(ui->text, ui->ren, prompt, ui->w / 2, ui->h / 3, 1.0f, 200, 200, 200);

    for (int i = 0; i < option_count; i++)
    {
        int is_selected = (i == selected_index);
        int y = ui->h / 2 + i * 40;

        // Draw selection indicator
        if (is_selected)
        {
            char indicator[128];
            snprintf(indicator, sizeof(indicator), "> %s <", options[i]);
            text_sdl_draw_centered(ui->text, ui->ren, indicator, ui->w / 2, y, 1.0f, 255, 255, 0);
        }
        else
        {
            text_sdl_draw_centered(ui->text, ui->ren, options[i], ui->w / 2, y, 1.0f, 180, 180, 180);
        }
    }

    SDL_RenderPresent(ui->ren);
}

UiMenuAction ui_sdl_poll_host_setup(UiSdl *ui, int *out_quit)
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_QUIT)
        {
            *out_quit = 1;
            return UI_MENU_NONE;
        }
        if (e.type == SDL_KEYDOWN)
        {
            SDL_Keycode key = e.key.keysym.sym;
            if (key == SDLK_ESCAPE)
                return UI_MENU_BACK;
            if (key == SDLK_UP || key == SDLK_w)
                return UI_MENU_UP;
            if (key == SDLK_DOWN || key == SDLK_s)
                return UI_MENU_DOWN;
            if (key == SDLK_RETURN || key == SDLK_KP_ENTER)
                return UI_MENU_SELECT;
        }
    }
    return UI_MENU_NONE;
}

void ui_sdl_render_join_select(UiSdl *ui, int selected_index)
{
    SDL_SetRenderDrawColor(ui->ren, 0, 0, 0, 255);
    SDL_RenderClear(ui->ren);

    const char *title = "Join Game";
    const char *prompt = "Join Type:";
    const char *options[] = {"Public", "Private"};
    int option_count = 2;

    text_sdl_draw_centered(ui->text, ui->ren, title, ui->w / 2, ui->h / 4, 1.5f, 255, 255, 255);
    text_sdl_draw_centered(ui->text, ui->ren, prompt, ui->w / 2, ui->h / 3, 1.0f, 200, 200, 200);

    for (int i = 0; i < option_count; i++)
    {
        int is_selected = (i == selected_index);
        int y = ui->h / 2 + i * 40;

        if (is_selected)
        {
            char indicator[128];
            snprintf(indicator, sizeof(indicator), "> %s <", options[i]);
            text_sdl_draw_centered(ui->text, ui->ren, indicator, ui->w / 2, y, 1.0f, 255, 255, 0);
        }
        else
        {
            text_sdl_draw_centered(ui->text, ui->ren, options[i], ui->w / 2, y, 1.0f, 180, 180, 180);
        }
    }

    SDL_RenderPresent(ui->ren);
}

UiMenuAction ui_sdl_poll_join_select(UiSdl *ui, int *out_quit)
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_QUIT)
        {
            *out_quit = 1;
            return UI_MENU_NONE;
        }
        if (e.type == SDL_KEYDOWN)
        {
            SDL_Keycode key = e.key.keysym.sym;
            if (key == SDLK_ESCAPE)
                return UI_MENU_BACK;
            if (key == SDLK_UP || key == SDLK_w)
                return UI_MENU_UP;
            if (key == SDLK_DOWN || key == SDLK_s)
                return UI_MENU_DOWN;
            if (key == SDLK_RETURN || key == SDLK_KP_ENTER)
                return UI_MENU_SELECT;
        }
    }
    return UI_MENU_NONE;
}

void ui_sdl_render_lobby_browser(UiSdl *ui, json_t *lobby_list, int selected_index)
{
    SDL_SetRenderDrawColor(ui->ren, 0, 0, 0, 255);
    SDL_RenderClear(ui->ren);

    const char *title = "Public Lobbies";
    text_sdl_draw_centered(ui->text, ui->ren, title, ui->w / 2, 50, 1.5f, 255, 255, 255);

    if (!lobby_list || json_array_size(lobby_list) == 0)
    {
        text_sdl_draw_centered(ui->text, ui->ren, "No public lobbies available", ui->w / 2, ui->h / 2, 1.0f, 180, 180, 180);
        text_sdl_draw_centered(ui->text, ui->ren, "Press ESC to go back", ui->w / 2, ui->h / 2 + 40, 0.8f, 150, 150, 150);
    }
    else
    {
        size_t count = json_array_size(lobby_list);
        int start_y = 120;

        for (size_t i = 0; i < count && i < 10; i++)  // Show max 10 lobbies
        {
            json_t *lobby = json_array_get(lobby_list, i);
            json_t *session_json = json_object_get(lobby, "session");
            json_t *name_json = json_object_get(lobby, "name");
            json_t *players_json = json_object_get(lobby, "players");

            const char *session = json_is_string(session_json) ? json_string_value(session_json) : "???";
            const char *name = json_is_string(name_json) ? json_string_value(name_json) : "Unknown";
            int players = json_is_integer(players_json) ? json_integer_value(players_json) : 0;

            char lobby_text[256];
            snprintf(lobby_text, sizeof(lobby_text), "%s (%.6s) - %d players", name, session, players);

            int is_selected = ((int)i == selected_index);
            int y = start_y + (int)i * 35;

            if (is_selected)
            {
                char indicator[300];
                snprintf(indicator, sizeof(indicator), "> %s <", lobby_text);
                text_sdl_draw_centered(ui->text, ui->ren, indicator, ui->w / 2, y, 0.9f, 255, 255, 0);
            }
            else
            {
                text_sdl_draw_centered(ui->text, ui->ren, lobby_text, ui->w / 2, y, 0.9f, 180, 180, 180);
            }
        }

        text_sdl_draw_centered(ui->text, ui->ren, "ENTER to join | ESC to go back", ui->w / 2, ui->h - 50, 0.8f, 150, 150, 150);
    }

    SDL_RenderPresent(ui->ren);
}

UiMenuAction ui_sdl_poll_lobby_browser(UiSdl *ui, int *out_quit)
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_QUIT)
        {
            *out_quit = 1;
            return UI_MENU_NONE;
        }
        if (e.type == SDL_KEYDOWN)
        {
            SDL_Keycode key = e.key.keysym.sym;
            if (key == SDLK_ESCAPE)
                return UI_MENU_BACK;
            if (key == SDLK_UP || key == SDLK_w)
                return UI_MENU_UP;
            if (key == SDLK_DOWN || key == SDLK_s)
                return UI_MENU_DOWN;
            if (key == SDLK_RETURN || key == SDLK_KP_ENTER)
                return UI_MENU_SELECT;
        }
    }
    return UI_MENU_NONE;
}

void ui_sdl_render_error(UiSdl *ui, const char *message)
{
    SDL_SetRenderDrawColor(ui->ren, 40, 0, 0, 255);
    SDL_RenderClear(ui->ren);

    text_sdl_draw_centered(ui->text, ui->ren, "Error", ui->w / 2, ui->h / 3, 1.5f, 255, 100, 100);
    text_sdl_draw_centered(ui->text, ui->ren, message, ui->w / 2, ui->h / 2, 1.0f, 255, 200, 200);

    SDL_RenderPresent(ui->ren);
}

int ui_sdl_get_session_id(UiSdl *ui, char *out_session_id, int out_size)
{
    // Simple text input for session ID (6 characters)
    SDL_Event e;
    char buffer[8] = {0};
    int cursor = 0;
    int done = 0;
    int canceled = 0;

    SDL_StartTextInput();

    while (!done)
    {
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
            {
                canceled = 1;
                done = 1;
            }
            else if (e.type == SDL_KEYDOWN)
            {
                if (e.key.keysym.sym == SDLK_ESCAPE)
                {
                    canceled = 1;
                    done = 1;
                }
                else if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_KP_ENTER)
                {
                    if (cursor == 6) // Session ID must be 6 characters
                    {
                        done = 1;
                    }
                }
                else if (e.key.keysym.sym == SDLK_BACKSPACE && cursor > 0)
                {
                    cursor--;
                    buffer[cursor] = '\0';
                }
            }
            else if (e.type == SDL_TEXTINPUT)
            {
                char c = e.text.text[0];
                if (cursor < 6 && ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')))
                {
                    // Convert to uppercase for case-insensitive session IDs
                    if (c >= 'a' && c <= 'z')
                    {
                        c = c - 'a' + 'A';
                    }
                    buffer[cursor++] = c;
                    buffer[cursor] = '\0';
                }
            }
        }

        // Render
        SDL_SetRenderDrawColor(ui->ren, 0, 0, 0, 255);
        SDL_RenderClear(ui->ren);

        text_sdl_draw_centered(ui->text, ui->ren, "Join Game", ui->w / 2, ui->h / 4, 1.5f, 255, 255, 255);
        text_sdl_draw_centered(ui->text, ui->ren, "Enter Session ID (6 characters):", ui->w / 2, ui->h / 3, 1.0f, 200, 200, 200);

        char display[32];
        snprintf(display, sizeof(display), "> %s_", buffer);
        text_sdl_draw_centered(ui->text, ui->ren, display, ui->w / 2, ui->h / 2, 1.2f, 255, 255, 0);

        text_sdl_draw_centered(ui->text, ui->ren, "Press ENTER when done, ESC to cancel", ui->w / 2, ui->h * 3 / 4, 0.8f, 150, 150, 150);

        SDL_RenderPresent(ui->ren);
        SDL_Delay(16);
    }

    SDL_StopTextInput();

    if (canceled)
        return 0;

    strncpy(out_session_id, buffer, out_size - 1);
    out_session_id[out_size - 1] = '\0';
    return 1;
}

void ui_sdl_render_online_lobby(UiSdl *ui, const OnlineMultiplayerContext *ctx)
{
    const MultiplayerGame_s *mg = ctx->game;

    // Compute board layout (centered with border) - same as game
    int ox, oy;
    compute_layout(ui, &mg->board, &ox, &oy);

    int board_w = mg->board.width;
    int board_h = mg->board.height;

    // Background
    SET_COLOR_BG_DARK(ui->ren);
    SDL_RenderClear(ui->ren);

    // Board background
    SDL_Rect board_bg;
    board_bg.x = ox;
    board_bg.y = oy;
    board_bg.w = (board_w + 2) * ui->cell;
    board_bg.h = (board_h + 2) * ui->cell;
    ui_draw_filled_rect(ui->ren, board_bg.x, board_bg.y, board_bg.w, board_bg.h,
                        COLOR_BG_BOARD_R, COLOR_BG_BOARD_G, COLOR_BG_BOARD_B);

    // White border frame
    ui_draw_filled_rect(ui->ren, ox, oy, (board_w + 2) * ui->cell, ui->cell,
                        COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B);
    ui_draw_filled_rect(ui->ren, ox, oy + (board_h + 1) * ui->cell,
                        (board_w + 2) * ui->cell, ui->cell,
                        COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B);
    ui_draw_filled_rect(ui->ren, ox, oy, ui->cell, (board_h + 2) * ui->cell,
                        COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B);
    ui_draw_filled_rect(ui->ren, ox + (board_w + 1) * ui->cell, oy,
                        ui->cell, (board_h + 2) * ui->cell,
                        COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B);

    // Player colors (head, body)
    typedef struct
    {
        SDL_Color head;
        SDL_Color body;
    } PlayerColor;
    PlayerColor player_colors[MAX_PLAYERS] = {
        {{60, 220, 120, 255}, {35, 160, 90, 255}},  // Green
        {{60, 160, 255, 255}, {30, 100, 200, 255}}, // Blue
        {{255, 220, 60, 255}, {200, 170, 30, 255}}, // Yellow
        {{255, 60, 220, 255}, {200, 30, 160, 255}}  // Magenta
    };

    // Render snakes for players who are ready
    for (int p = 0; p < MAX_PLAYERS; p++)
    {
        if (!mg->players[p].joined || !mg->players[p].ready)
            continue;

        const Snake *snake = &mg->players[p].snake;
        PlayerColor colors = player_colors[p];

        for (int i = 0; i < snake->length; i++)
        {
            int cell_x = ox + (1 + snake->segments[i].x) * ui->cell;
            int cell_y = oy + (1 + snake->segments[i].y) * ui->cell;

            if (i == 0)
            {
                ui_draw_filled_rect_with_outline(ui->ren, cell_x, cell_y, ui->cell, ui->cell,
                                    colors.head.r, colors.head.g, colors.head.b);
            }
            else
            {
                ui_draw_filled_rect_with_outline(ui->ren, cell_x, cell_y, ui->cell, ui->cell,
                                    colors.body.r, colors.body.g, colors.body.b);
            }
        }
    }

    // HUD - show player lobby status
    if (ui->text_ok)
    {
        const int pad = 10;
        const PlayerColor *colors = (const PlayerColor *)PLAYER_COLORS;

        // Show session ID at top center
        char session_text[64];
        snprintf(session_text, sizeof(session_text), "Session ID: %s", ctx->game->session_id);
        text_draw_center(ui->ren, &ui->text, ui->w / 2, 20, session_text);

        for (int p = 0; p < MAX_PLAYERS; p++)
        {
            if (!mg->players[p].joined)
                continue;

            // Corner anchors (x,y)
            int x, y;
            switch (p)
            {
            case 0: // P1: top-left
                x = pad;
                y = 50;
                break;
            case 1: // P2: top-right
                x = ui->w - 150;
                y = 50;
                break;
            case 2: // P3: bottom-left
                x = pad;
                y = ui->h - 80;
                break;
            case 3: // P4: bottom-right
                x = ui->w - 150;
                y = ui->h - 80;
                break;
            }

            char hud1[64], hud2[64], hud3[64];

            snprintf(hud1, sizeof(hud1), "%s%s", mg->players[p].name,
                     mg->players[p].is_local_player ? " (YOU)" : "");
            snprintf(hud2, sizeof(hud2), "Wins: %d", mg->players[p].wins);
            snprintf(hud3, sizeof(hud3), "%s", mg->players[p].ready ? "READY" : "Not Ready");

            text_draw(ui->ren, &ui->text, x, y, hud1);
            text_draw(ui->ren, &ui->text, x, y + 18, hud2);
            text_draw(ui->ren, &ui->text, x, y + 36, hud3);
        }

        // Instructions at bottom center
        text_draw_center(ui->ren, &ui->text, ui->w / 2, ui->h / 2,
                         "USE key: Toggle Ready | ESC: Leave");
        const char *hint = ctx->game->is_host ? "ENTER: Start (when all ready)" : "Waiting for host...";
        text_draw_center(ui->ren, &ui->text, ui->w / 2, ui->h / 2 - 30, hint);
    }

    SDL_RenderPresent(ui->ren);
}

UiMenuAction ui_sdl_poll_online_lobby(UiSdl *ui, const Settings *settings, int *out_quit)
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_QUIT)
        {
            *out_quit = 1;
            return UI_MENU_NONE;
        }
        if (e.type == SDL_KEYDOWN)
        {
            if (e.key.keysym.sym == SDLK_ESCAPE)
                return UI_MENU_BACK;
            if (e.key.keysym.sym == SDLK_RETURN)
                return UI_MENU_SELECT; // Host starts game with ENTER

            // Check for USE key (player 0's USE binding)
            if (e.key.keysym.sym == settings->keybindings[0][SETTING_ACTION_USE])
                return UI_MENU_USE; // Toggle ready
        }
    }
    return UI_MENU_NONE;
}

void ui_sdl_render_online_countdown(UiSdl *ui, const OnlineMultiplayerContext *ctx, int countdown)
{
    const MultiplayerGame_s *mg = ctx->game;

    // Render the game board first (same as lobby/game rendering)
    int ox, oy;
    compute_layout(ui, &mg->board, &ox, &oy);

    int board_w = mg->board.width;
    int board_h = mg->board.height;

    // Background
    SET_COLOR_BG_DARK(ui->ren);
    SDL_RenderClear(ui->ren);

    // Board background
    SDL_Rect board_bg;
    board_bg.x = ox;
    board_bg.y = oy;
    board_bg.w = (board_w + 2) * ui->cell;
    board_bg.h = (board_h + 2) * ui->cell;
    ui_draw_filled_rect(ui->ren, board_bg.x, board_bg.y, board_bg.w, board_bg.h,
                        COLOR_BG_BOARD_R, COLOR_BG_BOARD_G, COLOR_BG_BOARD_B);

    // White border frame
    ui_draw_filled_rect(ui->ren, ox, oy, (board_w + 2) * ui->cell, ui->cell,
                        COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B);
    ui_draw_filled_rect(ui->ren, ox, oy + (board_h + 1) * ui->cell,
                        (board_w + 2) * ui->cell, ui->cell,
                        COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B);
    ui_draw_filled_rect(ui->ren, ox, oy, ui->cell, (board_h + 2) * ui->cell,
                        COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B);
    ui_draw_filled_rect(ui->ren, ox + (board_w + 1) * ui->cell, oy,
                        ui->cell, (board_h + 2) * ui->cell,
                        COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B);

    // Player colors
    typedef struct
    {
        SDL_Color head;
        SDL_Color body;
    } PlayerColor;
    PlayerColor player_colors[MAX_PLAYERS] = {
        {{60, 220, 120, 255}, {35, 160, 90, 255}},
        {{60, 160, 255, 255}, {30, 100, 200, 255}},
        {{255, 220, 60, 255}, {200, 170, 30, 255}},
        {{255, 60, 220, 255}, {200, 30, 160, 255}}
    };

    // Render snakes
    for (int p = 0; p < MAX_PLAYERS; p++)
    {
        if (!mg->players[p].joined)
            continue;

        const Snake *snake = &mg->players[p].snake;
        PlayerColor colors = player_colors[p];

        for (int i = 0; i < snake->length; i++)
        {
            int cell_x = ox + (1 + snake->segments[i].x) * ui->cell;
            int cell_y = oy + (1 + snake->segments[i].y) * ui->cell;

            if (i == 0)
            {
                ui_draw_filled_rect_with_outline(ui->ren, cell_x, cell_y, ui->cell, ui->cell,
                                    colors.head.r, colors.head.g, colors.head.b);
            }
            else
            {
                ui_draw_filled_rect_with_outline(ui->ren, cell_x, cell_y, ui->cell, ui->cell,
                                    colors.body.r, colors.body.g, colors.body.b);
            }
        }
    }

    // Overlay semi-transparent background for countdown
    SDL_SetRenderDrawBlendMode(ui->ren, SDL_BLENDMODE_BLEND);
    ui_draw_filled_rect_alpha(ui->ren, 0, 0, ui->w, ui->h, 0, 0, 0, 128);

    // Render countdown text on top
    if (ui->text_ok)
    {
        char countdown_text[32];
        if (countdown > 0)
        {
            snprintf(countdown_text, sizeof(countdown_text), "%d", countdown);
        }
        else
        {
            snprintf(countdown_text, sizeof(countdown_text), "GO!");
        }
        text_draw_center(ui->ren, &ui->text, ui->w / 2, ui->h / 2, countdown_text);
    }

    SDL_RenderPresent(ui->ren);
}

void ui_sdl_render_online_game(UiSdl *ui, const OnlineMultiplayerContext *ctx)
{
    const MultiplayerGame_s *mg = ctx->game;

    // Compute board layout (centered with border) - same as singleplayer
    int ox, oy;
    compute_layout(ui, &mg->board, &ox, &oy);

    int board_w = mg->board.width;
    int board_h = mg->board.height;

    // Board bounds for mp HUD (in pixels)
    const int pad = 10;
    const int left = ox;
    const int top = oy;
    const int right = ox + (board_w + 2) * ui->cell;
    const int bottom = oy + (board_h + 2) * ui->cell;

    // Background
    SET_COLOR_BG_DARK(ui->ren);
    SDL_RenderClear(ui->ren);

    // Board background
    SDL_Rect board_bg;
    board_bg.x = ox;
    board_bg.y = oy;
    board_bg.w = (board_w + 2) * ui->cell;
    board_bg.h = (board_h + 2) * ui->cell;
    ui_draw_filled_rect(ui->ren, board_bg.x, board_bg.y, board_bg.w, board_bg.h,
                        COLOR_BG_BOARD_R, COLOR_BG_BOARD_G, COLOR_BG_BOARD_B);

    // White border frame
    // Top border
    ui_draw_filled_rect(ui->ren, ox, oy, (board_w + 2) * ui->cell, ui->cell,
                        COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B);

    // Bottom border
    ui_draw_filled_rect(ui->ren, ox, oy + (board_h + 1) * ui->cell,
                        (board_w + 2) * ui->cell, ui->cell,
                        COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B);

    // Left border
    ui_draw_filled_rect(ui->ren, ox, oy, ui->cell, (board_h + 2) * ui->cell,
                        COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B);

    // Right border
    ui_draw_filled_rect(ui->ren, ox + (board_w + 1) * ui->cell, oy,
                        ui->cell, (board_h + 2) * ui->cell,
                        COLOR_BORDER_R, COLOR_BORDER_G, COLOR_BORDER_B);

    // Render food (orange like singleplayer)
    ui_draw_filled_rect(ui->ren, ox + (1 + mg->board.food.x) * ui->cell,
                        oy + (1 + mg->board.food.y) * ui->cell,
                        ui->cell, ui->cell, COLOR_FOOD_R, COLOR_FOOD_G, COLOR_FOOD_B);

    // Render extra food items
    for (int i = 0; i < mg->food_count; i++)
    {
        ui_draw_filled_rect(ui->ren, ox + (1 + mg->food[i].x) * ui->cell,
                            oy + (1 + mg->food[i].y) * ui->cell,
                            ui->cell, ui->cell, COLOR_FOOD_R, COLOR_FOOD_G, COLOR_FOOD_B);
    }

    // Player colors (head, body)
    typedef struct
    {
        SDL_Color head;
        SDL_Color body;
    } PlayerColor;
    PlayerColor player_colors[MAX_PLAYERS] = {
        {{60, 220, 120, 255}, {35, 160, 90, 255}},  // Green
        {{60, 160, 255, 255}, {30, 100, 200, 255}}, // Blue
        {{255, 220, 60, 255}, {200, 170, 30, 255}}, // Yellow
        {{255, 60, 220, 255}, {200, 30, 160, 255}}  // Magenta
    };

    // Render players' snakes
    for (int p = 0; p < MAX_PLAYERS; p++)
    {
        if (!mg->players[p].joined)
            continue;

        const Snake *snake = &mg->players[p].snake;
        PlayerColor colors = player_colors[p];

        for (int i = 0; i < snake->length; i++)
        {
            int cell_x = ox + (1 + snake->segments[i].x) * ui->cell;
            int cell_y = oy + (1 + snake->segments[i].y) * ui->cell;

            if (i == 0)
            {
                ui_draw_filled_rect_with_outline(ui->ren, cell_x, cell_y, ui->cell, ui->cell,
                                    colors.head.r, colors.head.g, colors.head.b);
            }
            else
            {
                ui_draw_filled_rect_with_outline(ui->ren, cell_x, cell_y, ui->cell, ui->cell,
                                    colors.body.r, colors.body.g, colors.body.b);
            }
        }
    }

    // HUD - show player info
    if (ui->text_ok)
    {
        int hud_y = oy - 40;

        for (int p = 0; p < MAX_PLAYERS; p++)
        {
            if (!mg->players[p].joined)
                continue;

            // Corner anchors (x,y)
            int x = left, y = top; // default
            switch (p)
            {
            case 0: // P1: top-left
                x = pad;
                y = pad;
                break;
            case 1: // P2: top-right
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

            snprintf(hud1, sizeof(hud1), "%s%s", mg->players[p].name,
                     mg->players[p].is_local_player ? " (YOU)" : "");
            snprintf(hud2, sizeof(hud2), "Score: %d | Wins: %d",
                     mg->players[p].score, mg->players[p].wins);

            if (mg->players[p].is_local_player)
            {
                snprintf(hud3, sizeof(hud3), "Lives: %d | Combo x%d",
                         mg->players[p].lives, mg->players[p].combo_count);
            }
            else
            {
                snprintf(hud3, sizeof(hud3), "Lives: %d", mg->players[p].lives);
            }

            text_draw(ui->ren, &ui->text, x, y, hud1);
            text_draw(ui->ren, &ui->text, x, y + 18, hud2);
            text_draw(ui->ren, &ui->text, x, y + 36, hud3);

            // Draw combo bar if player has an active combo
            if (mg->players[p].combo_count > 0 && mg->players[p].combo_expiry_time > 0)
            {
                unsigned int current_time = SDL_GetTicks();
                int bar_width = 150;
                int bar_height = 6;
                int bar_y = y + 54;

                // Calculate time remaining
                float time_remaining = 0.0f;
                if (current_time < mg->players[p].combo_expiry_time) {
                    time_remaining = (float)(mg->players[p].combo_expiry_time - current_time) / (float)mg->combo_window_ms;
                }

                // Clamp to [0, 1]
                if (time_remaining < 0.0f) time_remaining = 0.0f;
                if (time_remaining > 1.0f) time_remaining = 1.0f;

                int filled_width = (int)(bar_width * time_remaining);

                // Draw bar background (dark gray)
                SDL_Rect bar_bg = {x, bar_y, bar_width, bar_height};
                SDL_SetRenderDrawColor(ui->ren, 40, 40, 40, 255);
                SDL_RenderFillRect(ui->ren, &bar_bg);

                // Draw filled portion (based on player color from multiplayer_game.h)
                if (filled_width > 0) {
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

Direction ui_sdl_poll_online_game_input(UiSdl *ui, const Settings *settings, int *out_quit)
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_QUIT)
        {
            *out_quit = 1;
            return DIR_UP; // Dummy
        }
        if (e.type == SDL_KEYDOWN)
        {
            SDL_Keycode key = e.key.keysym.sym;

            // Use Player 1's keybindings
            int action = settings_find_action(settings, 0, key);
            if (action == SETTING_ACTION_UP)
                return DIR_UP;
            if (action == SETTING_ACTION_DOWN)
                return DIR_DOWN;
            if (action == SETTING_ACTION_LEFT)
                return DIR_LEFT;
            if (action == SETTING_ACTION_RIGHT)
                return DIR_RIGHT;
        }
    }
    return -1; // No input
}

void ui_sdl_render_online_gameover(UiSdl *ui, const OnlineMultiplayerContext *ctx)
{
    SDL_SetRenderDrawColor(ui->ren, 0, 0, 0, 255);
    SDL_RenderClear(ui->ren);

    text_sdl_draw_centered(ui->text, ui->ren, "GAME OVER", ui->w / 2, ui->h / 4, 2.0f, 255, 0, 0);

    // Find the winner (player who is alive or has highest score)
    int winner_idx = -1;
    int highest_score = -1;
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (ctx->game->players[i].joined)
        {
            if (ctx->game->players[i].alive ||
                (winner_idx == -1 && ctx->game->players[i].score > highest_score))
            {
                winner_idx = i;
                highest_score = ctx->game->players[i].score;
            }
        }
    }

    // Show final standings
    int y = ui->h / 3 + 40;
    const PlayerColor *colors = (const PlayerColor *)PLAYER_COLORS;
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (ctx->game->players[i].joined)
        {
            char player_text[128];
            const char *name_suffix = ctx->game->players[i].is_local_player ? " (YOU)" : "";
            int is_winner = (i == winner_idx);

            if (is_winner) {
                snprintf(player_text, sizeof(player_text), "%s%s: Score %d, Combo Best %d  << WINNER! >>",
                         ctx->game->players[i].name, name_suffix,
                         ctx->game->players[i].score, ctx->game->players[i].combo_best);
                // Draw winner in bright gold/yellow
                text_sdl_draw(ui->text, ui->ren, player_text, ui->w / 2 - 250, y, 1.0f, 255, 215, 0);
            } else {
                snprintf(player_text, sizeof(player_text), "%s%s: Score %d, Combo Best %d",
                         ctx->game->players[i].name, name_suffix,
                         ctx->game->players[i].score, ctx->game->players[i].combo_best);
                text_sdl_draw(ui->text, ui->ren, player_text, ui->w / 2 - 200, y, 0.9f,
                              colors[i].r, colors[i].g, colors[i].b);
            }
            y += 30;
        }
    }

    SDL_RenderPresent(ui->ren);
}

UiMenuAction ui_sdl_poll_online_gameover(UiSdl *ui, int *out_quit)
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_QUIT)
        {
            *out_quit = 1;
            return UI_MENU_NONE;
        }
        if (e.type == SDL_KEYDOWN)
        {
            return UI_MENU_SELECT; // Any key continues
        }
    }
    return UI_MENU_NONE;
}