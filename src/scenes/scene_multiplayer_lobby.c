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
    /* Lobby state tracked in Multiplayer object */
    int dummy;
} MultiplayerLobbySceneData;

static MultiplayerLobbySceneData *state = NULL;

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

/* Static helper function to render online lobby */
static void render_online_lobby(UiSdl *ui, const Multiplayer *mp)
{
    // Compute board layout (centered with border) - same as game
    int ox, oy;
    compute_layout(ui, &mp->board, &ox, &oy);

    int board_w = mp->board.width;
    int board_h = mp->board.height;

    // Render board using render_utils
    render_board(ui->ren, &mp->board, ui->cell, ox, oy);

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
        if (!mp->players[p].joined || !mp->players[p].ready)
            continue;

        const Snake *snake = &mp->players[p].snake;
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

        // Show session ID at top center
        char session_text[64];
        snprintf(session_text, sizeof(session_text), "Session ID: %s", mp->session_id);
        text_draw_center(ui->ren, &ui->text, ui->w / 2, 20, session_text);

        for (int p = 0; p < MAX_PLAYERS; p++)
        {
            if (!mp->players[p].joined)
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

            snprintf(hud1, sizeof(hud1), "%s%s", mp->players[p].name,
                     mp->players[p].is_local_player ? " (YOU)" : "");
            snprintf(hud2, sizeof(hud2), "Wins: %d", mp->players[p].wins);
            snprintf(hud3, sizeof(hud3), "%s", mp->players[p].ready ? "READY" : "Not Ready");

            text_draw(ui->ren, &ui->text, x, y, hud1);
            text_draw(ui->ren, &ui->text, x, y + 18, hud2);
            text_draw(ui->ren, &ui->text, x, y + 36, hud3);
        }

        // Instructions at bottom center
        text_draw_center(ui->ren, &ui->text, ui->w / 2, ui->h / 2,
                         "USE key: Toggle Ready | ESC: Leave");
        const char *hint = mp->is_host ? "ENTER: Start (when all ready)" : "Waiting for host...";
        text_draw_center(ui->ren, &ui->text, ui->w / 2, ui->h / 2 - 30, hint);
    }

    SDL_RenderPresent(ui->ren);
}

/* Static helper function to render online countdown */
static void render_online_countdown(UiSdl *ui, const Multiplayer *mp, int countdown)
{
    // Render the game board first (same as lobby/game rendering)
    int ox, oy;
    compute_layout(ui, &mp->board, &ox, &oy);

    int board_w = mp->board.width;
    int board_h = mp->board.height;

    // Render board and snakes using render_utils
    render_board(ui->ren, &mp->board, ui->cell, ox, oy);
    render_all_snakes(ui->ren, mp->players, MAX_PLAYERS, ui->cell, ox, oy, &mp->board);

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

static void multiplayer_lobby_scene_init(UiSdl *ui, Settings *settings, void *user_data)
{
    (void)ui;
    (void)settings;
    (void)user_data;

    state = (MultiplayerLobbySceneData *)malloc(sizeof(MultiplayerLobbySceneData));
}

static void multiplayer_lobby_scene_update(UiSdl *ui, Settings *settings)
{
    if (!state) return;

    SceneContext *ctx = scene_master_get_context();

    /* Poll input */
    int quit = 0;
    UiMenuAction action = ui_sdl_poll_online_lobby(ui, settings, &quit);

    if (quit) {
        scene_master_transition(SCENE_QUIT, NULL);
        return;
    }

    if (action == UI_MENU_BACK) {
        /* Leave lobby and return to menu */
        scene_master_transition(SCENE_MULTIPLAYER_ONLINE_MENU, NULL);
        return;
    }

    /* Check if game is starting */
    if (ctx && ctx->multiplayer) {
        /* TODO: Check if multiplayer->state indicates game is starting */
        /* If so, transition to SCENE_MULTIPLAYER_GAME */
    }
}

static void multiplayer_lobby_scene_render(UiSdl *ui, Settings *settings)
{
    (void)settings;
    if (!state) return;

    SceneContext *ctx = scene_master_get_context();

    /* Render lobby */
    if (ctx && ctx->multiplayer) {
        render_online_lobby(ui, ctx->multiplayer);
    }
}

static void multiplayer_lobby_scene_cleanup(void)
{
    if (state) {
        free(state);
        state = NULL;
    }
}

/* Register this scene with scene_master */
void scene_multiplayer_lobby_register(void)
{
    Scene scene = {
        .init = multiplayer_lobby_scene_init,
        .update = multiplayer_lobby_scene_update,
        .render = multiplayer_lobby_scene_render,
        .cleanup = multiplayer_lobby_scene_cleanup
    };

    scene_master_register_scene(SCENE_MULTIPLAYER_LOBBY, scene);
}
