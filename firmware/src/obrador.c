/* SPDX-License-Identifier: GPL-3.0-only
 * TONITOR: OBRADOR, the eighth engine -- an analog polysynth voice after the Oberheim SEM and the two-voice
 * Oberheims, with OberVave (zvenson/obervave, GPL-3.0: an Oberheim-style firmware for the FM-1) as its frame of
 * reference; written for Tonitor, not ported from it. Per voice:
 *   two oscillators, saw and / or pulse, band-limited by polyBLEP: the second detuned, an interval away, now and then
 *   hard-synced to the first (its pitch swept by the filter's envelope) or modulating it (cross-modulation, linear);
 *   noise; in the lush kinds a second voice card -- two more oscillators -- detuned against the first, as the
 *   two-voice Oberheims' cards were;
 *   a drive into the filter (tanh);
 *   the SEM's filter: a 12 dB state-variable filter (Simper's, as dsp.h's) its output morphing low-pass -> notch ->
 *   high-pass (MORPH), its resonance soft-limited (it rings, warm, and never runs away); for brass and basses a
 *   second low-pass stage after it (24 dB), for the metal ones its band-pass;
 *   two envelopes, the filter's and the amplifier's, on analog RC curves (an attack aiming past full, as a
 *   capacitor's charge);
 *   an LFO (triangle, ramp, square, steps, smooth random) after a delay, fading in, on the pitch, the pulse widths,
 *   the cutoff and the morph;
 *   vintage: each card its own small offsets of tune and cutoff, and a slow random walk on its pitch.
 * Control values at each 32-sample block, the amplifier interpolated across it. */

enum { OBK_STRINGS, OBK_BRASS, OBK_PWM, OBK_BASS, OBK_SYNC, OBK_NOTCH, OBK_METAL, OBK_DRONE, OBK_KEYS, OBK_SWELL, OBK_N };
static const char *const OBK_NAME[OBK_N] = {"STRINGS", "BRASS", "PWM PAD", "BASS", "SYNC",
                                            "NOTCH", "METAL", "DRONE", "KEYS", "SWELL"};
static const uint8_t OBK_W[PATH_N][OBK_N] = {
    {3, 3, 3, 2, 2, 2, 2, 2, 2, 2},
    {5, 1, 5, 0, 0, 2, 1, 5, 1, 4},
    {1, 2, 1, 3, 4, 4, 4, 1, 2, 1},
};
enum { OBF_SEM, OBF_LP24, OBF_BP };              /* the filter: the 12 dB morph; a second low-pass stage after; band-pass */
#define OB_SAW 1u
#define OB_PLS 2u

typedef struct {
    uint32_t seed;               /* what made it: ob_design(kind, seed, path, gen) + ob_measure give it again */
    uint8_t kind, path, gen, dual;   /* dual: the second voice card */
    uint8_t w1, w2, ftype, sync; /* the waves (OB_SAW | OB_PLS); the filter; OSC 2 synced to OSC 1 */
    uint8_t lfo_shape, rsv[3];
    int32_t semi2;               /* OSC 2's interval, 1/16 semitone */
    int32_t det, cdet;           /* OSC 2's detune; the second card's: Q16 of the frequency (+- a few cents) */
    int32_t pw1, pw2, pwm;       /* pulse widths (Q15 of a cycle); the LFO's on them, Q15 */
    int32_t lvl1, lvl2, noise;   /* Q15 */
    int32_t xmod;                /* OSC 2 -> OSC 1, Q15 */
    int32_t env2;                /* the filter envelope on OSC 2's pitch (the sync sweep), 1/16 semitone at full */
    int32_t cut;                 /* the cutoff, 1/16 semitone (as a MIDI note x 16) */
    int32_t reso;                /* Q15: 0 none .. 1 near self-oscillation */
    int32_t morph;               /* Q15: 0 low-pass, 0.5 notch, 1 high-pass (the SEM's) */
    int32_t fenv;                /* the filter envelope's depth, 1/16 semitone (+-) */
    int32_t keytrk;              /* Q15: the cutoff following the note */
    int32_t velcut;              /* 1/16 semitone at full velocity */
    int32_t drive;               /* Q12, 1 .. */
    int32_t fa, fd, fs, fr;      /* the filter envelope: ms, sustain Q15 */
    int32_t aa, ad, as, ar;      /* the amplifier's */
    int32_t lfo_hz;              /* Q16 */
    int32_t lfo_delay;           /* ms */
    int32_t lfo_pitch, lfo_cut;  /* 1/16 semitone at full */
    int32_t lfo_morph;           /* Q15 */
    int32_t vint;                /* Q15: the cards' offsets and drift */
    int32_t out;                 /* Q12: the measured level brought to the target */
} ob_design_t;

typedef struct {
    uint8_t live, gate, hold, fst, ast, ncard;   /* stages: 0 idle, 1 attack, 2 decay / sustain, 3 release */
    int8_t mac[NMAC];
    rnd_t r;
    int32_t note16, vel;         /* the note (1/16 semitone); its level, Q15 */
    int32_t fe, ae;              /* the envelopes, Q24 */
    int32_t kfa, kfd, kfr, kaa, kad, kar;   /* their steps a block, Q24 */
    uint32_t ph[4];              /* the oscillators: card 1's OSC 1, OSC 2; card 2's */
    int32_t xprev[2];            /* OSC 2's last sample, each card (cross-modulation) */
    uint32_t inc[4];             /* this block's steps */
    uint32_t pw[2];              /* this block's pulse widths, a cycle in 32 bits */
    svf_t f[2][2];               /* each card's filter: the stage, the 24 dB's second */
    svf_c c[2], c2[2];           /* this block's coefficients */
    int32_t morph;               /* this block's, Q15 */
    int32_t drift[2];            /* each card's walk, 1/16 semitone Q8 */
    uint32_t lfo_ph;
    int32_t lfo_a, lfo_b;        /* the random shapes' last and next values */
    int32_t lfo_fade;            /* Q15 */
    uint32_t blk;                /* blocks since the note */
    int32_t amp0, amp1;          /* Q15 */
    int32_t quiet;
} ob_voice_t;

static const int16_t OB_CARD_TUNE[2] = {-40, 55};   /* the cards' tune offsets, tenths of a cent at full vintage */
static const int16_t OB_CARD_CUT[2] = {-12, 15};    /* .. cutoff offsets, 1/16 semitone at full vintage */

/* ------------------------------------------------------------- helpers --- */
/* an RC step a block for a time of ms: the time constant, Q24 (attack: the charge aims at 1.3 and stops at 1 -- 1.47
 * time constants; decay and release: three time constants make the time) */
static int32_t ob_k(int32_t ms, int32_t tcs8)
{
    int32_t n = clampi(ms * 1378 / 1000, 1, 1 << 20);   /* blocks */
    uint32_t k = (16777216u / (uint32_t)n) * (uint32_t)tcs8 >> 3;
    return (int32_t)(k > 16777216u ? 16777216u : k);   /* (shorter than a block: at once) */
}
/* a cutoff in 1/16 semitone (MIDI x 16) to Hz: 440 x 2^((c - A4) / 192), 20 Hz .. 28 kHz */
static int32_t ob_hz(int32_t c16)
{
    c16 = clampi(c16, 16 * 16, 2047 + 16 * 14);
    return (int32_t)(((uint64_t)exp2_q16((c16 - 69 * 16) * 1024 / 3) * 440u) >> 16);   /* (Q16 / 192 = 1024 / 3) */
}
static int32_t ob_cents(int32_t cents10)          /* tenths of a cent as Q16 of the frequency */
{
    return cents10 * 38 / 10;                      /* (2^(1/1200) - 1 = 0.000578 = 37.9 / 65536) */
}
/* polyBLEP: the step's correction at phase t of dt (both Q16 of a cycle), Q15 */
AINL int32_t ob_blep(int32_t t, int32_t dt, uint32_t inv)
{
    int32_t x;
    if (t < dt) {
        x = (int32_t)(((uint64_t)(uint32_t)t * inv) >> 16);
        return (2 * x - (int32_t)(((int64_t)x * x) >> 16) - Q16) >> 1;
    }
    if (t > Q16 - dt) {
        x = -(int32_t)(((uint64_t)(uint32_t)(Q16 - t) * inv) >> 16);
        return ((int32_t)(((int64_t)x * x) >> 16) + 2 * x + Q16) >> 1;
    }
    return 0;
}
/* an oscillator's sample at phase p (its step dt, 1/dt inv): saw, pulse (a saw less a saw pw later), or both, Q15 */
AINL int32_t ob_osc(uint32_t p, int32_t dt, uint32_t inv, uint32_t w, uint32_t pw)
{
    int32_t t = (int32_t)(p >> 16), v = 0;
    int32_t saw = t - Q15 - ob_blep(t, dt, inv);
    if (w & OB_SAW)
        v = saw;
    if (w & OB_PLS) {
        int32_t t2 = (int32_t)((p + pw) >> 16);
        v += saw - (t2 - Q15 - ob_blep(t2, dt, inv));
    }
    return w == (OB_SAW | OB_PLS) ? v >> 1 : v;
}
/* the filter a sample on: low-pass, band-pass, high-pass at once; the band-pass integrator softly limited above
 * three quarters of full scale (the resonance stays bounded and warm) */
AINL void ob_svf(svf_t *s, const svf_c *c, int32_t x, int32_t *lp, int32_t *bp, int32_t *hp)
{
    int32_t v3 = x - s->ic2;
    int32_t v1 = mulr16(c->a1, s->ic1) + mulr24(c->a2, v3);
    int32_t v2 = s->ic2 + mulr24(c->a2, s->ic1) + mulr24(c->a3, v3);
    int32_t n1 = 2 * v1 - s->ic1;
    s->ic1 = absi(n1) < 49152 ? n1 : tanh_q15(n1 >> 1) * 2;
    s->ic2 = 2 * v2 - s->ic2;
    *lp = v2;
    *bp = v1;
    *hp = x - mulr24(c->k, v1) - v2;
}

/* ------------------------------------------------------------ the voice --- */
static void ob_init(ob_voice_t *v, uint32_t seed)
{
    memset(v, 0, sizeof *v);
    rnd_seed(&v->r, seed);
}
static void ob_note_on(ob_voice_t *v, const ob_design_t *d, int32_t note, int32_t vel127, int fresh)
{
    if (fresh || !v->live) {
        rnd_t r = v->r;
        uint32_t k;
        memset(v, 0, sizeof *v);
        v->r = r;
        for (k = 0; k < 4u; k++)                   /* free-running oscillators: anywhere */
            v->ph[k] = rnd_next(&v->r);
        v->lfo_a = rnd_bi(&v->r);
        v->lfo_b = rnd_bi(&v->r);
        v->drift[0] = rnd_bi(&v->r) >> 4;
        v->drift[1] = rnd_bi(&v->r) >> 4;
    }
    v->note16 = note * 16;
    v->vel = clampi(vel127, 1, 127) * 258;
    v->ncard = (uint8_t)(d->dual ? 2u : 1u);
    v->kfa = ob_k(d->fa, 12);                      /* (attack: 1.47 time constants, x 8) */
    v->kfd = ob_k(d->fd, 24);
    v->kfr = ob_k(d->fr, 24);
    v->kaa = ob_k(d->aa, 5);
    v->kad = ob_k(d->ad, 24);
    v->kar = ob_k(d->ar, 24);
    v->live = v->gate = 1;
    v->fst = v->ast = 1;                           /* (from where they are: a voice taken again glides on) */
    v->blk = 0;
    v->lfo_fade = d->lfo_delay ? 0 : Q15;
    v->quiet = 0;
}
static void ob_note_off(ob_voice_t *v)
{
    v->gate = 0;
    if (v->fst)
        v->fst = 3;
    if (v->ast && !v->hold)
        v->ast = 3;
}
/* an envelope a block on (level Q24): attack aiming at 1.3, decay to the sustain, release to nothing */
static void ob_env(int32_t *e, uint8_t *st, int32_t ka, int32_t kd, int32_t s24, int32_t kr)
{
    switch (*st) {
    case 1:
        *e += mul24(16777216 * 13 / 10 - *e, ka);
        if (*e >= 16777216) {
            *e = 16777216;
            *st = 2;
        }
        break;
    case 2:
        *e += mul24(s24 - *e, kd);
        break;
    case 3:
        *e += mul24(-*e, kr) - 16;
        if (*e <= 0) {
            *e = 0;
            *st = 0;
        }
        break;
    default:
        *e = 0;
    }
}
static int32_t ob_lfo_shape(ob_voice_t *v, uint32_t ph, uint32_t sh)   /* Q15, +- */
{
    switch (sh) {
    case 0: {
        int32_t p = (int32_t)((ph + 0x40000000u) >> 15);
        return p < Q16 ? p - Q15 : 98303 - p;
    }
    case 1:
        return (int32_t)(ph >> 16) - Q15;
    case 2:
        return ph < 0x80000000u ? 32767 : -32767;
    case 3:
        return v->lfo_b;
    default:
        return v->lfo_a + (int32_t)(((int64_t)(v->lfo_b - v->lfo_a) * (ph >> 16)) >> 16);
    }
}

/* a control block: the envelopes, the LFO, the drift; the pitches, widths, cutoffs and the amplifier */
static void ob_control(ob_voice_t *v, const ob_design_t *d)
{
    int32_t lfo, fenv, cut, det, reso, q12, k, mor;
    uint32_t old = v->lfo_ph, c;
    int32_t m0 = v->mac[0], m1 = v->mac[1], m2 = v->mac[2], m3 = v->mac[3];
    ob_env(&v->fe, &v->fst, v->kfa, v->kfd, d->fs * 512, v->kfr);
    ob_env(&v->ae, &v->ast, v->kaa, v->kad, d->as * 512, v->kar);
    fenv = v->fe >> 9;                              /* Q15 */
    /* the LFO, after its delay, fading in */
    v->lfo_ph += (uint32_t)(((uint64_t)(uint32_t)d->lfo_hz * 3116558u) >> 16);   /* hz Q16 x 2^32 / 1378.125 */
    if (v->lfo_ph < old) {
        v->lfo_a = v->lfo_b;
        v->lfo_b = rnd_bi(&v->r);
    }
    lfo = ob_lfo_shape(v, v->lfo_ph, d->lfo_shape);
    if (v->lfo_fade < Q15 && (int32_t)v->blk >= d->lfo_delay * 1378 / 1000)
        v->lfo_fade = clampi(v->lfo_fade + Q15 / (d->lfo_delay * 1378 / 2000 + 1) + 1, 0, Q15);
    v->blk++;
    lfo = mulq15(lfo, v->lfo_fade);
    /* the drift: a slow random walk each card */
    for (c = 0; c < 2u; c++) {
        v->drift[c] += (rnd_bi(&v->r) >> 9) - (v->drift[c] >> 9);
    }
    /* the pitches: OSC 1, OSC 2 (the interval, its detune, the sync sweep), each card */
    det = d->det + d->det * m3 / (MAC_MAX / 2);  /* (DETUNE: none .. three times; det < 2^12: 32 bits) */
    det = clampi(det, 0, Q16 / 20);
    for (c = 0; c < v->ncard; c++) {
        int32_t p = v->note16 + mulq15(lfo, d->lfo_pitch);
        int32_t p2 = p + d->semi2 + mulq15(fenv, d->env2);
        uint32_t i1 = pitch_inc(clampi(p, 0, 2047)), i2 = pitch_inc(clampi(p2, 0, 2047));
        /* the fine offsets, Q16 of the frequency: the card's tune and drift (vintage); the second card's detune; OSC 2's */
        int32_t fine = ob_cents((OB_CARD_TUNE[c] * d->vint) >> 15) + (int32_t)(((int64_t)v->drift[c] * d->vint) >> 21);
        int32_t cd = c ? clampi(d->cdet + d->cdet * m3 / (MAC_MAX / 2), 0, Q16 / 20) : 0;
        i1 = (uint32_t)((int64_t)i1 + (((int64_t)i1 * (fine + cd)) >> 16));
        i2 = (uint32_t)((int64_t)i2 + (((int64_t)i2 * (fine + det - cd / 2)) >> 16));
        v->inc[2u * c] = i1;
        v->inc[2u * c + 1u] = i2;
    }
    {   /* the pulse widths, the LFO on them */
        int32_t pm = mulq15(lfo, d->pwm);
        v->pw[0] = (uint32_t)clampi(d->pw1 + pm, 1638, 31130) << 17;
        v->pw[1] = (uint32_t)clampi(d->pw2 - pm, 1638, 31130) << 17;
    }
    /* the cutoff: CUTOFF's macro (+- 4 octaves), the envelope, the key, the velocity, the LFO */
    cut = d->cut + m0 * (768 / MAC_MAX) + mulq15(fenv, d->fenv) + mulq15(v->note16 - 60 * 16, d->keytrk) +
          mulq15(v->vel, d->velcut) + mulq15(lfo, d->lfo_cut);
    reso = clampi(d->reso + m2 * (Q15 / (2 * MAC_MAX)), 0, 31130);
    q12 = 2048 + (int32_t)(((int64_t)reso * reso >> 15) * 18 * 4096 >> 15);   /* q 0.5 .. ~18.5 */
    mor = clampi(d->morph + m1 * (Q15 / (2 * MAC_MAX)) + mulq15(lfo, d->lfo_morph), 0, Q15);
    v->morph = mor;
    for (c = 0; c < v->ncard; c++) {
        int32_t hz = ob_hz(cut + ((OB_CARD_CUT[c] * d->vint) >> 15));
        svf_coef(&v->c[c], hz, q12);
        if (d->ftype == OBF_LP24)
            svf_coef(&v->c2[c], hz, 2896);         /* (the second stage plain: q 0.71) */
    }
    /* the amplifier: its envelope (or full: the track's own), the velocity */
    v->amp0 = v->amp1;
    k = v->hold ? Q15 : v->ae >> 9;
    v->amp1 = mulq15(k, 16384 + (v->vel >> 1));
}
static __typeof__(&ob_control) volatile ob_control_fp = ob_control;   /* (the RAM code's way into the flash) */

/* n samples (a block) into out, Q15 */
static FAST int ob_render(ob_voice_t *v, const ob_design_t *d, int32_t *out, uint32_t n)
{
    uint32_t i, c, nc;
    int32_t pk = 0, a0, da, mor = v->morph;
    int32_t dt[4];
    uint32_t inv[4];
    int32_t l1 = d->lvl1, l2 = d->lvl2, nz = d->noise, xm = d->xmod, drv = d->drive;
    if (!v->live)
        return 0;
    n = n > CTL ? CTL : n;
    ob_control_fp(v, d);
    nc = v->ncard;
    for (c = 0; c < 2u * nc; c++) {
        dt[c] = (int32_t)(v->inc[c] >> 16);
        dt[c] = dt[c] < 1 ? 1 : dt[c];
        inv[c] = 0xFFFFFFFFu / (uint32_t)dt[c];
    }
    a0 = v->amp0;
    da = v->amp1 - v->amp0;
    for (i = 0; i < n; i++) {
        int32_t y = 0;
        for (c = 0; c < nc; c++) {
            uint32_t *ph = &v->ph[2u * c], i1 = v->inc[2u * c];
            int32_t s1, s2, x, lp, bp, hp, z;
            if (xm)                                /* OSC 2 -> OSC 1, linear (through zero no lower than a sixteenth) */
                i1 = (uint32_t)clampi((int32_t)i1 + (int32_t)(((int64_t)(int32_t)(i1 >> 1) * mulq15(v->xprev[c], xm)) >> 14),
                                      (int32_t)(i1 >> 4), 0x7FFFFFFF);
            ph[0] += i1;
            s1 = ob_osc(ph[0], dt[2u * c], inv[2u * c], d->w1, v->pw[0]);
            if (d->sync && ph[0] < i1)             /* OSC 1 went round: OSC 2 starts again, as far into it (the
                                                    * fraction of a sample since: ph / its step, by the step's inverse) */
                ph[1] = (uint32_t)(((uint64_t)(uint32_t)(((uint64_t)ph[0] * inv[2u * c]) >> 32) * v->inc[2u * c + 1u]) >> 16);
            else
                ph[1] += v->inc[2u * c + 1u];
            s2 = ob_osc(ph[1], dt[2u * c + 1u], inv[2u * c + 1u], d->w2, v->pw[1]);
            v->xprev[c] = s2;
            x = mulq15(s1, l1) + mulq15(s2, l2);
            if (nz)
                x += mulq15(rnd_bi(&v->r), nz);
            x = tanh_q15((int32_t)(((int64_t)x * drv) >> 13));   /* (the drive: tanh(x drive / 2)) */
            ob_svf(&v->f[c][0], &v->c[c], x, &lp, &bp, &hp);
            if (d->ftype == OBF_BP)
                z = bp;
            else
                z = lp + mulq15(hp - lp, mor);     /* the SEM: low-pass .. notch (the two summed) .. high-pass */
            if (d->ftype == OBF_LP24)
                z = svf_run(&v->f[c][1], &v->c2[c], z, 1);
            y += z;
        }
        if (nc > 1u)
            y = (y * 3) >> 2;                      /* (two cards: -2.5 dB, not -6: they add apart) */
        y = clampi(y, -98304, 98304);
        y = mulq15(y, a0 + ((da * (int32_t)(i + 1u)) >> 5));
        y = (int32_t)(((int64_t)y * d->out) >> 12);
        out[i] = y;
        pk = absi(y) > pk ? absi(y) : pk;
    }
    if (!v->ast && !v->hold && !v->amp0 && !v->amp1)
        v->live = 0;                               /* (its envelope over) */
    (void)pk;
    return 1;
}
static __typeof__(&ob_render) volatile ob_render_fp = ob_render;   /* (the flash's way into the RAM code) */

/* ----------------------------------------------------------- the design --- */
static int32_t ob_ms(rnd_t *r, int32_t lo, int32_t hi)   /* a time between lo and hi ms, drawn evenly on a log scale */
{
    int32_t l = log2_q16((uint32_t)lo), h = log2_q16((uint32_t)hi);   /* (log2, Q16; exp2_q16 gives Q16) */
    return (int32_t)(exp2_q16(rnd_in(r, l, h)) >> 16);
}
static void ob_design(ob_design_t *d, int32_t kind, uint32_t seed, uint32_t path, uint32_t gen)
{
    rnd_t rr, *r = &rr;
    path = path < PATH_N ? path : PATH_OPEN;
    rnd_seed(r, seed ^ 0x0BEADu);
    {   /* (the family drawn always, a family given or not: the seed and the family then make the sound again, however
         * the family was come to -- SHIFT + PRESETS, a preset, a session) */
        int32_t drawn = (int32_t)rnd_weighted(r, OBK_W[path], OBK_N);
        if (kind < 0 || kind >= OBK_N)
            kind = drawn;
    }
    memset(d, 0, sizeof *d);
    d->seed = seed;
    d->kind = (uint8_t)kind;
    d->path = (uint8_t)path;
    d->gen = (uint8_t)gen;
    /* the common ground: two saws, the second a few cents away, the SEM's low-pass, a gentle vintage */
    d->w1 = d->w2 = OB_SAW;
    d->lvl1 = d->lvl2 = 23170;
    d->det = ob_cents(rnd_in(r, 30, 90));
    d->cdet = ob_cents(rnd_in(r, 40, 120));
    d->pw1 = d->pw2 = Q15 / 2;
    d->drive = 4096 + rnd_in(r, 0, 4096);
    d->keytrk = rnd_in(r, 8192, 24576);
    d->velcut = rnd_in(r, 0, 16 * 12);
    d->vint = rnd_in(r, 8192, 24576);
    d->lfo_shape = (uint8_t)rnd_below(r, 5);
    d->lfo_hz = rnd_in(r, Q16 / 5, 6 * Q16);
    d->lfo_delay = rnd_in(r, 0, 1500);
    d->reso = rnd_in(r, 0, 9830);
    switch (kind) {
    case OBK_STRINGS:                              /* the OB string pad: two cards of saws, slow, open, a delayed vibrato */
        d->dual = 1;
        d->cut = 16 * rnd_in(r, 84, 100);
        d->fenv = 16 * rnd_in(r, 0, 12);
        d->fa = ob_ms(r, 200, 1500), d->fd = ob_ms(r, 800, 3000), d->fs = rnd_in(r, 22937, Q15), d->fr = ob_ms(r, 800, 3000);
        d->aa = ob_ms(r, 250, 1600), d->ad = ob_ms(r, 600, 2500), d->as = rnd_in(r, 26214, Q15), d->ar = ob_ms(r, 900, 3500);
        d->lfo_shape = 0, d->lfo_hz = rnd_in(r, Q16 * 4, Q16 * 6), d->lfo_pitch = rnd_in(r, 2, 5);
        d->lfo_delay = rnd_in(r, 400, 1500);
        d->morph = rnd_in(r, 0, 3277);
        break;
    case OBK_BRASS:                                /* the envelope opening the 24 dB low-pass, a blat */
        d->ftype = OBF_LP24;
        d->cut = 16 * rnd_in(r, 48, 66);
        d->fenv = 16 * rnd_in(r, 30, 48);
        d->reso = rnd_in(r, 0, 6554);
        d->fa = ob_ms(r, 30, 160), d->fd = ob_ms(r, 250, 900), d->fs = rnd_in(r, 9830, 19661), d->fr = ob_ms(r, 200, 700);
        d->aa = ob_ms(r, 15, 90), d->ad = ob_ms(r, 300, 900), d->as = rnd_in(r, 24576, 31130), d->ar = ob_ms(r, 200, 700);
        d->dual = (uint8_t)rnd_chance(r, 50);
        d->lfo_pitch = rnd_in(r, 0, 3);
        if (rnd_chance(r, 30))
            d->semi2 = 16 * 12;
        break;
    case OBK_PWM:                                  /* two pulses breathing (the widths moved slowly) */
        d->dual = 1;
        d->w1 = d->w2 = OB_PLS;
        d->pw1 = rnd_in(r, 13107, 16384), d->pw2 = rnd_in(r, 13107, 16384);
        d->pwm = rnd_in(r, 6554, 11469);
        d->lfo_shape = 0, d->lfo_hz = rnd_in(r, Q16 / 6, Q16), d->lfo_delay = 0;
        d->cut = 16 * rnd_in(r, 76, 96);
        d->fenv = 16 * rnd_in(r, 0, 18);
        d->fa = ob_ms(r, 300, 2000), d->fd = ob_ms(r, 1000, 4000), d->fs = rnd_in(r, 19661, Q15), d->fr = ob_ms(r, 1000, 4000);
        d->aa = ob_ms(r, 300, 2500), d->ad = ob_ms(r, 1000, 3000), d->as = rnd_in(r, 26214, Q15), d->ar = ob_ms(r, 1200, 4000);
        break;
    case OBK_BASS:                                 /* the second an octave down, the 24 dB low-pass, a pluck of the envelope */
        d->ftype = OBF_LP24;
        d->w1 = (uint8_t)(rnd_chance(r, 60) ? OB_SAW : OB_PLS);
        d->semi2 = -16 * 12;
        d->det = ob_cents(rnd_in(r, 10, 50));
        d->cut = 16 * rnd_in(r, 36, 58);
        d->fenv = 16 * rnd_in(r, 24, 48);
        d->reso = rnd_in(r, 3277, 16384);
        d->fa = ob_ms(r, 2, 15), d->fd = ob_ms(r, 120, 600), d->fs = rnd_in(r, 0, 6554), d->fr = ob_ms(r, 60, 300);
        d->aa = ob_ms(r, 2, 10), d->ad = ob_ms(r, 300, 1200), d->as = rnd_in(r, 13107, 26214), d->ar = ob_ms(r, 60, 300);
        d->lfo_delay = 0;
        break;
    case OBK_SYNC:                                 /* OSC 2 synced, its pitch swept by the envelope: the tearing lead */
        d->sync = 1;
        d->semi2 = 16 * rnd_in(r, 5, 19);
        d->env2 = 16 * rnd_in(r, 12, 30);
        d->lvl1 = rnd_in(r, 0, 13107), d->lvl2 = 26214;
        d->cut = 16 * rnd_in(r, 80, 104);
        d->fenv = 16 * rnd_in(r, 0, 18);
        d->fa = ob_ms(r, 5, 80), d->fd = ob_ms(r, 300, 1500), d->fs = rnd_in(r, 0, 16384), d->fr = ob_ms(r, 200, 800);
        d->aa = ob_ms(r, 5, 60), d->ad = ob_ms(r, 400, 1500), d->as = rnd_in(r, 19661, Q15), d->ar = ob_ms(r, 200, 900);
        d->lfo_pitch = rnd_in(r, 0, 4);
        break;
    case OBK_NOTCH:                                /* the SEM's morph near its notch, the LFO moving it: a phasing sweep */
        d->dual = (uint8_t)rnd_chance(r, 60);
        d->morph = rnd_in(r, 13107, 19661);
        d->lfo_morph = rnd_in(r, 6554, 13107);
        d->lfo_shape = (uint8_t)(rnd_chance(r, 70) ? 0 : 4), d->lfo_hz = rnd_in(r, Q16 / 10, Q16 * 2), d->lfo_delay = 0;
        d->reso = rnd_in(r, 6554, 19661);
        d->cut = 16 * rnd_in(r, 60, 90);
        d->lfo_cut = 16 * rnd_in(r, 0, 12);
        d->fa = ob_ms(r, 100, 1500), d->fd = ob_ms(r, 600, 3000), d->fs = rnd_in(r, 13107, Q15), d->fr = ob_ms(r, 600, 3000);
        d->aa = ob_ms(r, 50, 1500), d->ad = ob_ms(r, 600, 3000), d->as = rnd_in(r, 19661, Q15), d->ar = ob_ms(r, 600, 3000);
        break;
    case OBK_METAL:                                /* OSC 2 modulating OSC 1 at an odd interval, a band-pass: bells, clangs */
        d->ftype = OBF_BP;
        d->xmod = rnd_in(r, 6554, 22937);
        d->semi2 = 16 * rnd_in(r, 7, 31) + rnd_in(r, -8, 8);
        d->w1 = (uint8_t)(rnd_chance(r, 50) ? OB_SAW : OB_PLS);
        d->cut = 16 * rnd_in(r, 72, 100);
        d->reso = rnd_in(r, 9830, 22937);
        d->fenv = 16 * rnd_in(r, -12, 18);
        d->fa = ob_ms(r, 2, 20), d->fd = ob_ms(r, 400, 2500), d->fs = rnd_in(r, 0, 9830), d->fr = ob_ms(r, 400, 2000);
        d->aa = ob_ms(r, 2, 20), d->ad = ob_ms(r, 800, 4000), d->as = 0, d->ar = ob_ms(r, 600, 3000);
        break;
    case OBK_DRONE:                                /* two cards, a fifth or an octave, slow movement of cutoff and morph */
        d->dual = 1;
        d->semi2 = 16 * (rnd_chance(r, 50) ? 7 : 12);
        d->w2 = (uint8_t)(rnd_chance(r, 50) ? OB_SAW : OB_PLS);
        d->cut = 16 * rnd_in(r, 64, 86);
        d->lfo_cut = 16 * rnd_in(r, 4, 16);
        d->lfo_morph = rnd_in(r, 0, 6554);
        d->lfo_shape = 4, d->lfo_hz = rnd_in(r, Q16 / 20, Q16 / 3), d->lfo_delay = 0;
        d->pwm = rnd_in(r, 0, 6554);
        d->fa = ob_ms(r, 1000, 5000), d->fd = ob_ms(r, 2000, 6000), d->fs = rnd_in(r, 19661, Q15), d->fr = ob_ms(r, 2000, 6000);
        d->aa = ob_ms(r, 1500, 5000), d->ad = ob_ms(r, 2000, 5000), d->as = Q15, d->ar = ob_ms(r, 2500, 6000);
        d->vint = rnd_in(r, 19661, Q15);
        break;
    case OBK_KEYS:                                 /* a polysynth's keys: a struck filter, no sustain */
        d->w1 = (uint8_t)(rnd_chance(r, 50) ? OB_SAW : OB_SAW | OB_PLS);
        d->cut = 16 * rnd_in(r, 56, 80);
        d->fenv = 16 * rnd_in(r, 18, 40);
        d->fa = ob_ms(r, 1, 10), d->fd = ob_ms(r, 200, 1200), d->fs = rnd_in(r, 0, 6554), d->fr = ob_ms(r, 150, 600);
        d->aa = ob_ms(r, 1, 8), d->ad = ob_ms(r, 600, 2500), d->as = rnd_in(r, 0, 6554), d->ar = ob_ms(r, 200, 900);
        d->dual = (uint8_t)rnd_chance(r, 40);
        break;
    default:                                       /* SWELL: the filter opening slowly under a held chord */
        d->dual = 1;
        d->cut = 16 * rnd_in(r, 46, 64);
        d->fenv = 16 * rnd_in(r, 24, 42);
        d->fa = ob_ms(r, 1200, 4000), d->fd = ob_ms(r, 1500, 4000), d->fs = rnd_in(r, 19661, Q15), d->fr = ob_ms(r, 1200, 3000);
        d->aa = ob_ms(r, 400, 2000), d->ad = ob_ms(r, 1000, 3000), d->as = Q15, d->ar = ob_ms(r, 1200, 3500);
        d->reso = rnd_in(r, 3277, 11469);
        break;
    }
    if (path == PATH_AMBIENT) {                    /* softer, slower, darker */
        d->cut -= 16 * rnd_in(r, 0, 10);
        d->aa = d->aa < 300 ? d->aa + 300 : d->aa;
        d->ar = d->ar * 3 / 2;
        d->drive = 4096;
        d->lfo_hz = d->lfo_hz < Q16 ? d->lfo_hz : d->lfo_hz / 3;
    } else if (path == PATH_EXP) {                 /* harder: more drive, more resonance, cross-modulation now and then */
        d->drive += 8192;
        d->reso = clampi(d->reso + rnd_in(r, 0, 9830), 0, 27853);
        if (!d->xmod && rnd_chance(r, 35))
            d->xmod = rnd_in(r, 3277, 13107);
    }
    d->out = 4096;
}
static const char *ob_mac_name(const ob_design_t *d, uint32_t j)
{
    static const char *const N[NMAC] = {"CUTOFF", "MORPH", "RESONANCE", "DETUNE"};
    (void)d;
    return N[j & 3u];
}
/* the design's own envelope as A D S R (ms, sustain %): the amplifier's (the ENVELOPE page, as drawn) */
static void ob_env_adsr(const ob_design_t *d, int32_t *a)
{
    a[0] = d->aa;
    a[1] = d->ad;
    a[2] = d->as * 100 / Q15;
    a[3] = d->ar;
}
/* the design's level: C3 held 1 s, let go; the 75th percentile of 12 ms windows brought to 0.1 RMS, no more than
 * keeps its loudest moment under half of full scale (as BAYAREA's) */
static void ob_measure_on(ob_design_t *d, ob_voice_t *probe)
{
    static int32_t win[2067u / 16u + 1u];
    int32_t out[CTL];
    uint32_t b, k, nwin = 0;
    int64_t acc = 0;
    int32_t peak = 1;
    d->out = 4096;
    ob_init(probe, d->seed ^ 0x77u);
    ob_note_on(probe, d, 48, 127, 1);
    for (b = 0; b < 2067u; b++) {
        uint32_t i;
        if (b == 1378u)
            ob_note_off(probe);
        if (!ob_render_fp(probe, d, out, CTL))
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
