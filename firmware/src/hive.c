/* SPDX-License-Identifier: GPL-3.0-only
 * TONITOR: HIVE, the seventh engine -- a swarm of oscillators, ported from the SWARMER machine of DigiRust (Nuno
 * Zimas, mvave-digirust: firmware/src/wav.c, gen.c), contributed by its author, and grown here. Per voice, seven
 * oscillators, each drawn with the sound:
 *   its place from the note (an octave, a fifth, an inharmonic partial, a cluster's step, or the note itself), its
 *   share of DETUNE's spread, its wave (a place along sine -> triangle -> saw -> square), its level, and which of
 *   three LFOs (their rates drawn too) moves its pitch and its level, at its own phase; a slow noise on each pitch;
 *   the seventh, the main, set against the others by MIX;
 *   the key followed fully, or partly (the landscapes: hardly at all); noise; a ring of the first and the main;
 *   a 4-pole filter (two state-variable filters: low-, band- or high-pass), its cutoff following the key by half,
 *   swept by its own envelope and by the first LFO; an ADSR on the level; a drive (tanh).
 * Twelve kinds: six after DigiRust's recipes (SUB, LEAD, STAB, PAD, DRONE, WASH), each drawing its layout, waves
 * and motion anew; six inharmonic and textural ones for non-melodic use (CLUSTER, METAL, SPECTRA, GROUND, AIR,
 * TIDES).
 * Changed from DigiRust: the waves made as they play (the sine from its table, the triangle reckoned, the saw and
 * the square band-limited by polyBLEP) instead of band-limited tables in RAM (no room for them here); the filter,
 * envelopes and drive its own, not the Digitone-like voice chain's. */

enum { HK_SUB, HK_LEAD, HK_STAB, HK_PAD, HK_DRONE, HK_WASH, HK_CLUSTER, HK_METAL, HK_SPECTRA, HK_GROUND, HK_AIR, HK_TIDES, HK_N };
static const char *const HK_NAME[HK_N] = {"SUB", "LEAD", "STAB", "PAD", "DRONE", "WASH", "CLUSTER", "METAL", "SPECTRA",
                                          "GROUND", "AIR", "TIDES"};
static const uint8_t HK_W[PATH_N][HK_N] = {
    {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
    {1, 1, 0, 4, 4, 4, 2, 1, 4, 2, 2, 4},
    {2, 2, 4, 1, 1, 1, 4, 4, 2, 4, 4, 2},
};
/* each kind's level envelope: attack, decay, release (ms), sustain (x 100), each a range */
static const uint16_t HK_ENV[HK_N][4][2] = {
    {{1, 40}, {150, 900}, {80, 600}, {50, 95}},          /* SUB */
    {{1, 80}, {150, 900}, {100, 900}, {45, 95}},         /* LEAD */
    {{1, 4}, {80, 700}, {80, 600}, {0, 25}},             /* STAB */
    {{200, 3000}, {400, 2000}, {800, 4500}, {70, 100}},  /* PAD */
    {{300, 3500}, {500, 1500}, {1500, 6000}, {90, 100}}, /* DRONE */
    {{800, 5000}, {500, 1500}, {3000, 9000}, {90, 100}}, /* WASH */
    {{20, 2500}, {300, 1500}, {600, 5000}, {60, 100}},   /* CLUSTER */
    {{1, 6}, {400, 4000}, {600, 6000}, {0, 40}},         /* METAL */
    {{800, 6000}, {500, 2000}, {2500, 9000}, {85, 100}}, /* SPECTRA */
    {{300, 4000}, {500, 2000}, {1500, 8000}, {80, 100}}, /* GROUND */
    {{400, 5000}, {500, 2000}, {2000, 9000}, {80, 100}}, /* AIR */
    {{600, 5000}, {500, 2000}, {2500, 9000}, {85, 100}}, /* TIDES */
};

typedef struct {
    uint32_t seed;               /* what made it: hv_design(kind, seed, path, gen) + hv_measure give it again */
    uint8_t kind, path, gen, fmode;   /* fmode: the filter, 1 low-pass, 2 band-pass, 3 high-pass */
    uint8_t wv[7];               /* each oscillator's wave, 0 (sine) .. 255 (square) */
    uint8_t lk[7];               /* each one's LFO (low 2 bits) and its phase (the rest, x 1/64 of a turn) */
    int32_t base[7];             /* each one's place from the note, semitones Q16 */
    int16_t spr[7];              /* .. its share of DETUNE's span, -1000 .. 1000 */
    int16_t amp[7];              /* .. its level, Q15 */
    uint16_t lrate[3];           /* the LFOs, x 0.01 Hz */
    int32_t dspan, aspan, nspan; /* DETUNE's span, the LFOs' pitch depth, the noise's on pitch: semitones Q16 */
    int32_t mix, shim, ring, noise, ktrk;   /* Q15: MIX; the LFOs' depth on the levels; ring; noise; key followed */
    int32_t cut, res, famt, fdec, flfo;     /* Q15: cutoff (of 9 octaves above 40 Hz), resonance, the envelope's
                                               octaves (of 4), its decay (of 2 s), the first LFO's (of 3 octaves) */
    int32_t att, dec, sus, rel;  /* ms; sus Q15 */
    int32_t drive;               /* Q12 */
    int32_t out;                 /* Q12: the measured level brought to the target */
} hv_design_t;

#define HV_SIZE21 72u           /* (the design's size in session format 21, before the rework) */

typedef struct {
    uint8_t live, gate, hold, stage;   /* stage: 1 attack, 2 decay, 3 sustain, 4 release, 0 done */
    int8_t mac[NMAC];
    int32_t note, vel;
    uint32_t ph[7], lph[3];
    rnd_t rn;
    int32_t nlp[7];
    int32_t lv, flv;             /* the level's envelope, Q24; the filter's, Q24 */
    uint8_t fst;                 /* the filter envelope: 1 rising, 2 falling */
    svf_t f1, f2;
} hv_voice_t;

/* an oscillator's step for a block: Q16 of a cycle, and its reciprocal (for polyBLEP without a division) */
typedef struct { uint32_t dt, inv; } hv_dt_t;
static FAST hv_dt_t hv_dt_of(uint32_t inc)
{
    hv_dt_t r;
    r.dt = (inc >> 16) | 1u;
    r.inv = 0xFFFFFFFFu / r.dt;
    return r;
}
/* polyBLEP at a phase (Q32): the step's correction, Q15 */
AINL int32_t hv_blep(uint32_t ph, const hv_dt_t *s)
{
    uint32_t t = ph >> 16;
    if (t < s->dt) {
        int32_t x = (int32_t)(((uint64_t)t * s->inv) >> 16);   /* t / dt, Q16 */
        return (2 * x - (int32_t)(((int64_t)x * x) >> 16) - Q16) >> 1;
    }
    if (t > Q16 - s->dt) {
        int32_t x = -(int32_t)(((uint64_t)(Q16 - t) * s->inv) >> 16);
        return ((int32_t)(((int64_t)x * x) >> 16) + 2 * x + Q16) >> 1;
    }
    return 0;
}
/* wave w (0 sine, 1 triangle, 2 saw, 3 square) at phase ph, Q15 */
AINL int32_t hv_wave(uint32_t w, uint32_t ph, const hv_dt_t *inc)
{
    if (w == 0u)
        return sin_ph(ph);
    if (w == 1u) {
        int32_t q = (int32_t)(ph >> 16);                 /* Q16 of a cycle */
        return q < Q16 / 2 ? 2 * q - Q15 : 3 * Q15 - 2 * q;
    }
    if (w == 2u)
        return (int32_t)(ph >> 16) - Q15 - hv_blep(ph, inc);
    return (ph < 0x80000000u ? Q15 - 1 : -Q15) + hv_blep(ph, inc) - hv_blep(ph + 0x80000000u, inc);
}
/* the place along the four waves: the two and their crossfade */
static FAST void hv_morph(int32_t x, uint32_t *a, int32_t *f)
{
    int32_t q = clampi(x, 0, Q15) * 3;
    *a = (uint32_t)(q >> 15) > 2u ? 2u : (uint32_t)(q >> 15);
    *f = q - (int32_t)(*a << 15);
}
AINL int32_t hv_read(uint32_t a, int32_t f, uint32_t ph, const hv_dt_t *inc)
{
    int32_t s = hv_wave(a, ph, inc);
    return f ? s + mulq15(hv_wave(a + 1u, ph, inc) - s, f) : s;
}
static FAST uint32_t hv_inc(int32_t semi16)                  /* a pitch, semitones Q16 from MIDI 0: a phase step */
{
    int32_t whole = semi16 >> 16, frac = semi16 & 0xFFFF;
    uint32_t inc = pitch_inc(whole * 16);
    return (uint32_t)(((uint64_t)inc * exp2_q16(frac / 12)) >> 16);
}

static void hv_init(hv_voice_t *v, uint32_t seed)
{
    memset(v, 0, sizeof *v);
    rnd_seed(&v->rn, seed);
}
static void hv_note_on(hv_voice_t *v, const hv_design_t *d, int32_t note, int32_t vel127, int fresh)
{
    uint32_t k;
    if (fresh || !v->live) {
        rnd_t r = v->rn;
        memset(v, 0, sizeof *v);
        v->rn = r;
        for (k = 0; k < 7u; k++)
            v->ph[k] = rnd_next(&v->rn);                 /* (free running) */
    }
    (void)d;
    v->note = note;
    v->vel = clampi(vel127, 1, 127) * 258;
    v->live = v->gate = 1;
    v->stage = 1;
    v->fst = 1;
}
static void hv_note_off(hv_voice_t *v)
{
    v->gate = 0;
    if (v->stage && !v->hold)
        v->stage = 4;
}
/* a macro as a factor around the sound's own, Q15: 0 .. 2 (turned fully down, none) */
static FAST int32_t hv_fac(int32_t m) { return clampi(Q15 + m * (Q15 / MAC_MAX), 0, 2 * Q15); }
#define HV_TOP (118 << 16)       /* (above this an oscillator is silent: no aliasing from its harmonics) */

static volatile uint8_t hv_thin;   /* (three HIVE voices or more sounding: each a little thinner -- layers.c) */
static FAST int hv_render(hv_voice_t *v, const hv_design_t *d, int32_t *out, uint32_t n)
{
    uint32_t inc[7], i, k, wa[7];
    hv_dt_t dts[7];
    int32_t wf[7], g[7], fdet, fmov, fmix, cut, key16, l0, l1, dl, lfo[3];
    svf_c fc;
    if (!v->live)
        return 0;
    n = n > CTL ? CTL : n;
    fdet = hv_fac(v->mac[0]);
    fmov = hv_fac(v->mac[1]);
    fmix = hv_fac(v->mac[2]);
    cut = clampi(d->cut + v->mac[3] * (Q15 / (2 * MAC_MAX)), 0, Q15);
    for (k = 0; k < 3u; k++) {
        v->lph[k] += (uint32_t)d->lrate[k] * 31165u;      /* 2^32 / FS / 100 a sample, x CTL */
        lfo[k] = sin_ph(v->lph[k]);
    }
    key16 = (60 << 16) + (v->note - 60) * d->ktrk * 2;    /* (the key followed by ktrk) */
    {
        int32_t span = mulq15(mulq15(d->dspan, fdet), fdet) / 2;   /* (finer near none; up to 2 x 1.5 x 2 semitones,
                                                                   x 1000 still in 32 bits) */
        int32_t asp = mulq15(d->aspan, fmov), shim = clampi(mulq15(d->shim, fmov), 0, Q15);
        int32_t gs = d->kind == HK_GROUND || d->kind == HK_AIR ? 7000 : mulq15(clampi(mulq15(d->mix, fmix), 0, Q15), 7000);
        int32_t gm = d->kind == HK_GROUND || d->kind == HK_AIR ? Q15 / 2 : Q15 - clampi(mulq15(d->mix, fmix), 0, Q15) / 2;
        for (k = 0; k < 7u; k++) {
            uint32_t ph = (uint32_t)(d->lk[k] >> 2) << 26;
            int32_t lf = sin_ph(v->lph[d->lk[k] & 3u] + ph), p;
            v->nlp[k] += (rnd_bi(&v->rn) - v->nlp[k]) >> 3;
            p = key16 + d->base[k] + span * d->spr[k] / 1000 + mulq15(lf, asp) +
                mulq15(clampi(v->nlp[k] * 4, -Q15, Q15), d->nspan);
            p = clampi(p, 12 << 16, 126 << 16);
            inc[k] = hv_inc(p);
            dts[k] = hv_dt_of(inc[k]);
            hv_morph((int32_t)d->wv[k] * 128, &wa[k], &wf[k]);
            g[k] = p > HV_TOP ? 0 : mulq15(mulq15(d->amp[k], Q15 - mulq15(shim, (Q15 - sin_ph(v->lph[d->lk[k] & 3u] + ph + 0x40000000u)) / 2)),
                                           k == 6u ? gm : gs);
        }
    }
    /* the envelopes, a block on */
    l0 = v->lv;
    switch (v->stage) {
    case 1:
        v->lv += (int32_t)(12173000u / (uint32_t)clampi(d->att, 1, 20000));
        if (v->lv >= 1 << 24) {
            v->lv = 1 << 24;
            v->stage = 2;
        }
        break;
    case 2: {
        int32_t s = d->sus << 9;
        v->lv = s + (int32_t)(((int64_t)(v->lv - s) * cal_ex(d->dec)) >> 16);
        if (absi(v->lv - s) < (1 << 12))
            v->stage = 3;
        break;
    }
    case 3:
        v->lv = d->sus << 9;
        break;
    case 4:
        v->lv = (int32_t)(((int64_t)v->lv * cal_ex(d->rel)) >> 16);
        if (v->lv < (1 << 10)) {
            v->lv = 0;
            v->stage = 0;
        }
        break;
    default:
        v->lv = 0;
        break;
    }
    if (v->hold)
        v->lv = l0 = 1 << 24;
    l1 = v->lv;
    if (v->fst == 1) {                                    /* the filter's envelope: up in ~3 ms, then down */
        v->flv += (1 << 24) / 4;
        if (v->flv >= 1 << 24) {
            v->flv = 1 << 24;
            v->fst = 2;
        }
    } else
        v->flv = (int32_t)(((int64_t)v->flv * cal_ex(20 + mulq15(d->fdec, 2000))) >> 16);
    {   /* the cutoff: 40 Hz x 2^(9 x cut), half the key's own move from C3, the envelope's octaves and the LFO's */
        int32_t oct = cut * 9 * 2 + ((v->note - 48) * Q16 / 24) +
                      (int32_t)(((int64_t)mulq15(d->famt, 4 * Q15) * 2 * (v->flv >> 9)) >> 15) +
                      mulq15(mulq15(mulq15(d->flfo, fmov), lfo[0]), 3 * Q16);
        int32_t hz = (int32_t)((40u * exp2_q16(clampi(oct, 0, 9 * Q16))) >> 16);
        svf_coef(&fc, clampi(hz, 30, TANG_FMAX), 2896 + mulq15(d->res, 14000));
    }
    dl = (l1 - l0) / (int32_t)n;
    {
        int32_t nz = d->kind == HK_GROUND || d->kind == HK_AIR ? clampi(mulq15(d->noise, fmix), 0, Q15) : d->noise;
        int type = d->fmode;
        uint8_t idx[7];
        uint32_t na = 0, j;
        int32_t gmax = 0;
        for (k = 0; k < 6u; k++)                          /* (the loudest of the six, for the thinning) */
            gmax = g[k] > gmax ? g[k] : g[k] < -gmax ? -g[k] : gmax;
        for (k = 0; k < 7u; k++) {                        /* the oscillators heard (after the SMK-37's build 31): none
                                                          * silent; with several HIVE voices (hv_thin), none of the six
                                                          * 12 dB under the loudest. The main always, the first with ring */
            int32_t a = g[k] < 0 ? -g[k] : g[k];
            if (k == 6u || (k == 0u && d->ring) || (a && (!hv_thin || a * 4 >= gmax)))
                idx[na++] = (uint8_t)k;
            else
                v->ph[k] += inc[k] * n;                    /* (kept turning, unheard: no jump when it comes back) */
        }
        for (i = 0; i < n; i++) {
            int32_t x = 0, o0 = 0, o = 0;
            for (j = 0; j < na; j++) {                    /* (the seventh, the main, last) */
                k = idx[j];
                v->ph[k] += inc[k];
                o = hv_read(wa[k], wf[k], v->ph[k], &dts[k]);
                if (!k)
                    o0 = o;
                x += mulq15(o, g[k]);
            }
            if (d->ring)
                x += mulq15(mulq15(o0, o), d->ring);
            if (nz)
                x += mulq15(rnd_bi(&v->rn), nz) / 2;
            x = svf_run(&v->f1, &fc, x, type);
            if (type == 2)                                /* (band-pass: its peak at unity, not at Q) */
                x = mulr24(fc.k, x);
            x = svf_run(&v->f2, &fc, x, type);
            if (type == 2)
                x = mulr24(fc.k, x);
            x = tanh_q15((int32_t)(((int64_t)x * d->drive) >> 12));
            l0 += dl;
            x = (int32_t)(((int64_t)x * (l0 >> 9)) >> 15);
            x = (int32_t)(((int64_t)x * d->out) >> 12);
            out[i] = x;
        }
    }
    if (!v->stage && !v->hold)
        v->live = 0;
    return 1;
}
static __typeof__(&hv_render) volatile hv_render_fp = hv_render;   /* (the flash's way into the RAM code: a call cannot
                                                                    * reach from one to the other) */

/* the draws, out of line (inline, each one's 64-bit product would be repeated a hundred times) */
static __attribute__((noinline)) int32_t hv_in(rnd_t *r, int32_t a, int32_t b) { return rnd_in(r, a, b); }
static __attribute__((noinline)) int32_t hv_below(rnd_t *r, int32_t n) { return rnd_below(r, n); }
static __attribute__((noinline)) int hv_odds(rnd_t *r, int32_t pct) { return rnd_chance(r, pct); }
/* semitones Q16 of a ratio Q16 (> 0) */
static int32_t hv_semis(uint32_t r16) { return 12 * (log2_q16(r16) - (16 << 16)); }
/* the swarm's shares of the spread: symmetric, a cluster, one side, at random, in pairs */
static void hv_spread(rnd_t *r, hv_design_t *d)
{
    static const int16_t SYM[6] = {-1000, -620, -280, 300, 640, 1000};
    uint32_t k, how = (uint32_t)hv_below(r, 5);
    for (k = 0; k < 6u; k++) {
        int32_t s = how == 0u ? SYM[k] : how == 1u ? hv_in(r, -350, 350) : how == 2u ? hv_in(r, 0, 1000)
                  : how == 3u ? hv_in(r, -1000, 1000) : (k & 1u ? 1 : -1) * hv_in(r, 250, 1000);
        d->spr[k] = (int16_t)s;
    }
    d->spr[6] = 0;
}
/* inharmonic partials for oscillators k0 .. 6: a bar's, a stretched series, a bell's, or at random; key on the
 * first. Their levels falling with height by a drawn slope */
static void hv_partials(rnd_t *r, hv_design_t *d, uint32_t k0, int32_t lo_r, int32_t hi_r)
{
    static const uint16_t BAR[6] = {256, 706, 1383, 2287, 3416, 4772};   /* x 1/256: a free bar's modes */
    static const uint16_t BELL[6] = {256, 512, 614, 768, 1075, 1382};    /* .. a bell's (hum, prime, tierce ..) */
    uint32_t k, how = (uint32_t)hv_below(r, 4);
    int32_t st = hv_in(r, 2400, 9000), tilt = hv_in(r, 2000, 9000);   /* (stretch x 1/8192 + 1; level tilt) */
    for (k = k0; k < 7u; k++) {
        uint32_t i = k - k0, r16;
        if (how == 0u)
            r16 = (uint32_t)BAR[i % 6u] << 8;
        else if (how == 1u)
            r16 = (uint32_t)BELL[i % 6u] << 8;
        else if (how == 2u)                               /* (i + 1)^(1 + st): a stretched series */
            r16 = exp2_q16((int32_t)(((int64_t)(log2_q16(i + 1u) ) * (8192 + st)) >> 13));
        else
            r16 = (uint32_t)hv_in(r, lo_r, hi_r) << 8;
        r16 = (uint32_t)((uint64_t)r16 * (uint32_t)hv_in(r, 64880, 66190) >> 16);   /* (each a little off) */
        d->base[k] = hv_semis(r16 | 1u);
        d->amp[k] = (int16_t)clampi(Q15 - (int32_t)i * tilt / 2 + hv_in(r, -4000, 4000), 3000, Q15 - 1);
    }
}
static void hv_design(hv_design_t *d, int32_t kind, uint32_t seed, uint32_t path, uint32_t gen)
{
    rnd_t rr, *r = &rr;
    uint32_t k;
    int32_t sw, wj, lo, hi;
    path = path < PATH_N ? path : PATH_OPEN;
    rnd_seed(r, seed ^ 0x41E5u);
    {   /* (the family drawn always, a family given or not: the seed and the family then make the sound again, however
         * the family was come to -- SHIFT + PRESETS, a preset, a session) */
        int32_t drawn = (int32_t)rnd_weighted(r, HK_W[path], HK_N);
        if (kind < 0 || kind >= HK_N)
            kind = drawn;
    }
    memset(d, 0, sizeof *d);
    d->seed = seed;
    d->kind = (uint8_t)kind;
    d->path = (uint8_t)path;
    d->gen = (uint8_t)gen;
    d->fmode = 1;
    d->ktrk = Q15;
    for (k = 0; k < 7u; k++) {
        d->amp[k] = Q15 - 1;
        d->lk[k] = (uint8_t)(hv_below(r, 3) | hv_below(r, 64) << 2);
    }
    hv_spread(r, d);
    /* the common draws, each kind's ranges after */
    sw = hv_in(r, 0, Q15);                               /* the swarm's wave, and how far each strays from it */
    wj = hv_in(r, 0, 9000);
    lo = 10, hi = 300;                                    /* the LFOs' rates, x 0.01 Hz */
    d->dspan = hv_in(r, 10, 60) * Q16 / 100;
    d->aspan = hv_in(r, 2, 30) * Q16 / 100;
    d->nspan = hv_in(r, 0, 25) * Q16 / 100;
    d->mix = hv_in(r, 16000, Q15);
    d->shim = hv_odds(r, 40) ? hv_in(r, 3000, 16000) : 0;
    d->cut = hv_in(r, 16000, 26000);
    d->res = hv_in(r, 0, 14000);
    d->famt = hv_in(r, 0, 12000);
    d->fdec = hv_in(r, 4000, 20000);
    d->flfo = hv_odds(r, 30) ? hv_in(r, 1000, 6000) : 0;
    d->drive = 4096 + hv_in(r, 0, 6000);
    if (hv_odds(r, 15))                                /* (a band-pass now and then) */
        d->fmode = 2;
    switch (kind) {
    case HK_SUB:
        d->base[6] = hv_odds(r, 35) ? -(24 << 16) : -(12 << 16);
        sw = hv_in(r, 0, 30000);
        d->wv[6] = (uint8_t)hv_in(r, 0, 255);
        d->cut = hv_in(r, 7000, 22000);
        d->res = hv_in(r, 0, 24000);
        d->fmode = 1;
        d->dspan = hv_in(r, 4, 45) * Q16 / 100;
        d->drive = 4096 + hv_in(r, 0, 14000);
        if (hv_odds(r, 30))                            /* (an octave up in the swarm) */
            d->base[1] = d->base[4] = 12 << 16;
        d->nspan = hv_odds(r, 30) ? hv_in(r, 10, 40) * Q16 / 100 : d->nspan;
        break;
    case HK_LEAD:
        d->base[6] = hv_odds(r, 30) ? -(12 << 16) : 0;
        d->wv[6] = (uint8_t)hv_in(r, 60, 255);
        d->cut = hv_in(r, 18000, 28000);
        d->res = hv_in(r, 4000, 20000);
        d->dspan = hv_in(r, 8, 50) * Q16 / 100;
        break;
    case HK_STAB:
        d->famt = hv_in(r, 12000, Q15);
        d->fdec = hv_in(r, 1500, 8000);
        d->res = hv_in(r, 6000, 22000);
        d->cut = hv_in(r, 9000, 20000);
        d->dspan = hv_in(r, 15, 80) * Q16 / 100;
        if (hv_odds(r, 40))                            /* (a chord in the swarm: a fifth, an octave) */
            d->base[1] = 7 << 16, d->base[4] = 12 << 16;
        break;
    case HK_PAD:
    case HK_DRONE:
    case HK_WASH:
        d->dspan = hv_in(r, 20, kind == HK_WASH ? 120 : 80) * Q16 / 100;
        d->shim = hv_in(r, 2000, 18000);
        lo = 5, hi = kind == HK_PAD ? 200 : 80;
        d->base[6] = (int32_t)hv_below(r, kind == HK_PAD ? 2 : 3) * (-(12 << 16));
        if (kind == HK_WASH) {
            d->cut = hv_in(r, 8000, 24000), d->noise = hv_odds(r, 50) ? hv_in(r, 1000, 9000) : 0;
            d->flfo = hv_odds(r, 50) ? hv_in(r, 2000, 9000) : 0;
            d->fmode = (uint8_t)(hv_odds(r, 30) ? 2 : 1);
            if (hv_odds(r, 40))                        /* (a fifth or an octave in the swarm) */
                d->base[1] = d->base[3] = (hv_odds(r, 50) ? 7 : 12) << 16;
        }
        if (kind == HK_DRONE) {
            d->nspan = hv_in(r, 10, 50) * Q16 / 100;
            if (hv_odds(r, 50))                        /* (fifths and octaves stacked) */
                for (k = 0; k < 6u; k++)
                    d->base[k] = (int32_t)(k % 3u == 0u ? 0 : k % 3u == 1u ? 7 : 12) << 16;
        }
        if (kind == HK_PAD && hv_odds(r, 35))
            d->base[2] = 12 << 16, d->base[5] = hv_odds(r, 50) ? 19 << 16 : -(12 << 16);
        break;
    case HK_CLUSTER:                                      /* steps off the scale: quarter-tones, seconds */
        for (k = 0; k < 6u; k++)
            d->base[k] = hv_in(r, -(int32_t)hv_in(r, 1, 7) * 2, hv_in(r, 1, 7) * 2) * (Q16 / 2);
        d->dspan = hv_in(r, 10, 100) * Q16 / 100;
        d->ktrk = hv_in(r, 16000, Q15);
        lo = 5, hi = 150;
        d->shim = hv_in(r, 4000, 20000);
        break;
    case HK_METAL:
        hv_partials(r, d, 0, 300, 2600);
        d->base[6] = 0;
        sw = hv_in(r, 0, 9000), wj = 0;
        d->wv[6] = 0;
        d->dspan = hv_in(r, 2, 20) * Q16 / 100;
        d->ktrk = hv_in(r, 10000, Q15);
        d->ring = hv_odds(r, 40) ? hv_in(r, 4000, 16000) : 0;
        d->cut = hv_in(r, 20000, 30000);
        d->famt = hv_in(r, 0, 8000);
        d->mix = hv_in(r, 20000, Q15);
        break;
    case HK_SPECTRA:                                      /* a slowly turning spectrum */
        hv_partials(r, d, 0, 128, 1800);
        d->base[6] = hv_odds(r, 50) ? -(12 << 16) : 0;
        sw = hv_odds(r, 30) ? hv_in(r, 8000, 24000) : hv_in(r, 0, 8000), wj = hv_in(r, 0, 4000);
        d->ktrk = hv_in(r, 0, 16000);
        lo = 2, hi = hv_odds(r, 40) ? 300 : 60;        /* (slowly turning, or shimmering) */
        d->shim = hv_in(r, 10000, Q15);
        d->aspan = hv_in(r, 2, 40) * Q16 / 100;
        d->nspan = hv_in(r, 5, 40) * Q16 / 100;
        d->dspan = hv_in(r, 5, 40) * Q16 / 100;
        d->flfo = hv_in(r, 2000, 9000);
        d->fmode = (uint8_t)(hv_odds(r, 40) ? 2 : 1);
        d->cut = hv_in(r, 10000, 30000);
        d->res = hv_in(r, 0, 24000);
        break;
    case HK_GROUND:                                       /* low, dense, rumbling; MIX gives the noise */
        for (k = 0; k < 7u; k++)
            d->base[k] = -(24 << 16) + hv_in(r, -5, 7) * (Q16 / 2) + (k == 6u ? 0 : hv_in(r, 0, 3) * (12 << 16) / 3);
        d->ktrk = hv_in(r, 0, 14000);
        d->noise = hv_in(r, 4000, 16000);
        d->nspan = hv_in(r, 20, 120) * Q16 / 100;
        d->dspan = hv_in(r, 20, 150) * Q16 / 100;
        lo = 3, hi = 60;
        d->shim = hv_in(r, 8000, 24000);
        d->cut = hv_in(r, 6000, 15000);
        d->res = hv_in(r, 4000, 20000);
        d->flfo = hv_in(r, 2000, 10000);
        d->fmode = (uint8_t)(hv_odds(r, 40) ? 2 : 1);
        d->drive = 4096 + hv_in(r, 4000, 12000);
        break;
    case HK_AIR:                                          /* wind and whistles; MIX gives the noise */
        {
            int32_t b0 = hv_in(r, 0, 24), nw = hv_in(r, 1, 7);   /* (how many whistles, from where) */
            for (k = 0; k < 7u; k++)
                d->base[k] = (b0 + hv_in(r, 0, 18)) << 16;
            for (k = 0; k < 7u; k++)
                d->amp[k] = (int16_t)((int32_t)k < nw ? hv_in(r, 2000, 14000) : 0);
        }
        sw = hv_in(r, 0, 6000), wj = 0;
        d->ktrk = hv_in(r, 0, 12000);
        d->noise = hv_in(r, 6000, 30000);
        d->nspan = hv_in(r, 50, 300) * Q16 / 100;
        d->aspan = hv_in(r, 20, 200) * Q16 / 100;
        lo = 3, hi = 80;
        d->shim = hv_in(r, 18000, Q15);
        d->cut = hv_in(r, 7000, 30000);
        d->res = hv_in(r, 6000, 28000);
        lo = hv_odds(r, 50) ? 3 : 30;
        d->flfo = hv_in(r, 4000, 18000);
        d->fmode = (uint8_t)(hv_odds(r, 70) ? 2 : 3);
        break;
    case HK_TIDES:                                        /* the swarm gliding apart and back, slowly */
        if (hv_odds(r, 50))
            hv_partials(r, d, 1, 200, 1600);
        d->aspan = hv_in(r, 50, 500) * Q16 / 100;
        d->dspan = hv_in(r, 20, 150) * Q16 / 100;
        d->ktrk = hv_in(r, 8000, Q15);
        lo = 2, hi = 30;
        d->shim = hv_in(r, 6000, 22000);
        d->flfo = hv_in(r, 2000, 8000);
        sw = hv_in(r, 0, 20000);
        break;
    }
    for (k = 0; k < 3u; k++)                              /* the LFOs: rates spread over the kind's range */
        d->lrate[k] = (uint16_t)(lo + (int32_t)(((int64_t)(hi - lo) * rnd_q15(r) * rnd_q15(r)) >> 30));
    for (k = 0; k < 6u; k++)
        d->wv[k] = (uint8_t)(clampi(sw + hv_in(r, -wj, wj), 0, Q15) >> 7);
    if (kind != HK_SUB && kind != HK_LEAD && kind != HK_METAL)
        d->wv[6] = (uint8_t)(clampi(sw + hv_in(r, -6000, 6000), 0, Q15) >> 7);
    d->att = hv_in(r, HK_ENV[kind][0][0], HK_ENV[kind][0][1]);
    d->dec = hv_in(r, HK_ENV[kind][1][0], HK_ENV[kind][1][1]);
    d->rel = hv_in(r, HK_ENV[kind][2][0], HK_ENV[kind][2][1]);
    d->sus = hv_in(r, HK_ENV[kind][3][0], HK_ENV[kind][3][1]) * Q15 / 100;
    if (path == PATH_EXP) {                               /* rougher: more noise in the pitch, more drive, a ring */
        d->nspan += hv_in(r, 5, 40) * Q16 / 100;
        d->drive += 4096;
        if (!d->ring && hv_odds(r, 25))
            d->ring = hv_in(r, 3000, 12000);
    } else if (path == PATH_AMBIENT) {                    /* darker, slower */
        d->cut = mulq15(d->cut, 29491);
        d->att = d->att < 200 ? d->att + hv_in(r, 50, 400) : d->att;
        for (k = 0; k < 3u; k++)
            d->lrate[k] = (uint16_t)(d->lrate[k] * 2u / 3u + 1u);
    }
    d->out = 4096;
}
static const char *hv_mac_name(const hv_design_t *d, uint32_t j)
{
    static const char *const N[NMAC] = {"DETUNE", "MOVEMENT", "MIX", "CUTOFF"};
    return (j & 3u) == 2u && (d->kind == HK_GROUND || d->kind == HK_AIR) ? "NOISE" : N[j & 3u];
}
static void hv_env_adsr(const hv_design_t *d, int32_t *a)
{
    a[0] = d->att;
    a[1] = d->dec;
    a[2] = d->sus * 100 / Q15;
    a[3] = d->rel;
}
/* the design's level: C3 held 1.5 s, its envelope at full; as the other engines' measures */
static void hv_measure_on(hv_design_t *d, hv_voice_t *probe)
{
    static int32_t win[2067u / 16u + 1u];
    int32_t out[CTL];
    uint32_t b, k, nwin = 0;
    int64_t acc = 0;
    int32_t peak = 1;
    d->out = 4096;
    hv_init(probe, d->seed ^ 0x77u);
    hv_note_on(probe, d, 48, 127, 1);
    probe->hold = 1;                                      /* (at full: attacks of seconds would read as quiet) */
    for (b = 0; b < 2067u; b++) {
        uint32_t i;
        if (b == 1378u)
            hv_note_off(probe);
        if (!hv_render_fp(probe, d, out, CTL))
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
        d->out = clampi(g < gp ? g : gp, 64, 4096 * 16);
    }
}
