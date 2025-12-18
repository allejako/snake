#include "ui_sdl.h"

#define MIN_CELL_SIZE 8
#define MAX_CELL_SIZE 40
#define BOARD_BORDER_CELLS 2 // 1-cell border on each side
#define LAYOUT_PADDING_CELLS 4
#define DEFAULT_FONT_SIZE 18

static void compute_layout(UiSdl *ui, const Game *g, int *out_origin_x, int *out_origin_y)
{
    // Draw a board with 1-cell border, but in pixels
    int board_cells_w = g->board.width + BOARD_BORDER_CELLS;
    int board_cells_h = g->board.height + BOARD_BORDER_CELLS;

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
    *out_origin_y = (ui->h - board_px_h) / 2;
    if (*out_origin_x < ui->pad)
        *out_origin_x = ui->pad;
    if (*out_origin_y < ui->pad)
        *out_origin_y = ui->pad;
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
    if (text_init(&ui->text, "assets/fonts/PTF-NORDIC-Rnd.ttf", DEFAULT_FONT_SIZE))
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
void ui_sdl_draw_game(UiSdl *ui, const Game *g, const char *player_name)
{
    int ox, oy;
    compute_layout(ui, g, &ox, &oy);

    // bakgrund
    SDL_SetRenderDrawColor(ui->ren, 15, 15, 18, 255);
    SDL_RenderClear(ui->ren);

    // board area (bakgrund)
    SDL_Rect board_bg = cell_rect(ui, ox, oy, 0, 0);
    board_bg.w = (g->board.width + 2) * ui->cell;
    board_bg.h = (g->board.height + 2) * ui->cell;

    SDL_SetRenderDrawColor(ui->ren, 25, 25, 30, 255);
    SDL_RenderFillRect(ui->ren, &board_bg);

    // ram (1-cell tjock)
    SDL_SetRenderDrawColor(ui->ren, 220, 220, 220, 255);

    // top
    SDL_Rect top = cell_rect(ui, ox, oy, 0, 0);
    top.w = (g->board.width + 2) * ui->cell;
    SDL_RenderFillRect(ui->ren, &top);

    // bottom
    SDL_Rect bottom = cell_rect(ui, ox, oy, 0, g->board.height + 1);
    bottom.w = (g->board.width + 2) * ui->cell;
    SDL_RenderFillRect(ui->ren, &bottom);

    // left
    SDL_Rect left = cell_rect(ui, ox, oy, 0, 0);
    left.h = (g->board.height + 2) * ui->cell;
    SDL_RenderFillRect(ui->ren, &left);

    // right
    SDL_Rect right = cell_rect(ui, ox, oy, g->board.width + 1, 0);
    right.h = (g->board.height + 2) * ui->cell;
    SDL_RenderFillRect(ui->ren, &right);

    // Food (in board coords -> +1 for border)
    SDL_Rect food = cell_rect(ui, ox, oy, 1 + g->board.food.x, 1 + g->board.food.y);
    SDL_SetRenderDrawColor(ui->ren, 240, 180, 40, 255);
    SDL_RenderFillRect(ui->ren, &food);

    // Snake segments
    for (int i = 0; i < g->snake.length; ++i)
    {
        Vec2 seg = g->snake.segments[i];
        SDL_Rect sr = cell_rect(ui, ox, oy, 1 + seg.x, 1 + seg.y);

        if (i == 0)
            SDL_SetRenderDrawColor(ui->ren, 60, 220, 120, 255); // head
        else
            SDL_SetRenderDrawColor(ui->ren, 35, 160, 90, 255); // body

        SDL_RenderFillRect(ui->ren, &sr);
    }

    if (ui->text_ok)
    {
        char hud[128];
        snprintf(hud, sizeof(hud), "Score: %d", g->score);
        text_draw(ui->ren, &ui->text, ox, oy - 28, hud);

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
void ui_sdl_render(UiSdl *ui, const Game *g, const char *player_name)
{
    ui_sdl_draw_game(ui, g, player_name);
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

        SDL_SetRenderDrawColor(ui->ren, 10, 10, 12, 255);
        SDL_RenderClear(ui->ren);

        int cx = ui->w / 2;
        int cy = ui->h / 2;

        // Dark overlay box
        SDL_Rect box;
        box.w = ui->w / 2;
        box.h = ui->h / 4;
        box.x = cx - box.w / 2;
        box.y = cy - box.h / 2;

        SDL_SetRenderDrawColor(ui->ren, 25, 25, 30, 255);
        SDL_RenderFillRect(ui->ren, &box);

        SDL_SetRenderDrawColor(ui->ren, 200, 200, 200, 255);
        SDL_RenderDrawRect(ui->ren, &box);

        // Text
        if (show_game_over)
        {
            text_draw_center(ui->ren, &ui->text, cx, box.y + 30,
                             "GAME OVER");
            text_draw_center(ui->ren, &ui->text, cx, box.y + 70,
                             "Enter your name:");
        }
        else
        {
            text_draw_center(ui->ren, &ui->text, cx, box.y + 50,
                             "Enter your name:");
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

        text_draw_center(ui->ren, &ui->text, cx, box.y + 110, display);

        text_draw_center(ui->ren, &ui->text, cx, box.y + box.h - 30,
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

        SDL_SetRenderDrawColor(ui->ren, 10, 10, 12, 255);
        SDL_RenderClear(ui->ren);

        if (ui->text_ok)
        {
            // Title
            text_draw_center(ui->ren, &ui->text, center_x, segment_y, "HIGH SCORES");

            // Instructions
            text_draw_center(ui->ren, &ui->text, center_x, ui->h - 40, "ESC = Back");

            // Display all entries (up to 5)
            int display_count = sb->count < 5 ? sb->count : 5;
            for (int i = 0; i < display_count; ++i)
            {
                char row[128];
                snprintf(row, sizeof(row), "%2d) %-20s  %d", i + 1,
                         sb->entries[i].name, sb->entries[i].score);
                text_draw_center(ui->ren, &ui->text, center_x, offset_y + i * segment_y, row);
            }

            // Show message if no entries
            if (display_count == 0)
            {
                text_draw_center(ui->ren, &ui->text, center_x, offset_y, "No scores yet");
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
    SDL_SetRenderDrawColor(ui->ren, 10, 10, 12, 255);
    SDL_RenderClear(ui->ren);

    if (ui->text_ok)
    {
        int center_x = ui->w / 2;
        int y = ui->h / 2 - 120;

        text_draw_center(ui->ren, &ui->text, center_x, y, "SNAKE");
        y += 60;

        const char *items[] = {
            "Singleplayer",
            "Multiplayer",
            "Options",
            "Scoreboard",
            "Quit"};

        for (int i = 0; i < 5; ++i)
        {
            char line[128];
            if (i == selected_index)
            {
                snprintf(line, sizeof(line), "> %s <", items[i]);
            }
            else
            {
                snprintf(line, sizeof(line), "  %s  ", items[i]);
            }
            text_draw_center(ui->ren, &ui->text, center_x, y + i * 32, line);
        }

        // Build instruction string with actual keybindings
        const char *up = settings_key_name(settings_get_key(settings, 0, SETTING_ACTION_UP));
        const char *down = settings_key_name(settings_get_key(settings, 0, SETTING_ACTION_DOWN));
        char instructions[128];
        snprintf(instructions, sizeof(instructions), "%s/%s + ENTER | ESC = Quit", up, down);
        text_draw_center(ui->ren, &ui->text, center_x, ui->h - 40, instructions);
    }

    SDL_RenderPresent(ui->ren);
}

void ui_sdl_render_options(UiSdl *ui)
{
    SDL_SetRenderDrawColor(ui->ren, 10, 10, 12, 255);
    SDL_RenderClear(ui->ren);

    if (ui->text_ok)
    {
        int cx = ui->w / 2;
        text_draw_center(ui->ren, &ui->text, cx, ui->h / 2 - 20, "OPTIONS");
        text_draw_center(ui->ren, &ui->text, cx, ui->h / 2 + 20, "(empty for now)");
        text_draw_center(ui->ren, &ui->text, cx, ui->h - 40, "ESC = Back to menu");
    }

    SDL_RenderPresent(ui->ren);
}

void ui_sdl_render_multiplayer_placeholder(UiSdl *ui)
{
    SDL_SetRenderDrawColor(ui->ren, 10, 10, 12, 255);
    SDL_RenderClear(ui->ren);

    if (ui->text_ok)
    {
        int cx = ui->w / 2;
        text_draw_center(ui->ren, &ui->text, cx, ui->h / 2 - 20, "MULTIPLAYER");
        text_draw_center(ui->ren, &ui->text, cx, ui->h / 2 + 20, "(not implemented yet)");
        text_draw_center(ui->ren, &ui->text, cx, ui->h - 40, "ESC = Back to menu");
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

void ui_sdl_render_pause_menu(UiSdl *ui, const Game *g, const char *player_name, int selected_index)
{
    // Render the game frame behind the pause overlay
    ui_sdl_draw_game(ui, g, player_name);

    // Draw a semi-transparent overlay
    SDL_SetRenderDrawBlendMode(ui->ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ui->ren, 0, 0, 0, 160);
    SDL_Rect full = {0, 0, ui->w, ui->h};
    SDL_RenderFillRect(ui->ren, &full);

    // Menu box
    SDL_Rect box;
    box.w = ui->w / 2;
    box.h = ui->h / 3;
    box.x = (ui->w - box.w) / 2;
    box.y = (ui->h - box.h) / 2;

    SDL_SetRenderDrawColor(ui->ren, 25, 25, 30, 255);
    SDL_RenderFillRect(ui->ren, &box);

    SDL_SetRenderDrawColor(ui->ren, 220, 220, 220, 255);
    SDL_RenderDrawRect(ui->ren, &box);

    if (ui->text_ok)
    {
        const char *items[] = {"Continue", "Options", "Quit"};
        int cx = ui->w / 2;
        int yseg = box.h / 5;
        text_draw_center(ui->ren, &ui->text, cx, box.y - yseg, "PAUSED");
        
        for (int i = 0; i < 3; ++i)
        {
            char line[64];
            if (i == selected_index)
                snprintf(line, sizeof(line), "> %s <", items[i]);
            else
                snprintf(line, sizeof(line), "  %s  ", items[i]);

            text_draw_center(ui->ren, &ui->text, cx, box.y + yseg + i * yseg, line);
        }

        text_draw_center(ui->ren, &ui->text, cx, box.y + 4 * yseg,
                         "UP/DOWN + ENTER");
    }

    SDL_RenderPresent(ui->ren);
}

void ui_sdl_render_pause_options(UiSdl *ui, const Game *g, const char *player_name)
{
    // render the game behind
    ui_sdl_draw_game(ui, g, player_name);

    SDL_SetRenderDrawBlendMode(ui->ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ui->ren, 0, 0, 0, 170);
    SDL_Rect full = {0, 0, ui->w, ui->h};
    SDL_RenderFillRect(ui->ren, &full);

    SDL_Rect box;
    box.w = ui->w / 2;
    box.h = ui->h / 3;
    box.x = (ui->w - box.w) / 2;
    box.y = (ui->h - box.h) / 2;

    SDL_SetRenderDrawColor(ui->ren, 25, 25, 30, 255);
    SDL_RenderFillRect(ui->ren, &box);
    SDL_SetRenderDrawColor(ui->ren, 220, 220, 220, 255);
    SDL_RenderDrawRect(ui->ren, &box);

    if (ui->text_ok)
    {
        int cx = ui->w / 2;
        int yseg = box.h / 5;
        text_draw_center(ui->ren, &ui->text, cx, box.y - yseg, "PAUSED");
        text_draw_center(ui->ren, &ui->text, cx, box.y + 1 * yseg, "OPTIONS");
        text_draw_center(ui->ren, &ui->text, cx, box.y + 2 * yseg, "(empty for now)");
        text_draw_center(ui->ren, &ui->text, cx, box.y + 4 * yseg, "ESC = Back");
    }

    SDL_RenderPresent(ui->ren);
}

// ==== New Keybindings UI Functions ====

void ui_sdl_render_options_menu(UiSdl *ui, const Settings *settings, int selected_index)
{
    SDL_SetRenderDrawColor(ui->ren, 10, 10, 12, 255);
    SDL_RenderClear(ui->ren);

    if (ui->text_ok)
    {
        int cx = ui->w / 2;
        int y = ui->h / 2 - 80;

        text_draw_center(ui->ren, &ui->text, cx, y, "OPTIONS");
        y += 60;

        const char *items[] = {"Keybinds", "Sound", "Back"};

        for (int i = 0; i < 3; ++i)
        {
            char line[128];
            if (i == selected_index)
            {
                snprintf(line, sizeof(line), "> %s <", items[i]);
            }
            else
            {
                snprintf(line, sizeof(line), "  %s  ", items[i]);
            }
            text_draw_center(ui->ren, &ui->text, cx, y + i * 32, line);
        }

        // Build instruction string with actual keybindings
        const char *up = settings_key_name(settings_get_key(settings, 0, SETTING_ACTION_UP));
        const char *down = settings_key_name(settings_get_key(settings, 0, SETTING_ACTION_DOWN));
        char instructions[128];
        snprintf(instructions, sizeof(instructions), "%s/%s + ENTER | ESC = Back", up, down);
        text_draw_center(ui->ren, &ui->text, cx, ui->h - 40, instructions);
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
    SDL_SetRenderDrawColor(ui->ren, 10, 10, 12, 255);
    SDL_RenderClear(ui->ren);

    if (ui->text_ok)
    {
        int cx = ui->w / 2;
        int y = ui->h / 2 - 100;

        text_draw_center(ui->ren, &ui->text, cx, y, "CONFIGURE KEYBINDS");
        y += 60;

        const char *items[] = {"Player 1", "Player 2", "Player 3", "Player 4", "Back"};

        for (int i = 0; i < 5; ++i)
        {
            char line[128];
            if (i == selected_index)
            {
                snprintf(line, sizeof(line), "> %s <", items[i]);
            }
            else
            {
                snprintf(line, sizeof(line), "  %s  ", items[i]);
            }
            text_draw_center(ui->ren, &ui->text, cx, y + i * 32, line);
        }

        // Build instruction string with actual keybindings
        const char *up = settings_key_name(settings_get_key(settings, 0, SETTING_ACTION_UP));
        const char *down = settings_key_name(settings_get_key(settings, 0, SETTING_ACTION_DOWN));
        char instructions[128];
        snprintf(instructions, sizeof(instructions), "%s/%s + ENTER | ESC = Back", up, down);
        text_draw_center(ui->ren, &ui->text, cx, ui->h - 40, instructions);
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
    SDL_SetRenderDrawColor(ui->ren, 10, 10, 12, 255);
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
    text_draw_center(ui->ren, &ui->text, cx, 40, title);

    // Progress
    char progress[32];
    snprintf(progress, sizeof(progress), "Binding %d/5", action + 1);
    text_draw_center(ui->ren, &ui->text, cx, 80, progress);

    // Main prompt
    char prompt[64];
    snprintf(prompt, sizeof(prompt), "Bind %s", settings_action_name(action));
    text_draw_center(ui->ren, &ui->text, cx, cy - 40, prompt);

    text_draw_center(ui->ren, &ui->text, cx, cy, "Press any key...");

    // Current binding
    SDL_Keycode current_binding = settings_get_key(settings, player_index, action);
    const char *current_key_name = settings_key_name(current_binding);
    char current[64];
    snprintf(current, sizeof(current), "Current: %s", current_key_name);
    text_draw_center(ui->ren, &ui->text, cx, cy + 40, current);

    // Show already bound keys
    int y_offset = cy + 100;
    if (action > 0)
    {
        text_draw_center(ui->ren, &ui->text, cx, y_offset, "Already bound:");
        y_offset += 30;

        for (int i = 0; i < action; i++)
        {
            SDL_Keycode bound = settings_get_key(settings, player_index, (SettingAction)i);
            char entry[64];
            snprintf(entry, sizeof(entry), "%s: %s",
                     settings_action_name((SettingAction)i),
                     settings_key_name(bound));
            text_draw_center(ui->ren, &ui->text, cx, y_offset, entry);
            y_offset += 24;
        }
    }

    // Cancel hint
    text_draw_center(ui->ren, &ui->text, cx, ui->h - 40, "ESC = Cancel");

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

// ==== Multiplayer Menu Functions ====

void ui_sdl_render_multiplayer_menu(UiSdl *ui, const Settings *settings, int selected_index)
{
    SDL_SetRenderDrawColor(ui->ren, 10, 10, 12, 255);
    SDL_RenderClear(ui->ren);

    if (ui->text_ok)
    {
        int cx = ui->w / 2;
        int y = ui->h / 2 - 80;

        text_draw_center(ui->ren, &ui->text, cx, y, "MULTIPLAYER");
        y += 60;

        const char *items[] = {"Local", "Online", "Back"};

        for (int i = 0; i < 3; ++i)
        {
            char line[128];
            if (i == selected_index)
            {
                snprintf(line, sizeof(line), "> %s <", items[i]);
            }
            else
            {
                snprintf(line, sizeof(line), "  %s  ", items[i]);
            }
            text_draw_center(ui->ren, &ui->text, cx, y + i * 32, line);
        }

        // Build instruction string with actual keybindings
        const char *up = settings_key_name(settings_get_key(settings, 0, SETTING_ACTION_UP));
        const char *down = settings_key_name(settings_get_key(settings, 0, SETTING_ACTION_DOWN));
        char instructions[128];
        snprintf(instructions, sizeof(instructions), "%s/%s + ENTER | ESC = Back", up, down);
        text_draw_center(ui->ren, &ui->text, cx, ui->h - 40, instructions);
    }

    SDL_RenderPresent(ui->ren);
}

UiMenuAction ui_sdl_poll_multiplayer_menu(UiSdl *ui, const Settings *settings, int *out_quit)
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

void ui_sdl_render_multiplayer_local_placeholder(UiSdl *ui)
{
    SDL_SetRenderDrawColor(ui->ren, 10, 10, 12, 255);
    SDL_RenderClear(ui->ren);

    if (ui->text_ok)
    {
        int cx = ui->w / 2;
        text_draw_center(ui->ren, &ui->text, cx, ui->h / 2 - 20, "LOCAL MULTIPLAYER");
        text_draw_center(ui->ren, &ui->text, cx, ui->h / 2 + 20, "(not implemented yet)");
        text_draw_center(ui->ren, &ui->text, cx, ui->h - 40, "ESC = Back");
    }

    SDL_RenderPresent(ui->ren);
}

void ui_sdl_render_multiplayer_online_placeholder(UiSdl *ui)
{
    SDL_SetRenderDrawColor(ui->ren, 10, 10, 12, 255);
    SDL_RenderClear(ui->ren);

    if (ui->text_ok)
    {
        int cx = ui->w / 2;
        text_draw_center(ui->ren, &ui->text, cx, ui->h / 2 - 20, "ONLINE MULTIPLAYER");
        text_draw_center(ui->ren, &ui->text, cx, ui->h / 2 + 20, "(not implemented yet)");
        text_draw_center(ui->ren, &ui->text, cx, ui->h - 40, "ESC = Back");
    }

    SDL_RenderPresent(ui->ren);
}

// ==== Local Multiplayer UI Functions ====

#include "multiplayer_game.h"

void ui_sdl_render_multiplayer_lobby(UiSdl *ui, const Settings *settings, const MultiplayerGame_s *mg)
{
    SDL_SetRenderDrawColor(ui->ren, 10, 10, 12, 255);
    SDL_RenderClear(ui->ren);

    if (!ui->text_ok)
    {
        SDL_RenderPresent(ui->ren);
        return;
    }

    int cx = ui->w / 2;
    int cy = ui->h / 2;

    // Title
    text_draw_center(ui->ren, &ui->text, cx, 40, "LOCAL MULTIPLAYER LOBBY");

    // Draw 4 player slots in quadrants
    const char *player_labels[] = {"Player 1", "Player 2", "Player 3", "Player 4"};
    int quadrant_x[] = {ui->w / 4, 3 * ui->w / 4, ui->w / 4, 3 * ui->w / 4};
    int quadrant_y[] = {cy - 80, cy - 80, cy + 80, cy + 80};

    for (int i = 0; i < 4; i++)
    {
        char status[128];
        if (mg->players[i].joined)
        {
            snprintf(status, sizeof(status), "%s [JOINED]", player_labels[i]);
        }
        else
        {
            const char *use_key = settings_key_name(settings_get_key(settings, i, SETTING_ACTION_USE));
            snprintf(status, sizeof(status), "%s: Press %s", player_labels[i], use_key);
        }
        text_draw_center(ui->ren, &ui->text, quadrant_x[i], quadrant_y[i], status);
    }

    // Instructions
    char instructions[256];
    if (mg->total_joined >= 2)
    {
        snprintf(instructions, sizeof(instructions), "%d players joined - Press ENTER to start", mg->total_joined);
    }
    else
    {
        snprintf(instructions, sizeof(instructions), "Waiting for players... (need 2+ to start)");
    }
    text_draw_center(ui->ren, &ui->text, cx, ui->h - 60, instructions);
    text_draw_center(ui->ren, &ui->text, cx, ui->h - 30, "ESC = Back to menu");

    SDL_RenderPresent(ui->ren);
}

int ui_sdl_poll_multiplayer_lobby(UiSdl *ui, const Settings *settings, int *out_quit, int *players_pressed, int *start_pressed)
{
    *out_quit = 0;
    *start_pressed = 0;
    for (int i = 0; i < 4; i++)
    {
        players_pressed[i] = 0;
    }

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

            if (key == SDLK_ESCAPE)
            {
                *out_quit = 1;
                return 1;
            }

            if (key == SDLK_RETURN || key == SDLK_KP_ENTER)
            {
                *start_pressed = 1;
            }

            // Check USE key for each player
            for (int i = 0; i < 4; i++)
            {
                if (key == settings_get_key(settings, i, SETTING_ACTION_USE))
                {
                    players_pressed[i] = 1;
                }
            }
        }
    }

    SDL_GetWindowSize(ui->win, &ui->w, &ui->h);
    return 1;
}

void ui_sdl_render_multiplayer_countdown(UiSdl *ui, const MultiplayerGame_s *mg, int countdown)
{
    SDL_SetRenderDrawColor(ui->ren, 10, 10, 12, 255);
    SDL_RenderClear(ui->ren);

    if (!ui->text_ok)
    {
        SDL_RenderPresent(ui->ren);
        return;
    }

    int cx = ui->w / 2;
    int cy = ui->h / 2;

    // Show large countdown number
    char countdown_text[16];
    if (countdown > 0)
    {
        snprintf(countdown_text, sizeof(countdown_text), "%d", countdown);
    }
    else
    {
        snprintf(countdown_text, sizeof(countdown_text), "GO!");
    }

    text_draw_center(ui->ren, &ui->text, cx, cy, countdown_text);

    SDL_RenderPresent(ui->ren);
}

void ui_sdl_render_multiplayer_game(UiSdl *ui, const MultiplayerGame_s *mg)
{
    int ox, oy;
    // Use a dummy game to compute layout
    Game dummy_game;
    dummy_game.board = mg->board;
    compute_layout(ui, &dummy_game, &ox, &oy);

    // Background
    SDL_SetRenderDrawColor(ui->ren, 15, 15, 18, 255);
    SDL_RenderClear(ui->ren);

    // Board area background
    SDL_Rect board_bg = cell_rect(ui, ox, oy, 0, 0);
    board_bg.w = (mg->board.width + 2) * ui->cell;
    board_bg.h = (mg->board.height + 2) * ui->cell;

    SDL_SetRenderDrawColor(ui->ren, 25, 25, 30, 255);
    SDL_RenderFillRect(ui->ren, &board_bg);

    // Border (1-cell thick)
    SDL_SetRenderDrawColor(ui->ren, 220, 220, 220, 255);

    // Top
    SDL_Rect top = cell_rect(ui, ox, oy, 0, 0);
    top.w = (mg->board.width + 2) * ui->cell;
    SDL_RenderFillRect(ui->ren, &top);

    // Bottom
    SDL_Rect bottom = cell_rect(ui, ox, oy, 0, mg->board.height + 1);
    bottom.w = (mg->board.width + 2) * ui->cell;
    SDL_RenderFillRect(ui->ren, &bottom);

    // Left
    SDL_Rect left = cell_rect(ui, ox, oy, 0, 0);
    left.h = (mg->board.height + 2) * ui->cell;
    SDL_RenderFillRect(ui->ren, &left);

    // Right
    SDL_Rect right = cell_rect(ui, ox, oy, mg->board.width + 1, 0);
    right.h = (mg->board.height + 2) * ui->cell;
    SDL_RenderFillRect(ui->ren, &right);

    // Main food
    SDL_Rect food = cell_rect(ui, ox, oy, 1 + mg->board.food.x, 1 + mg->board.food.y);
    SDL_SetRenderDrawColor(ui->ren, 240, 180, 40, 255);
    SDL_RenderFillRect(ui->ren, &food);

    // Additional food from dead snakes
    for (int f = 0; f < mg->food_count; f++)
    {
        SDL_Rect extra_food = cell_rect(ui, ox, oy, 1 + mg->food[f].x, 1 + mg->food[f].y);
        SDL_SetRenderDrawColor(ui->ren, 240, 180, 40, 255);
        SDL_RenderFillRect(ui->ren, &extra_food);
    }

    // Render all snakes with player colors
    for (int p = 0; p < 4; p++)
    {
        if (!mg->players[p].joined)
            continue;

        const Snake *snake = &mg->players[p].snake;
        const PlayerColor *color = &PLAYER_COLORS[p];

        for (int i = 0; i < snake->length; i++)
        {
            Vec2 seg = snake->segments[i];
            SDL_Rect sr = cell_rect(ui, ox, oy, 1 + seg.x, 1 + seg.y);

            if (i == 0)
            {
                // Head - brighter
                SDL_SetRenderDrawColor(ui->ren, color->r, color->g, color->b, 255);
            }
            else
            {
                // Body - darker
                SDL_SetRenderDrawColor(ui->ren, color->r * 0.7, color->g * 0.7, color->b * 0.7, 255);
            }

            SDL_RenderFillRect(ui->ren, &sr);
        }
    }

    SDL_RenderPresent(ui->ren);
}

int ui_sdl_poll_multiplayer_game(UiSdl *ui, const Settings *settings, MultiplayerGame_s *mg)
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_QUIT)
            return 0;

        if (e.type == SDL_KEYDOWN)
        {
            SDL_Keycode key = e.key.keysym.sym;

            // Check directional keys for each player
            for (int i = 0; i < 4; i++)
            {
                if (!mg->players[i].alive)
                    continue;

                int action = settings_find_action(settings, i, key);
                if (action >= 0 && action <= SETTING_ACTION_RIGHT)
                {
                    Direction dir = DIR_RIGHT;
                    switch (action)
                    {
                    case SETTING_ACTION_UP:    dir = DIR_UP; break;
                    case SETTING_ACTION_DOWN:  dir = DIR_DOWN; break;
                    case SETTING_ACTION_LEFT:  dir = DIR_LEFT; break;
                    case SETTING_ACTION_RIGHT: dir = DIR_RIGHT; break;
                    default: break;
                    }

                    // Push to player's input buffer
                    input_buffer_push(&mg->players[i].input, dir, mg->players[i].snake.dir);
                }
            }
        }
    }

    SDL_GetWindowSize(ui->win, &ui->w, &ui->h);
    return 1;
}

void ui_sdl_render_game_over(UiSdl *ui, int score, int fruits, int time_seconds, int selected_index)
{
    SDL_SetRenderDrawColor(ui->ren, 10, 10, 12, 255);
    SDL_RenderClear(ui->ren);

    if (ui->text_ok)
    {
        int cx = ui->w / 2;
        int y = ui->h / 2 - 120;

        // Title
        text_draw_center(ui->ren, &ui->text, cx, y, "GAME OVER");
        y += 60;

        // Stats
        char line[128];
        snprintf(line, sizeof(line), "Score: %d", score);
        text_draw_center(ui->ren, &ui->text, cx, y, line);
        y += 32;

        snprintf(line, sizeof(line), "Fruits eaten: %d", fruits);
        text_draw_center(ui->ren, &ui->text, cx, y, line);
        y += 32;

        int minutes = time_seconds / 60;
        int seconds = time_seconds % 60;
        snprintf(line, sizeof(line), "Time survived: %d:%02d", minutes, seconds);
        text_draw_center(ui->ren, &ui->text, cx, y, line);
        y += 60;

        // Menu options
        const char *items[] = {"Try again", "Quit"};
        for (int i = 0; i < 2; ++i)
        {
            if (i == selected_index)
            {
                snprintf(line, sizeof(line), "> %s <", items[i]);
            }
            else
            {
                snprintf(line, sizeof(line), "  %s  ", items[i]);
            }
            text_draw_center(ui->ren, &ui->text, cx, y + i * 32, line);
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