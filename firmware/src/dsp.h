/* SPDX-License-Identifier: GPL-3.0-only
 * TONITOR: fixed-point helpers. The target (JieLi AC79, pi32v2) has no FPU and the build links no
 * compiler runtime: no float, no double, no 64-bit division or modulo anywhere. 32-bit divides are in
 * hardware (kept out of the per-sample loops); 64-bit products are fine.
 *
 * Number formats used across the DSP:
 *   samples      int32, 1.0 = 1 << 15 (Q15), headroom above it
 *   phases       uint32, 2^32 = one cycle
 *   controllers  0 .. 32768 (Q15)
 */
#pragma once
#include <stdint.h>

/* big zero-initialised buffers go to the pool (app.ld: 336 KiB beside the 96 KiB of .data / .bss) */
/* always inline: the audio's RAM code (FAST) may not call into the flash */
#define AINL static inline __attribute__((always_inline))
#ifdef HOST
#define POOL
#define FAST                     /* (the host: no RAM code) */
#define DLYMEM
#else
#define POOL __attribute__((section(".pool")))
#define FAST __attribute__((section(".fast_text"), noinline))   /* run from RAM (app.ld): the audio's hottest code */
#define DLYMEM __attribute__((section(".dly_mem")))   /* data in the RAM beside the code (app.ld .dly): the delay's */
#endif

#define Q15 32768
#define Q16 65536

/* a layer's generative path: what HOME's draws lean towards, on every engine (the open path: no leaning) */
enum { PATH_OPEN, PATH_AMBIENT, PATH_EXP, PATH_N };
/* the generators' generation: a sound is drawn again from (seed, path, gen) bit for bit, so a sound saved as its
 * seed (sessions, presets) keeps its generation, and every generation stays as it was (tests/golden: 0 is the
 * generators up to 0.20). New sounds are drawn on GEN_NOW. */
#define GEN_NOW 6u
#define GEN_N 7u                 /* 0: up to 0.20; 1: 0.21 (more families, kinds, species); 2: FM's ratios as meant; 3: 0.29 (plucks, mallets, warps, glitches, rubber; eight ANIMA kinds more, and breadth); 4: 0.47 (FM's IMPACT and GLITCH more various); 5: 0.56, after the SMK-37's build 25 (FM's GLISSE and RUBBER folded into WARP); 6: 0.56, after its build 27 (FM's five families learned from a DX7 collection, fm_dx7.c) */
/* EDIT's four macros on a sound: -MAC_MAX .. MAC_MAX each, 0 = the sound as it was drawn */
#define MAC_MAX 64
#define NMAC 4u

AINL int32_t clampi(int32_t x, int32_t lo, int32_t hi) { return x < lo ? lo : x > hi ? hi : x; }
AINL int32_t mulq15(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b) >> 15); }
AINL int32_t mulq16(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b) >> 16); }
AINL int32_t mulq12(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b) >> 12); }
AINL int32_t absi(int32_t x) { return x < 0 ? -x : x; }
AINL int32_t maxi(int32_t a, int32_t b) { return a > b ? a : b; }

/* 2^(x / 65536), Q16 in and out; x from -16 to +15 octaves */
AINL uint32_t exp2_q16(int32_t x)
{
    int32_t i = x >> 16;
    uint32_t f = (uint32_t)x & 0xFFFFu, k = f >> 8, a = EXP2_FRAC[k];
    uint32_t m = a + (((EXP2_FRAC[k + 1u] - a) * (f & 0xFFu)) >> 8);   /* 2^frac, Q16 (65536 .. 131072) */
    if (i >= 15)
        return 0x7FFFFFFFu;
    if (i >= 0)
        return m << i;
    if (i < -24)
        return 0;
    return m >> -i;
}

/* log2(x), x > 0 an integer; Q16 result */
AINL int32_t log2_q16(uint32_t x)
{
    int32_t e = 0;
    uint32_t f, k;
    if (!x)
        return -(32 << 16);
    while (x >= (2u << 15)) {               /* bring x into 1 .. 2 as Q15 (32768 .. 65535) */
        x >>= 1;
        e++;
    }
    while (x < (1u << 15)) {
        x <<= 1;
        e--;
    }
    f = (x - (1u << 15)) << 1;              /* the fraction, Q16 */
    k = f >> 8;
    return ((e + 15) << 16) + (int32_t)(LOG2_FRAC[k] + (((LOG2_FRAC[k + 1u] - LOG2_FRAC[k]) * (f & 0xFFu)) >> 8));
}

/* tanh of a Q15 value (any range), Q15 */
AINL int32_t tanh_q15(int32_t x)
{
    int32_t a = x < 0 ? -x : x, i, f, y;
    if (a >= 4 * Q15)
        y = TANH_Q15[512];
    else {
        i = a >> 8;                          /* 4.0 = 512 steps: one step = 1/128 = 256 in Q15 */
        f = a & 255;
        y = TANH_Q15[i] + (((TANH_Q15[i + 1] - TANH_Q15[i]) * f) >> 8);
    }
    return x < 0 ? -y : y;
}

/* sine of a phase (2^32 = one cycle), Q15, linear between SINE's 1024 points */
AINL int32_t sin_ph(uint32_t ph)
{
    const int16_t *s = &SINE[ph >> 22];
    return s[0] + (((s[1] - s[0]) * (int32_t)((ph >> 7) & 0x7FFFu)) >> 15);
}

/* integer square root of a 32-bit value */
AINL uint32_t isqrt32(uint32_t x)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > x)
        b >>= 2;
    while (b) {
        if (x >= r + b) {
            x -= r + b;
            r = (r >> 1) + b;
        } else
            r >>= 1;
        b >>= 2;
    }
    return r;
}

/* (hi:lo) / d, 64 by 32 bits, by shift and subtract on 32-bit halves (no runtime call, no 64-bit shift by
 * a variable); saturates at 0xFFFFFFFF. Rare paths only (a note-on). */
static uint32_t udiv64_32(uint32_t hi, uint32_t lo, uint32_t d)
{
    uint32_t r, q = 0, carry;
    int i;
    if (!d || hi >= d)
        return 0xFFFFFFFFu;
    r = hi;
    for (i = 31; i >= 0; i--) {
        carry = r >> 31;
        r = (r << 1) | ((lo >> i) & 1u);
        q <<= 1;
        if (carry || r >= d) {
            r -= d;
            q |= 1u;
        }
    }
    return q;
}

/* the phase increment of a frequency in Hz Q8 (2^32 / FS per hertz = 97391.5) */
AINL uint32_t hz8_inc(int32_t hz8)
{
    if (hz8 <= 0)
        return 0;
    return (uint32_t)(((uint64_t)(uint32_t)hz8 * 24933460u) >> 16);   /* x (2^32 / FS) / 256, as Q16 */
}
/* .. and back: Hz Q8 of a phase increment */
static inline int32_t inc_hz8(uint32_t inc) { return (int32_t)(((uint64_t)inc * 11289600u) >> 32); }   /* x FS * 256 / 2^32 */

/* the state-variable filter's g = tan(pi f / FS), Q24, for f in Hz (integer) */
AINL int32_t svf_g(int32_t hz)
{
    uint32_t x, i, f8;
    hz = clampi(hz, 16, TANG_FMAX);
    x = (uint32_t)hz * 512u;                 /* the table is linear in f: 512 steps up to TANG_FMAX */
    i = x / TANG_FMAX;
    if (i >= 512u)
        return (int32_t)TAN_G[512];
    f8 = (x - i * TANG_FMAX) * 256u / TANG_FMAX;
    return (int32_t)(TAN_G[i] + (((TAN_G[i + 1u] - TAN_G[i]) * f8) >> 8));
}

static inline int32_t mul24(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b) >> 24); }
/* rounded, for the filters' integrators: a floor's bias, summed by an integrator at a low cutoff, is DC */
AINL int32_t mulr16(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b + (1 << 15)) >> 16); }
AINL int32_t mulr24(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b + (1 << 23)) >> 24); }

/* A zero-delay state-variable filter (Simper / Cytomic), as AnimaCore.h Svf: coefficients once a block
 * (one 32-bit divide), then per sample. a1 Q16, a2 a3 k Q24. */
typedef struct { int32_t ic1, ic2; } svf_t;
typedef struct { int32_t a1, a2, a3, k; } svf_c;
AINL void svf_coef(svf_c *c, int32_t hz, int32_t q12)   /* q in Q12, 0.5 .. 25 */
{
    int32_t g = svf_g(hz), den;
    q12 = clampi(q12, 2048, 25 * 4096);
    c->k = (int32_t)((0xFFFFFFFFu / (uint32_t)q12) << 4);   /* 1 / q, Q24 */
    den = Q16 + (int32_t)(((int64_t)g * (g + c->k)) >> 32);   /* 1 + g (g + k), Q16 */
    c->a1 = (int32_t)(0xFFFFFFFFu / (uint32_t)den);           /* 1 / den, Q16 */
    c->a2 = (int32_t)(((int64_t)g * c->a1) >> 16);           /* g a1, Q24 */
    c->a3 = mul24(g, c->a2);                                 /* g a2, Q24 */
}
/* one step: type 1 low-pass, 2 band-pass, 3 high-pass */
AINL int32_t svf_run(svf_t *s, const svf_c *c, int32_t x, int type)
{
    int32_t v3 = x - s->ic2;
    int32_t v1 = mulr16(c->a1, s->ic1) + mulr24(c->a2, v3);
    int32_t v2 = s->ic2 + mulr24(c->a2, s->ic1) + mulr24(c->a3, v3);
    s->ic1 = 2 * v1 - s->ic1;
    s->ic2 = 2 * v2 - s->ic2;
    if (type == 1)
        return v2;
    if (type == 2)
        return v1;
    return x - mulr24(c->k, v1) - v2;
}

/* a small fast random generator (xorshift32): per sample noise, the generators' draws */
typedef struct { uint32_t s; } rnd_t;
AINL uint32_t rnd_next(rnd_t *r)
{
    uint32_t x = r->s ? r->s : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    r->s = x;
    return x;
}
static inline void rnd_seed(rnd_t *r, uint32_t seed)
{
    uint32_t i;
    r->s = seed * 2654435761u + 0x6A09E667u;
    if (!r->s)
        r->s = 1;
    for (i = 0; i < 4u; i++)
        rnd_next(r);
}
AINL int32_t rnd_q15(rnd_t *r) { return (int32_t)(rnd_next(r) >> 17); }               /* 0 .. 32767 */
AINL int32_t rnd_bi(rnd_t *r) { return (int32_t)(rnd_next(r) >> 16) - 32768; }        /* -32768 .. 32767 */
AINL int32_t rnd_below(rnd_t *r, int32_t n) { return n <= 1 ? 0 : (int32_t)(((uint64_t)rnd_next(r) * (uint32_t)n) >> 32); }
AINL int rnd_chance(rnd_t *r, int32_t p_pct) { return rnd_below(r, 1000) < p_pct * 10; }   /* p in percent */
/* one of n, each as likely as its weight */
static inline uint32_t rnd_weighted(rnd_t *r, const uint8_t *w, uint32_t n)
{
    uint32_t i, t = 0, x;
    for (i = 0; i < n; i++)
        t += w[i];
    x = (uint32_t)rnd_below(r, (int32_t)t);
    for (i = 0; i + 1u < n && x >= w[i]; i++)
        x -= w[i];
    return i;
}
static inline int rnd_chance_q15(rnd_t *r, int32_t p) { return rnd_q15(r) < p; }
/* uniform in [a, b] (any integer unit) */
AINL int32_t rnd_in(rnd_t *r, int32_t a, int32_t b)
{
    return a + (int32_t)(((int64_t)(b - a) * rnd_q15(r)) >> 15);
}
