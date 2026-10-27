/* SPDX-License-Identifier: GPL-3.0-only
 * TONITOR: DRUID, the fifth engine -- strings and resonant bodies, after Mutable Instruments Rings by Emilie Gillet
 * (pichenettes/eurorack, rings/dsp: string.cc, resonator.cc, part.cc, plucker.h; MIT licence, its notice in
 * LICENSES/MIT-rings.txt). Its three resonators, ported to fixed point for a chip without floating point:
 *   STRING       a Karplus-Strong string: a delay read with Hermite interpolation; its dispersion (an all-pass
 *                stage stretching the partials, delay noise, or below the middle a curved bridge buzzing, as a
 *                sitar's); damping by a short FIR (brightness) and a state-variable low-pass, the decay set as a
 *                time to -60 dB; the pickup a comb at the position along the string
 *   SYMPATHETIC  a string and three more tuned by STRUCTURE to intervals of it (octaves, fifths, a twelfth), driven
 *                only by the first string's sound, their position wandering slowly
 *   MODAL        a bank of up to 16 two-pole resonators at the partials of a bar or plate (STRUCTURE stretches or
 *                squeezes them, from harmonic to bell-like), each partial's level by the position
 * Rings is played from audio; the FM-1 has no input, so the strings are played by internal exciters: PLUCK (Rings'
 * plucker: a burst of noise as long as the position's share of the period, through a comb and a low-pass), STRIKE
 * (a mallet: a pulse, softer as BRIGHTNESS goes down), BOW (noise while the key is held, swelling in and dying
 * away) and DUST (random impacts while the key is held: rattles). The four macros are Rings' four knobs:
 * STRUCTURE, BRIGHTNESS, DAMPING, POSITION. Letting go of a key damps the string by the design's own amount (from
 * ringing on to stopping short).
 *
 * Each voice's delay lines (16-bit) share the memory of an ANIMA voice (layers.c vmem_t): a voice is one engine at
 * a time. A string down to ~29 Hz with its stretch line; the sympathetic strings shorter, an octave up where they
 * would not fit. */

#define DR_MEM 2176u             /* 16-bit samples a voice */
#define DR_MODES 16u
#define DR_NSTR 4u

enum { DM_STRING, DM_SYMPA, DM_MODAL };
enum { DX_PLUCK, DX_STRIKE, DX_BOW, DX_DUST };
/* the kinds: a resonator and an exciter, with their ranges */
enum { DK_PLUCK, DK_BOWED, DK_STRUCK, DK_SITAR, DK_RATTLE, DK_HARP, DK_DRONE, DK_BELL, DK_BOWL, DK_PLATE, DK_N };
static const char *const DK_NAME[DK_N] = {"PLUCK", "BOWED", "STRUCK", "SITAR", "RATTLE", "HARP", "DRONE", "BELL",
                                          "BOWL", "PLATE"};
static const uint8_t DK_MODEL[DK_N] = {DM_STRING, DM_STRING, DM_STRING, DM_STRING, DM_STRING, DM_SYMPA, DM_SYMPA,
                                       DM_MODAL, DM_MODAL, DM_MODAL};
static const uint8_t DK_EXC[DK_N] = {DX_PLUCK, DX_BOW, DX_STRIKE, DX_PLUCK, DX_DUST, DX_PLUCK, DX_BOW, DX_STRIKE,
                                     DX_BOW, DX_DUST};
/* STRUCTURE's range, a kind each (Q15 / 100) */
static const uint8_t DK_ST[DK_N][2] = {{26, 55}, {25, 45}, {55, 85}, {0, 18}, {30, 95}, {20, 80}, {30, 90}, {60, 95},
                                       {40, 70}, {75, 100}};
static const uint8_t DK_W[PATH_N][DK_N] = {
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {2, 3, 1, 1, 0, 3, 4, 3, 4, 0},
    {1, 2, 2, 2, 4, 1, 1, 2, 1, 4},
};

typedef struct {
    uint32_t seed;               /* what made it: dr_design(kind, seed, path, gen) + dr_measure give it again */
    uint8_t kind, path, gen, model, exc, rsv[3];
    int32_t structure, brightness, damping, position;   /* Rings' four, Q15 */
    int32_t mute;                /* letting go: the damping brought down by this share, Q15 */
    int32_t bow_att;             /* BOW: its swell, ms */
    int32_t level;               /* BOW, DUST: how hard, Q15 */
    int32_t rate;                /* DUST: impacts a second, Q8 */
    int32_t out;                 /* Q12: the measured level brought to the target */
} dr_design_t;

typedef struct { uint16_t base, len, w; } dr_line_t;
typedef struct {
    dr_line_t ln;
    uint32_t inc;                /* its frequency, Q32 of the sample rate */
    int32_t delay;               /* its period, Q16 samples, as it glides */
    int32_t pos;                 /* the pickup's place, Q15 (glides) */
    int32_t dcomp;               /* the low-pass's delay made good, Q16 (glides) */
    int32_t fx1, fx2;            /* the FIR's last two inputs */
    svf_t lp;
    int32_t acx, acy;            /* the dispersion's AC blocker */
    int32_t bridge, dnoise;
    int32_t out1, aux1;
    uint32_t lfo;                /* the sympathetic strings' slow wander */
} dr_str_t;
typedef struct { int32_t y1, y2, a1, a2, b0; } dr_mode_t;   /* y Q15; a1, a2, b0 Q30 */

typedef struct {
    uint8_t live, gate, trig, nmodes;
    uint8_t exq;                 /* blocks the exciter has been silent (then it is not run) */
    int8_t mac[NMAC];            /* EDIT's macros, felt (layers.c sets them each block) */
    int32_t note;
    uint32_t inc;                /* the note's frequency, Q32 */
    rnd_t r;
    svf_t exf, pkf;              /* the exciter's low-pass; the plucker's */
    int32_t exdx, exdy;          /* its DC blocker */
    int32_t burst;               /* the plucker's noise left, samples */
    dr_line_t comb;              /* the plucker's comb */
    int32_t cper, cgain;
    int32_t bow;                 /* the bow's (or dust's) level as it swells, Q15 */
    int32_t pulse;               /* STRIKE: samples of the mallet's pulse left */
    int32_t plen;
    int32_t quiet;               /* blocks the voice has been near silence */
    int32_t mkey;                /* (MODAL: what its coefficients were made for) */
    int16_t amp[DR_MODES];       /* MODAL: each partial's level at the position, Q15 */
    dr_str_t s[DR_NSTR];
    union {
        int16_t mem[DR_MEM];
        dr_mode_t md[DR_MODES];
    } u;
} dr_voice_t;

/* ------------------------------------------------------------- helpers --- */
AINL int32_t dr_sat16(int32_t x) { return x > 32767 ? 32767 : x < -32768 ? -32768 : x; }
AINL void ln_write(dr_voice_t *v, dr_line_t *l, int32_t x)
{
    v->u.mem[l->base + l->w] = (int16_t)dr_sat16(x);
    l->w = (uint16_t)(l->w ? l->w - 1u : l->len - 1u);
}
AINL int32_t ln_at(const dr_voice_t *v, const dr_line_t *l, uint32_t d)   /* d samples ago (stmlib's Read) */
{
    uint32_t i = l->w + d;
    while (i >= l->len)
        i -= l->len;
    return v->u.mem[l->base + i];
}
AINL int32_t ln_lin(const dr_voice_t *v, const dr_line_t *l, int32_t d16)   /* linear, Q16 delay */
{
    uint32_t di = (uint32_t)d16 >> 16;
    int32_t f = d16 & 0xFFFF, a = ln_at(v, l, di), b = ln_at(v, l, di + 1u);
    return a + (int32_t)(((int64_t)(b - a) * f) >> 16);
}
AINL int32_t ln_hermite(const dr_voice_t *v, const dr_line_t *l, int32_t d16)   /* stmlib's ReadHermite */
{
    uint32_t di = (uint32_t)d16 >> 16;
    int32_t f = d16 & 0xFFFF;
    int32_t xm1 = ln_at(v, l, di - 1u), x0 = ln_at(v, l, di), x1 = ln_at(v, l, di + 1u), x2 = ln_at(v, l, di + 2u);
    int32_t c = (x1 - xm1) >> 1, vv = x0 - x1, w = c + vv, a = w + vv + ((x2 - x0) >> 1), bn = w + a;
    int32_t y = (int32_t)(((int64_t)a * f) >> 16) - bn;
    y = (int32_t)(((int64_t)y * f) >> 16) + c;
    return (int32_t)(((int64_t)y * f) >> 16) + x0;
}
/* 2^(e / 65536) for e <= 0, Q30 -- near 1 to 30 bits (a string's loss a period is a hair below 1) */
static int32_t dr_exp2n(int32_t e)
{
    if (e >= 0)
        return 1 << 30;
    if (e > -3277) {                                  /* > -0.05: the series, x = e ln 2 */
        int32_t x = (int32_t)(((int64_t)e * 744261118) >> 16);   /* ln 2, Q30 */
        int32_t x2 = (int32_t)(((int64_t)x * x) >> 30), x3 = (int32_t)(((int64_t)x2 * x) >> 30);
        return (1 << 30) + x + x2 / 2 + x3 / 6;
    }
    return (int32_t)(exp2_q16(e) << 14);
}
/* sin(pi f), f a phase increment Q32 (f < 0.5), Q30 */
static int32_t dr_sinpi(uint32_t f)
{
    uint32_t x = (uint32_t)(((uint64_t)f * 3373259426u) >> 32);   /* pi f, Q30 (x 2^32 / 2^32: pi / 4 x 2^32 = 3373259426) */
    x <<= 2;                                                        /* (pi f Q32 / 4 -> Q30) */
    if (x < 322122547u) {                                           /* < 0.3: x - x^3/6 + x^5/120 */
        int32_t xx = (int32_t)x, x2 = (int32_t)(((int64_t)xx * xx) >> 30), x3 = (int32_t)(((int64_t)x2 * xx) >> 30);
        int32_t x5 = (int32_t)(((int64_t)x3 * x2) >> 30);
        return xx - x3 / 6 + x5 / 120;
    }
    return sin_ph(f >> 1) << 15;
}
/* Stmlib's Squash: a steep S, 0 .. 1 Q15 */
static int32_t dr_squash(int32_t x)
{
    int64_t y;
    int k;
    if (x < Q15 / 2) {
        y = (int64_t)x * 2;
        for (k = 0; k < 4; k++)
            y = (y * y) >> 15;
        return (int32_t)(y / 2);
    }
    y = 2 * Q15 - (int64_t)x * 2;
    for (k = 0; k < 4; k++)
        y = (y * y) >> 15;
    return (int32_t)(Q15 - y / 2);
}
/* a phase increment's period, Q16 samples */
static int32_t dr_period(uint32_t inc) { return inc <= Q16 ? 0x7FFFFFFF : (int32_t)udiv64_32((uint32_t)Q16, 0, inc); }

/* ------------------------------------------------------------ the voice --- */
static void dr_init(dr_voice_t *v, uint32_t seed)
{
    memset(v, 0, sizeof *v);
    rnd_seed(&v->r, seed);
}
/* the lines of a resonator, a string's period from its note */
static void dr_layout(dr_voice_t *v, uint32_t model)
{
    static const uint16_t L[3][DR_NSTR + 2u][2] = {
        {{0, 1536}, {0, 0}, {0, 0}, {0, 0}, {1536, 384}, {1920, 256}},        /* string, -, stretch, comb */
        {{0, 1024}, {1024, 288}, {1312, 288}, {1600, 288}, {0, 0}, {1888, 256}},
        {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}}};
    uint32_t k;
    for (k = 0; k < DR_NSTR; k++) {
        v->s[k].ln.base = L[model][k][0];
        v->s[k].ln.len = L[model][k][1];
        v->s[k].ln.w = 0;
    }
    v->s[1].lfo = 0;
    if (model == DM_STRING) {                          /* (the stretch line in string 1's place: it is not used) */
        v->s[1].ln.base = L[0][4][0];
        v->s[1].ln.len = L[0][4][1];
    }
    v->comb.base = L[model][5][0];
    v->comb.len = L[model][5][1];
    v->comb.w = 0;
}
static void dr_note_on(dr_voice_t *v, const dr_design_t *d, int32_t note, int fresh)
{
    uint32_t k;
    if (fresh || !v->live) {
        rnd_t r = v->r;
        memset(v, 0, sizeof *v - sizeof v->u);
        v->r = r;
        if (d->model == DM_MODAL)
            memset(v->u.md, 0, sizeof v->u.md);
        else
            memset(v->u.mem, 0, sizeof v->u.mem);
        dr_layout(v, d->model);
        v->mkey = -1;
    }
    v->note = note;
    v->inc = pitch_inc(note * 16);
    for (k = 0; k < DR_NSTR; k++)
        if (!v->s[k].delay)
            v->s[k].delay = dr_period(v->inc);
    v->live = 1;
    v->gate = 1;
    v->trig = 1;
    v->quiet = 0;
}
static void dr_note_off(dr_voice_t *v) { v->gate = 0; }

/* the macros on the design's four, Q15 (each half the range either way) */
static int32_t dr_knob(int32_t base, int32_t m) { return clampi(base + m * (Q15 / (2 * MAC_MAX)), 0, Q15); }

/* the excitation: n samples into x (Q15), at the note's frequency */
static void dr_excite(dr_voice_t *v, const dr_design_t *d, int32_t *x, uint32_t n, int32_t bright, int32_t pos,
                      int32_t nstr)
{
    uint32_t i;
    int32_t cut = mulq15(bright, 2 * Q15 - bright);          /* Rings: b (2 - b) */
    int32_t f_hz = (int32_t)(((uint64_t)v->inc * FS) >> 32), fc;
    svf_c ec;
    if (!v->trig && !v->burst && !v->pulse && v->exq > 8u &&
        !(d->exc == DX_BOW && v->bow > 0) && !(d->exc == DX_DUST && v->gate)) {   /* (nothing left in it) */
        for (i = 0; i < n; i++)
            x[i] = 0;
        return;
    }
    fc = (int32_t)(((int64_t)f_hz * exp2_q16((cut - Q15 / 2) * 8 * 2)) >> 16);   /* f x 2^((c - 0.5) x 8) */
    svf_coef(&ec, clampi(fc, 20, TANG_FMAX), 6144);         /* q 1.5 */
    if (v->trig) {
        v->trig = 0;
        if (d->exc == DX_PLUCK) {                            /* Rings' plucker */
            int32_t ratio = mulq15(pos, 29491) + 1638;       /* 0.05 .. 0.95 */
            int32_t per = (int32_t)(((int64_t)dr_period(v->inc) * ratio) >> 15);   /* Q16 */
            v->burst = per >> 16;
            while (per >= (255 << 16))
                per >>= 1;
            v->cper = per;
            v->cgain = mulq15(Q15 - pos, 26214);             /* (1 - pos) 0.8 */
        } else if (d->exc == DX_STRIKE) {                    /* a mallet: harder, shorter */
            v->plen = v->pulse = clampi(4 + mulq15(Q15 - bright, 60), 2, 64);
        }
    }
    for (i = 0; i < n; i++) {
        int32_t e = 0;
        if (d->exc == DX_STRIKE && v->pulse) {
            uint32_t ph = (uint32_t)(v->plen - v->pulse) * (0x80000000u / (uint32_t)v->plen);
            e = sin_ph(ph) * 2;                              /* half a sine */
            v->pulse--;
        } else if (d->exc == DX_BOW) {
            e = mulq15(rnd_bi(&v->r), mulq15(v->bow, d->level));
        } else if (d->exc == DX_DUST && v->gate) {
            if ((rnd_next(&v->r) >> 8) < (uint32_t)((d->rate * 380) >> 8))   /* (rate Hz Q8 / FS x 2^24) */
                e = mulq15(rnd_bi(&v->r), d->level) * 3;
        }
        x[i] = svf_run(&v->exf, &ec, e, 1);
    }
    if (d->exc == DX_PLUCK) {                                /* noise for its share of the period, a comb, a low-pass */
        svf_c pc;
        int32_t g = v->cgain;
        svf_coef(&pc, clampi(fc * 8, 20, TANG_FMAX), 4096);
        for (i = 0; i < n; i++) {
            int32_t in = 0, o;
            if (v->burst) {
                in = rnd_bi(&v->r);
                v->burst--;
            }
            o = in + mulq15(ln_lin(v, &v->comb, v->cper), g);
            ln_write(v, &v->comb, o);
            x[i] += svf_run(&v->pkf, &pc, o, 1);
        }
    }
    {   /* the DC blocker (1 - 10 / FS), and Rings' 1 / sqrt(2 strings) */
        int32_t g = nstr > 1 ? 11585 : 23170, pk = 0;
        for (i = 0; i < n; i++) {
            int32_t y = x[i] - v->exdx + v->exdy - (v->exdy >> 12);
            v->exdx = x[i];
            v->exdy = y;
            x[i] = mulq15(y, g) >> 1;                        /* (and half: the lines' headroom) */
            pk = absi(x[i]) > pk ? absi(x[i]) : pk;
        }
        v->exq = pk > 1 ? 0 : (uint8_t)(v->exq < 255u ? v->exq + 1u : 255u);
    }
}

/* ------------------------------------------------------------ a string --- */
typedef struct {
    int32_t c;                   /* the loss a period, Q30 */
    int32_t h0, h1;              /* the FIR: brightness, Q15 */
    svf_c lpc;
    int32_t dcomp;               /* Q16 */
    int32_t disp;                /* Q15, -1 .. 1 */
    int32_t pos;                 /* the clamped position, Q15 */
    int32_t nfil;                /* the dispersion noise's low-pass, Q15 */
} dr_sc_t;
/* a string's coefficients for a block (Rings: String::ProcessInternal, its head) */
static void dr_coef(dr_sc_t *k, int32_t delay16, uint32_t inc, int32_t bright, int32_t damp, int32_t pos, int32_t disp)
{
    int32_t lf = mulq15(damp, 2 * Q15 - damp), b2 = mulq15(bright, bright), cut, f_hz, df, e;
    uint32_t rt60;
    /* the time to -60 dB: 0.07 s x 2^(lf x 8), samples; the loss a period 2^(-10 delay / rt60) */
    rt60 = (uint32_t)(((uint64_t)3087 * exp2_q16(lf * 16)) >> 16);
    e = -(int32_t)(udiv64_32(0, (uint32_t)delay16 * 10u, rt60 ? rt60 : 1u) > 0x7FFFFFFFu ? 0x7FFFFFFF
                   : udiv64_32(0, (uint32_t)delay16 * 10u, rt60 ? rt60 : 1u));
    e = e < -(127 << 16) / 12 ? -(127 << 16) / 12 : e;
    k->c = dr_exp2n(e);
    k->nfil = (int32_t)(exp2_q16((bright - Q15) * 8) >> 1);     /* 2^((b - 1) x 4), Q15 */
    cut = 24 * 256 + ((mulq15(damp, damp) * 48 + b2 * 24) >> 7);  /* semitones, Q8 */
    cut = cut > 84 * 256 ? 84 * 256 : cut;
    if (damp >= 31130) {                                          /* >= 0.95: on to no decay at all */
        int32_t t = (damp - 31130) * 20;                          /* Q15 */
        k->c += (int32_t)(((int64_t)((1 << 30) - k->c) * t) >> 15);
        b2 += mulq15(Q15 - b2, t);
        cut += mulq15(128 * 256 - cut, t);
    }
    f_hz = (int32_t)(((uint64_t)inc * FS) >> 32);
    df = (int32_t)(((int64_t)f_hz * exp2_q16((cut << 8) / 12)) >> 16);
    svf_coef(&k->lpc, clampi(df, 20, TANG_FMAX), 2048);           /* q 0.5 */
    {   /* the low-pass's delay at the string's frequency, made good: 1 - atan(2^(-cut / 12)) / pi */
        int32_t y = (int32_t)exp2_q16(-(cut << 8) / 12) >> 1;   /* Q15, <= 0.25 */
        int32_t y3 = mulq15(mulq15(y, y), y), at = y - y3 / 3 + mulq15(mulq15(y3, y), y) / 5;
        k->dcomp = Q16 - mulq15(at, 10430) * 2;                   /* (/ pi; Q15 to Q16) */
    }
    k->h0 = (Q15 + b2) >> 1;
    k->h1 = (Q15 - b2) >> 2;
    k->disp = disp;
    k->pos = Q15 / 2 - mulq15(32113, absi(pos - Q15 / 2));       /* 0.5 - 0.98 |pos - 0.5| */
}
/* n samples of string s on in[] into out[] and aux[] (the pickup), with or without dispersion */
static void dr_string(dr_voice_t *v, dr_str_t *s, const dr_sc_t *k, int32_t delay1, const int32_t *in, int32_t *out,
                      int32_t *aux, uint32_t n, int disp_on)
{
    uint32_t i;
    int32_t d0 = s->delay, dd = (delay1 - d0) / (int32_t)n;
    int32_t p0 = s->pos, dp = (k->pos - p0) / (int32_t)n, c0 = s->dcomp ? s->dcomp : k->dcomp;
    int32_t dc = (k->dcomp - c0) / (int32_t)n;
    int32_t sp = 0, na = 0, bc = 0, acb = 0, apg = 0, nf = k->nfil, ninv = 0;
    dr_line_t *st = &v->s[1].ln;                                   /* (STRING: the stretch line) */
    if (disp_on) {
        int32_t dsp = k->disp;
        sp = dsp <= 0 ? 0 : mulq15(mulq15(dsp, 2 * Q15 - dsp), 15565);   /* x 0.475 */
        na = dsp > 24576 ? (dsp - 24576) * 4 : 0;
        na = mulq15(mulq15(na, na), 819);                          /* x 0.025 */
        acb = dsp < 0 ? -dsp : 0;
        bc = mulq15(mulq15(acb, acb), 328);                        /* x 0.01 */
        apg = -(20250 * dsp / (4915 + absi(dsp)));                    /* -0.618 d / (0.15 + |d|), Q15 */
        ninv = (int32_t)(0x40000000u / (uint32_t)(6554 + nf));         /* 1 / (0.2 + nf), Q15 */
    }
    for (i = 0; i < n; i++) {
        int32_t delay, cdelay, x, sa;
        s->delay = d0 += dd;
        s->pos = p0 += dp;
        s->dcomp = c0 += dc;
        cdelay = (int32_t)(((int64_t)d0 * p0) >> 15);
        delay = (int32_t)(((int64_t)d0 * c0) >> 16) - Q16;        /* (the IIR's delay, and the FIR's sample) */
        if (disp_on) {
            int32_t ap, mn, fm = Q16;
            int32_t nz = (int32_t)(((int64_t)rnd_bi(&v->r) * ninv) >> 15);   /* / (0.2 + nf) */
            s->dnoise += mulq15(nz - s->dnoise, nf);
            fm += (int32_t)(((int64_t)s->dnoise * na) >> 14);
            fm -= (int32_t)(((int64_t)s->bridge * bc) >> 14);
            delay = (int32_t)(((int64_t)delay * fm) >> 16);
            ap = (int32_t)(((int64_t)delay * sp) >> 15);
            mn = delay - ap;
            if (ap >= (4 << 16) && mn >= (4 << 16) && (ap >> 16) < (int32_t)st->len) {
                int32_t rd, wr;
                x = ln_hermite(v, &s->ln, mn);
                rd = ln_at(v, st, (uint32_t)(ap >> 16));          /* stmlib's Allpass */
                wr = x + mulq15(apg, rd);
                ln_write(v, st, wr);
                x = rd - mulq15(wr, apg);
            } else
                x = ln_hermite(v, &s->ln, clampi(delay, 4 << 16, (int32_t)(s->ln.len - 4u) << 16));
            {   /* the AC blocker, the curved bridge */
                int32_t y = x - s->acx + s->acy - (s->acy >> 11), val, sg;
                s->acx = x;
                s->acy = y;
                x += mulq15(acb, y - x);
                val = absi(x) - 819;
                sg = x > 0 ? 2 : -3;
                s->bridge = ((absi(val) + val) * sg) >> 1;
            }
        } else
            x = ln_hermite(v, &s->ln, clampi(delay, 4 << 16, (int32_t)(s->ln.len - 4u) << 16));
        x += in[i];
        sa = mulq15(k->h0, s->fx1) + mulq15(k->h1, x + s->fx2);    /* the FIR (brightness) */
        s->fx2 = s->fx1;
        s->fx1 = x;
        x = (int32_t)(((int64_t)sa * k->c) >> 30);
        x = svf_run(&s->lp, &k->lpc, x, 1);
        ln_write(v, &s->ln, x);
        out[i] += x;
        aux[i] += ln_lin(v, &s->ln, cdelay < (2 << 16) ? 2 << 16 : cdelay);
    }
    s->delay = delay1;
}

/* ------------------------------------------------------------- modal --- */
/* the stiffness of a structure, Q16 (Rings' lut_stiffness) */
static int32_t dr_stiff(int32_t s)
{
    if (s < 8192)
        return -((8192 - s) >> 1);                          /* -(0.25 - s) x 0.25, Q16 */
    if (s < 9830)
        return 0;
    if (s < 29491) {
        int32_t g = (s - 9830) * Q15 / 19661;                /* (s - 0.3) / 0.6 */
        return (int32_t)((655u * (uint64_t)exp2_q16((int32_t)(((int64_t)g * 436504) >> 15))) >> 16) - 655;   /* 0.01 x 10^(2.005 g) - 0.01 */
    }
    {
        int32_t g = (s - 29491) * 10, c;                      /* (s - 0.9) / 0.1, then squared */
        g = mulq15(g, g);
        c = sin_ph(((uint32_t)g << 16) + 0x40000000u);        /* cos(g pi) */
        return 98304 - c;                                     /* 1.5 - cos / 2, Q16 */
    }
}
static void dr_modes(dr_voice_t *v, int32_t st, int32_t bright, int32_t damp, int32_t pos)
{
    int32_t stiff = dr_stiff(st), stretch = Q16, ql, qlr, att, br;
    int64_t q;                                                  /* Q8 */
    uint32_t k, nm = 0;
    q = ((int64_t)500 * 256 * exp2_q16((int32_t)(((int64_t)damp * 435411) >> 14))) >> 16;   /* 500 x 10^(4 d) */
    att = Q15 - st;
    att = mulq15(att, att);
    att = mulq15(att, att);
    att = mulq15(att, att);
    br = mulq15(bright, Q15 - mulq15(6554, att));
    ql = mulq15(mulq15(br, 2 * Q15 - br), 27853) + 4915;        /* b (2 - b) 0.85 + 0.15 */
    qlr = mulq15(mulq15(st, 2 * Q15 - st), 3277);               /* s (2 - s) 0.1 */
    for (k = 0; k < DR_MODES; k++) {
        uint64_t f = ((uint64_t)v->inc * (uint32_t)(k + 1u) * (uint32_t)stretch) >> 16;   /* the partial, Q32 */
        dr_mode_t *m = &v->u.md[k];
        int32_t s1, s2, r, cw, x;
        uint64_t qm;
        if (f >= 2104533975ull || stretch <= 0)                /* 0.49 */
            break;
        nm = k + 1u;
        qm = Q16 + (((uint64_t)f * (uint64_t)q) >> 24);         /* 1 + f q, Q16 */
        {   /* pi f / Q, Q30 (the quotient by 64-by-32 bits: both brought under 32 bits first) */
            uint64_t nm = (((uint64_t)f * 205887u) >> 18) << 16;
            while (qm > 0xFFFFFFFFull) {
                qm >>= 1;
                nm >>= 1;
            }
            x = (int32_t)udiv64_32((uint32_t)(nm >> 32), (uint32_t)nm, (uint32_t)qm);
        }
        r = x < 107374182 ? (1 << 30) - x + (int32_t)(((int64_t)x * x) >> 31) - (int32_t)(((((int64_t)x * x) >> 30) * x) >> 30) / 6
                          : (int32_t)(exp2_q16(-(int32_t)(((int64_t)x * 94548) >> 30)) << 14);   /* e^-x */
        s1 = dr_sinpi((uint32_t)f);                              /* sin(pi f) */
        cw = (1 << 30) - (int32_t)((((int64_t)s1 * s1) >> 30) * 2);   /* cos(2 pi f) */
        s2 = (int32_t)((((int64_t)s1 * 2) * dr_sinpi(0x80000000u - (uint32_t)f) ) >> 30);   /* sin(2 pi f) */
        m->a1 = (int32_t)clampi((int32_t)((((int64_t)r * cw) >> 29) > 0x7FFFFFFF ? 0x7FFFFFFF : (((int64_t)r * cw) >> 29)),
                                -0x7FFFFFFF, 0x7FFFFFFF);
        m->a2 = (int32_t)(((int64_t)r * r) >> 30);
        m->b0 = (int32_t)(((int64_t)s2 * s2) >> 33);             /* sin^2, x 0.125 (Rings' input) */
        v->amp[k] = (int16_t)(sin_ph((uint32_t)(((uint64_t)(uint32_t)pos * (k + 1u)) << 17) + 0x40000000u) >> 0);
        stretch += stiff;
        stiff = stiff < 0 ? mulq15(stiff, 30474) : mulq15(stiff, 32113);   /* x 0.93, x 0.98 */
        ql += mulq15(qlr, Q15 - ql);
        q = (q * ql) >> 15;
    }
    v->nmodes = (uint8_t)nm;
}

/* ------------------------------------------------------------ a block --- */
static int dr_render(dr_voice_t *v, const dr_design_t *d, int32_t *out, uint32_t n)
{
    int32_t ex[CTL], o[CTL], a[CTL], sy[CTL];
    int32_t st = dr_knob(d->structure, v->mac[0]), br = dr_knob(d->brightness, v->mac[1]);
    int32_t dm = dr_knob(d->damping, v->mac[2]), ps = dr_knob(d->position, v->mac[3]);
    uint32_t i, k;
    int32_t pk = 0;
    if (!v->live)
        return 0;
    n = n > CTL ? CTL : n;
    if (!v->gate)                                             /* let go: the damping down by the design's share */
        dm = mulq15(dm, Q15 - d->mute);
    if (d->exc == DX_BOW || d->exc == DX_DUST) {              /* the bow swells in, dies away when let go */
        int32_t att = clampi(d->bow_att, 2, 5000), step = (int32_t)(Q15 * 726 / (att * 1000));
        if (v->gate)
            v->bow = v->bow + step + 1 > Q15 ? Q15 : v->bow + step + 1;
        else
            v->bow -= v->bow >> 4;
    }
    for (i = 0; i < n; i++)
        o[i] = a[i] = 0;
    if (d->model == DM_MODAL) {
        int32_t key = (int32_t)((uint32_t)(st >> 9) | (uint32_t)(br >> 9) << 6 | (uint32_t)(dm >> 9) << 12 |
                                (uint32_t)(ps >> 9) << 18 | (uint32_t)(v->note & 63) << 24);
        dr_excite(v, d, ex, n, br, ps, 1);
        for (i = 0; i < n; i++)
            ex[i] *= 4;                                        /* (a body takes more to ring than a string) */
        if (key != v->mkey) {
            dr_modes(v, st, mulq15(br, br), dm, ps);
            v->mkey = key;
        }
        for (k = 0; k < v->nmodes; k++) {
            dr_mode_t *m = &v->u.md[k];
            int32_t y1 = m->y1, y2 = m->y2, amp = v->amp[k], *dst = (k & 1u) ? a : o;
            for (i = 0; i < n; i++) {
                int32_t y = (int32_t)(((int64_t)m->b0 * ex[i] + (int64_t)m->a1 * y1 - (int64_t)m->a2 * y2) >> 30);
                y2 = y1;
                y1 = y;
                dst[i] += mulq15(y, amp);
            }
            if (absi(y1) > (1 << 26) || absi(y2) > (1 << 26)) {   /* (a bowed body ringing up: held in range) */
                y1 >>= 1;
                y2 >>= 1;
            }
            m->y1 = y1;
            m->y2 = y2;
        }
    } else if (d->model == DM_STRING) {
        dr_sc_t c;
        int32_t disp = st < 7864 ? (st - 7864) * 544 / 131 : st > 8520 ? mulq15(st - 8520, 44282) : 0;   /* x 4.166 */
        int32_t per = dr_period(v->inc);
        dr_excite(v, d, ex, n, br, ps, 1);
        dr_coef(&c, per, v->inc, br, dm, ps, clampi(disp, -Q15, Q15));
        dr_string(v, &v->s[0], &c, per, ex, o, a, n, 1);
    } else {                                                   /* SYMPATHETIC: a string, and three driven by it */
        static const uint32_t LFO[DR_NSTR] = {0, 1246900, 1091037, 716968};   /* 0.4, 0.35, 0.23 Hz: a block's step */
        int32_t notes[9], fr[DR_NSTR], p = st;
        int32_t nn = v->note * 256;                            /* semitones Q8 */
        dr_excite(v, d, ex, n, br, ps, DR_NSTR);
        notes[0] = nn - 24 * 256;
        notes[1] = nn - 12 * 256;
        notes[2] = nn - 1797;                                  /* 7.01955 */
        notes[3] = nn;
        notes[4] = nn + 1797;
        notes[5] = nn + 12 * 256;
        notes[6] = nn + 4869;                                  /* 19.01955 */
        notes[7] = notes[8] = nn + 24 * 256;
        fr[0] = nn;
        for (k = 1; k < 3u; k++) {
            int32_t x = p * 7, ii = x >> 15, fq = dr_squash(x & 0x7FFF);
            ii = ii > 7 ? 7 : ii;
            fr[k] = notes[ii] + mulq15(notes[ii + 1] - notes[ii], fq);
            p += mulq15(Q15 - p, 6554);
        }
        fr[3] = nn + 3;                                        /* + 0.013 semitone */
        for (k = 0; k < DR_NSTR; k++) {
            dr_str_t *s = &v->s[k];
            dr_sc_t c;
            int32_t b = br, dd = dm, pp = ps, per, tgt;
            uint32_t inc = (uint32_t)(((uint64_t)pitch_inc((fr[k] * 16) >> 8)));
            const int32_t *src = ex;
            while (inc < 0xFFFFFFFFu / (uint32_t)(s->ln.len - 6u) && inc < 0x40000000u)
                inc <<= 1;                                     /* (an octave up where it would not fit) */
            per = dr_period(inc);
            if (k) {
                int32_t amt = mulq15(Q15 / 2 - absi(Q15 / 2 - ps), 29491), lv;
                b = mulq15(b, 2 * Q15 - b);
                b = mulq15(b, 2 * Q15 - b);
                dd = 22938 + mulq15(dm, 8847);                 /* 0.7 + d 0.27 */
                s->lfo += LFO[k];
                lv = sin_ph(s->lfo + 0x40000000u);
                pp = clampi(ps + mulq15(lv, amt), 0, Q15);
                src = sy;
                tgt = s->delay + (int32_t)(((int64_t)(per - s->delay) * exp2_q16((b - Q15) * 6)) >> 16);   /* glide: 2^(3 (b - 1)) */
            } else
                tgt = per;
            dd += (int32_t)(k * (uint32_t)(31130 - dd) / DR_NSTR);
            dr_coef(&c, tgt, inc, b, dd, pp, 0);
            dr_string(v, s, &c, tgt, src, o, a, n, 0);
            if (!k)                                            /* the others: driven by the first's sound */
                for (i = 0; i < n; i++)
                    sy[i] = (o[i] - a[i]) / 20;                /* 0.2 / 4 */
        }
    }
    for (i = 0; i < n; i++) {
        int32_t y = (int32_t)(((int64_t)(o[i] - a[i]) * d->out) >> 12);
        out[i] = y;
        pk = absi(y) > pk ? absi(y) : pk;
    }
    v->quiet = pk < 6 ? v->quiet + 1 : 0;
    if (v->quiet > 64 && !(v->gate && (d->exc == DX_BOW || d->exc == DX_DUST)))
        v->live = 0;                                           /* (silent ~50 ms, nothing driving it: done) */
    return 1;
}

/* ----------------------------------------------------------- the design --- */
static void dr_design(dr_design_t *d, int32_t kind, uint32_t seed, uint32_t path, uint32_t gen)
{
    rnd_t rr, *r = &rr;
    path = path < PATH_N ? path : PATH_OPEN;
    rnd_seed(r, seed ^ 0xD401Du);
    {   /* (the family drawn always, a family given or not: the seed and the family then make the sound again, however
         * the family was come to -- SHIFT + PRESETS, a preset, a session) */
        int32_t drawn = (int32_t)rnd_weighted(r, DK_W[path], DK_N);
        if (kind < 0 || kind >= DK_N)
            kind = drawn;
    }
    memset(d, 0, sizeof *d);
    d->seed = seed;
    d->kind = (uint8_t)kind;
    d->path = (uint8_t)path;
    d->gen = (uint8_t)gen;
    d->model = DK_MODEL[kind];
    d->exc = DK_EXC[kind];
    d->structure = rnd_in(r, DK_ST[kind][0] * Q15 / 100, DK_ST[kind][1] * Q15 / 100);
    d->brightness = rnd_in(r, 9830, 29491);                   /* 0.3 .. 0.9 */
    d->position = rnd_in(r, 3277, 29491);
    d->level = rnd_in(r, 9830, 26214);
    d->rate = rnd_in(r, 4 * 256, 30 * 256);
    d->bow_att = rnd_in(r, 40, 500);
    if (path == PATH_AMBIENT) {                               /* long rings, soft bows, they ring on when let go */
        d->damping = rnd_in(r, 24576, 31457);                 /* 0.75 .. 0.96 */
        d->mute = rnd_in(r, 0, 6554);
        d->bow_att = rnd_in(r, 300, 1500);
        d->brightness = mulq15(d->brightness, 26214);
    } else if (path == PATH_EXP) {                            /* shorter, harder, rattling faster, stopped short */
        d->damping = rnd_in(r, 13107, 27853);
        d->mute = rnd_in(r, 9830, 29491);
        d->rate = rnd_in(r, 10 * 256, 60 * 256);
        d->bow_att = rnd_in(r, 10, 200);
        d->position = rnd_chance(r, 50) ? rnd_in(r, 0, 4915) : rnd_in(r, 27853, Q15);
    } else {
        d->damping = rnd_in(r, 16384, 30147);
        d->mute = rnd_in(r, 3277, 22938);
    }
    if (d->exc == DX_PLUCK || d->exc == DX_STRIKE)            /* (a struck or plucked string damped when let go) */
        d->mute = mulq15(d->mute, 29491) + 1638;
    d->out = 4096;
}
/* the design's own envelope as A D S R, ms (the ENVELOPE page, as drawn): the exciter's onset; the ring's time to
 * -60 dB at its damping (decay), and once let go (release); held only while a bow or dust drives it */
static void dr_env_adsr(const dr_design_t *d, int32_t *a)
{
    int32_t lf = mulq15(d->damping, 2 * Q15 - d->damping), dr = mulq15(d->damping, Q15 - d->mute), lr;
    lr = mulq15(dr, 2 * Q15 - dr);
    a[0] = d->exc == DX_BOW ? d->bow_att : 1;
    a[1] = d->damping >= 31130 ? 10000 : (int32_t)((70u * exp2_q16(lf * 16)) >> 16);
    a[2] = d->exc == DX_BOW || d->exc == DX_DUST ? 100 : 0;
    a[3] = (int32_t)((70u * exp2_q16(lr * 16)) >> 16);
}
static const char *dr_mac_name(const dr_design_t *d, uint32_t j)
{
    static const char *const N[NMAC] = {"STRUCTURE", "BRIGHTNESS", "DAMPING", "POSITION"};
    (void)d;
    return N[j & 3u];
}

/* the design's level: C3 held 1 s, then let go; the 75th percentile of 12 ms windows brought to 0.1 RMS, and no
 * more than keeps its loudest moment under half of full scale (as rus_measure) */
static void dr_measure_on(dr_design_t *d, dr_voice_t *probe)
{
    static int32_t win[2067u / 16u + 1u];
    int32_t out[CTL];
    uint32_t b, k, nwin = 0;
    int64_t acc = 0;
    int32_t peak = 1;
    d->out = 4096;
    dr_init(probe, d->seed ^ 0x77u);
    dr_note_on(probe, d, 48, 1);
    for (b = 0; b < 2067u; b++) {
        uint32_t i;
        if (b == 1378u)
            dr_note_off(probe);
        if (!dr_render(probe, d, out, CTL))
            memset(out, 0, sizeof out);
        for (i = 0; i < CTL; i++) {
            acc += ((int64_t)out[i] * out[i]) >> 6;
            if (absi(out[i]) > peak)
                peak = absi(out[i]);
        }
        if ((b & 15u) == 15u) {
            if (nwin < NELEM(win))
                win[nwin++] = (int32_t)((acc >> 9) > 0x7FFFFFFF ? 0x7FFFFFFF : (acc >> 9));
            acc = 0;
        }
#ifndef HOST
        if ((b & 255u) == 255u)
            fm1_wdt_feed();
#endif
    }
    for (b = 1; b < nwin; b++)
        for (k = b; k > 0 && win[k - 1] > win[k]; k--) {
            int32_t t = win[k];
            win[k] = win[k - 1];
            win[k - 1] = t;
        }
    {
        uint32_t root = nwin ? isqrt32((uint32_t)win[nwin * 3u / 4u]) : 0;
        int32_t g = root ? (int32_t)(1677722u / root) : 4096 * 8;
        int32_t gp = (int32_t)((uint32_t)(Q15 / 2) * 4096u / (uint32_t)peak);
        d->out = clampi(g < gp ? g : gp, 64, 4096 * 48);
    }
}
