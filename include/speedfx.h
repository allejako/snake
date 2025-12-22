#pragma once

#include <SDL2/SDL.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define COMBO_RING_MAX 8

    typedef struct
    {
        float x, y;  // center (screen space)
        float t;     // time remaining
        float ttl;   // total lifetime
        float r;     // current radius
        float vr;    // radius velocity (px/sec)
        float thick; // thickness (px)
        float a;     // peak alpha (0..255)
    } ComboRing;

    typedef struct
    {
        float x, y;
        float vx, vy;
        float life, ttl;
        float len;
        unsigned char a;
    } WarpParticle;

#define WARP_MAX 512

    typedef struct
    {
        float fx; // 0..1 intensity
        float shake_phase;
        float shake_dx, shake_dy;

        WarpParticle p[WARP_MAX];
        int p_count;

        float spawn_accum;
        int w, h;        // cached viewport size for spawning
        float punch_t;   // time remaining
        float punch_ttl; // total time
        float punch_amp; // pixels
        ComboRing ring[COMBO_RING_MAX];
        int ring_count;
    } SpeedFX;

    void speedfx_init(SpeedFX *f, int w, int h);
    void speedfx_set_viewport(SpeedFX *f, int w, int h);

    // tick_ms + speed_start_ms + speed_floor_ms determine intensity.
    // origin_x/origin_y is where warp lines spawn from (snake head is ideal).
    void speedfx_update(
        SpeedFX *f,
        float dt,
        unsigned int tick_ms,
        int speed_start_ms,
        int speed_floor_ms,
        SDL_Rect board_rect);

    // Apply shake to a destination rect (translation only)
    void speedfx_apply_shake_rect(const SpeedFX *f, SDL_Rect *dst);

    // Draw particles on top (uses additive blending internally)
    void speedfx_render_particles(SDL_Renderer *ren, const SpeedFX *f);

    void speedfx_combo_punch(SpeedFX *f, float tier, float ttl_sec);

    void speedfx_combo_ring(SpeedFX *f, float x, float y, int tier, float amp);
    void speedfx_update_rings(SpeedFX *f, float dt);
    void speedfx_render_rings(SDL_Renderer *ren, const SpeedFX *f, Uint8 r, Uint8 g, Uint8 b);

#ifdef __cplusplus
}
#endif
