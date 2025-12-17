#ifndef AUDIO_SDL_H
#define AUDIO_SDL_H

typedef struct AudioSdl AudioSdl;

/**
 * Initialize SDL2_mixer and create audio manager.
 * Returns NULL on failure.
 */
AudioSdl *audio_sdl_create(void);

/**
 * Load background music from file path.
 * Returns 1 on success, 0 on failure.
 */
int audio_sdl_load_music(AudioSdl *audio, const char *music_path);

/**
 * Start playing loaded music.
 * loops: -1 for infinite loop, 0 for once, >0 for specific count
 */
void audio_sdl_play_music(AudioSdl *audio, int loops);

int audio_sdl_is_music_playing(AudioSdl *audio);
/**
 * Pause currently playing music.
 */
void audio_sdl_pause_music(AudioSdl *audio);

/**
 * Resume paused music.
 */
void audio_sdl_resume_music(AudioSdl *audio);

/**
 * Stop music playback.
 */
void audio_sdl_stop_music(AudioSdl *audio);

/**
 * Set music volume (0-100).
 */
void audio_sdl_set_music_volume(AudioSdl *audio, int volume);

/**
 * Get current music volume (0-100).
 */
int audio_sdl_get_music_volume(const AudioSdl *audio);

/**
 * Set effects volume (0-100).
 */
void audio_sdl_set_effects_volume(AudioSdl *audio, int volume);

/**
 * Get current effects volume (0-100).
 */
int audio_sdl_get_effects_volume(const AudioSdl *audio);

/**
 * Load a sound effect from file path.
 * Returns 1 on success, 0 on failure.
 */
int audio_sdl_load_sound(AudioSdl *audio, const char *sound_path, const char *sound_name);

/**
 * Play a loaded sound effect by name.
 */
void audio_sdl_play_sound(AudioSdl *audio, const char *sound_id);

/**
 * Check if sound is currently playing.
 * Returns 1 if playing, 0 otherwise.
 */
int audio_sdl_is_sound_playing(AudioSdl *audio, const char *sound_id);

/**
 * Save volume settings to config file.
 * Returns 1 on success, 0 on failure.
 */
int audio_sdl_save_config(const AudioSdl *audio, const char *config_path);

/**
 * Load volume settings from config file.
 * Returns 1 on success, 0 on failure (uses defaults).
 */
int audio_sdl_load_config(AudioSdl *audio, const char *config_path);

/**
 * Clean up and destroy audio manager.
 */
void audio_sdl_destroy(AudioSdl *audio);

#endif
