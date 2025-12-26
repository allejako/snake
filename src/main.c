#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include "constants.h"
#include "config.h"
#include "scoreboard.h"
#include "ui_sdl.h"
#include "settings.h"
#include "audio_sdl.h"
#include "scene_master.h"
#include "multiplayer.h"
#include <SDL2/SDL_ttf.h>

#define UUID "c609c6cf-ad69-4957-9aa4-6e7cac06a862"

/* Scene registration functions */
extern void scene_menu_register(void);
extern void scene_scoreboard_register(void);
extern void scene_options_menu_register(void);
extern void scene_sound_settings_register(void);
extern void scene_keybind_binding_register(void);
extern void scene_game_over_register(void);
extern void scene_pause_menu_register(void);
extern void scene_multiplayer_online_menu_register(void);
extern void scene_multiplayer_lobby_register(void);
extern void scene_singleplayer_register(void);
extern void scene_multiplayer_game_register(void);

int main(int argc, char *argv[])
{
    srand((unsigned int)time(NULL));

    /* Parse command-line arguments */
    int debug_mode = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0 || strcmp(argv[i], "-d") == 0) {
            debug_mode = 1;
        }
    }

    /* Load game configuration */
    GameConfig game_config;
    if (!config_load(&game_config, "data/config.ini")) {
        fprintf(stderr, "Warning: Could not load config, using defaults\n");
        config_init_defaults(&game_config);
    }

    /* Load unified settings */
    Settings settings;
    settings_init(&settings, "data/settings.ini");
    if (!settings_load(&settings)) {
        fprintf(stderr, "Warning: Could not load settings, using defaults\n");
    }

    /* Initialize UI */
    UiSdl *ui = ui_sdl_create("Snake", 1280, 720);
    if (!ui) {
        fprintf(stderr, "Failed to create UI\n");
        return 1;
    }

    /* Initialize audio */
    AudioSdl *audio = audio_sdl_create();
    if (!audio) {
        fprintf(stderr, "Warning: Failed to initialize audio\n");
        fprintf(stderr, "Game will continue without sound\n");
        fprintf(stderr, "To disable audio, set both volumes to 0 in Options > Sound Settings\n");
    } else {
        /* Apply volume settings */
        audio_sdl_set_music_volume(audio, settings.music_volume);
        audio_sdl_set_effects_volume(audio, settings.effects_volume);

        /* Load music */
        if (audio_sdl_load_music(audio, "assets/music/background.wav")) {
            if (!audio_sdl_is_music_playing(audio)) {
                audio_sdl_play_music(audio, -1);
            }
        } else {
            fprintf(stderr, "Warning: Failed to load background music\n");
        }

        /* Load sound effects */
        audio_sdl_load_sound(audio, "assets/audio/hitmarker.wav", "explosion");

        /* Load combo sound effects */
        const char *combo_files[] = {
            "assets/audio/Combo1.wav",
            "assets/audio/Combo2.wav",
            "assets/audio/Combo3.wav",
            "assets/audio/Combo4.wav",
            "assets/audio/Combo5.wav",
            "assets/audio/Combo6.wav",
            "assets/audio/Combo7.wav"
        };
        for (int i = 0; i < 7; i++) {
            char name[16];
            snprintf(name, sizeof(name), "combo%d", i + 1);
            audio_sdl_load_sound(audio, combo_files[i], name);
        }

        /* Load highscore sound */
        audio_sdl_load_sound(audio, "assets/audio/Highscore.wav", "highscore");
    }

    /* Initialize scoreboard */
    Scoreboard sb;
    scoreboard_init(&sb, "data/scoreboard.csv");
    scoreboard_load(&sb);
    scoreboard_sort(&sb);

    /* Prompt for profile name if not set */
    if (!settings_has_profile(&settings)) {
        char namebuf[SETTINGS_MAX_PROFILE_NAME];
        if (ui_sdl_get_name(ui, namebuf, sizeof(namebuf), 0)) {
            if (namebuf[0] != '\0') {
                strncpy(settings.profile_name, namebuf, SETTINGS_MAX_PROFILE_NAME - 1);
                settings.profile_name[SETTINGS_MAX_PROFILE_NAME - 1] = '\0';
                settings_save(&settings);
            }
        }
    }

    /* Create multiplayer context */
    Multiplayer *mp = multiplayer_create();
    if (!mp) {
        fprintf(stderr, "Failed to create multiplayer context\n");
        return 1;
    }

    /* Create mpapi instance */
    mpapi *mpapi_instance = mpapi_create(game_config.server_host, game_config.server_port, UUID);
    if (!mpapi_instance) {
        fprintf(stderr, "Failed to create mpapi instance\n");
        multiplayer_destroy(mp);
        return 1;
    }
    mp->api = mpapi_instance;

    /* Initialize scene master */
    scene_master_init(ui, &settings, audio, &sb, mpapi_instance, &game_config);

    /* Get context and set multiplayer pointer */
    SceneContext *ctx = scene_master_get_context();
    if (ctx) {
        ctx->multiplayer = mp;
    }

    /* Register all scenes */
    scene_menu_register();
    scene_scoreboard_register();
    scene_options_menu_register();
    scene_sound_settings_register();
    scene_keybind_binding_register();
    scene_game_over_register();
    scene_pause_menu_register();
    scene_multiplayer_online_menu_register();
    scene_multiplayer_lobby_register();
    scene_singleplayer_register();
    scene_multiplayer_game_register();

    /* Start with menu scene */
    scene_master_transition(SCENE_MENU, NULL);

    /* Run main loop */
    scene_master_run();

    /* Cleanup */
    scene_master_shutdown();
    scoreboard_free(&sb);
    settings_save(&settings);

    if (mp) {
        multiplayer_destroy(mp);
    }
    if (mpapi_instance) {
        mpapi_destroy(mpapi_instance);
    }
    if (audio) {
        audio_sdl_destroy(audio);
    }
    ui_sdl_destroy(ui);

    return 0;
}
