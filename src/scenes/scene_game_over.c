#include "scene_master.h"
#include "ui_sdl.h"
#include "settings.h"
#include "game.h"
#include "scoreboard.h"
#include "ui_helpers.h"
#include "constants.h"
#include <stdlib.h>

/* Game over menu items */
typedef enum {
    GAME_OVER_TRY_AGAIN = 0,
    GAME_OVER_QUIT,
    GAME_OVER_COUNT
} GameOverMenuItem;

/* Scene state */
typedef struct {
    int selected_index;
    int score;
    int fruits;
    int time_seconds;
    int combo_best;
} GameOverSceneData;

static GameOverSceneData *state = NULL;

#define MENU_FRAME_DELAY_MS 16

/* Static helper function to render game over screen */
static void render_game_over(UiSdl *ui, int score, int fruits, int time_seconds, int combo_best, const Scoreboard *sb, int selected_index)
{
    SDL_SetRenderDrawColor(ui->ren, COLOR_BG_MENU_R, COLOR_BG_MENU_G, COLOR_BG_MENU_B, 255);
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

static void game_over_scene_init(UiSdl *ui, Settings *settings, void *user_data)
{
    (void)ui;
    (void)settings;

    state = (GameOverSceneData *)malloc(sizeof(GameOverSceneData));
    if (state) {
        state->selected_index = 0;

        /* user_data should contain game over data */
        if (user_data) {
            GameOverSceneData *data = (GameOverSceneData *)user_data;
            state->score = data->score;
            state->fruits = data->fruits;
            state->time_seconds = data->time_seconds;
            state->combo_best = data->combo_best;
        } else {
            state->score = 0;
            state->fruits = 0;
            state->time_seconds = 0;
            state->combo_best = 0;
        }
    }
}

static void game_over_scene_update(UiSdl *ui, Settings *settings)
{
    if (!state) return;

    /* Poll input */
    int quit = 0;
    UiMenuAction action = ui_sdl_poll_game_over(ui, settings, &quit);

    if (quit) {
        scene_master_transition(SCENE_QUIT, NULL);
        return;
    }

    /* Handle menu navigation */
    if (action == UI_MENU_UP) {
        state->selected_index = (state->selected_index - 1 + GAME_OVER_COUNT) % GAME_OVER_COUNT;
    }
    else if (action == UI_MENU_DOWN) {
        state->selected_index = (state->selected_index + 1) % GAME_OVER_COUNT;
    }
    else if (action == UI_MENU_SELECT) {
        if (state->selected_index == GAME_OVER_TRY_AGAIN) {
            /* Try again - restart singleplayer */
            scene_master_transition(SCENE_SINGLEPLAYER, NULL);
        }
        else {
            /* Quit - return to main menu */
            scene_master_transition(SCENE_MENU, NULL);
        }
    }
}

static void game_over_scene_render(UiSdl *ui, Settings *settings)
{
    (void)settings;
    if (!state) return;

    SceneContext *ctx = scene_master_get_context();

    /* Render game over screen */
    render_game_over(ui, state->score, state->fruits, state->time_seconds,
                     state->combo_best, ctx ? ctx->scoreboard : NULL,
                     state->selected_index);
    SDL_Delay(MENU_FRAME_DELAY_MS);
}

static void game_over_scene_cleanup(void)
{
    if (state) {
        free(state);
        state = NULL;
    }
}

/* Register this scene with scene_master */
void scene_game_over_register(void)
{
    Scene scene = {
        .init = game_over_scene_init,
        .update = game_over_scene_update,
        .render = game_over_scene_render,
        .cleanup = game_over_scene_cleanup
    };

    scene_master_register_scene(SCENE_GAME_OVER, scene);
}
