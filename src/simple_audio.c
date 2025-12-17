/*
 * Simple SDL2 Audio Implementation
 * Based on Simple-SDL2-Audio concept but implemented for this project
 * Uses raw SDL2 audio callbacks for better WSL2 compatibility
 */

#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_SOUNDS 50
#define AUDIO_FORMAT AUDIO_S16LSB
#define AUDIO_FREQUENCY 48000
#define AUDIO_CHANNELS 2
#define AUDIO_SAMPLES 4096

typedef struct Sound
{
    uint8_t *buffer;
    uint32_t length;
    uint32_t position;
    int playing;
    int loop;
    float volume;
} Sound;

typedef struct
{
    SDL_AudioDeviceID device;
    SDL_AudioSpec spec;
    Sound sounds[MAX_SOUNDS];
    int initialized;
} SimpleAudio;

static SimpleAudio g_audio = {0};

// Audio callback - mixes all active sounds
static void audio_callback(void *userdata, Uint8 *stream, int len)
{
    (void)userdata;

    SDL_memset(stream, 0, len);

    for (int i = 0; i < MAX_SOUNDS; i++)
    {
        Sound *sound = &g_audio.sounds[i];
        if (!sound->playing || !sound->buffer)
            continue;

        float v = sound->volume;
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        int mixvol = (int)(SDL_MIX_MAXVOLUME * v);

        uint32_t out_offset = 0;

        while (out_offset < (uint32_t)len && sound->playing)
        {
            uint32_t bytes_remaining = sound->length - sound->position;
            if (bytes_remaining == 0)
            {
                if (sound->loop) sound->position = 0;
                else { sound->playing = 0; sound->position = 0; }
                continue;
            }

            uint32_t bytes_to_mix = (uint32_t)len - out_offset;
            if (bytes_to_mix > bytes_remaining)
                bytes_to_mix = bytes_remaining;

            SDL_MixAudioFormat(stream + out_offset,
                               sound->buffer + sound->position,
                               AUDIO_FORMAT,
                               bytes_to_mix,
                               mixvol);

            sound->position += bytes_to_mix;
            out_offset += bytes_to_mix;

            if (sound->position >= sound->length)
            {
                if (sound->loop) sound->position = 0;
                else { sound->playing = 0; sound->position = 0; }
            }
        }
    }
}


int simple_audio_init(void)
{
    if (g_audio.initialized)
        return 1;

    // Initialize SDL audio subsystem
    fprintf(stderr, "Simple Audio: Initializing SDL audio subsystem...\n");
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
    {
        fprintf(stderr, "Simple Audio: Failed to initialize SDL audio: %s\n", SDL_GetError());
        return 0;
    }

    // List available audio devices for diagnostics
    int num_devices = SDL_GetNumAudioDevices(0);
    fprintf(stderr, "Simple Audio: Found %d audio output device(s)\n", num_devices);
    for (int i = 0; i < num_devices; i++)
    {
        const char *name = SDL_GetAudioDeviceName(i, 0);
        fprintf(stderr, "  Device %d: %s\n", i, name ? name : "(unnamed)");
    }

    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq = AUDIO_FREQUENCY;
    want.format = AUDIO_FORMAT;
    want.channels = AUDIO_CHANNELS;
    want.samples = AUDIO_SAMPLES;
    want.callback = audio_callback;
    want.userdata = NULL;

    fprintf(stderr, "\nSimple Audio: Opening audio device with:\n");
    fprintf(stderr, "  Frequency: %d Hz\n", want.freq);
    fprintf(stderr, "  Format: AUDIO_S16LSB\n");
    fprintf(stderr, "  Channels: %d (mono)\n", want.channels);
    fprintf(stderr, "  Samples: %d\n", want.samples);

    g_audio.device = SDL_OpenAudioDevice(NULL, 0, &want, &g_audio.spec,
    SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE);

    if (g_audio.device == 0)
    {
        fprintf(stderr, "Simple Audio: Failed to open audio device: %s\n", SDL_GetError());
        return 0;
    }

    fprintf(stderr, "Simple Audio: Got audio spec:\n");
    fprintf(stderr, "  Frequency: %d Hz\n", g_audio.spec.freq);
    fprintf(stderr, "  Channels: %d\n", g_audio.spec.channels);
    fprintf(stderr, "  Samples: %d\n", g_audio.spec.samples);
    fprintf(stderr, "  Size: %d bytes\n", g_audio.spec.size);

    // Initialize all sound slots
    for (int i = 0; i < MAX_SOUNDS; i++)
    {
        g_audio.sounds[i].buffer = NULL;
        g_audio.sounds[i].length = 0;
        g_audio.sounds[i].position = 0;
        g_audio.sounds[i].playing = 0;
        g_audio.sounds[i].loop = 0;
        g_audio.sounds[i].volume = 1.0f;
    }

    // Start audio playback
    SDL_PauseAudioDevice(g_audio.device, 0);

    g_audio.initialized = 1;
    return 1;
}

int simple_audio_load_wav(const char *filename, int *out_id)
{
    if (!g_audio.initialized)
        return 0;

    // Find free slot
    int slot = -1;
    for (int i = 0; i < MAX_SOUNDS; i++)
    {
        if (g_audio.sounds[i].buffer == NULL)
        {
            slot = i;
            break;
        }
    }

    if (slot == -1)
    {
        fprintf(stderr, "Simple Audio: No free sound slots\n");
        return 0;
    }

    // Load WAV file
    SDL_AudioSpec wav_spec;
    uint8_t *wav_buffer;
    uint32_t wav_length;

    if (SDL_LoadWAV(filename, &wav_spec, &wav_buffer, &wav_length) == NULL)
    {
        fprintf(stderr, "Simple Audio: Failed to load WAV '%s': %s\n", filename, SDL_GetError());
        return 0;
    }

    // Convert WAV to our audio format if needed
    SDL_AudioCVT cvt;
    int ret = SDL_BuildAudioCVT(&cvt,
                                wav_spec.format, wav_spec.channels, wav_spec.freq,
                                g_audio.spec.format, g_audio.spec.channels, g_audio.spec.freq);

    if (ret < 0)
    {
        fprintf(stderr, "Simple Audio: Failed to build audio converter: %s\n", SDL_GetError());
        SDL_FreeWAV(wav_buffer);
        return 0;
    }

    uint8_t *converted_buffer;
    uint32_t converted_length;

    if (ret == 0)
    {
        // No conversion needed
        converted_buffer = malloc(wav_length);
        if (!converted_buffer)
        {
            SDL_FreeWAV(wav_buffer);
            return 0;
        }
        memcpy(converted_buffer, wav_buffer, wav_length);
        converted_length = wav_length;
        SDL_FreeWAV(wav_buffer);
    }
    else
    {
        // Conversion needed
        cvt.len = wav_length;
        cvt.buf = malloc(wav_length * cvt.len_mult);
        if (!cvt.buf)
        {
            SDL_FreeWAV(wav_buffer);
            return 0;
        }

        memcpy(cvt.buf, wav_buffer, wav_length);
        SDL_FreeWAV(wav_buffer);

        if (SDL_ConvertAudio(&cvt) < 0)
        {
            fprintf(stderr, "Simple Audio: Failed to convert audio: %s\n", SDL_GetError());
            free(cvt.buf);
            return 0;
        }

        converted_buffer = cvt.buf;
        converted_length = cvt.len_cvt;
    }

    // Store in slot
    SDL_LockAudioDevice(g_audio.device);
    g_audio.sounds[slot].buffer = converted_buffer;
    g_audio.sounds[slot].length = converted_length;
    g_audio.sounds[slot].position = 0;
    g_audio.sounds[slot].playing = 0;
    g_audio.sounds[slot].loop = 0;
    g_audio.sounds[slot].volume = 1.0f;
    SDL_UnlockAudioDevice(g_audio.device);

    *out_id = slot;
    fprintf(stderr, "Simple Audio: Loaded '%s' to slot %d (%u bytes)\n", filename, slot, converted_length);

    return 1;
}

void simple_audio_play(int sound_id, int loop)
{
    if (!g_audio.initialized || sound_id < 0 || sound_id >= MAX_SOUNDS)
        return;

    SDL_LockAudioDevice(g_audio.device);

    if (g_audio.sounds[sound_id].buffer)
    {
        g_audio.sounds[sound_id].position = 0;
        g_audio.sounds[sound_id].playing = 1;
        g_audio.sounds[sound_id].loop = loop;
    }

    SDL_UnlockAudioDevice(g_audio.device);
}

void simple_audio_stop(int sound_id)
{
    if (!g_audio.initialized || sound_id < 0 || sound_id >= MAX_SOUNDS)
        return;

    SDL_LockAudioDevice(g_audio.device);
    g_audio.sounds[sound_id].playing = 0;
    g_audio.sounds[sound_id].position = 0;
    SDL_UnlockAudioDevice(g_audio.device);
}

void simple_audio_set_volume(int sound_id, float volume)
{
    if (!g_audio.initialized || sound_id < 0 || sound_id >= MAX_SOUNDS)
        return;

    if (volume < 0.0f)
        volume = 0.0f;
    if (volume > 1.0f)
        volume = 1.0f;

    SDL_LockAudioDevice(g_audio.device);
    g_audio.sounds[sound_id].volume = volume;
    SDL_UnlockAudioDevice(g_audio.device);
}

int simple_audio_is_playing(int sound_id)
{
    if (!g_audio.initialized || sound_id < 0 || sound_id >= MAX_SOUNDS)
        return 0;

    return g_audio.sounds[sound_id].playing;
}

void simple_audio_cleanup(void)
{
    if (!g_audio.initialized)
        return;

    fprintf(stderr, "Simple Audio: Cleaning up...\n");

    SDL_PauseAudioDevice(g_audio.device, 1);
    SDL_CloseAudioDevice(g_audio.device);

    for (int i = 0; i < MAX_SOUNDS; i++)
    {
        if (g_audio.sounds[i].buffer)
        {
            free(g_audio.sounds[i].buffer);
            g_audio.sounds[i].buffer = NULL;
        }
    }

    SDL_QuitSubSystem(SDL_INIT_AUDIO);

    g_audio.initialized = 0;
    fprintf(stderr, "Simple Audio: Cleanup complete\n");
}

void simple_audio_pause(int sound_id)
{
    if (!g_audio.initialized || sound_id < 0 || sound_id >= MAX_SOUNDS)
        return;

    SDL_LockAudioDevice(g_audio.device);

    if (g_audio.sounds[sound_id].buffer)
    {
        g_audio.sounds[sound_id].playing = 0;   // do NOT change position
    }

    SDL_UnlockAudioDevice(g_audio.device);
}

void simple_audio_resume(int sound_id)
{
    if (!g_audio.initialized || sound_id < 0 || sound_id >= MAX_SOUNDS)
        return;

    SDL_LockAudioDevice(g_audio.device);

    if (g_audio.sounds[sound_id].buffer)
    {
        // If we reached the end, resume should probably restart
        if (g_audio.sounds[sound_id].position >= g_audio.sounds[sound_id].length)
            g_audio.sounds[sound_id].position = 0;

        g_audio.sounds[sound_id].playing = 1;   // preserve position
        // keep loop as-is; or set it explicitly if you prefer
    }

    SDL_UnlockAudioDevice(g_audio.device);
}