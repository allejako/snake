/*
 * Simple SDL2 Audio Implementation
 * Uses raw SDL2 audio callbacks for better WSL2 compatibility
 */

#ifndef SIMPLE_AUDIO_H
#define SIMPLE_AUDIO_H

/**
 * Initialize the audio system
 * Returns 1 on success, 0 on failure
 */
int simple_audio_init(void);

/**
 * Load a WAV file and return its ID
 * Returns 1 on success, 0 on failure
 * out_id will contain the sound ID to use with other functions
 */
int simple_audio_load_wav(const char *filename, int *out_id);

/**
 * Play a loaded sound
 * sound_id: ID returned from simple_audio_load_wav
 * loop: 1 to loop infinitely, 0 to play once
 */
void simple_audio_play(int sound_id, int loop);

/**
 * Stop a playing sound
 */
void simple_audio_stop(int sound_id);

/**
 * Set volume for a sound (0.0 to 1.0)
 */
void simple_audio_set_volume(int sound_id, float volume);

/**
 * Check if a sound is currently playing
 * Returns 1 if playing, 0 if not
 */
int simple_audio_is_playing(int sound_id);

/**
 * Clean up and free all audio resources
 */
void simple_audio_cleanup(void);

void simple_audio_pause(int sound_id);
void simple_audio_resume(int sound_id);

#endif /* SIMPLE_AUDIO_H */
