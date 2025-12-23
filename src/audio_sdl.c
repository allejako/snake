/*
 * Audio SDL Implementation using SDL_mixer
 * Industry-standard audio library with better format support
 */

#include "audio_sdl.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_SOUNDS 32

// Volume conversion helpers
static inline int volume_to_mixer(int vol_0_100)
{
    // Convert 0-100 scale to SDL_mixer's 0-128 scale
    return (vol_0_100 * MIX_MAX_VOLUME) / 100;
}

typedef struct
{
    char name[64];
    Mix_Chunk *chunk;
    int channel; // Last channel played on (-1 if not playing)
} SoundEntry;

struct AudioSdl
{
    Mix_Music *music;
    int music_volume;   // 0-100 scale
    int effects_volume; // 0-100 scale
    SoundEntry sounds[MAX_SOUNDS];
    int sound_count;
    int initialized;
};

AudioSdl *audio_sdl_create(void)
{
    AudioSdl *audio = malloc(sizeof(AudioSdl));
    if (!audio)
    {
        fprintf(stderr, "Failed to allocate AudioSdl\n");
        return NULL;
    }

    audio->music = NULL;
    audio->music_volume = 50;    // Default 50%
    audio->effects_volume = 100; // Default 100%
    audio->sound_count = 0;
    audio->initialized = 0;

    // Initialize sound entries
    for (int i = 0; i < MAX_SOUNDS; i++)
    {
        audio->sounds[i].name[0] = '\0';
        audio->sounds[i].chunk = NULL;
        audio->sounds[i].channel = -1;
    }

    fprintf(stderr, "=== SDL_mixer Initialization ===\n");

    // Initialize SDL audio subsystem if needed
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
    {
        fprintf(stderr, "ERROR: Failed to initialize SDL audio: %s\n", SDL_GetError());
        free(audio);
        return NULL;
    }

    // Initialize SDL_mixer
    // Frequency: 44100 Hz (CD quality)
    // Format: AUDIO_S16SYS (signed 16-bit, system byte order)
    // Channels: 2 (stereo)
    // Chunk size: 2048 (good balance for latency and performance)
    if (Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 2048) < 0)
    {
        fprintf(stderr, "ERROR: Failed to initialize SDL_mixer: %s\n", Mix_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        free(audio);
        return NULL;
    }

    // Allocate mixing channels for sound effects (32 channels)
    Mix_AllocateChannels(32);

    audio->initialized = 1;
    fprintf(stderr, "SDL_mixer: Initialization successful\n");
    fprintf(stderr, "  Frequency: 44100 Hz\n");
    fprintf(stderr, "  Channels: 2 (stereo)\n");
    fprintf(stderr, "  Mixing channels: 32\n");
    fprintf(stderr, "===================================\n\n");

    return audio;
}

int audio_sdl_is_music_playing(AudioSdl *audio)
{
    if (!audio || !audio->initialized)
        return 0;
    return Mix_PlayingMusic();
}

int audio_sdl_load_music(AudioSdl *audio, const char *music_path)
{
    if (!audio || !audio->initialized)
        return 0;

    fprintf(stderr, "Loading music: %s\n", music_path);

    // Free existing music if loaded
    if (audio->music)
    {
        Mix_HaltMusic();
        Mix_FreeMusic(audio->music);
        audio->music = NULL;
    }

    audio->music = Mix_LoadMUS(music_path);
    if (!audio->music)
    {
        fprintf(stderr, "WARNING: Failed to load music '%s': %s\n",
                music_path, Mix_GetError());
        return 0;
    }

    // Set music volume
    Mix_VolumeMusic(volume_to_mixer(audio->music_volume));

    fprintf(stderr, "Music loaded successfully (volume: %d%%)\n", audio->music_volume);
    return 1;
}

void audio_sdl_play_music(AudioSdl *audio, int loops)
{
    if (!audio || !audio->initialized || !audio->music)
    {
        fprintf(stderr, "audio_sdl_play_music: Cannot play (initialized=%d, music=%p)\n",
                audio ? audio->initialized : 0,
                audio ? (void *)audio->music : NULL);
        return;
    }

    // SDL_mixer uses -1 for infinite loop, 0 for once, 1 for twice, etc.
    // Our API: -1 = infinite, 0 = once, >0 = specific count
    int mixer_loops = loops;

    fprintf(stderr, "Playing music (loops: %d)\n", mixer_loops);

    if (Mix_PlayMusic(audio->music, mixer_loops) == -1)
    {
        fprintf(stderr, "ERROR: Failed to play music: %s\n", Mix_GetError());
    }
}

void audio_sdl_pause_music(AudioSdl *audio)
{
    if (!audio || !audio->initialized)
        return;
    Mix_PauseMusic();
}

void audio_sdl_resume_music(AudioSdl *audio)
{
    if (!audio || !audio->initialized)
        return;
    Mix_ResumeMusic();
}

void audio_sdl_stop_music(AudioSdl *audio)
{
    if (!audio || !audio->initialized)
        return;
    Mix_HaltMusic();
}

void audio_sdl_set_music_volume(AudioSdl *audio, int volume)
{
    if (!audio)
        return;

    // Clamp volume to valid range (0-100)
    if (volume < 0)
        volume = 0;
    if (volume > 100)
        volume = 100;

    audio->music_volume = volume;

    if (audio->initialized)
    {
        Mix_VolumeMusic(volume_to_mixer(volume));
    }
}

int audio_sdl_get_music_volume(const AudioSdl *audio)
{
    if (!audio)
        return 0;
    return audio->music_volume;
}

void audio_sdl_set_effects_volume(AudioSdl *audio, int volume)
{
    if (!audio)
        return;

    // Clamp volume to valid range (0-100)
    if (volume < 0)
        volume = 0;
    if (volume > 100)
        volume = 100;

    audio->effects_volume = volume;

    if (audio->initialized)
    {
        int mixer_vol = volume_to_mixer(volume);

        // Update all loaded sound effects
        for (int i = 0; i < audio->sound_count; i++)
        {
            if (audio->sounds[i].chunk)
            {
                Mix_VolumeChunk(audio->sounds[i].chunk, mixer_vol);
            }
        }
    }
}

int audio_sdl_get_effects_volume(const AudioSdl *audio)
{
    if (!audio)
        return 0;
    return audio->effects_volume;
}

int audio_sdl_load_sound(AudioSdl *audio, const char *sound_path, const char *sound_name)
{
    if (!audio || !audio->initialized || !sound_path || !sound_name)
        return 0;

    if (audio->sound_count >= MAX_SOUNDS)
    {
        fprintf(stderr, "Cannot load sound '%s': maximum sounds reached\n", sound_name);
        return 0;
    }

    // Check if sound already exists
    for (int i = 0; i < audio->sound_count; i++)
    {
        if (strcmp(audio->sounds[i].name, sound_name) == 0)
        {
            fprintf(stderr, "Sound '%s' already loaded\n", sound_name);
            return 1;
        }
    }

    // Load the sound
    Mix_Chunk *chunk = Mix_LoadWAV(sound_path);
    if (!chunk)
    {
        fprintf(stderr, "Failed to load sound '%s' from '%s': %s\n",
                sound_name, sound_path, Mix_GetError());
        return 0;
    }

    // Set volume for the chunk
    Mix_VolumeChunk(chunk, volume_to_mixer(audio->effects_volume));

    // Store the sound in the array
    strncpy(audio->sounds[audio->sound_count].name, sound_name,
            sizeof(audio->sounds[0].name) - 1);
    audio->sounds[audio->sound_count].name[sizeof(audio->sounds[0].name) - 1] = '\0';
    audio->sounds[audio->sound_count].chunk = chunk;
    audio->sounds[audio->sound_count].channel = -1;
    audio->sound_count++;

    fprintf(stderr, "Loaded sound effect '%s' (volume: %d%%)\n",
            sound_name, audio->effects_volume);

    return 1;
}

void audio_sdl_play_sound(AudioSdl *audio, const char *sound_name)
{
    if (!audio || !audio->initialized)
        return;

    // Find the sound by name
    for (int i = 0; i < audio->sound_count; i++)
    {
        if (strcmp(audio->sounds[i].name, sound_name) == 0)
        {
            if (!audio->sounds[i].chunk)
            {
                fprintf(stderr, "Sound '%s' has no chunk loaded\n", sound_name);
                return;
            }

            fprintf(stderr, "AUDIO: play_sound '%s' vol=%d\n",
                    audio->sounds[i].name, audio->effects_volume);

            // Play on first available channel (-1)
            // Returns channel number or -1 on error
            int channel = Mix_PlayChannel(-1, audio->sounds[i].chunk, 0);
            if (channel == -1)
            {
                fprintf(stderr, "WARNING: Failed to play sound '%s': %s\n",
                        sound_name, Mix_GetError());
            }
            else
            {
                audio->sounds[i].channel = channel;
            }
            return;
        }
    }

    fprintf(stderr, "Sound '%s' not found. Make sure it's loaded first.\n", sound_name);
}

int audio_sdl_is_sound_playing(AudioSdl *audio, const char *sound_name)
{
    if (!audio || !audio->initialized || !sound_name)
        return 0;

    for (int i = 0; i < audio->sound_count; i++)
    {
        if (strcmp(audio->sounds[i].name, sound_name) == 0)
        {
            // Check if the channel is still playing
            if (audio->sounds[i].channel >= 0)
            {
                return Mix_Playing(audio->sounds[i].channel);
            }
            return 0;
        }
    }
    return 0;
}

int audio_sdl_save_config(const AudioSdl *audio, const char *config_path)
{
    if (!audio)
        return 0;

    FILE *f = fopen(config_path, "w");
    if (!f)
    {
        fprintf(stderr, "Failed to open audio config file for writing: %s\n", config_path);
        return 0;
    }

    fprintf(f, "music_volume=%d\n", audio->music_volume);
    fprintf(f, "effects_volume=%d\n", audio->effects_volume);

    fclose(f);
    return 1;
}

int audio_sdl_load_config(AudioSdl *audio, const char *config_path)
{
    if (!audio)
        return 0;

    FILE *f = fopen(config_path, "r");
    if (!f)
    {
        return 0; // File doesn't exist, use defaults
    }

    char line[128];
    while (fgets(line, sizeof(line), f))
    {
        int value;
        if (sscanf(line, "music_volume=%d", &value) == 1)
        {
            audio_sdl_set_music_volume(audio, value);
        }
        else if (sscanf(line, "effects_volume=%d", &value) == 1)
        {
            audio_sdl_set_effects_volume(audio, value);
        }
    }

    fclose(f);
    return 1;
}

void audio_sdl_destroy(AudioSdl *audio)
{
    if (!audio)
        return;

    if (audio->initialized)
    {
        // Stop and free music
        if (audio->music)
        {
            Mix_HaltMusic();
            Mix_FreeMusic(audio->music);
            audio->music = NULL;
        }

        // Free all sound effects
        for (int i = 0; i < audio->sound_count; i++)
        {
            if (audio->sounds[i].chunk)
            {
                Mix_FreeChunk(audio->sounds[i].chunk);
                audio->sounds[i].chunk = NULL;
            }
        }

        Mix_CloseAudio();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        audio->initialized = 0;
    }

    free(audio);
}
