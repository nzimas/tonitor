/* SPDX-License-Identifier: GPL-3.0-only
 * TONITOR: the granulator, one a layer (KNOB 2), after the layer's filter.
 *
 * The layer's sound is written, always, into a buffer of about half a second (at half the rate, 22.05 kHz,
 * in 8-bit mu-law: 12 KiB a layer); grains are read back out of it, and the knob, bipolar, decides what kind.
 * The middle is off -- the layer as it is. Turned:
 *
 *   left   towards dark ambient: long grains (60 .. 400 ms), overlapping, slow, drifting in position and
 *          size, pitched down now and then (an octave, a fifth, the profile's intervals), reversed now and
 *          then, darkened by a low-pass that closes as it is turned, fed back into the buffer so the cloud
 *          smears on itself; and, as the profile has it, a freeze now and then: the buffer stops listening
 *          for a few seconds, and the cloud turns over what it holds
 *   right  towards scattering: short grains (40 .. 4 ms), faster and faster (to ~650 a second), their start,
 *          timing, pitch and level jittered more and more, scattered across the whole buffer, reversed,
 *          stuttered (a grain repeated in place), dropped out (a gate full of holes), with a percussive or
 *          a swelling window, and at the far end decimated (each sample held)
 *
 * The further from the middle, the more of it and the less of the layer, until only the grains are heard.
 * Each layer's granulator has a character of its own, drawn at random when the FM-1 starts and when a session
 * is loaded (gran_profile): its intervals, how often it reverses, stutters, drops out, freezes, the shape of
 * its bright grains, how far its feedback and its darkening go, how it drifts. */

#define GR_N 12288u              /* the buffer: samples at FS / 2 (0.557 s) */
#define GR_MAXG 6u               /* grains at once */
#define GR_TAIL 3000u            /* blocks the grains go on after the layer's last voice (~2.2 s) */

typedef struct {
    int32_t pos, inc;            /* the read head in the buffer (Q16 samples) and its step a sample */
    uint32_t wph, winc;          /* the window's phase (a grain = 2^32) and step */
    int32_t gain;                /* Q15 */
    uint8_t on, shape;           /* 0 Hann, 1 percussive, 2 swell */
} grain_t;

typedef struct {
    /* the character (gran_profile) */
    int8_t pd[4], pb[4];         /* intervals, semitones: dark, bright */
    uint8_t rev, stut, drop, freeze;   /* chances, percent (at the knob's far end) */
    uint8_t shape;               /* the bright grains' window: 0 Hann, 1 percussive, 2 swell */
    int32_t fb;                  /* the dark side's feedback at its far end, Q15 */
    int32_t dark_hz;             /* the dark side's low-pass at its far end */
    int32_t scat_d;              /* the dark side's scatter, Q15 of the buffer */
    int32_t detune;              /* the bright side's pitch jitter, cents */
    uint32_t drift_inc;          /* the slow drift's step a block (a turn = 2^32) */
    /* the state */
    grain_t g[GR_MAXG];
    uint32_t w;                  /* the write index */
    int32_t half;                /* the sample waiting for its pair (the buffer is at FS / 2) */
    uint8_t odd, frozen;
    uint32_t freeze_left;        /* blocks of a freeze left */
    int32_t next;                /* samples to the next grain */
    int32_t last;                /* the grains' last output (the feedback) */
    svf_t dark;                  /* the darkening low-pass */
    int32_t hold, hold_n;        /* the decimation: the sample held, samples left */
    int32_t amt;                 /* the wet share as it glides, Q15 */
    int32_t stut_pos;            /* a stutter: the start it repeats, and how many times more */
    uint8_t stut_left;
    uint32_t drift;              /* the drift's phase */
    uint32_t tail;
    uint32_t seed;               /* what drew its character (gran_profile): sessions keep it */
    rnd_t r;
} gran_t;

static uint8_t gr_mem[NLAYER][GR_N] POOL;
static gran_t gran[NLAYER];

static inline int32_t gr_dec(uint8_t b)
{
    int32_t m = MULAW_DEC[b & 0x7Fu];
    return b & 0x80u ? -m : m;
}
static inline uint8_t gr_enc(int32_t x)
{
    uint32_t a = (uint32_t)(x < 0 ? -x : x);
    if (a > 32767u)
        a = 32767u;
    return (uint8_t)((x < 0 ? 0x80u : 0u) | MULAW_ENC[a >> 4]);
}

/* a character, drawn */
static void gran_profile(uint32_t l, uint32_t seed)
{
    static const int8_t DARK[8] = {0, 0, -12, -7, -5, -24, 7, -12};
    static const int8_t BRIGHT[8] = {0, 12, 7, 19, -12, 5, 24, 6};
    gran_t *g = &gran[l];
    uint32_t k;
    g->seed = seed;
    rnd_seed(&g->r, seed);
    g->pd[0] = 0;
    g->pb[0] = 0;
    for (k = 1; k < 4u; k++) {
        g->pd[k] = DARK[rnd_below(&g->r, 8)];
        g->pb[k] = BRIGHT[rnd_below(&g->r, 8)];
    }
    g->rev = (uint8_t)rnd_in(&g->r, 0, 55);
    g->stut = (uint8_t)rnd_in(&g->r, 0, 50);
    g->drop = (uint8_t)rnd_in(&g->r, 0, 35);
    g->freeze = (uint8_t)(rnd_chance(&g->r, 60) ? rnd_in(&g->r, 5, 30) : 0);
    g->shape = (uint8_t)rnd_below(&g->r, 3);
    g->fb = rnd_in(&g->r, LV(0.15), LV(0.55));
    g->dark_hz = rnd_in(&g->r, 600, 2000);
    g->scat_d = rnd_in(&g->r, LV(0.1), LV(0.6));
    g->detune = rnd_in(&g->r, 0, 45);
    g->drift_inc = (uint32_t)rnd_in(&g->r, 100000, 600000);   /* 0.03 .. 0.2 Hz at 1378 blocks a second */
}

static void gran_init(void)
{
    uint32_t l, i;
    for (l = 0; l < NLAYER; l++) {
        memset(&gran[l], 0, sizeof gran[l]);
        for (i = 0; i < GR_N; i++)
            gr_mem[l][i] = 0;                            /* (mu-law 0: silence) */
        gran_profile(l, 0x6A0u + l * 7919u);
    }
}

/* a grain, now: from the knob's v (-FLT .. ; a = |v| in Q15), into slot s */
static void gran_spawn(gran_t *g, int32_t v, int32_t a, uint32_t len, int32_t scat, int32_t norm)
{
    grain_t *gr = 0;
    uint32_t k, back, need, dmax;
    int32_t semis, cents = 0, r_q16, inc, delay, start;
    int rev;
    for (k = 0; k < GR_MAXG; k++)
        if (!g->g[k].on) {
            gr = &g->g[k];
            break;
        }
    if (!gr)
        return;
    if (v > 0 && rnd_q15(&g->r) < mulq15(a, LV(0.01) * g->drop))
        return;                                         /* dropped: a hole */
    /* the pitch: the profile's intervals, more of them the further it is turned */
    if (rnd_q15(&g->r) < a) {
        semis = v < 0 ? g->pd[rnd_below(&g->r, 4)] : g->pb[rnd_below(&g->r, 4)];
    } else
        semis = 0;
    if (v > 0)
        cents = mulq15(rnd_bi(&g->r), mulq15(a, g->detune));   /* jitter */
    r_q16 = (int32_t)exp2_q16((semis * 100 + cents) * 65536 / 1200);
    inc = r_q16 >> 1;                                  /* the buffer is at half the rate */
    rev = rnd_q15(&g->r) < mulq15(a, LV(0.01) * g->rev);
    /* where it starts: far enough back that the read head never meets the write head */
    back = (uint32_t)(((uint64_t)len * (uint32_t)inc) >> 16);   /* buffer samples the grain reads */
    need = rev ? 4u : (r_q16 > Q16 ? (uint32_t)(((uint64_t)len * (uint32_t)(inc - Q16 / 2)) >> 16) + 8u : 8u);
    /* .. and not so far back that the write head comes round to it: a reversed grain goes away from it, a slow
     * one falls behind it */
    dmax = GR_N - 64u - (rev ? back + len / 2u : inc < Q16 / 2 ? (uint32_t)(((uint64_t)len * (uint32_t)(Q16 / 2 - inc)) >> 16) : 0u);
    if (need + 16u > dmax)
        return;
    delay = (int32_t)need + mulq15(rnd_q15(&g->r), mulq15(scat, (int32_t)(dmax - need)));
    if (g->stut_left && g->stut_pos >= 0) {             /* a stutter: the same start again */
        start = g->stut_pos;
        g->stut_left--;
    } else {
        start = (int32_t)g->w - delay;
        if (start < 0)
            start += GR_N;
        if (v > 0 && rnd_q15(&g->r) < mulq15(a, LV(0.01) * g->stut)) {
            g->stut_pos = start;
            g->stut_left = (uint8_t)(1 + rnd_below(&g->r, 4));
        }
    }
    gr->pos = start << 16;
    gr->inc = rev ? -inc : inc;
    gr->wph = 0;
    gr->winc = 0xFFFFFFFFu / (len ? len : 1u);
    gr->shape = (uint8_t)(v < 0 ? 0 : g->shape);
    {   /* level: overlapping grains add up; and, scattering, jittered */
        int32_t gain = norm;
        if (v > 0)
            gain = mulq15(gain, Q15 - mulq15(rnd_q15(&g->r), a >> 1));
        gr->gain = gain;
    }
    gr->on = 1;
}

/* one block of layer l's granulator over x (in place); v the knob (-FLT_MAX .. FLT_MAX) */
static void gran_block(uint32_t l, int32_t *x, uint32_t n, int32_t v, int any)
{
    gran_t *g = &gran[l];
    int32_t a = (v < 0 ? -v : v) * (Q15 / FLT_MAX), amt1 = a * 2 > Q15 ? Q15 : a * 2, da;
    int32_t len_ms, int_ms, jit, scat, fb = 0, lpk = 0, crush = 0, overlap, norm;   /* lpk: the darkening's Hz */
    svf_c dc;
    uint32_t i, k, len, interval, k2;
    if (any)
        g->tail = GR_TAIL;
    else if (g->tail)
        g->tail--;
    /* the drift: a slow wander of size and place (the dark side mostly) */
    g->drift += g->drift_inc;
    if (v < 0) {                                         /* dark */
        int32_t dr = sin_ph(g->drift);
        len_ms = 60 + mulq15(a, 340);
        len_ms += mulq15(mulq15(dr, a), len_ms >> 2);
        overlap = 2 + mulq15(a, 3);
        int_ms = len_ms / overlap;
        jit = LV(0.25);
        scat = mulq15(g->scat_d, Q15 / 2 + (dr >> 2) + (a >> 1));
        fb = mulq15(g->fb, a);
        /* the darkening: a two-pole low-pass closing four octaves, exponentially, down to the profile's
         * (600 .. 2000 Hz) */
        lpk = (int32_t)(((int64_t)g->dark_hz * exp2_q16((Q15 - a) * 8)) >> 16);
        /* a freeze: the profile's chance a second (at the far end), a block at a time */
        if (g->freeze && !g->frozen && rnd_q15(&g->r) < mulq15(a, g->freeze) * 238 / 1000)
            g->frozen = 1, g->freeze_left = (uint32_t)rnd_in(&g->r, 1378, 1378 * 4);   /* 1 .. 4 s */
    } else {                                             /* scatter */
        len_ms = 40 - mulq15(a, 36);
        int_ms = 25 - mulq15(a, 23);
        jit = LV(0.3) + mulq15(a, LV(0.6));
        scat = LV(0.15) + mulq15(a, LV(0.85));
        fb = mulq15(LV(0.1), a);
        if (a > LV(0.75))
            crush = 1 + ((a - LV(0.75)) >> 12);         /* the far end: decimated, 1 .. 3 samples held */
    }
    if (g->frozen && (!g->freeze_left-- || v >= 0))
        g->frozen = 0;
    len = (uint32_t)len_ms * 441u / 10u;
    interval = (uint32_t)(int_ms > 1 ? int_ms : 1) * 441u / 10u;
    if (interval < 66u)
        interval = 66u;                                  /* at most ~650 grains a second */
    /* the grains' level: their power adds up with the overlap O = len / interval, a Hann grain's mean square
     * being 3/8: each at sqrt(8 / (3 O)) -- above 1 for sparse ones (at most x 2), less for a dense cloud */
    k2 = (interval << 15) / (3u * (len ? len : 1u));     /* 8 interval / (3 len), Q12 */
    if (k2 > 65535u)
        k2 = 65535u;
    norm = (int32_t)isqrt32(k2 << 16) << 1;             /* its square root, Q15 */
    norm = clampi(norm, 4096, 2 * Q15);
    if (v > 0)                                           /* the holes, the jitter, the sharp windows: made up */
        norm = mulq15(norm, Q15 + a + (a >> 2));
    if (lpk)
        svf_coef(&dc, clampi(lpk, 200, 18000), 2896);
    da = (amt1 - g->amt) / (int32_t)n;
    for (i = 0; i < n; i++) {
        int32_t in = x[i], sum = 0, wet;
        /* the buffer listens (unless frozen), with the grains fed back */
        if (!g->frozen) {
            int32_t s = in + mulq15(g->last, fb);
            if (g->odd) {
                gr_mem[l][g->w] = gr_enc((g->half + s) >> 1);
                if (++g->w >= GR_N)
                    g->w = 0;
            } else
                g->half = s;
            g->odd ^= 1u;
        }
        g->amt += da;
        if (!g->amt && !amt1) {
            g->last = 0;
            continue;                                    /* off: the layer as it is */
        }
        if (--g->next <= 0 && v) {                       /* the next grain */
            int32_t j = mulq15(rnd_bi(&g->r), jit);
            gran_spawn(g, v, a, len, scat, norm);
            g->next = (int32_t)interval + mulq15((int32_t)interval, j);
            if (g->next < 8)
                g->next = 8;
        }
        for (k = 0; k < GR_MAXG; k++) {
            grain_t *gr = &g->g[k];
            int32_t p, f, s0, s1, w, t;
            uint32_t i0, i1, was;
            if (!gr->on)
                continue;
            p = gr->pos;
            i0 = (uint32_t)p >> 16;
            i1 = i0 + 1u >= GR_N ? 0u : i0 + 1u;
            f = (p & 0xFFFF) >> 1;
            s0 = gr_dec(gr_mem[l][i0]);
            s1 = gr_dec(gr_mem[l][i1]);
            t = (int32_t)(gr->wph >> 17);                /* 0 .. Q15 through the grain */
            if (gr->shape == 1)                          /* percussive: a quick rise, a fall */
                w = t < 2048 ? t * 16 : mulq15(Q15 - t, Q15 - t);
            else if (gr->shape == 2)                     /* swell: a slow rise, a quick fall */
                w = t > 30720 ? (Q15 - t) * 16 : mulq15(t, t);
            else {                                       /* Hann */
                w = sin_ph(gr->wph >> 1);
                w = mulq15(w, w);
            }
            sum += mulq15(mulq15(s0 + (((s1 - s0) * f) >> 15), w), gr->gain);
            p += gr->inc;
            if (p < 0)
                p += (int32_t)(GR_N << 16);
            else if (p >= (int32_t)(GR_N << 16))
                p -= (int32_t)(GR_N << 16);
            gr->pos = p;
            was = gr->wph;
            gr->wph += gr->winc;
            if (gr->wph < was)
                gr->on = 0;
        }
        wet = lpk ? svf_run(&g->dark, &dc, sum, 1) : sum;   /* darkened (the dark side) */
        if (crush) {
            if (g->hold_n-- <= 0) {
                g->hold = wet;
                g->hold_n = crush;
            }
            wet = g->hold;
        }
        g->last = wet;
        x[i] = in + mulq15(wet - in, g->amt);
    }
    g->amt = amt1;
}
