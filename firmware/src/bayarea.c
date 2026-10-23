/* SPDX-License-Identifier: GPL-3.0-only
 * TONITOR: BAYAREA, the sixth engine -- a West Coast voice: a fixed-point port of the bayarea opcode of Vascular
 * (BayareaCore.h), contributed by its author to this firmware. Per voice:
 *   a carrier morphing sine -> triangle -> square -> saw (SHAPE), its frequency modulated linearly (through zero)
 *   by two sines at ratios of it (each ratio wandering a little, slowly);
 *   a wavefolder (FOLDS) reflecting it into +-0.5;
 *   a 12 kHz Butterworth smoothing filter, a breath of noise;
 *   then either a low-pass gate -- a resonant low-pass and an amplifier struck together by one-shot envelopes
 *   (a curved attack of 2-3 ms, a decay of DECAY) -- or a sustaining envelope over two resonant low-passes (an
 *   attack while the key is held, a release when let go); the filters sweep from 300 Hz (or the peak, if lower)
 *   to the design's peak, higher the harder the note;
 *   a saturating output (tanh, then a soft clip), driven by the design's drive;
 *   a bank of three resonators (508, 270, 1153 Hz, ringing 1 .. 1.4 s) beside it, mixed by its spring amount;
 *   one LFO (triangle, ramp, square, random steps) moving the shape, folds, both FM depths, attack, peak, decay
 *   and spring by amounts drawn with the sound.
 * Changed from the opcode for the FM-1: the 44-partial additive square and saw are band-limited by polyBLEP
 * (their even partials crossfaded as there); the reverb's combs and all-passes are left to the track's sends
 * (no memory for them), the resonator bank kept, its limiter a tanh; polyphonic from the shared pool of voices.
 * Control values at each 32-sample block, interpolated across it. */

#define BA_HZ8(x) ((int32_t)((x) * 256))

enum { BK_BONGO, BK_PLUCK, BK_CHIME, BK_FOLDED, BK_SWELL, BK_DRONE, BK_N };
static const char *const BK_NAME[BK_N] = {"BONGO", "PLUCK", "CHIME", "FOLDED", "SWELL", "DRONE"};
static const uint8_t BK_W[PATH_N][BK_N] = {
    {1, 1, 1, 1, 1, 1},
    {3, 1, 3, 0, 3, 4},
    {1, 3, 1, 4, 1, 1},
};
/* each kind's ranges, slider positions x 100: shape, folds, FM low, FM high, peak, decay, drive, spring */
static const uint8_t BK_R[BK_N][8][2] = {
    {{0, 30}, {0, 15}, {0, 30}, {0, 20}, {20, 50}, {10, 35}, {10, 30}, {0, 30}},    /* BONGO */
    {{30, 80}, {0, 40}, {0, 40}, {0, 30}, {50, 90}, {15, 45}, {10, 40}, {0, 40}},   /* PLUCK */
    {{0, 40}, {0, 20}, {0, 30}, {30, 80}, {60, 100}, {40, 70}, {5, 25}, {20, 60}},  /* CHIME */
    {{20, 70}, {50, 100}, {0, 50}, {0, 50}, {40, 90}, {30, 60}, {15, 50}, {0, 40}}, /* FOLDED */
    {{0, 70}, {0, 50}, {0, 40}, {0, 40}, {40, 80}, {50, 80}, {5, 25}, {10, 50}},    /* SWELL */
    {{0, 80}, {0, 60}, {20, 60}, {0, 50}, {20, 70}, {55, 85}, {5, 30}, {10, 60}},   /* DRONE */
};

typedef struct {
    uint32_t seed;               /* what made it: ba_design(-1, seed, path, gen) + ba_measure give it again */
    uint8_t kind, path, gen, asr;   /* asr: the sustaining envelope (else the low-pass gate) */
    uint8_t lfo_shape, rsv[3];
    int32_t shape, folds, fm1, fm2;    /* Q15 sliders (folds 0 .. 3 as Q15 x 3) */
    int32_t fm1r, fm2r;          /* the modulators' ratios, Q16 */
    int32_t attack, decay;       /* ms */
    int32_t peak;                /* Hz */
    int32_t drive;               /* x 0 .. 11, Q12 */
    int32_t spring;              /* Q15 */
    int32_t lfo_hz;              /* Q16 */
    int32_t to[8];               /* the LFO's amounts on shape, folds, FM low, FM high, attack, peak, decay, spring: Q15 */
    int32_t out;                 /* Q12: the measured level brought to the target */
} ba_design_t;

typedef struct { int32_t from, to, c, n, at, run; } ba_seg_t;   /* a curved segment, Q15 levels, n blocks */
typedef struct { int32_t a0, b1, b2; } ba_rc_t;                  /* a resonant low-pass's coefficients, Q29 */
typedef struct {
    uint8_t live, gate, hold, stage_c, stage_a, stage_g;   /* (stages: 0 idle, 1 attack, 2 decay / held, 3 release) */
    int8_t mac[NMAC];
    int32_t vel;                 /* Q15 */
    int32_t f;                   /* the note, Hz Q8 */
    rnd_t r;
    uint32_t lfo_ph, rph;        /* the LFO's phase; the random step's */
    int32_t lfo_rand;
    int32_t lag[9];              /* smoothed: ratio 1, ratio 2, A1, A2, shape, folds, attack, peak, decay ... spring */
    uint8_t primed;
    int32_t n1[3], n2[3], nu1, nu2;   /* the ratios' smooth wander: control points, place (Q16) */
    int32_t pm1, pm2, pamp1, pamp2, pxf, pfold, pev, pmix;   /* last block's control values (interpolated from) */
    int32_t m1, m2, amp1, amp2, xf, fold, ev, mix;
    uint32_t ph1, ph2, ph;       /* phases: the modulators, the carrier */
    int32_t bx1, bx2, by1, by2;  /* the 12 kHz smoothing filter */
    int32_t pk;                  /* the noise's low-pass */
    ba_seg_t cut, amp, gte;      /* the gate's two envelopes; the sustaining one */
    int32_t cl0, al0, gl0;       /* levels at the block's start (interpolated from) */
    ba_rc_t rc0, rc1;            /* the filter's coefficients, last block's and this */
    int32_t lv1, lv2, sv1[2], sv2[2];   /* the gate's low-pass; the two sustaining ones */
    int32_t rs[3][2];            /* the resonators */
    int32_t decay_ms, attack_ms; /* (as the block read them) */
    int32_t quiet;
} ba_voice_t;

static const int32_t BA_RES[3][3] = {{2141525796, -1073405498, 1229}, {2145614805, -1073461545, 2048},
                                     {2118335556, -1073501581, 819}};   /* c1, c2 Q30; amp x 0.25 Q15 */

/* ------------------------------------------------------------- helpers --- */
/* a segment from y0 to y1 over ms, its curve c (the Env law: (1 - e^(c t)) / (1 - e^c)) */
static void ba_seg(ba_seg_t *s, int32_t y0, int32_t y1, int32_t ms, int32_t c)
{
    s->from = y0;
    s->to = y1;
    s->c = c;
    s->n = clampi(ms * 1378 / 1000, 1, 1 << 20);   /* blocks */
    s->at = 0;
    s->run = 1;
}
static int32_t ba_seg_tick(ba_seg_t *s)          /* the level a block further in, Q15 */
{
    int32_t t, e1, et, g;
    if (!s->run)
        return s->to;
    if (++s->at >= s->n) {
        s->run = 0;
        return s->to;
    }
    t = (int32_t)((uint32_t)s->at * 32768u / (uint32_t)s->n);   /* (at < 2^16: 32 bits) */
    /* e^(c t) = 2^(c t log2 e): c t log2 e, Q16 */
    et = (int32_t)exp2_q16((int32_t)(((int64_t)s->c * t * 94548) >> 15));
    e1 = (int32_t)exp2_q16(s->c * 94548);
    {   /* (1 - e^(c t)) / (1 - e^c), Q15: both of one sign; 64 by 32 bits */
        uint32_t nm = (uint32_t)absi(Q16 - et), dn = (uint32_t)absi(Q16 - e1);
        g = dn ? (int32_t)udiv64_32(nm >> 17, nm << 15, dn) : Q15;
    }
    return s->from + (int32_t)(((int64_t)(s->to - s->from) * g) >> 15);
}
/* SuperCollider's RLPF at cutoff hz, rq (Q12): coefficients, Q29 */
static void ba_rlpf(ba_rc_t *k, int32_t hz, int32_t rq12)
{
    int32_t d = svf_g(clampi((int32_t)(((int64_t)hz * rq12) >> 12), 16, TANG_FMAX)), c;   /* tan(w rq / 2), Q24 */
    uint32_t w = (uint32_t)((uint64_t)(uint32_t)clampi(hz, 1, TANG_FMAX) * 97391u);   /* 2 pi f / FS as a phase */
    int32_t cw = sin_ph(w + 0x40000000u);                                          /* cos w, Q15 */
    {   /* (1 - D) / (1 + D), Q29 (64 by 32 bits, by its size and then its sign) */
        uint32_t nm = (uint32_t)absi((1 << 24) - d);
        c = (int32_t)udiv64_32(nm >> 3, nm << 29, (uint32_t)((1 << 24) + d));
        c = d > (1 << 24) ? -c : c;
    }
    k->b1 = (int32_t)(((int64_t)((1 << 29) + c) * cw) >> 15);
    k->b2 = -c;
    k->a0 = ((1 << 29) + c - k->b1) >> 2;
}
AINL int32_t ba_rlpf_tick(int32_t *v1, int32_t *v2, int32_t a0, int32_t b1, int32_t b2, int32_t x)
{
    int32_t v = (int32_t)(((int64_t)a0 * x + (int64_t)b1 * *v1 + (int64_t)b2 * *v2) >> 29);
    int32_t y = v + 2 * *v1 + *v2;
    *v2 = *v1;
    *v1 = v;
    return y;
}
/* polyBLEP: the step's correction at phase t of dt (both Q16 of a cycle) */
AINL int32_t ba_blep(int32_t t, int32_t dt, uint32_t inv)
{
    int32_t x;
    if (t < dt) {
        x = (int32_t)(((uint64_t)(uint32_t)t * inv) >> 16);   /* t / dt, Q16 */
        return (2 * x - (int32_t)(((int64_t)x * x) >> 16) - Q16) >> 1;   /* Q15 */
    }
    if (t > Q16 - dt) {
        x = -(int32_t)(((uint64_t)(uint32_t)(Q16 - t) * inv) >> 16);
        return ((int32_t)(((int64_t)x * x) >> 16) + 2 * x + Q16) >> 1;
    }
    return 0;
}
static int32_t ba_quad(const int32_t *p, int32_t u)   /* the quadratic B-spline through p at u (Q16), Q15 */
{
    int32_t v = Q16 - u, a = (int32_t)(((int64_t)v * v) >> 17), c = (int32_t)(((int64_t)u * u) >> 17);
    int32_t b = Q16 - a - c;
    return (int32_t)(((int64_t)a * p[0] + (int64_t)b * p[1] + (int64_t)c * p[2]) >> 16);
}

/* ------------------------------------------------------------ the voice --- */
static void ba_init(ba_voice_t *v, uint32_t seed)
{
    memset(v, 0, sizeof *v);
    rnd_seed(&v->r, seed);
}
static void ba_note_on(ba_voice_t *v, const ba_design_t *d, int32_t note, int32_t vel127, int fresh)
{
    if (fresh || !v->live) {
        rnd_t r = v->r;
        memset(v, 0, sizeof *v);
        v->r = r;
        v->n1[0] = rnd_bi(&v->r), v->n1[1] = rnd_bi(&v->r), v->n1[2] = rnd_bi(&v->r);
        v->n2[0] = rnd_bi(&v->r), v->n2[1] = rnd_bi(&v->r), v->n2[2] = rnd_bi(&v->r);
        v->lfo_rand = rnd_bi(&v->r);
    }
    v->f = (int32_t)(((uint64_t)pitch_inc(note * 16) * (uint32_t)FS) >> 24);   /* (inc x FS / 2^32, x 256) */
    v->vel = clampi(vel127, 1, 127) * 258;
    v->live = v->gate = 1;
    v->quiet = 0;
    if (d->asr) {                                  /* the sustain, opened (from where it is) */
        if (v->stage_g != 1 && v->stage_g != 2) {
            ba_seg(&v->gte, v->gl0, Q15, v->attack_ms ? v->attack_ms : d->attack, -4);
            v->stage_g = 1;
        }
    } else {                                       /* the gate, struck (from where it is) */
        ba_seg(&v->cut, v->cl0, Q15, 3, 4);
        ba_seg(&v->amp, v->al0, Q15, 2, 4);
        v->stage_c = v->stage_a = 1;
    }
}
static void ba_note_off(ba_voice_t *v, const ba_design_t *d)
{
    v->gate = 0;
    if (d->asr && v->stage_g && !v->hold) {
        ba_seg(&v->gte, v->gl0, 0, v->decay_ms ? v->decay_ms : d->decay, -4);
        v->stage_g = 3;
    }
}
static int32_t ba_knob(int32_t base, int32_t m, int32_t top) { return clampi(base + m * (top / (2 * MAC_MAX)), 0, top); }

/* a control block: the LFO, the destinations smoothed, the envelopes stepped, the filters set */
static void ba_control(ba_voice_t *v, const ba_design_t *d)
{
    int32_t L, sh, fo, a1, a2, at, pk, dc, sp, lo, top, fc, vel = v->vel;
    static const int32_t LAG5 = 20743, LAG10 = 12918;
    int32_t want[9], k;
    /* 8.1: the LFO */
    uint32_t sq = v->lfo_ph;
    v->lfo_ph += (uint32_t)(((uint64_t)(uint32_t)d->lfo_hz * 3116558u) >> 16);   /* hz Q16 x 2^32 / 1378.125 */
    switch (d->lfo_shape & 3u) {
    case 0: {                                      /* triangle */
        uint32_t p = v->lfo_ph >> 17;              /* Q15 of a cycle */
        L = p < 8192u ? (int32_t)p * 4 : p < 24576u ? 2 * Q15 - (int32_t)p * 4 : (int32_t)p * 4 - 4 * Q15;
        break;
    }
    case 1:
        L = (int32_t)((v->lfo_ph + 0x80000000u) >> 16) - Q15;   /* ramp */
        break;
    case 2:
        L = sq < 0x80000000u ? Q15 : 0;            /* square, 0 .. 1 */
        break;
    default:
        L = v->lfo_rand;
        break;
    }
    if ((v->rph += 2u * (uint32_t)(((uint64_t)(uint32_t)d->lfo_hz * 3116558u) >> 16)) < 2u * (uint32_t)(((uint64_t)(uint32_t)d->lfo_hz * 3116558u) >> 16))
        v->lfo_rand = rnd_bi(&v->r);
    /* 8.3: the destinations (macros on four of them), clipped, smoothed */
    sh = ba_knob(d->shape, v->mac[0], Q15);
    fo = ba_knob(d->folds, v->mac[1], Q15);
    a1 = ba_knob(d->fm1, v->mac[2], Q15);
    a2 = ba_knob(d->fm2, v->mac[2], Q15);
    dc = clampi((int32_t)(((int64_t)d->decay * exp2_q16(v->mac[3] * (Q16 * 3 / MAC_MAX))) >> 16), 10, 8000);
    a1 = clampi(a1 + mulq15(L, d->to[2]) / 2, 0, Q15);
    a2 = clampi(a2 + mulq15(L, d->to[3]) / 2, 0, Q15);
    want[0] = d->fm1r;
    want[1] = d->fm2r;
    want[2] = mulq15(a1, a1);
    want[3] = mulq15(a2, a2);
    want[4] = clampi(sh + mulq15(L, d->to[0]), 0, Q15);
    want[5] = clampi(fo + mulq15(L, d->to[1]) * 2 / 3, 0, Q15);   /* (folds + L x amount x 2, of 3) */
    at = clampi(d->attack + mulq15(L, d->to[4]) * 2200 / Q15, 3, 8000);
    pk = clampi((int32_t)(((int64_t)d->peak * exp2_q16(mulq15(L, d->to[5]) * 4)) >> 16), 100, 10000);   /* x 2^(2 L) */
    dc = clampi(dc + mulq15(L, d->to[6]) * 2200 / Q15, 10, 8000);
    sp = clampi(d->spring + mulq15(L, d->to[7]), 0, Q15);
    want[6] = at;
    want[7] = pk;
    want[8] = dc;
    if (!v->primed)
        for (k = 0; k < 9; k++)
            v->lag[k] = want[k];
    for (k = 0; k < 9; k++)
        v->lag[k] += (int32_t)(((int64_t)(want[k] - v->lag[k]) * LAG5) >> 15);
    v->attack_ms = v->lag[6];
    v->decay_ms = v->lag[8];
    /* 4.1: the ratios' slow wander (0.1 Hz) */
    v->nu1 += 4755;                                /* 0.1 / 1378 x 2^16 */
    v->nu2 += 4755;
    if (v->nu1 >= Q16) {
        v->nu1 -= Q16;
        v->n1[0] = v->n1[1], v->n1[1] = v->n1[2], v->n1[2] = rnd_bi(&v->r);
    }
    if (v->nu2 >= Q16) {
        v->nu2 -= Q16;
        v->n2[0] = v->n2[1], v->n2[1] = v->n2[2], v->n2[2] = rnd_bi(&v->r);
    }
    v->pm1 = v->m1, v->pm2 = v->m2, v->pamp1 = v->amp1, v->pamp2 = v->amp2;
    v->pxf = v->xf, v->pfold = v->fold, v->pev = v->ev, v->pmix = v->mix;
    v->m1 = (int32_t)(((int64_t)v->f * v->lag[0]) >> 16);
    v->m1 += (int32_t)(((int64_t)v->m1 * ba_quad(v->n1, v->nu1)) >> 25);   /* x (1 + 0.001 n), ~ */
    v->m2 = (int32_t)(((int64_t)v->f * v->lag[1]) >> 16);
    v->m2 += (int32_t)(((int64_t)v->m2 * ba_quad(v->n2, v->nu2)) >> 23);   /* x (1 + 0.005 n), ~ */
    v->amp1 = (int32_t)(((int64_t)v->m1 * v->lag[2] * 22) >> 15);   /* Hz Q8 of deviation */
    v->amp2 = (int32_t)(((int64_t)v->m2 * v->lag[3] * 12) >> 15);
    v->xf = v->lag[4] * 3;                         /* 0 .. 3, Q15 */
    v->fold = 4096 + (int32_t)(((int64_t)v->lag[5] * 8192 * 3) >> 15);   /* 1 + 2 folds (0 .. 3), Q12 */
    v->ev = v->lag[4] <= 21845 ? 0 : (v->lag[4] - 21845) * 3;   /* the even partials: shape 2/3 .. 1 */
    v->mix += (int32_t)(((int64_t)(sp - v->mix) * LAG10) >> 15);
    if (!v->primed) {
        v->pm1 = v->m1, v->pm2 = v->m2, v->pamp1 = v->amp1, v->pamp2 = v->amp2;
        v->pxf = v->xf, v->pfold = v->fold, v->pev = v->ev, v->pmix = v->mix = sp;
        v->primed = 1;
    }
    /* 5: the envelopes a block on; the filters' sweep */
    v->cl0 = v->stage_c ? ba_seg_tick(&v->cut) : 0;
    v->al0 = v->stage_a ? ba_seg_tick(&v->amp) : 0;
    if (v->stage_c == 1 && !v->cut.run) {
        ba_seg(&v->cut, Q15, 0, v->decay_ms, -20);
        v->stage_c = 2;
    } else if (v->stage_c == 2 && !v->cut.run)
        v->stage_c = 0;
    if (v->stage_a == 1 && !v->amp.run) {
        ba_seg(&v->amp, Q15, 0, v->decay_ms, -10);
        v->stage_a = 2;
    } else if (v->stage_a == 2 && !v->amp.run)
        v->stage_a = 0;
    if (v->stage_g == 1 || v->stage_g == 3) {
        v->gl0 = ba_seg_tick(&v->gte);
        if (!v->gte.run)
            v->stage_g = v->stage_g == 1 ? 2 : 0;
    }
    top = (int32_t)(((int64_t)v->lag[7] * (16384 + (vel >> 1))) >> 15);   /* peak x (0.5 .. 1 by the note's level) */
    lo = top < 300 ? top : 300;
    v->rc0 = v->rc1;
    if (d->asr) {
        fc = lo + (int32_t)(((int64_t)(top - lo) * (v->hold ? Q15 : v->gl0)) >> 15);
        ba_rlpf(&v->rc1, fc, 3891);                /* rq 0.95 */
    } else {
        fc = lo + (int32_t)(((int64_t)(top - lo) * v->cl0) >> 15);
        ba_rlpf(&v->rc1, fc, 3686);                /* rq 0.9 */
    }
    if (v->primed == 1) {
        v->rc0 = v->rc1;
        v->primed = 2;
    }
}
static __typeof__(&ba_control) volatile ba_control_fp = ba_control;   /* (the RAM code's way into the flash) */

/* n samples (a block) into out, Q15 */
static FAST int ba_render(ba_voice_t *v, const ba_design_t *d, int32_t *out, uint32_t n)
{
    static const int32_t BW[5] = {179736481, 359472962, 179736481, 87442742, 94632269};   /* 12 kHz, Q29 */
    uint32_t i;
    int32_t g0, g1, pk = 0, dt;
    uint32_t inv;
    int32_t drive = (int32_t)(((int64_t)d->drive * 6144) >> 12);   /* x 1.5 (no pressure), Q12 */
    if (!v->live)
        return 0;
    n = n > CTL ? CTL : n;
    {
        int32_t was_c = v->cl0, was_a = v->al0, was_g = v->gl0;
        ba_control_fp(v, d);
        g0 = d->asr ? was_g : was_a;
        g1 = d->asr ? v->gl0 : v->al0;
        (void)was_c;
    }
    if (v->hold)                                   /* (the track's own envelope: this one at full) */
        g0 = g1 = Q15;
    dt = (int32_t)(((uint64_t)(uint32_t)v->f * 97391u) >> 24);   /* the carrier's step, Q16 of a cycle */
    dt = dt < 1 ? 1 : dt;
    inv = 0xFFFFFFFFu / (uint32_t)dt;                             /* 1 / dt, Q16 */
    for (i = 0; i < n; i++) {
        int32_t t = (int32_t)(i + 1u) * (Q15 / CTL);              /* across the block, Q15 */
        int32_t m1 = v->pm1 + mulq15(v->m1 - v->pm1, t), m2 = v->pm2 + mulq15(v->m2 - v->pm2, t);
        int32_t a1 = v->pamp1 + mulq15(v->amp1 - v->pamp1, t), a2 = v->pamp2 + mulq15(v->amp2 - v->pamp2, t);
        int32_t fc, x, s, sine, tri, sq, saw, add, fold, u, y, g;
        uint32_t p;
        int32_t inc;
        fc = v->f + mulq15(sin_ph(v->ph1), a1) + mulq15(sin_ph(v->ph2), a2);   /* Hz Q8, through zero */
        v->ph1 += (uint32_t)(((uint64_t)(uint32_t)m1 * 97391u) >> 8);           /* hz8 x 2^32 / FS / 256 */
        v->ph2 += (uint32_t)(((uint64_t)(uint32_t)m2 * 97391u) >> 8);
        inc = (int32_t)(((int64_t)fc * 97391) >> 8);
        v->ph += (uint32_t)inc;
        p = v->ph;
        sine = sin_ph(p) >> 1;
        {
            int32_t q = (int32_t)(p >> 17);                         /* Q15 of a cycle */
            tri = (q < 16384 ? 4 * q - Q15 : 3 * Q15 - 4 * q) >> 1;
        }
        s = v->pxf + mulq15(v->xf - v->pxf, t);
        if (s > Q15) {                                              /* the square and the saw, band-limited */
            int32_t ph16 = (int32_t)(p >> 16), ev = v->pev + mulq15(v->ev - v->pev, t);
            int32_t ph2 = (ph16 + 32768) & 0xFFFF;
            sq = (ph16 < 32768 ? Q15 : -Q15) + ba_blep(ph16, dt, inv) - ba_blep(ph2, dt, inv);
            saw = (ph16 >> 0) - Q15 - ba_blep(ph16, dt, inv);       /* 2p - 1 */
            sq = mulq15(sq, 25736);                                  /* x pi / 4: the odd partials' sum */
            saw = -mulq15(saw, 51472);                               /* x -pi / 2: every partial's */
            add = sq + mulq15(saw - sq, ev);
        } else
            add = 0;
        if (s <= Q15)
            x = sine + mulq15(tri - sine, s);
        else if (s <= 2 * Q15)
            x = tri + mulq15(add - tri, s - Q15);
        else
            x = add;
        /* 4.4: the folder, into +-0.5 */
        fold = v->pfold + (int32_t)(((int64_t)(v->fold - v->pfold) * t) >> 15);
        u = ((int32_t)(((int64_t)x * fold) >> 12) + 16384) & 0xFFFF;   /* mod 2 (Q15) */
        x = (u < Q15 ? u : 2 * Q15 - u) - 16384;
        /* 4.5: the smoothing filter, the noise */
        y = (int32_t)(((int64_t)BW[0] * x + (int64_t)BW[1] * v->bx1 + (int64_t)BW[2] * v->bx2 - (int64_t)BW[3] * v->by1 -
                       (int64_t)BW[4] * v->by2) >> 29);
        v->bx2 = v->bx1, v->bx1 = x, v->by2 = v->by1, v->by1 = y;
        v->pk += (rnd_bi(&v->r) - v->pk) >> 3;
        x = y + (v->pk * 3 >> 9);                                    /* (~ -50 dB) */
        /* 5: the gate, or the sustain */
        {
            int32_t a0 = v->rc0.a0 + mulq15(v->rc1.a0 - v->rc0.a0, t), b1 = v->rc0.b1 + mulq15(v->rc1.b1 - v->rc0.b1, t);
            int32_t b2 = v->rc0.b2 + mulq15(v->rc1.b2 - v->rc0.b2, t);
            if (d->asr)
                x = ba_rlpf_tick(&v->sv1[1], &v->sv2[1], a0, b1, b2, ba_rlpf_tick(&v->sv1[0], &v->sv2[0], a0, b1, b2, x));
            else
                x = ba_rlpf_tick(&v->lv1, &v->lv2, a0, b1, b2, x);
        }
        g = g0 + mulq15(g1 - g0, t);
        x = mulq15(x, g);
        /* 6: the drive, tanh, a soft clip */
        x = tanh_q15((int32_t)(((int64_t)x * drive) >> 12));
        if (absi(x) > 16384)                                          /* (|v| - 0.25) / v */
            x = x > 0 ? Q15 - (int32_t)(0x10000000u / (uint32_t)x) : -(Q15 - (int32_t)(0x10000000u / (uint32_t)(-x)));
        /* 7.3: the resonator bank, beside */
        {
            int32_t bank = 0, k, mix = v->pmix + mulq15(v->mix - v->pmix, t);
            for (k = 0; k < 3; k++) {
                int32_t *r = v->rs[k];
                int32_t vv = x + (int32_t)(((int64_t)BA_RES[k][0] * r[0] + (int64_t)BA_RES[k][1] * r[1]) >> 30);
                bank += mulq15(vv - r[1], BA_RES[k][2]);
                r[1] = r[0];
                r[0] = vv;
            }
            x += mulq15(tanh_q15(bank), mulq15(mix, 11469));          /* (0.35) */
        }
        x = (int32_t)(((int64_t)x * d->out) >> 12);
        out[i] = x;
        pk = absi(x) > pk ? absi(x) : pk;
    }
    if (!(d->asr ? v->stage_g : (v->stage_a || v->stage_c)) && !v->hold)
        v->quiet = pk < 8 ? v->quiet + 1 : 0;
    if (v->quiet > 64)
        v->live = 0;                                                 /* (its envelope over, the bank rung out) */
    return 1;
}
static __typeof__(&ba_render) volatile ba_render_fp = ba_render;   /* (the flash's way into the RAM code) */

/* ----------------------------------------------------------- the design --- */
static void ba_design(ba_design_t *d, int32_t kind, uint32_t seed, uint32_t path, uint32_t gen)
{
    rnd_t rr, *r = &rr;
    int32_t sl[8], k;
    path = path < PATH_N ? path : PATH_OPEN;
    rnd_seed(r, seed ^ 0xBA7A4Eu);
    if (kind < 0 || kind >= BK_N)
        kind = (int32_t)rnd_weighted(r, BK_W[path], BK_N);
    memset(d, 0, sizeof *d);
    d->seed = seed;
    d->kind = (uint8_t)kind;
    d->path = (uint8_t)path;
    d->gen = (uint8_t)gen;
    for (k = 0; k < 8; k++)
        sl[k] = rnd_in(r, BK_R[kind][k][0] * Q15 / 100, BK_R[kind][k][1] * Q15 / 100);
    d->shape = sl[0];
    d->folds = sl[1];
    d->fm1 = sl[2];
    d->fm2 = sl[3];
    d->peak = (int32_t)((100u * exp2_q16((int32_t)(((int64_t)sl[4] * 435411) >> 15))) >> 16);   /* 100 x 100^v */
    d->decay = (int32_t)((10u * exp2_q16((int32_t)(((int64_t)sl[5] * 632071) >> 15))) >> 16);   /* 10 ms x 800^v */
    d->drive = (int32_t)(((int64_t)sl[6] * 11 * 4096) >> 15) + 410;                             /* (x 0.1 at least) */
    d->spring = sl[7];
    d->fm1r = 43254;                                                    /* 0.66 */
    d->fm2r = 216269;                                                   /* 3.3 */
    if (rnd_chance(r, path == PATH_EXP ? 60 : 30)) {                    /* (other ratios, now and then) */
        d->fm1r = rnd_in(r, 6554, Q16);
        d->fm2r = rnd_in(r, Q16, 10 * Q16);
    }
    /* what the opcode's dice draw beyond the sliders (less what would move the pitch) */
    d->asr = (uint8_t)(kind == BK_SWELL || kind == BK_DRONE);
    {
        int32_t x = rnd_q15(r), x2 = mulq15(x, x);
        d->attack = 3 + mulq15(mulq15(x2, x2), 8000);                   /* 3 ms .. 8 s, x^4 */
    }
    if (kind == BK_SWELL)
        d->attack = rnd_in(r, 300, 3000);
    else if (kind == BK_DRONE)
        d->attack = rnd_in(r, 500, 4000);
    d->lfo_shape = (uint8_t)rnd_below(r, 4);
    d->lfo_hz = rnd_in(r, 66, 10 * Q16);                                /* 0.001 .. 10 Hz */
    if (path == PATH_AMBIENT)
        d->lfo_hz = rnd_in(r, 66, Q16);
    for (k = 0; k < 8; k++)
        d->to[k] = rnd_chance(r, k == 2 || k == 3 ? 25 : 35) ? rnd_q15(r) : 0;
    if (path == PATH_EXP) {                                             /* harder: more drive, more fold */
        d->drive += 4096;
        d->folds = clampi(d->folds + rnd_in(r, 0, Q15 / 3), 0, Q15);
    }
    d->out = 4096;
}
static const char *ba_mac_name(const ba_design_t *d, uint32_t j)
{
    static const char *const N[NMAC] = {"SHAPE", "FOLDS", "FM", "DECAY"};
    (void)d;
    return N[j & 3u];
}
/* the design's own envelope as A D S R, ms (the ENVELOPE page, as drawn) */
static void ba_env_adsr(const ba_design_t *d, int32_t *a)
{
    a[0] = d->asr ? d->attack : 2;
    a[1] = d->asr ? 5 : d->decay;
    a[2] = d->asr ? 100 : 0;
    a[3] = d->decay;
}
/* the design's level: C3 held 1 s, let go; the 75th percentile of 12 ms windows brought to 0.1 RMS, no more than
 * keeps its loudest moment under half of full scale */
static void ba_measure_on(ba_design_t *d, ba_voice_t *probe)
{
    static int32_t win[2067u / 16u + 1u];
    int32_t out[CTL];
    uint32_t b, k, nwin = 0;
    int64_t acc = 0;
    int32_t peak = 1;
    d->out = 4096;
    ba_init(probe, d->seed ^ 0x77u);
    ba_note_on(probe, d, 48, 127, 1);
    for (b = 0; b < 2067u; b++) {
        uint32_t i;
        if (b == 1378u)
            ba_note_off(probe, d);
        if (!ba_render_fp(probe, d, out, CTL))
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
