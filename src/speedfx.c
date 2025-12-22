#include "speedfx.h"
#include <math.h>
#include <stdlib.h>
#ifndef PI_F
#define PI_F 3.14159265358979323846f
#endif

// ===================== SPEEDFX TUNING =====================
// Tick values are ms per tick: lower = faster.
// Effects are OFF when tick_ms > FX_SHAKE_START_TICK_MS.
// Effects reach FULL when tick_ms <= FX_FULL_TICK_MS.
//
// With your game: SPEED_START_MS = 95, SPEED_FLOOR_MS = 40
// A sensible starting point: start at ~75, full at 40.

#define FX_SHAKE_START_TICK_MS 70.0f // start shake/FX when tick <= this
#define FX_FULL_TICK_MS 40.0f        // full intensity when tick <= this

// How quickly the FX intensity follows the current tick (bigger = snappier)
#define FX_RESPONSE_K 14.0f

// Shake parameters
#define FX_SHAKE_MAX_PX 0.7f    // max shake in pixels at full intensity
#define FX_SHAKE_FREQ_BASE 6.0f // base shake frequency
#define FX_SHAKE_FREQ_ADD 12.0f // extra frequency at full intensity
#define FX_SHAKE_SIN_MUL_X 11.0f
#define FX_SHAKE_SIN_MUL_Y 13.0f
#define FX_SHAKE_MIN_PX 0.5f    // minimum shake once enabled (prevents rounding to 0)
#define FX_SHAKE_APPLY_PIXELS 0 // 1 = quantize to integer pixels, 0 = allow subpixel (not possible in SDL copy)

// Particle parameters (border streaks)
#define FX_PARTICLES_BASE_RATE 0.0f  // particles/sec at intensity=0
#define FX_PARTICLES_MAX_RATE 220.0f // particles/sec at intensity=1
#define FX_PARTICLE_SPEED_MIN 600.0f
#define FX_PARTICLE_SPEED_MAX 2600.0f
#define FX_PARTICLE_LEN_MIN 8.0f
#define FX_PARTICLE_LEN_MAX 32.0f
#define FX_PARTICLE_TTL_MIN 0.15f
#define FX_PARTICLE_TTL_RAND 0.25f
#define FX_PARTICLE_ALPHA_MIN 20
#define FX_PARTICLE_ALPHA_MAX 110

// 0 = linear ramp, 1 = ease-in (starts gentler), 2 = ease-out (starts stronger)
#define FX_RAMP_MODE 0

// ==========================================================
static float clamp01(float v)
{
    if (v < 0.0f)
        return 0.0f;
    if (v > 1.0f)
        return 1.0f;
    return v;
}

static float tick_to_intensity(unsigned int tick_ms)
{
    float t = (float)tick_ms;

    // OFF above start threshold
    if (t > FX_SHAKE_START_TICK_MS)
        return 0.0f;

    // FULL at/below full threshold
    if (t <= FX_FULL_TICK_MS)
        return 1.0f;

    // Linear ramp in between:
    // tick = FX_SHAKE_START -> 0
    // tick = FX_FULL       -> 1
    float denom = (FX_SHAKE_START_TICK_MS - FX_FULL_TICK_MS);
    if (denom < 0.0001f)
        return 1.0f;

    float ramp = (FX_SHAKE_START_TICK_MS - t) / denom; // 0..1
    ramp = clamp01(ramp);

#if FX_RAMP_MODE == 1
    // ease-in: slower at start, faster near full
    ramp = ramp * ramp;
#elif FX_RAMP_MODE == 2
    // ease-out: faster at start, slower near full
    ramp = 1.0f - (1.0f - ramp) * (1.0f - ramp);
#endif

    return ramp;
}
void speedfx_init(SpeedFX *f, int w, int h)
{
    if (!f)
        return;

    f->fx = 0.0f;
    f->shake_phase = 0.0f;
    f->shake_dx = 0.0f;
    f->shake_dy = 0.0f;

    f->p_count = 0;
    f->spawn_accum = 0.0f;

    f->w = w;
    f->h = h;
    f->punch_ttl = 0.0f;
    f->punch_t = 0.0f;
    f->punch_amp = 0.0f;
}

void speedfx_set_viewport(SpeedFX *f, int w, int h)
{
    if (!f)
        return;
    f->w = w;
    f->h = h;
}

void speedfx_update(
    SpeedFX *f,
    float dt,
    unsigned int tick_ms,
    int speed_start_ms,
    int speed_floor_ms,
    SDL_Rect board_rect)
{
    if (!f)
        return;
    if (dt < 0.0f)
        dt = 0.0f;
    if (dt > 0.1f)
        dt = 0.1f;

    float target = tick_to_intensity(tick_ms);

    // Smooth response (so it doesn't jitter if tick changes)
    float alpha = 1.0f - expf(-FX_RESPONSE_K * dt);
    f->fx += (target - f->fx) * alpha;

    speedfx_update_rings(f, dt);

    // --- combo punch decay ---
    if (f->punch_t > 0.0f)
    {
        f->punch_t -= dt;
        if (f->punch_t < 0.0f)
            f->punch_t = 0.0f;
    }

    float punch_k = 0.0f;
    if (f->punch_ttl > 0.0001f && f->punch_t > 0.0f)
    {
        // 1 -> 0 over ttl
        float u = f->punch_t / f->punch_ttl;

        // nice "hit" curve: sharp then quick fade
        punch_k = u * u; // or u*u*u for even snappier
    }

    // punch_mult is typically 1..(1+punch_amp)
    float punch_mult = 1.0f + f->punch_amp * punch_k;
    // Shake should start exactly when target becomes > 0 (no "late start" from smoothing)
    if (target <= 0.0f)
    {
        f->shake_dx = 0.0f;
        f->shake_dy = 0.0f;
    }
    else
    {
        // use target for immediate response
        float shake_i = target;

        f->shake_phase += dt * (FX_SHAKE_FREQ_BASE + FX_SHAKE_FREQ_ADD * shake_i);

        float amp = FX_SHAKE_MAX_PX * target * punch_mult;

        // Minimum amplitude once target is on, so rounding doesn't kill it
        if (amp < FX_SHAKE_MIN_PX)
            amp = FX_SHAKE_MIN_PX;
        if (amp > FX_SHAKE_MAX_PX)
            amp = FX_SHAKE_MAX_PX;

        float max_amp = FX_SHAKE_MAX_PX * (1.0f + 0.6f * punch_k); // allow brief overshoot
        if (amp > max_amp)
            amp = max_amp;

        f->shake_dx = sinf(f->shake_phase * FX_SHAKE_SIN_MUL_X) * amp;
        f->shake_dy = cosf(f->shake_phase * FX_SHAKE_SIN_MUL_Y) * amp;
    }

    if (f->punch_t > 0.0f)
    {
        f->punch_t -= dt;
        float t = (f->punch_ttl > 0.0001f) ? (f->punch_t / f->punch_ttl) : 0.0f; // 1..0
        if (t < 0.0f)
            t = 0.0f;

        // Quick ease-out punch
        float k = t * t;

        // Add on top of existing shake
        f->shake_dx += (sinf((1.0f - t) * 40.0f) * f->punch_amp) * k;
        f->shake_dy += (cosf((1.0f - t) * 43.0f) * f->punch_amp) * k;
    }

    // Particle spawn rate: fewer, cleaner streaks
    float spawn_per_sec = FX_PARTICLES_BASE_RATE +
                          (FX_PARTICLES_MAX_RATE - FX_PARTICLES_BASE_RATE) * f->fx;
    f->spawn_accum += spawn_per_sec * dt;

    // Board rect sanity: if invalid, disable particles
    if (board_rect.w <= 0 || board_rect.h <= 0)
    {
        f->spawn_accum = 0.0f;
    }

    // Precompute board center (used for outward direction)
    float cx = (float)(board_rect.x + board_rect.w * 0.5f);
    float cy = (float)(board_rect.y + board_rect.h * 0.5f);

    while (f->spawn_accum >= 1.0f)
    {
        f->spawn_accum -= 1.0f;
        if (f->p_count >= WARP_MAX)
            break;

        WarpParticle *p = &f->p[f->p_count++];

        // Pick a random point on the board perimeter
        float perim = 2.0f * (float)(board_rect.w + board_rect.h);
        float t = ((float)rand() / (float)RAND_MAX) * perim;

        float x, y;
        if (t < board_rect.w)
        {
            // top edge
            x = (float)board_rect.x + t;
            y = (float)board_rect.y;
        }
        else if (t < board_rect.w + board_rect.h)
        {
            // right edge
            x = (float)(board_rect.x + board_rect.w);
            y = (float)board_rect.y + (t - board_rect.w);
        }
        else if (t < 2.0f * board_rect.w + board_rect.h)
        {
            // bottom edge
            x = (float)board_rect.x + (2.0f * board_rect.w + board_rect.h - t);
            y = (float)(board_rect.y + board_rect.h);
        }
        else
        {
            // left edge
            x = (float)board_rect.x;
            y = (float)board_rect.y + (perim - t);
        }

        // Nudge slightly outward from the board (so they start on/just outside border)
        // Compute outward direction from center -> point
        float dx = x - cx;
        float dy = y - cy;
        float dlen = sqrtf(dx * dx + dy * dy);
        if (dlen < 0.001f)
        {
            dx = 1.0f;
            dy = 0.0f;
            dlen = 1.0f;
        }
        dx /= dlen;
        dy /= dlen;

        float border_push = 2.0f + 6.0f * f->fx;
        x += dx * border_push;
        y += dy * border_push;

        // Velocity: outward + slight jitter so it feels alive
        float jitter = 0.25f * (1.0f - f->fx); // less jitter at higher speed
        float jx = (((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f) * jitter;
        float jy = (((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f) * jitter;

        float odx = dx + jx;
        float ody = dy + jy;
        float olen = sqrtf(odx * odx + ody * ody);
        if (olen < 0.001f)
        {
            odx = dx;
            ody = dy;
            olen = 1.0f;
        }
        odx /= olen;
        ody /= olen;

        float speed = FX_PARTICLE_SPEED_MIN +
                      (FX_PARTICLE_SPEED_MAX - FX_PARTICLE_SPEED_MIN) * f->fx;

        p->x = x;
        p->y = y;
        p->vx = odx * speed;
        p->vy = ody * speed;
        p->ttl = FX_PARTICLE_TTL_MIN +
                 ((float)rand() / (float)RAND_MAX) * FX_PARTICLE_TTL_RAND;
        p->life = p->ttl;

        p->len = FX_PARTICLE_LEN_MIN +
                 (FX_PARTICLE_LEN_MAX - FX_PARTICLE_LEN_MIN) * f->fx;

        p->a = (unsigned char)(FX_PARTICLE_ALPHA_MIN +
                               (FX_PARTICLE_ALPHA_MAX - FX_PARTICLE_ALPHA_MIN) * f->fx);
    }

    // Update & compact
    int w = 0;
    for (int i = 0; i < f->p_count; i++)
    {
        WarpParticle *p = &f->p[i];
        p->life -= dt;
        if (p->life <= 0.0f)
            continue;

        p->x += p->vx * dt;
        p->y += p->vy * dt;

        // Cull when far outside screen
        if (p->x < -200.0f || p->x > (float)f->w + 200.0f ||
            p->y < -200.0f || p->y > (float)f->h + 200.0f)
        {
            continue;
        }

        f->p[w++] = *p;
    }
    f->p_count = w;
}

void speedfx_apply_shake_rect(const SpeedFX *f, SDL_Rect *dst)
{
    if (!f || !dst)
        return;
    int dx = (int)lroundf(f->shake_dx);
    int dy = (int)lroundf(f->shake_dy);

// If we are shaking, avoid getting stuck at 0 due to rounding noise
#if FX_SHAKE_APPLY_PIXELS
    if (dx == 0 && fabsf(f->shake_dx) > 0.001f)
        dx = (f->shake_dx > 0.0f) ? 1 : -1;
    if (dy == 0 && fabsf(f->shake_dy) > 0.001f)
        dy = (f->shake_dy > 0.0f) ? 1 : -1;
#endif

    dst->x += dx;
    dst->y += dy;
}

void speedfx_render_particles(SDL_Renderer *ren, const SpeedFX *f)
{
    if (!ren || !f)
        return;

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_ADD);

    for (int i = 0; i < f->p_count; i++)
    {
        const WarpParticle *p = &f->p[i];
        if (f->fx <= 0.01f)
        {
            return;
        }
        float t = (p->ttl > 0.0001f) ? (p->life / p->ttl) : 0.0f; // 1..0
        if (t < 0.0f)
            t = 0.0f;

        unsigned char a = (unsigned char)((float)p->a * t);
        SDL_SetRenderDrawColor(ren, 180, 200, 255, a);

        float vlen = sqrtf(p->vx * p->vx + p->vy * p->vy);
        float nx = (vlen > 0.001f) ? (p->vx / vlen) : 0.0f;
        float ny = (vlen > 0.001f) ? (p->vy / vlen) : 0.0f;

        int x1 = (int)lroundf(p->x);
        int y1 = (int)lroundf(p->y);
        int x2 = (int)lroundf(p->x - nx * p->len);
        int y2 = (int)lroundf(p->y - ny * p->len);

        SDL_RenderDrawLine(ren, x1, y1, x2, y2);
    }

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
}

void speedfx_combo_punch(SpeedFX *f, float tier, float ttl_sec)
{
    if (!f)
        return;
    float amp = 0.6f + 0.18f * tier; // scale with tier
    if (amp > 2.2f)
        amp = 2.2f;

    f->punch_ttl = ttl_sec;
    f->punch_t = ttl_sec;
    f->punch_amp = amp;
}

void speedfx_combo_ring(SpeedFX *f, float x, float y, int tier, float amp)
{
    if (!f)
        return;

    if (amp < 0.2f)
        amp = 0.2f;
    if (amp > 2.0f)
        amp = 2.0f;

    // If full, overwrite the oldest (index 0) by shifting down (simple + fine at small N)
    if (f->ring_count >= COMBO_RING_MAX)
    {
        for (int i = 1; i < f->ring_count; i++)
            f->ring[i - 1] = f->ring[i];
        f->ring_count = COMBO_RING_MAX - 1;
    }

    ComboRing *q = &f->ring[f->ring_count++];

    q->x = x;
    q->y = y;

    // Lifetime scales slightly with tier
    q->ttl = 0.22f + 0.03f * (float)tier;
    if (q->ttl > 0.45f)
        q->ttl = 0.45f;
    q->t = q->ttl;

    // Start small, expand fast (laser shockwave feel)
    q->r = 2.0f;
    q->vr = (900.0f + 140.0f * tier) * amp; // px/sec
    q->thick = (2.0f + 0.45f * tier) * amp; // px

    // Peak alpha
    q->a = 140.0f + 14.0f * tier;
    if (q->a > 230.0f)
        q->a = 230.0f;
}

void speedfx_update_rings(SpeedFX *f, float dt)
{
    if (!f)
        return;

    int w = 0;
    for (int i = 0; i < f->ring_count; i++)
    {
        ComboRing *q = &f->ring[i];

        q->t -= dt;
        if (q->t <= 0.0f)
            continue;

        q->r += q->vr * dt;

        f->ring[w++] = *q;
    }
    f->ring_count = w;
}

void speedfx_render_rings(SDL_Renderer *ren, const SpeedFX *f, Uint8 cr, Uint8 cg, Uint8 cb)
{
    if (!ren || !f)
        return;
    if (f->ring_count <= 0)
        return;

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_ADD);

    for (int i = 0; i < f->ring_count; i++)
    {
        const ComboRing *q = &f->ring[i];

        float u = (q->ttl > 0.0001f) ? (q->t / q->ttl) : 0.0f; // 1..0
        if (u < 0.0f)
            u = 0.0f;

        // Fade out; keep a punchy front
        float k = u * u; // stronger at start
        Uint8 a = (Uint8)(q->a * k);

        SDL_SetRenderDrawColor(ren, cr, cg, cb, a);

        // Segment count based on radius (clamped)
        int seg = (int)(q->r * 0.35f);
        if (seg < 24)
            seg = 24;
        if (seg > 96)
            seg = 96;

        // Thickness: draw multiple concentric circles
        int layers = (int)lroundf(q->thick);
        if (layers < 1)
            layers = 1;
        if (layers > 8)
            layers = 8;

        for (int L = 0; L < layers; L++)
        {
            float rr = q->r - (q->thick * 0.5f) + (float)L;

            float prevx = q->x + cosf(0.0f) * rr;
            float prevy = q->y + sinf(0.0f) * rr;

            for (int s = 1; s <= seg; s++)
            {
                float ang = (float)s * (2.0f * PI_F / (float)seg);
                float x = q->x + cosf(ang) * rr;
                float y = q->y + sinf(ang) * rr;

                SDL_RenderDrawLine(
                    ren,
                    (int)lroundf(prevx), (int)lroundf(prevy),
                    (int)lroundf(x), (int)lroundf(y));

                prevx = x;
                prevy = y;
            }
        }
    }

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
}
