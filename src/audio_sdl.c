/*
 * Audio SDL Implementation using Simple Audio
 * Rebuilt for better WSL2 compatibility
 */

#include "audio_sdl.h"
#include "simple_audio.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_SOUNDS 32

typedef struct
{
    char name[64];
    int sound_id;
    int playing;
} SoundEntry;

struct AudioSdl
{
    int music_id;
    //int music_instance;
    int music_volume;   // 0-100 scale
    int effects_volume; // 0-100 scale
    SoundEntry sounds[MAX_SOUNDS];
    int sound_count;
    int initialized;
    int music_playing;
};

AudioSdl *audio_sdl_create(void)
{
    AudioSdl *audio = malloc(sizeof(AudioSdl));
    if (!audio)
    {
        fprintf(stderr, "Failed to allocate AudioSdl\n");
        return NULL;
    }

    audio->music_id = -1;
    audio->music_volume = 50;    // Default 50%
    audio->effects_volume = 100; // Default 100%
    audio->sound_count = 0;
    audio->initialized = 0;

    // Initialize sound entries
    for (int i = 0; i < MAX_SOUNDS; i++)
    {
        audio->sounds[i].name[0] = '\0';
        audio->sounds[i].sound_id = -1;
    }

    fprintf(stderr, "=== Simple Audio Initialization ===\n");

    if (!simple_audio_init())
    {
        fprintf(stderr, "ERROR: Failed to initialize Simple Audio system\n");
        free(audio);
        return NULL;
    }
    if (audio)
    {
        audio->sound_count = 0;
        audio->music_playing = 0; // Initially, no music is playing
    }
    audio->initialized = 1;
    fprintf(stderr, "Simple Audio: Initialization successful\n");
    fprintf(stderr, "===================================\n\n");
    return audio;
}

int audio_sdl_is_music_playing(AudioSdl *audio)
{
    return audio ? audio->music_playing : 0; // Return 1 if music is playing, otherwise 0
}

int audio_sdl_load_music(AudioSdl *audio, const char *music_path)
{
    if (!audio || !audio->initialized)
        return 0;

    fprintf(stderr, "Loading music: %s\n", music_path);

    // Stop and free existing music if loaded
    if (audio->music_id >= 0)
    {
        simple_audio_stop(audio->music_id);
        // Note: simple_audio doesn't have a way to unload sounds yet
        // This is fine since we only load music once
    }

    int music_id;
    if (!simple_audio_load_wav(music_path, &music_id))
    {
        fprintf(stderr, "WARNING: Failed to load music '%s'\n", music_path);
        fprintf(stderr, "NOTE: Simple Audio only supports WAV format\n");
        fprintf(stderr, "Convert your music to WAV with: ffmpeg -i input.ogg output.wav\n");
        return 0;
    }

    audio->music_id = music_id;

    // Set music volume
    float volume = (float)audio->music_volume / 100.0f;
    simple_audio_set_volume(music_id, volume);

    fprintf(stderr, "Music loaded successfully (ID: %d, volume: %.2f)\n", music_id, volume);

    return 1;
}

void audio_sdl_play_music(AudioSdl *audio, int loops)
{
    if (!audio || !audio->initialized || audio->music_id < 0)
    {
        fprintf(stderr, "audio_sdl_play_music: Cannot play (initialized=%d, music_id=%d)\n",
                audio ? audio->initialized : 0,
                audio ? audio->music_id : -1);
        return;
    }

    int loop = (loops == -1 || loops > 0) ? 1 : 0;
    fprintf(stderr, "Playing music (ID: %d, loop: %d)\n", audio->music_id, loop);
    simple_audio_play(audio->music_id, loop);

    //audio->music_instance = simple_audio_play(audio->music_id, 1);
    audio->music_playing = 1;
}

void audio_sdl_pause_music(AudioSdl *audio)
{
    if (!audio || !audio->initialized || audio->music_id < 0) return;
    simple_audio_pause(audio->music_id);
    audio->music_playing = 0;
}

void audio_sdl_resume_music(AudioSdl *audio)
{
    if (!audio || !audio->initialized || audio->music_id < 0) return;
    simple_audio_resume(audio->music_id);
    audio->music_playing = 1;
}

void audio_sdl_stop_music(AudioSdl *audio)
{
    if (!audio || !audio->initialized || audio->music_id < 0)
        return;
    audio->music_playing = 0;
    simple_audio_stop(audio->music_id);
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

    if (audio->initialized && audio->music_id >= 0)
    {
        float vol = (float)volume / 100.0f;
        simple_audio_set_volume(audio->music_id, vol);
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
        float vol = (float)volume / 100.0f;

        // Update all loaded sound effects
        for (int i = 0; i < audio->sound_count; i++)
        {
            if (audio->sounds[i].sound_id >= 0)
            {
                simple_audio_set_volume(audio->sounds[i].sound_id, vol);
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
            return 1; // Already loaded
        }
    }

    // Load the sound
    int sound_id;
    if (!simple_audio_load_wav(sound_path, &sound_id))
    {
        fprintf(stderr, "Failed to load sound '%s' from '%s'\n", sound_name, sound_path);
        return 0;
    }

    // Set volume for the sound
    float vol = (float)audio->effects_volume / 100.0f;
    simple_audio_set_volume(sound_id, vol);

    // Store the sound in the array
    strncpy(audio->sounds[audio->sound_count].name, sound_name, sizeof(audio->sounds[0].name) - 1);
    audio->sounds[audio->sound_count].name[sizeof(audio->sounds[0].name) - 1] = '\0';
    audio->sounds[audio->sound_count].sound_id = sound_id;
    audio->sound_count++;

    fprintf(stderr, "Loaded sound effect '%s' (ID: %d, volume: %.2f)\n", sound_name, sound_id, vol);

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
            // Sound already loaded, play it
            fprintf(stderr, "AUDIO: play_sound '%s' id=%d vol=%d\n",
                    audio->sounds[i].name, audio->sounds[i].sound_id, audio->effects_volume);
            simple_audio_play(audio->sounds[i].sound_id, 0); // Play sound without looping
            audio->sounds[i].playing = 1;                    // Mark the sound as playing
            return;                                          // Exit after playing the sound
        }
    }

    // If sound was not found, load it and then play
    fprintf(stderr, "Sound '%s' not found. Make sure it's loaded first.\n", sound_name);
}

// Check if a specific sound is playing
int audio_sdl_is_sound_playing(AudioSdl *audio, const char *sound_name)
{
    if (!audio || !audio->initialized || !sound_name)
        return 0;

    for (int i = 0; i < audio->sound_count; ++i)
    {
        if (strcmp(audio->sounds[i].name, sound_name) == 0)
        {
            return simple_audio_is_playing(audio->sounds[i].sound_id);
        }
    }
    return 0;
}

// Stop a specific sound
void audio_sdl_stop_sound(AudioSdl *audio, const char *sound_name)
{
    if (audio)
    {
        for (int i = 0; i < audio->sound_count; ++i)
        {
            if (strcmp(audio->sounds[i].name, sound_name) == 0)
            {
                // Stop the sound and mark it as not playing
                simple_audio_stop(audio->sounds[i].sound_id);
                audio->sounds[i].playing = 0;
                break;
            }
        }
    }
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
        simple_audio_cleanup();
        audio->initialized = 0;
    }

    free(audio);
}
