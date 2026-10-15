/* SPDX-License-Identifier: GPL-3.0-only
 * TONITOR: ANIMA, one voice a layer.
 *
 * A fixed-point port of the anima opcode of Vascular (Copyright (C) Nuno Zimas; Sources/CVascularOpcodes/src/
 * AnimaCore.h, AnimaDesign.h), contributed by its author to this firmware under the GPL: a
 * synthesizer played by a simulated hand. In Vascular six fingers are six voices; here a layer has one
 * voice, so the hand keeps one finger on the sound -- down while the key is held, dragging one figure
 * after another (orbits, figures of eight, scribbles, slow glides, now and then a flick), pressing in slow
 * swells, and now and then lifting to land somewhere else (the handover). A second, ghost finger comes and
 * goes on a clock that will not keep time: it plays no note, but it moves what the whole hand moves (how
 * many fingers are down, where the hand is, its hardest press, its quickest finger). The key's note is
 * the note: the motion moves timbre, level and space, never pitch.
 *
 * The voice: two wavetable oscillators, an FM pair, a stack of up to four copies, a grain cloud, a filtered
 * noise, a state-variable filter and a wavetable tremolo, through an envelope. The hand's chain: a
 * downsampler, a waveshaper (tanh, folding, third order), a four-stage phaser, a second filter and a comb
 * resonator tuned to the note; then Anima's slow leveller and a soft ceiling.
 *
 * Everything is drawn from a design: which objects, which waveforms, and for every parameter the
 * controller of the hand that moves it, over what range and along what slope. The kinds are Anima's
 * own (HAZE, VOYAGE, GLASS, BELLS, GOLDEN) and three made for this firmware (BREATH, GRIT, SWARM),
 * all bent towards the acousmatic: the sliders of Anima are drawn too, often far from their middle.
 *
 * Generation 1 (0.21) adds six kinds -- TIDE (waves of noise over a low drone), CHOIR (vowels, a formant the
 * hand moves), SHARDS (glass grains scattered and crushed), BOWL (a beating inharmonic hum in a ringing comb),
 * DUST (sparse crackle over a dark drone), MACHINE (a pulsing, folded mechanism) -- and draws with more care: the
 * filter follows the keyboard (low notes not muffled, high ones not shrill), the second oscillator's interval
 * suits the path (consonant on AMBIENT, any on EXPERIMENTAL), and AMBIENT always has a space for the sound to
 * ring in.
 *
 * A layer's path leans the draw (PATH_OPEN: every kind alike, the sliders as above):
 *   AMBIENT       the hazes, voyages and golden stacks, bells and breaths; the sliders towards a darker tone,
 *                 less texture and FM, more space, a slower hand; long releases, no crusher, a gentle drive
 *   EXPERIMENTAL  grit and swarms most, breath and glass, never a haze; the sliders near their tops (FM, grain, texture,
 *                 density, a quick hand), the resonance often screaming; then made wilder: the hand crushing
 *                 the rate down, folding drive, scattered grains, a filtered noise it drags about, a fast
 *                 tremolo, the comb ringing hard -- each on most designs, all of them moved by the hand
 *
 * No float: designs are integers in the units below; the controllers run once a 32-sample block. */

/* ------------------------------------------------------------- units --- */
#define LV(x) ((int32_t)((x) * 32768.0))     /* levels, depths, mixes, feedback: Q15 */
#define IX(x) ((int32_t)((x) * 4096.0))      /* FM index, filter Q, drive: Q12 */
#define H8(x) ((int32_t)((x) * 256.0))       /* slow rates (grains, tremolo): Hz Q8 */
#define H16(x) ((int32_t)((x) * 65536.0))    /* very slow rates (phaser, LFOs): Hz Q16 */
#define HZ(x) ((int32_t)(x))                 /* cutoffs, the downsampler: Hz */
#define MS4(x) ((int32_t)((x) * 16.0))       /* grain lengths: ms Q4 */
#define R16(x) ((int32_t)((x) * 65536.0))    /* any other real, Q16 */
/* (each used with literals only: the compiler folds them, nothing floats at run time) */

enum { AS_CONST, AS_X, AS_Y, AS_SIZE, AS_SPEED, AS_ANGLE, AS_DIST, AS_AGE, AS_GX, AS_GY, AS_COUNT, AS_LFO1,
       AS_LFO2, AS_N };

typedef struct {                 /* a parameter: the controller that moves it, over a range, along a slope */
    uint8_t src;
    int16_t slope;               /* Q8: -256 lower weighting .. 256 upper */
    int32_t lo, hi;
} an_map;

enum { AK_HAZE, AK_VOYAGE, AK_GLASS, AK_BELLS, AK_GOLDEN, AK_BREATH, AK_GRIT, AK_SWARM,
       AK_N0,                    /* (generation 0 draws these eight only) */
       AK_TIDE = AK_N0, AK_CHOIR, AK_SHARDS, AK_BOWL, AK_DUST, AK_MACHINE,
       AK_N1,                    /* (generations 1 and 2 draw these fourteen) */
       AK_PLUCK = AK_N1, AK_REED, AK_FOG, AK_ORGAN, AK_BITS, AK_DRIFT, AK_MURMUR, AK_HOWL, AK_N };
static const char *const AK_NAME[AK_N] = {"HAZE", "VOYAGE", "GLASS", "BELLS", "GOLDEN", "BREATH", "GRIT", "SWARM",
                                          "TIDE", "CHOIR", "SHARDS", "BOWL", "DUST", "MACHINE", "PLUCK", "REED", "FOG",
                                          "ORGAN", "BITS", "DRIFT", "MURMUR", "HOWL"};

enum { MO_TAPS, MO_DRAGS, MO_ORBITS, MO_SWARM, MO_SWEEPS, MO_HOLDS };

typedef struct {
    uint32_t seed;               /* what made it: an_design(-1, seed, path, gen) + an_measure gives it again */
    uint8_t kind;
    uint8_t path;                /* PATH_*: what it was drawn on */
    uint8_t oscA, oscB, fm, multi, gran, ampMod, down, shaper, phaser, resonator, phByHand, golden;
    uint8_t noise;               /* 0 none, 1 white, 2 low-pass, 3 band-pass, 4 high-pass */
    uint8_t filter;              /* 0 off, 1 low-pass, 2 band-pass, 3 high-pass */
    uint8_t groupFilter, shaperCurve;
    uint8_t tableA, tableB, tableFM, tableMulti, tableGran, tableAM;
    uint8_t multiCount, motion, lfo1Shape, lfo2Shape;
    int8_t intervalB;            /* semitones above the note */
    int16_t detuneB;             /* cents x 16 */
    int16_t multiStep;           /* semitones x 16 between the stacked copies */
    int32_t fmRatio;             /* Q16 */
    int32_t granSpread;          /* Q15: the share of grains scattered off the note */
    int32_t att, hold, dec, rel; /* ms */
    int32_t sus;                 /* Q15 */
    an_map levelA, levelB, levelFM, levelMulti, levelGran, levelNoise;   /* Q15 */
    an_map fmIndex;              /* Q12 */
    an_map granRate;             /* Hz Q8 */
    an_map granLen;              /* ms Q4 */
    an_map noiseCut, cutoff;     /* Hz */
    an_map q;                    /* Q12 */
    an_map amRate;               /* Hz Q8 */
    an_map amDepth, amp;         /* Q15 */
    an_map downRate;             /* Hz */
    an_map shaperDrive;          /* Q12 */
    an_map phaserRate;           /* Hz Q16 */
    an_map phaserDepth;          /* Q15 */
    an_map groupCut;             /* Hz */
    an_map groupQ;               /* Q12 */
    an_map resFeedback, resMix, phaserPlace;   /* Q15 */
    int32_t lfo1, lfo2;          /* Hz Q16 */
    int32_t travel;              /* surface units a second, Q16 */
    int32_t out;                 /* Q12 */
    int32_t motionScale, density;   /* Q16: the hand's pace and how busy it is */
    /* (generation 1 on: fields after here, so generation 0's stay where they were) */
    uint8_t gen;                 /* the generators' generation that drew it */
    uint8_t kt;                  /* how far the filter follows the keyboard, Q8 (256: an octave an octave) */
} an_design_t;

/* ----------------------------------------------------------- maps --- */
static int32_t an_curve(int32_t v, int32_t slope)   /* v Q15, slope Q8 -> Q15 */
{
    int32_t si = (slope + 256 + 16) >> 5, i, f;          /* nearest of the 17 curves (eighths) */
    const int16_t *c;
    if (si == 8)
        return v;
    si = clampi(si, 0, 16);
    c = &AN_CURVE[si * 65];
    i = v >> 9;
    if (i >= 64)
        return c[64];
    f = v & 511;
    return c[i] + (((c[i + 1] - c[i]) * f) >> 9);
}
static int32_t an_at(const an_map *m, const int32_t *c)
{
    int32_t v = clampi(c[m->src], 0, Q15);
    if (m->src == AS_CONST)
        return m->hi;
    if (m->slope)
        v = an_curve(v, m->slope);
    return m->lo + (int32_t)(((int64_t)(m->hi - m->lo) * v) >> 15);
}

/* ------------------------------------------------------ the voice --- */
#define AN_RES_N 2048u
#define AN_INC_200 19478310u     /* phase increments of 200, 600 and 2000 Hz: the wavetable levels */
#define AN_INC_600 58434930u
#define AN_INC_2000 194783100u

/* a wavetable's point at a phase, linear between two (the table chosen once a block: an_tab) */
AINL int32_t an_rd(const int16_t *d, uint32_t ph)
{
    uint32_t i = ph >> 22;
    int32_t f = (int32_t)((ph >> 7) & 0x7FFFu);
    return d[i] + (((d[i + 1u] - d[i]) * f) >> 15);
}

/* the wavetables in RAM: the flash's 196 KB of them are read through a cache that misses often (a read costs 6 to
 * 33 ticks of the chip, against 1.6 from RAM), so the tables the voices are playing are copied into RAM, as many as
 * fit; a table no voice has asked for in ~50 ms may make room for another. At most one copied a block (~0.2 % of
 * the chip's time); the others, meanwhile, from the flash. The main loop (measuring a sound) reads the flash */
/* AN_HALF 1: each voice computed at half the rate (22.05 kHz: nothing above 11 kHz, half the cost), its 16 samples
 * a block drawn out to 32 by straight lines between them. Its pitches, rates, delays and filters are scaled to sound
 * where they did; the wavetables, for the doubled increments, take their darker levels (clean below 11 kHz) */
#ifndef AN_HALF
#define AN_HALF 1
#endif
#define AN_SVF(c, hz, q) svf_coef(c, AN_HALF ? clampi((hz) * 2, 16, TANG_FMAX) : (hz), q)
#ifndef AN_CACHE_N
#define AN_CACHE_N 15u              /* (~30 KB of the pool: as many as leave it its 8 KB headroom) */
#endif
static int16_t an_cache[AN_CACHE_N][AN_TSIZE + 1u] POOL;
static uint16_t an_cache_tag[AN_CACHE_N];       /* the table x AN_LEVELS + its level, + 1 (0: empty) */
static uint32_t an_cache_used[AN_CACHE_N];      /* the block it was last asked for */
static uint32_t an_cache_now;                   /* the ISR's block (+ 1) while it renders; 0 elsewhere */
static uint8_t an_cache_filled;                 /* a table copied in this block */
static const int16_t *an_tab(uint32_t k, uint32_t inc)
{
    uint32_t lv = inc < AN_INC_200 ? 0u : inc < AN_INC_600 ? 1u : inc < AN_INC_2000 ? 2u : 3u, tag = k * AN_LEVELS + lv + 1u;
    uint32_t i, room = AN_CACHE_N;
    const int16_t *flash = &AN_WT[(tag - 1u) * (AN_TSIZE + 1u)];
    if (!an_cache_now)
        return flash;
    for (i = 0; i < AN_CACHE_N; i++)
        if (an_cache_tag[i] == tag) {
            an_cache_used[i] = an_cache_now;
            return an_cache[i];
        }
    if (an_cache_filled)
        return flash;
    for (i = 0; i < AN_CACHE_N; i++) {
        if (!an_cache_tag[i]) {
            room = i;
            break;
        }
        if (an_cache_now - an_cache_used[i] > 64u && (room == AN_CACHE_N || an_cache_used[i] < an_cache_used[room]))
            room = i;
    }
    if (room == AN_CACHE_N)
        return flash;                           /* (all of them in use: this one from the flash) */
    memcpy(an_cache[room], flash, sizeof an_cache[room]);
    an_cache_tag[room] = (uint16_t)tag;
    an_cache_used[room] = an_cache_now;
    an_cache_filled = 1;
    return an_cache[room];
}

typedef struct {
    int32_t x, y;                /* Q15 surface, 0 .. 1 */
    int32_t px, py, size, speed; /* Q15 */
    int32_t pressure, nseed;     /* Q15; noise seed Q16 radians */
    int32_t t, dur;              /* Q16 seconds (since down; how long a ghost stays) */
    int32_t ax, ay, tx, ty;      /* Q15: where the figure began, where it carries the anchor */
    int32_t figureAt, figureFor, invFor;   /* Q16 s; 1 / figureFor Q16 */
    int32_t r;                   /* Q15 radius */
    int32_t w, w2, phase, phase2;   /* Q16 rad/s, Q16 rad */
    int32_t dx1, dy1;            /* Q15 unit direction of a glide */
    int32_t glide;               /* Q15: a handover's slide from the old place (1 -> 0) */
    int32_t gx, gy;              /* where it slid from */
    uint8_t down, path, flick;
} an_finger;

typedef struct {
    uint32_t ph, pos, posinc, inc;
    const int16_t *tab;          /* its wavetable (an_tab) */
    uint8_t on;
} an_grain;

typedef struct {
    /* the note */
    uint32_t inc, incB, incM, incStack[4];
    int32_t stackGain[4];
    int32_t resDelay;            /* Q16 samples */
    /* oscillators */
    uint32_t pA, pB, pC, pM, pAM, pStack[4], grainClock;
    an_grain gr[3];
    svf_t filt, nfilt, gfilt;
    svf_c filtc, nfiltc, gfiltc;
    /* envelope (Q24) */
    uint8_t est;
    int32_t ev, et;
    int32_t attInc, decK, relK;  /* per block: Q24 step, Q16 factors */
    /* this block's controllers */
    int32_t lA, lB, lFM, lMulti, lGran, lNoise, index, gRate8, gLen4, amRate8, amDepth, amp, ampPrev;
    uint32_t incFMlevel;
    /* the hand */
    an_finger f, ghost;
    int32_t ghostClock, handover; /* Q16 s */
    int32_t countSmooth;          /* Q15 */
    uint32_t lfoP1, lfoP2;
    int32_t held1, held2, smooth1, smooth2;
    uint32_t last1, last2;
    int32_t dt;                   /* Q16 s of a block, x the hand's pace */
    /* the hand's chain */
    uint32_t downAcc, downInc;
    int32_t downHeld;
    int32_t drive, driveNorm;     /* Q12, Q15 */
    uint32_t phPhase, phInc;
    int32_t phA, ap[4], phFb, phPlaceSmooth, phDepth, phPlace;
    int32_t resFb, resMix, resLp;
    uint32_t resPos;
    int16_t res[AN_RES_N];       /* the comb's line, Q13 (Q15 >> 2: room for a resonance four times full scale) */
    /* the leveller */
    int32_t power;                /* Q24 */
    int32_t blkms;                /* the last block's mean square before the leveller, Q24 */
    int32_t level;                /* Q28 */
    rnd_t rnd, motion;
    uint8_t live;
    int32_t ktmul;               /* the filter's keyboard tracking for this note, Q16 */
    int8_t mac[NMAC];            /* EDIT's macros (layers.c sets them each block; 0: the design as drawn) */
    uint8_t res_on, shp_on;      /* the comb and the shaper, the design's or brought in by a macro */
    const int16_t *tA, *tB, *tFM, *tAM, *tM[4];   /* this block's wavetables (an_tab) */
    int32_t hprev;               /* (AN_HALF: the last half-rate sample, the line to the next starts there) */
    uint8_t am_on;               /* the tremolo: the design's, or brought in by MOTION */
} an_voice_t;

/* ------------------------------------------------------ the envelope --- */
enum { EN_IDLE, EN_ATT, EN_HOLD, EN_DEC, EN_SUS, EN_REL };
/* e^(-a), a in Q16, Q16 */
static int32_t an_expneg(int32_t a)
{
    return (int32_t)exp2_q16(-(int32_t)(((int64_t)a * 94548) >> 16));   /* log2(e) = 1.4427, Q16 */
}
static void an_env_times(an_voice_t *v, const an_design_t *d)
{
    /* a block is 32 / 44100 s = 0.7256 ms; AnimaCore.h: attack linear over att ms, decay and release
     * e^(-4.6 dt / time) a step */
    v->attInc = (int32_t)(12173000u / (uint32_t)clampi(d->att, 1, 600000));
    v->decK = Q16 - an_expneg((int32_t)(218760u / (uint32_t)clampi(d->dec, 1, 600000)));
    v->relK = an_expneg((int32_t)(218760u / (uint32_t)clampi(d->rel, 1, 600000)));
}
static int32_t an_env_tick(an_voice_t *v, const an_design_t *d)   /* once a block, Q24 */
{
    int32_t sus = d->sus << 9;
    switch (v->est) {
    case EN_ATT:
        v->ev += v->attInc;
        if (v->ev >= (1 << 24)) {
            v->ev = 1 << 24;
            v->est = EN_HOLD;
            v->et = 0;
        }
        break;
    case EN_HOLD:
        v->et += 726;                          /* microseconds of a block */
        if (v->et >= d->hold * 1000)
            v->est = EN_DEC;
        break;
    case EN_DEC:
        v->ev += mulq16(sus - v->ev, v->decK);
        if (absi(v->ev - sus) < (1 << 14))
            v->est = EN_SUS;
        break;
    case EN_SUS:
        v->ev = sus;
        break;
    case EN_REL:
        v->ev = mulq16(v->ev, v->relK);
        if (v->ev < (1 << 10)) {
            v->ev = 0;
            v->est = EN_IDLE;
        }
        break;
    default:
        v->ev = 0;
        break;
    }
    return v->ev;
}

/* ---------------------------------------------------------- geometry --- */
/* sin / cos of an angle in radians, Q16 -> Q15 */
static inline int32_t an_sinr(int32_t a) { return sin_ph((uint32_t)((int64_t)a * 10430)); }
static inline int32_t an_cosr(int32_t a) { return sin_ph((uint32_t)((int64_t)a * 10430) + 0x40000000u); }
static inline int32_t an_smooth(int32_t u)    /* smoothstep, Q15 */
{
    u = clampi(u, 0, Q15);
    return mulq15(mulq15(u, u), 3 * Q15 - 2 * u);
}
/* AnimaCore.h noise1: three sines, t and seed in Q16 radians, Q15 out */
static int32_t an_noise1(int32_t t, int32_t seed)
{
    return mulq15(an_sinr(mulq16(t, R16(1.3)) + seed), LV(0.6)) +
           mulq15(an_sinr(mulq16(t, R16(2.9)) + mulq16(seed, R16(1.7))), LV(0.3)) +
           mulq15(an_sinr(mulq16(t, R16(6.1)) + mulq16(seed, R16(0.3))), LV(0.1));
}
/* the angle of (dx, dy) as a turn 0 .. 1 (Q15), as (atan2 + pi) / 2 pi */
static int32_t an_angle(int32_t dx, int32_t dy)
{
    int32_t ax = absi(dx), ay = absi(dy), z, a;
    if (!ax && !ay)
        return Q15 / 2;
    /* atan of the smaller over the larger, 0 .. pi/4, as a share of a turn: z (0.125 - (z - 1)(0.0389 + 0.0105 z)) */
    z = ax > ay ? (int32_t)(((uint32_t)ay << 15) / (uint32_t)ax) : (int32_t)(((uint32_t)ax << 15) / (uint32_t)ay);
    a = mulq15(z, LV(0.125) - mulq15(z - Q15, LV(0.0389) + mulq15(z, LV(0.0105))));
    if (ay > ax)
        a = Q15 / 4 - a;
    if (dx < 0)
        a = Q15 / 2 - a;
    if (dy < 0)
        a = -a;
    return clampi(a + Q15 / 2, 0, Q15);   /* -1/2 .. 1/2 turn -> 0 .. 1 */
}

/* the next figure a finger drags, begun from where it is, so it never jumps */
static void an_figure(an_voice_t *v, const an_design_t *d, an_finger *f)
{
    rnd_t *r = &v->motion;
    int32_t speed, a;
    f->ax = f->x;
    f->ay = f->y;
    f->figureAt = f->t;
    f->path = (uint8_t)rnd_below(r, 4);    /* orbit, figure, scribble, glide */
    /* most figures sweep a good part of the surface over several seconds; one in five is a flick */
    f->flick = (uint8_t)rnd_chance(r, 20);
    f->figureFor = f->flick ? rnd_in(r, R16(1.5), R16(4.0)) : rnd_in(r, R16(6.0), R16(18.0));
    f->invFor = (int32_t)(0xFFFFFFFFu / (uint32_t)f->figureFor);
    f->r = f->flick ? rnd_in(r, LV(0.08), LV(0.18)) : rnd_in(r, LV(0.16), LV(0.4));
    speed = mulq16(d->travel > R16(0.12) ? d->travel : R16(0.12),
                   f->flick ? rnd_in(r, R16(5.0), R16(9.0)) : rnd_in(r, R16(1.6), R16(3.2)));
    f->w = (int32_t)(((int64_t)speed * (0xFFFFFFFFu / (uint32_t)f->r)) >> 17);   /* speed / r: Q16 rad/s */
    if (rnd_chance(r, 50))
        f->w = -f->w;
    f->w2 = rnd_chance(r, 50) ? f->w * 2 : f->w + (f->w >> 1);
    f->phase = rnd_in(r, 0, R16(6.2831853));
    f->phase2 = rnd_in(r, 0, R16(6.2831853));
    a = rnd_in(r, 0, R16(6.2831853));
    f->dx1 = an_cosr(a);
    f->dy1 = an_sinr(a);
    /* and the figure carries its anchor somewhere else on the surface as it goes */
    f->tx = rnd_in(r, LV(0.15), LV(0.85)) - f->ax;
    f->ty = rnd_in(r, LV(0.15), LV(0.85)) - f->ay;
    if (f->flick)
        f->tx = f->ty = 0;
}

static void an_land(an_voice_t *v, const an_design_t *d, an_finger *f, int ghost)
{
    rnd_t *r = &v->motion;
    an_finger old = *f;
    f->down = 1;
    f->t = 0;
    f->dur = ghost ? rnd_in(r, R16(3.0), R16(15.0)) : 0x7FFFFFFF;
    f->x = f->px = rnd_in(r, LV(0.1), LV(0.9));
    f->y = f->py = rnd_in(r, LV(0.1), LV(0.9));
    f->pressure = rnd_in(r, LV(0.55), LV(0.9));
    f->nseed = rnd_in(r, 0, R16(100.0));
    f->speed = 0;
    f->glide = 0;
    if (!ghost && old.down) {               /* a handover: the hand slides over from where it was */
        f->glide = Q15;
        f->gx = old.x;
        f->gy = old.y;
        f->size = old.size;
    } else
        f->size = 0;
    an_figure(v, d, f);
}

/* AnimaCore.h moveHeld, one block */
static void an_move(an_voice_t *v, const an_design_t *d, an_finger *f, int ghost)
{
    int32_t u, ox = 0, oy = 0, ease, carry, fade, x, y, dx, dy, sp, k;
    f->t += v->dt;
    u = f->t - f->figureAt;
    switch (f->path) {
    case 0:                                 /* an orbit */
        ox = mulq15(f->r, an_cosr(f->phase + mulq16(f->w, u)) - an_cosr(f->phase));
        oy = mulq15(f->r, an_sinr(f->phase + mulq16(f->w, u)) - an_sinr(f->phase));
        break;
    case 1:                                 /* a figure of eight */
        ox = mulq15(f->r, an_sinr(mulq16(f->w, u) + f->phase) - an_sinr(f->phase));
        oy = mulq15(mulq15(f->r, LV(0.7)), an_sinr(mulq16(f->w2, u) + f->phase2) - an_sinr(f->phase2));
        break;
    case 2:                                 /* a scribble */
        k = (d->travel > R16(0.04) ? d->travel : R16(0.04)) * 5;
        ox = mulq15(an_noise1(mulq16(u, k), f->nseed) - an_noise1(0, f->nseed), LV(0.25));
        oy = mulq15(an_noise1(mulq16(u, k), f->nseed + R16(40.0)) - an_noise1(0, f->nseed + R16(40.0)), LV(0.25));
        break;
    default: {                              /* a slow slide back and forth */
        int32_t a = mulq15(f->r + (f->r >> 1), an_sinr(mulq16(f->w, u) >> 1));   /* 1.5 r sin(w u / 2) */
        ox = mulq15(f->dx1, a);
        oy = mulq15(f->dy1, a);
        break;
    }
    }
    ease = an_smooth(clampi(u >> 1, 0, Q15));             /* eased in over a second */
    carry = an_smooth(clampi(mulq16(u, f->invFor) >> 1, 0, Q15));
    x = clampi(f->ax + mulq15(f->tx, carry) + mulq15(ox, ease), LV(0.03), LV(0.97));
    y = clampi(f->ay + mulq15(f->ty, carry) + mulq15(oy, ease), LV(0.03), LV(0.97));
    if (f->glide > 0) {                     /* a handover's slide, ~90 ms */
        x = f->gx + mulq15(x - f->gx, Q15 - f->glide);
        y = f->gy + mulq15(y - f->gy, Q15 - f->glide);
        f->glide -= 260;
    }
    f->x = x;
    f->y = y;
    fade = ghost ? clampi((f->dur - f->t) >> 1, 0, Q15) : Q15;
    /* pressing harder and easing off in slow swells */
    f->size = mulq15(mulq15(mulq15(f->pressure, LV(0.6) + mulq15(LV(0.4), an_sinr(mulq16(f->t, R16(0.45)) + f->nseed))),
                            clampi(f->t, 0, R16(0.5))), fade);   /* (t / 0.5 s, Q15) */
    dx = f->x - f->px;
    dy = f->y - f->py;
    /* distance a block / 0.3 surface a second: |d| / (0.3 * 0.7256 ms) */
    sp = (int32_t)isqrt32((uint32_t)(dx * dx + dy * dy));  /* Q15 */
    sp = clampi(sp * 4594, 0, Q15);                        /* 1 / (0.3 * 0.0007256) = 4594 */
    f->speed += (sp - f->speed) / 10;
    f->px = f->x;
    f->py = f->y;
    if (u >= f->figureFor)
        an_figure(v, d, f);
    if (ghost && f->t >= f->dur)
        f->down = 0;
}

static int32_t an_lfo(an_voice_t *v, int shape, uint32_t p, int which)
{
    int32_t *held = which == 1 ? &v->held1 : &v->held2, *sm = which == 1 ? &v->smooth1 : &v->smooth2;
    uint32_t *last = which == 1 ? &v->last1 : &v->last2;
    if (shape == 0)
        return Q15 / 2 + (sin_ph(p) >> 1);
    if (shape == 1)
        return p < 0x80000000u ? (int32_t)(p >> 16) : (int32_t)((0xFFFFFFFFu - p) >> 16);
    if (p < *last)
        *held = rnd_q15(&v->motion);
    *last = p;
    *sm += (*held - *sm) / 23;
    return *sm;
}

/* ---------------------------------------------------- note on / off --- */
static void an_note_on(an_voice_t *v, const an_design_t *d, uint32_t inc, int fresh)
{
    uint32_t i, n;
    if (fresh) {
        v->pA = v->pB = v->pC = v->pM = v->pAM = 0;
        for (i = 0; i < 4u; i++)
            v->pStack[i] = 0;
        for (i = 0; i < 3u; i++)
            v->gr[i].on = 0;
        v->filt.ic1 = v->filt.ic2 = v->nfilt.ic1 = v->nfilt.ic2 = v->gfilt.ic1 = v->gfilt.ic2 = 0;
        /* (and the chain's memories: a voice taken from another note is faded out, but its comb, phaser and
         * sample hold still hold that note -- heard again, at once and at full level, as the new one begins) */
        v->downAcc = 0;
        v->downHeld = 0;
        v->phFb = 0;
        for (i = 0; i < 4u; i++)
            v->ap[i] = 0;
        v->resLp = 0;
        v->hprev = 0;
        memset(v->res, 0, sizeof v->res);
        v->ev = 0;
        v->grainClock = 0x80000000u;
    }
    v->inc = inc;
    v->ktmul = Q16;
    if (d->kt) {                                     /* the filter follows the keyboard, from middle C */
        int32_t oct = log2_q16(inc) - log2_q16(pitch_inc(60 * 16));
        v->ktmul = (int32_t)exp2_q16(clampi((int32_t)(((int64_t)oct * d->kt) >> 8), -3 * Q16, 3 * Q16));
    }
    v->incB = (uint32_t)(((uint64_t)inc * exp2_q16((d->intervalB * 16 + d->detuneB / 100) * 341)) >> 16);
    v->incM = (uint32_t)(((uint64_t)inc * (uint32_t)(d->golden ? R16(1.6180340) : d->fmRatio)) >> 16);
    n = clampi(d->multiCount, 1, 4);
    for (i = 0; i < n; i++) {
        static const int32_t GOLDEN[4] = {R16(1.0), R16(1.2720196), R16(1.6180340), R16(2.0581710)};
        uint32_t k = d->golden ? (uint32_t)GOLDEN[i] : exp2_q16(d->multiStep * (int32_t)i * 341);   /* x 4096 / 12 / 16 */
        v->incStack[i] = (uint32_t)(((uint64_t)inc * k) >> 16);
    }
    {   /* 1 / (1 + 0.35 i) / sqrt(n) */
        static const int32_t ISQ[5] = {0, LV(1.0), LV(0.7071), LV(0.5774), LV(0.5)};
        static const int32_t FALL[4] = {LV(1.0), LV(0.7407), LV(0.5882), LV(0.4878)};
        for (i = 0; i < 4u; i++)
            v->stackGain[i] = mulq15(FALL[i], ISQ[n]);
    }
    v->resDelay = (int32_t)udiv64_32(1u << 16, 0, inc);   /* 2^48 / inc = FS / hz samples, Q16 */
    v->resDelay = clampi(v->resDelay, 2 << 16, (int32_t)(AN_RES_N - 4u) << 16);
    an_env_times(v, d);
    v->est = EN_ATT;
    v->et = 0;
    an_land(v, d, &v->f, 0);
    v->live = 1;
}
static void an_note_off(an_voice_t *v)
{
    if (v->est != EN_IDLE)
        v->est = EN_REL;
    v->f.down = 0;
}

static void an_init(an_voice_t *v, uint32_t seed)
{
    memset(v, 0, sizeof *v);
    rnd_seed(&v->rnd, seed * 31u + 7u);
    rnd_seed(&v->motion, seed);
    v->power = 167772;                   /* 0.01, Q24 */
    v->level = 1 << 28;
    v->ghostClock = R16(6.0);
    v->handover = R16(20.0);
    v->held1 = v->held2 = v->smooth1 = v->smooth2 = Q15 / 2;
    v->phPlaceSmooth = Q15 / 2;
}

/* ------------------------------------------------- the controllers --- */
/* EDIT's macros on ANIMA: TONE (every filter up or down), then what the kind is about -- GRAINS (the cloud's
 * level and rate), AIR (the noise), FM (the index), VOWEL (the formant) or DRIVE (the shaper, brought in if the
 * design has none) -- SPACE (the comb's mix and ringing, brought in if the design has none) and MOTION (the
 * hand's pace, its LFOs and the tremolo's depth) */
enum { AR_GRAINS, AR_AIR, AR_FM, AR_VOWEL, AR_DRIVE };
static uint32_t an_mac_role(const an_design_t *d)
{
    static const uint8_t PREF[AK_N][3] = {
        {AR_AIR, AR_GRAINS, AR_DRIVE}, {AR_GRAINS, AR_AIR, AR_DRIVE}, {AR_FM, AR_GRAINS, AR_DRIVE},
        {AR_FM, AR_DRIVE, AR_DRIVE}, {AR_FM, AR_GRAINS, AR_DRIVE}, {AR_AIR, AR_VOWEL, AR_DRIVE},
        {AR_DRIVE, AR_DRIVE, AR_DRIVE}, {AR_GRAINS, AR_DRIVE, AR_DRIVE}, {AR_AIR, AR_DRIVE, AR_DRIVE},
        {AR_VOWEL, AR_DRIVE, AR_DRIVE}, {AR_GRAINS, AR_DRIVE, AR_DRIVE}, {AR_FM, AR_DRIVE, AR_DRIVE},
        {AR_GRAINS, AR_AIR, AR_DRIVE}, {AR_DRIVE, AR_DRIVE, AR_DRIVE},
        {AR_AIR, AR_DRIVE, AR_DRIVE}, {AR_VOWEL, AR_AIR, AR_DRIVE}, {AR_GRAINS, AR_AIR, AR_DRIVE},
        {AR_DRIVE, AR_DRIVE, AR_DRIVE}, {AR_DRIVE, AR_DRIVE, AR_DRIVE}, {AR_FM, AR_GRAINS, AR_DRIVE},
        {AR_GRAINS, AR_AIR, AR_DRIVE}, {AR_AIR, AR_DRIVE, AR_DRIVE}};
    uint32_t i;
    for (i = 0; i < 3u; i++) {
        uint32_t r = PREF[d->kind % AK_N][i];
        if ((r == AR_GRAINS && d->gran) || (r == AR_AIR && d->noise) || (r == AR_FM && d->fm) ||
            (r == AR_VOWEL && d->groupFilter) || r == AR_DRIVE)
            return r;
    }
    return AR_DRIVE;
}
static const char *an_mac_name(const an_design_t *d, uint32_t j)
{
    static const char *const ROLE[5] = {"GRAINS", "AIR", "FM", "VOWEL", "DRIVE"};
    return j == 0u ? "TONE" : j == 1u ? ROLE[an_mac_role(d)] : j == 2u ? "SPACE" : "MOTION";
}
/* 2^(m / MAC_MAX x oct), Q16 (oct in Q16 / MAC_MAX steps: the caller's) */
static int32_t an_mac_exp(int32_t m, int32_t oct_q16) { return (int32_t)exp2_q16(m * (oct_q16 / MAC_MAX)); }

static void an_control(an_voice_t *v, const an_design_t *d)
{
    int32_t c[AS_N], gc[AS_N], l1, l2, count, gx, gy, hardest, fastest, dx, dy;
    an_finger *f = &v->f, *g = &v->ghost;
    int8_t mac[NMAC];
    uint32_t role;
    {   /* the knobs' turn, felt early: the square root of it (a quarter of the way: half the change) */
        uint32_t j;
        for (j = 0; j < NMAC; j++) {
            int32_t m = v->mac[j], a = m < 0 ? -m : m, r = (int32_t)isqrt32((uint32_t)(a * MAC_MAX));
            mac[j] = (int8_t)(m < 0 ? -r : r);
        }
    }
    role = mac[1] ? an_mac_role(d) : AR_DRIVE;
    int32_t pace = mac[3] ? mulq16(d->motionScale, an_mac_exp(mac[3], R16(2.0))) : d->motionScale;
    int32_t tone = mac[0] ? an_mac_exp(mac[0], R16(4.0)) : Q16;
    v->dt = mulq16(R16(0.00072562), pace);
    if (f->down) {
        /* now and then the hand moves on: the finger lands somewhere else */
        v->handover -= v->dt;
        if (v->handover <= 0) {
            an_land(v, d, f, 0);
            /* an exponential wait around 18 s: x -ln(u) = (15 - log2(u Q15)) ln 2 */
            int32_t wait = (int32_t)(((int64_t)(R16(15.0) - log2_q16((uint32_t)rnd_in(&v->motion, 64, Q15))) * 45426) >> 16);
            v->handover = mulq16(R16(18.0), mulq16(rnd_in(&v->motion, R16(0.4), R16(2.0)), wait));
        }
        an_move(v, d, f, 0);
    }
    /* a ghost finger, down a while and lifted again, on a clock that will not keep time */
    v->ghostClock -= v->dt;
    if (v->ghostClock <= 0) {
        int32_t mean, dens = d->density > R16(0.2) ? d->density : R16(0.2);
        if (!g->down && f->down && d->density > R16(0.45))
            an_land(v, d, g, 1);
        mean = (int32_t)((uint32_t)R16(10.0) / (uint32_t)((mulq16(dens, dens) >> 8) + 1) << 8);   /* 10 / dens^2 s */
        mean = mulq16(mean, rnd_in(&v->motion, R16(0.35), R16(2.5)));
        /* an exponential wait: mean x -ln(u) = mean x (15 - log2(u Q15)) x ln 2 */
        v->ghostClock = mulq16(mean, (int32_t)(((int64_t)(R16(15.0) - log2_q16((uint32_t)rnd_in(&v->motion, 4, Q15))) * 45426) >> 16));
    }
    if (g->down)
        an_move(v, d, g, 1);
    /* a block's phase step: Hz x 32 / FS x 2^32 = Hz Q16 x 3116605 / 2^16 */
    v->lfoP1 += (uint32_t)(((uint64_t)(uint32_t)mulq16(d->lfo1, pace) * 3116605u) >> 16);
    v->lfoP2 += (uint32_t)(((uint64_t)(uint32_t)mulq16(d->lfo2, pace) * 3116605u) >> 16);
    l1 = an_lfo(v, d->lfo1Shape, v->lfoP1, 1);
    l2 = an_lfo(v, d->lfo2Shape, v->lfoP2, 2);
    count = (f->down ? 1 : 0) + (g->down ? 1 : 0);
    gx = f->x;
    gy = f->y;
    if (f->down && g->down) {
        gx = (f->x + g->x) >> 1;
        gy = (f->y + g->y) >> 1;
    } else if (g->down) {
        gx = g->x;
        gy = g->y;
    }
    /* how many fingers are down: one is none of it; the ghost brings it up to a third of it */
    v->countSmooth += ((count > 1 ? Q15 / 3 : 0) - v->countSmooth) / 16;
    hardest = f->size > g->size ? f->size : g->size;
    fastest = f->speed > g->speed ? f->speed : g->speed;
    if (!g->down) {
        hardest = f->size;
        fastest = f->speed;
    }
    dx = f->x - Q15 / 2;
    dy = f->y - Q15 / 2;
    c[AS_CONST] = Q15;
    c[AS_X] = f->x;
    c[AS_Y] = Q15 - f->y;
    c[AS_SIZE] = f->size;
    c[AS_SPEED] = f->speed;
    c[AS_ANGLE] = an_angle(dx, dy);
    c[AS_DIST] = clampi(((int32_t)isqrt32((uint32_t)(dx * dx + dy * dy)) * 46341) >> 15, 0, Q15);   /* / 0.7071 */
    c[AS_AGE] = Q15 / 2 - (an_cosr(mulq16(f->t, R16(0.27318))) >> 1);   /* 2 pi / 23 s */
    c[AS_GX] = gx;
    c[AS_GY] = Q15 - gy;
    c[AS_COUNT] = v->countSmooth;
    c[AS_LFO1] = l1;
    c[AS_LFO2] = l2;
    v->lA = an_at(&d->levelA, c);
    v->lB = an_at(&d->levelB, c);
    v->lFM = an_at(&d->levelFM, c);
    v->lMulti = an_at(&d->levelMulti, c);
    v->lGran = an_at(&d->levelGran, c);
    v->lNoise = an_at(&d->levelNoise, c);
    v->index = clampi(an_at(&d->fmIndex, c), 0, IX(12.0));
    v->gRate8 = clampi(an_at(&d->granRate, c), H8(0.5), H8(200.0));
    v->gLen4 = clampi(an_at(&d->granLen, c), MS4(4.0), MS4(400.0));
    v->amRate8 = clampi(an_at(&d->amRate, c), H8(0.1), H8(40.0));
    v->amDepth = clampi(an_at(&d->amDepth, c), 0, Q15);
    if (mac[0]) {                                    /* TONE: the FM's brightness with the filters */
        v->index = clampi(mulq16(v->index, an_mac_exp(mac[0], R16(1.5))), 0, IX(12.0));
    }
    if (mac[1]) {                                    /* the kind's own macro: from nothing to far more */
        int32_t down = mac[1] < 0 ? (MAC_MAX + mac[1]) * (Q15 / MAC_MAX) : Q15;   /* (Q15 at the middle, 0 at the end) */
        if (role == AR_GRAINS) {
            v->lGran = mac[1] > 0 ? clampi(v->lGran + mulq15(v->lGran > LV(0.2) ? v->lGran : LV(0.2), mac[1] * (3 * Q15 / MAC_MAX)), 0, Q15)
                                  : mulq15(v->lGran, down);
            v->gRate8 = clampi(mulq16(v->gRate8, an_mac_exp(mac[1], R16(2.5))), H8(0.5), H8(200.0));
        } else if (role == AR_AIR)
            v->lNoise = mac[1] > 0 ? clampi(v->lNoise + mulq15(v->lNoise > LV(0.15) ? v->lNoise : LV(0.15), mac[1] * (5 * Q15 / MAC_MAX)), 0, Q15)
                                   : mulq15(v->lNoise, down);
        else if (role == AR_FM)
            v->index = clampi(mulq16(v->index > IX(0.3) ? v->index : IX(0.3), an_mac_exp(mac[1], R16(3.0))), 0, IX(12.0));
        else if (role == AR_DRIVE && mac[1] < 0) {   /* DRIVE turned down: towards a pure tone -- the second voice,
                                                      * the FM, the noise and the grains drawn out */
            v->lB = mulq15(v->lB, down);
            v->lFM = mulq15(v->lFM, down);
            v->lNoise = mulq15(v->lNoise, down);
            v->lGran = mulq15(v->lGran, down);
            v->index = mulq15(v->index, down);
        }
    }
    v->am_on = (uint8_t)(d->ampMod || mac[3] > 0);
    if (mac[3] > 0 && !d->ampMod) {                  /* MOTION up: a tremolo of its own, quicker and deeper */
        v->amRate8 = H8(1.5) + mac[3] * (H8(8.0) / MAC_MAX);
        v->amDepth = mac[3] * (LV(0.7) / MAC_MAX);
    } else if (mac[3])                               /* (the design's tremolo deeper, or gone at the bottom) */
        v->amDepth = clampi(v->amDepth + mulq15(mac[3] > 0 ? (v->amDepth > LV(0.3) ? v->amDepth : LV(0.3)) : v->amDepth,
                                                mac[3] * (Q15 / MAC_MAX)), 0, Q15);
    v->ampPrev = v->amp;
    if (f->down)
        v->amp = clampi(an_at(&d->amp, c), 0, Q15);   /* a lifted finger's voice keeps its level as it rings */
    if (d->filter) {
        int32_t cut = an_at(&d->cutoff, c);
        if (v->ktmul != Q16)
            cut = clampi((int32_t)(((int64_t)cut * v->ktmul) >> 16), 30, 18000);
        if (tone != Q16)                             /* (TONE: never below the low notes themselves) */
            cut = clampi((int32_t)(((int64_t)cut * tone) >> 16), tone < Q16 ? 150 : 30, 18000);
        {
            int32_t q = clampi(an_at(&d->q, c), IX(0.5), IX(25.0));
            if (mac[0] > 0)                          /* (TONE up: an edge, the filter's peak sharper) */
                q = clampi(q + mulq15(q, mac[0] * (Q15 * 3 / 2 / MAC_MAX)), IX(0.5), IX(25.0));
            AN_SVF(&v->filtc, cut, q);
        }
    }
    if (d->noise > 1)
        AN_SVF(&v->nfiltc, tone == Q16 ? an_at(&d->noiseCut, c)
                                       : clampi((int32_t)(((int64_t)an_at(&d->noiseCut, c) * tone) >> 16), 30, 18000), IX(1.2));
    {   /* the FM carrier's band-limit level: its frequency with the sidebands, hz (1 + index ratio) */
        int32_t ratio = d->golden ? R16(1.6180340) : d->fmRatio;
        uint64_t k = (uint64_t)Q16 + (uint64_t)(((int64_t)v->index * ratio) >> 12);
        uint64_t e = ((uint64_t)v->inc * k) >> 16;
        v->incFMlevel = e > 0xFFFFFFFFu ? 0xFFFFFFFFu : (uint32_t)e;
    }
    /* the whole hand: where it is, its hardest press, its quickest finger */
    gc[AS_CONST] = Q15;
    gc[AS_X] = gx;
    gc[AS_Y] = Q15 - gy;
    gc[AS_SIZE] = hardest;
    gc[AS_SPEED] = fastest;
    gc[AS_ANGLE] = gc[AS_DIST] = gc[AS_AGE] = Q15 / 2;
    gc[AS_GX] = gx;
    gc[AS_GY] = Q15 - gy;
    gc[AS_COUNT] = v->countSmooth;
    gc[AS_LFO1] = l1;
    gc[AS_LFO2] = l2;
    if (d->down)
        v->downInc = (uint32_t)clampi(an_at(&d->downRate, gc), 200, FS) * 97391u;   /* 2^32 / FS per Hz */
    v->shp_on = (uint8_t)(d->shaper || (role == AR_DRIVE && mac[1] > 0));
    if (v->shp_on) {
        v->drive = clampi(an_at(&d->shaperDrive, gc), IX(1.0), IX(12.0));
        if (role == AR_DRIVE && mac[1])                /* (DRIVE: harder, or back towards clean) */
            v->drive = clampi(mac[1] > 0 ? v->drive + mac[1] * (IX(9.0) / MAC_MAX)
                                         : IX(1.0) + mulq15(v->drive - IX(1.0), (MAC_MAX + mac[1]) * (Q15 / MAC_MAX)),
                              IX(1.0), IX(12.0));
        v->driveNorm = (int32_t)(((uint32_t)Q15 << 12) / (uint32_t)(v->drive * 6 / 10 > 4096 ? v->drive * 6 / 10 : 4096));
    }
    if (d->phaser) {
        int32_t hz, sweep;
        v->phInc = (uint32_t)(((uint64_t)(uint32_t)clampi(an_at(&d->phaserRate, gc), 0, H16(5.0)) * 97392u) >> 16) * 32u;
        v->phDepth = clampi(an_at(&d->phaserDepth, gc), 0, Q15);
        v->phPlace = clampi(an_at(&d->phaserPlace, gc), 0, Q15);
        v->phPhase += v->phInc;
        v->phPlaceSmooth += (v->phPlace - v->phPlaceSmooth) / 36;    /* ~50 ms */
        sweep = d->phByHand ? v->phPlaceSmooth : Q15 / 2 + (sin_ph(v->phPhase) >> 1);
        hz = (int32_t)((300u * exp2_q16(mulq15(mulq15(sweep, R16(3.5)), v->phDepth))) >> 16);
        hz = clampi(hz, 40, FS * 4 / 10);
        {
            int32_t gg = svf_g(hz);           /* tan(pi hz / FS), Q24 */
            v->phA = ((1 << 24) - gg) / (((1 << 24) + gg) >> 15);   /* (1 - t) / (1 + t), Q15 */
        }
    }
    if (d->groupFilter) {
        int32_t gcut = an_at(&d->groupCut, gc);
        int32_t gq = clampi(an_at(&d->groupQ, gc), IX(0.5), IX(25.0));
        if (tone != Q16 || (role == AR_VOWEL && mac[1]))
            gcut = clampi((int32_t)(((int64_t)gcut * mulq16(tone, role == AR_VOWEL && mac[1] ? an_mac_exp(mac[1], R16(2.5)) : Q16)) >> 16),
                          60, 17000);
        if (role == AR_VOWEL && mac[1] > 0)          /* (VOWEL up: the formant sharper too) */
            gq = clampi(gq + mulq15(gq, mac[1] * (Q15 / MAC_MAX)), IX(0.5), IX(25.0));
        AN_SVF(&v->gfiltc, gcut, gq);
    }
    v->resFb = clampi(an_at(&d->resFeedback, gc), 0, LV(0.95));
    v->resMix = clampi(an_at(&d->resMix, gc), 0, Q15);
    v->res_on = (uint8_t)(d->resonator || mac[2] > 0);
    if (mac[2] > 0) {                                /* SPACE: the comb wetter and ringing longer, or dry */
        v->resMix = clampi(v->resMix + mac[2] * (LV(0.65) / MAC_MAX), 0, LV(0.85));
        v->resFb = clampi(v->resFb + mac[2] * (LV(0.25) / MAC_MAX), 0, LV(0.95));
    } else if (mac[2])
        v->resMix = mulq15(v->resMix, (MAC_MAX + mac[2]) * (Q15 / MAC_MAX));
}

/* ------------------------------------------------------- one block --- */
/* renders n (= CTL) samples, Q15 into out; returns 0 when the voice is silent for good */
#ifndef AN_RENDER_PLACE
#define AN_RENDER_PLACE __attribute__((noinline))   /* (FAST, from RAM: measured no faster -- the cost is the work) */
#endif
/* a block's samples, e / a the envelope and the amplitude moving by de / da a sample; its mean square back, for the
 * leveller. Everything a sample touches is held in locals for the block (the compiler cannot otherwise tell the
 * voice, the design and the output apart: it would load and store them all again at every sample), the wavetables
 * through the pointers an_render chose for this block (the RAM copies when there are) */
static AN_RENDER_PLACE int64_t an_samples(an_voice_t *v, const an_design_t *d, int32_t *out, uint32_t n, int32_t e,
                                          int32_t de, int32_t a, int32_t da)
{
    const uint32_t ns = (uint32_t)clampi(d->multiCount, 1, 4), grainInc = hz8_inc(v->gRate8) << AN_HALF,
                   amInc = hz8_inc(v->amRate8) << AN_HALF;
    const int oscA = d->oscA, oscB = d->oscB, fm = d->fm, multi = d->multi, gran = d->gran && v->lGran > 0;
    const int noise = d->noise && v->lNoise > 0 ? d->noise : 0, filter = d->filter, ampMod = v->am_on, down = d->down;
    const int shp = v->shp_on, curve = d->shaperCurve, phaser = d->phaser, gfilter = d->groupFilter, res = v->res_on;
    const int32_t lA = v->lA, lB = v->lB, lFM = v->lFM, lMulti = v->lMulti, lGran = v->lGran, lNoise = v->lNoise;
    const int32_t index = v->index, amDepth = v->amDepth, drive = v->drive, driveNorm = v->driveNorm, phA = v->phA;
    const int32_t resDelay = v->resDelay, resFb = v->resFb, resMix = v->resMix, dout = d->out, level = v->level >> 13;
    const int32_t granSpread = d->granSpread;
    const uint32_t inc = v->inc, incB = v->incB, incM = v->incM, downInc = v->downInc;
    const int16_t *const tA = v->tA, *const tB = v->tB, *const tFM = v->tFM, *const tAM = v->tAM;
    const svf_c fc = v->filtc, nc = v->nfiltc, gc = v->gfiltc;
    uint32_t pA = v->pA, pB = v->pB, pC = v->pC, pM = v->pM, pAM = v->pAM, grainClock = v->grainClock;
    uint32_t downAcc = v->downAcc, resPos = v->resPos, i, k;
    int32_t downHeld = v->downHeld, phFb = v->phFb, resLp = v->resLp, ap0 = v->ap[0], ap1 = v->ap[1], ap2 = v->ap[2],
            ap3 = v->ap[3];
    svf_t filt = v->filt, nfilt = v->nfilt, gfilt = v->gfilt;
    rnd_t rnd = v->rnd;
    int16_t *const rbuf = v->res;
    int64_t ms = 0;
    for (i = 0; i < n; i++) {
        int32_t s = 0;
        e += de;
        a += da;
        if (oscA) {
            pA += inc;
            s += mulq15(lA, an_rd(tA, pA));
        }
        if (oscB) {
            pB += incB;
            s += mulq15(lB, an_rd(tB, pB));
        }
        if (fm) {
            int32_t mod;
            pM += incM;
            mod = sin_ph(pM) * index;                           /* Q27 radians */
            pC += inc;
            s += mulq15(lFM, an_rd(tFM, pC + (uint32_t)(((int64_t)mod * 333772) >> 16)));
        }
        if (multi) {
            int32_t sum = 0;
            for (k = 0; k < ns; k++) {
                v->pStack[k] += v->incStack[k];
                sum += mulq15(an_rd(v->tM[k], v->pStack[k]), v->stackGain[k]);
            }
            s += mulq15(lMulti, sum);
        }
        if (gran) {
            uint32_t was = grainClock;
            int32_t gs = 0;
            grainClock += grainInc;
            if (grainClock < was) {                             /* a grain is due */
                for (k = 0; k < 3u; k++)
                    if (!v->gr[k].on) {
                        an_grain *g = &v->gr[k];
                        uint32_t len = (uint32_t)v->gLen4 * 441u / 160u;   /* samples */
                        g->on = 1;
                        g->pos = 0;
                        g->posinc = 0xFFFFFFFFu / (len > 16u ? len : 16u);
                        g->ph = rnd_next(&rnd);
                        /* scattered, when the design asks, onto fixed octaves and a fifth */
                        if (rnd_q15(&rnd) < granSpread) {
                            uint32_t w = rnd_next(&rnd) >> 30;
                            g->inc = w == 0 ? inc << 1 : w == 1 ? inc >> 1 : w == 2 ? inc + (inc >> 1) : inc << 2;
                        } else
                            g->inc = inc;
                        g->tab = an_tab(d->tableGran, g->inc);
                        break;
                    }
            }
            for (k = 0; k < 3u; k++) {
                an_grain *g = &v->gr[k];
                int32_t w;
                uint32_t was2;
                if (!g->on)
                    continue;
                w = sin_ph(g->pos >> 1);
                w = mulq15(w, w);
                g->ph += g->inc;
                gs += mulq15(w, an_rd(g->tab, g->ph));
                was2 = g->pos;
                g->pos += g->posinc;
                if (g->pos < was2)
                    g->on = 0;
            }
            s += mulq15(lGran, mulq15(gs, LV(0.6)));
        }
        if (noise) {
            int32_t nz = rnd_bi(&rnd);
            if (noise > 1) {
                nz = svf_run(&nfilt, &nc, nz, noise - 1);
                if (noise == 3)
                    nz <<= 1;
            }
            s += mulq15(lNoise, nz);
        }
        if (filter)
            s = svf_run(&filt, &fc, s, filter);
        if (ampMod) {
            pAM += amInc;
            s = mulq15(s, Q15 - mulq15(amDepth, Q15 / 2 - (an_rd(tAM, pAM) >> 1)));
        }
        s = mulq15(mulq15(s, a), e >> 9);
        /* the hand's chain (AnimaCore.h group) */
        if (down) {
            uint32_t was = downAcc;
            downAcc += downInc;
            if (downAcc < was || downInc >= 0xFFFF0000u)
                downHeld = s;
            s = downHeld;
        }
        if (shp) {
            int32_t y = mulq12(s, drive);
            if (curve == 1)
                y = sin_ph((uint32_t)((int64_t)clampi(y, -4 * Q15, 4 * Q15) * 25033));   /* sin(1.2 y) */
            else if (curve == 2) {
                int32_t z = clampi(y, -Q15, Q15);
                y = mulq15(mulq15(mulq15(z, z), z), 4 * Q15) - 3 * z;
            } else
                y = tanh_q15(y);
            s = mulq15(y, driveNorm);
        }
        if (phaser) {
            int32_t y = s + (phFb >> 1), o;
            o = -mulq15(phA, y) + ap0;
            ap0 = y + mulq15(phA, o);
            y = o;
            o = -mulq15(phA, y) + ap1;
            ap1 = y + mulq15(phA, o);
            y = o;
            o = -mulq15(phA, y) + ap2;
            ap2 = y + mulq15(phA, o);
            y = o;
            o = -mulq15(phA, y) + ap3;
            ap3 = y + mulq15(phA, o);
            y = o;
            phFb = y;
            s = mulq15(s + y, LV(0.6));
        }
        if (gfilter)
            s = svf_run(&gfilt, &gc, s, gfilter);
        if (res) {
            uint32_t rp = (resPos << 16) - (uint32_t)resDelay, i0 = (rp >> 16) & (AN_RES_N - 1u);
            int32_t fr = (int32_t)(rp & 0xFFFFu) >> 1, d0 = rbuf[i0] * 4, d1 = rbuf[(i0 + 1u) & (AN_RES_N - 1u)] * 4;
            int32_t y = d0 + (int32_t)(((int64_t)(d1 - d0) * fr) >> 15);
            resLp += (y - resLp) >> 1;
            rbuf[resPos & (AN_RES_N - 1u)] = (int16_t)(clampi(s + mulq15(resLp, resFb), -4 * Q15, 4 * Q15 - 4) >> 2);
            resPos++;
            s = s + mulq15(y - s, resMix);
        }
        s = mulq12(s, dout);
        ms += ((int64_t)s * s) >> 6;                            /* Q24 */
        /* the slow level, and a soft ceiling for a resonance pushed into its peak */
        out[i] = mulq15(tanh_q15(mulq15((int32_t)(((int64_t)s * level) >> 15), LV(1.1111))), LV(0.9));
    }
    v->pA = pA;
    v->pB = pB;
    v->pC = pC;
    v->pM = pM;
    v->pAM = pAM;
    v->grainClock = grainClock;
    v->downAcc = downAcc;
    v->downHeld = downHeld;
    v->resPos = resPos;
    v->phFb = phFb;
    v->resLp = resLp;
    v->ap[0] = ap0;
    v->ap[1] = ap1;
    v->ap[2] = ap2;
    v->ap[3] = ap3;
    v->filt = filt;
    v->nfilt = nfilt;
    v->gfilt = gfilt;
    v->rnd = rnd;
    return ms;
}
/* (RAM is too far from the flash for a direct call: through a pointer, which reaches anywhere) */
static int64_t (*volatile an_samples_at)(an_voice_t *, const an_design_t *, int32_t *, uint32_t, int32_t, int32_t, int32_t,
                                         int32_t) = an_samples;
#if TELEMETRY
static uint32_t an_t_ctl, an_t_smp, an_t_n;    /* (a diagnostic build: the control's and the samples' ticks, summed) */
#endif
static int an_render(an_voice_t *v, const an_design_t *d, int32_t *out, uint32_t n)
{
    int32_t e0, e1, de, a0, da;
    int64_t ms;
#if TELEMETRY
    uint32_t t0 = an_cache_now ? fm1_ticks() : 0u, t1 = 0;
#endif
    if (!v->live)
        return 0;
    an_control(v, d);
#if TELEMETRY
    if (an_cache_now)
        t1 = fm1_ticks();
#endif
    e0 = v->ev;
    e1 = an_env_tick(v, d);
    if (v->est == EN_IDLE && e0 == 0) {
        v->live = 0;
        return 0;
    }
#if AN_HALF
    /* (the increments doubled and the delays halved for the half rate, given back after) */
    uint32_t sv_inc = v->inc, sv_incB = v->incB, sv_incM = v->incM, sv_fml = v->incFMlevel, sv_down = v->downInc, sv_st[4];
    int32_t sv_res = v->resDelay, sv_glen = v->gLen4;
    {
        uint32_t k;
        for (k = 0; k < 4u; k++) {
            sv_st[k] = v->incStack[k];
            v->incStack[k] <<= 1;
        }
        v->inc <<= 1;
        v->incB <<= 1;
        v->incM <<= 1;
        v->incFMlevel <<= 1;
        v->downInc = v->downInc >= 0x80000000u ? 0xFFFFFFFFu : v->downInc << 1;
        v->resDelay = clampi(v->resDelay >> 1, 2 << 16, (int32_t)(AN_RES_N - 4u) << 16);
        v->gLen4 = v->gLen4 >> 1;
    }
#endif
    {   /* the wavetables this block reads (the RAM copies when there are) */
        uint32_t k, ns = (uint32_t)clampi(d->multiCount, 1, 4);
        if (d->oscA)
            v->tA = an_tab(d->tableA, v->inc);
        if (d->oscB)
            v->tB = an_tab(d->tableB, v->incB);
        if (d->fm)
            v->tFM = an_tab(d->tableFM, v->incFMlevel);
        if (v->am_on)
            v->tAM = an_tab(d->tableAM, 0);
        if (d->multi)
            for (k = 0; k < ns; k++)
                v->tM[k] = an_tab(d->tableMulti, v->incStack[k]);
        if (d->gran)
            for (k = 0; k < 3u; k++)
                if (v->gr[k].on)
                    v->gr[k].tab = an_tab(d->tableGran, v->gr[k].inc);
    }
#if AN_HALF
    {
        int32_t h[CTL / 2u], p = v->hprev;
        uint32_t k, nh = n >> 1;
        de = (e1 - e0) / (int32_t)nh;
        a0 = v->ampPrev;
        da = (v->amp - v->ampPrev) / (int32_t)nh;
        ms = an_samples_at(v, d, h, nh, e0, de, a0, da) << 1;   /* (the mean square of twice as many) */
        for (k = 0; k < nh; k++) {                      /* drawn out: half-way, then the sample */
            out[2u * k] = (p + h[k]) >> 1;
            out[2u * k + 1u] = p = h[k];
        }
        v->hprev = p;
        for (k = 0; k < 4u; k++)
            v->incStack[k] = sv_st[k];
        v->inc = sv_inc;
        v->incB = sv_incB;
        v->incM = sv_incM;
        v->incFMlevel = sv_fml;
        v->downInc = sv_down;
        v->resDelay = sv_res;
        v->gLen4 = sv_glen;
    }
#else
    de = (e1 - e0) / (int32_t)n;
    a0 = v->ampPrev;
    da = (v->amp - v->ampPrev) / (int32_t)n;
    ms = an_samples_at(v, d, out, n, e0, de, a0, da);
#endif
#if TELEMETRY
    if (an_cache_now) {
        uint32_t t2 = fm1_ticks();
        an_t_ctl += t1 - t0;
        an_t_smp += t2 - t1;
        an_t_n++;
    }
#endif
    {   /* AnimaCore.h: towards 0.1 RMS over seconds, never more than nine decibels either way */
        int32_t blk = (int32_t)((ms >> 5) > 0x7FFFFFFF ? 0x7FFFFFFF : (ms >> 5)), want;
        uint32_t root;
        v->blkms = blk;
        v->power += (blk - v->power) >> 12;
        if (v->power > 16) {
            root = isqrt32((uint32_t)v->power);                 /* Q12 */
            want = (int32_t)(1677722u / (root ? root : 1u));    /* 0.1 / sqrt(power), Q12 */
            want = clampi(want, IX(0.25), IX(2.8)) << 16;       /* Q28 */
            /* (down four times as fast as up: a resonance found by the hand is caught before it rings) */
            v->level += (want - v->level) >> (want < v->level ? 9 : 11);
        }
    }
    return 1;
}

/* ------------------------------------------------------- the designs --- */
static an_map AM_(uint8_t s, int32_t lo, int32_t hi, int32_t slope)
{
    an_map m;
    m.src = s;
    m.lo = lo;
    m.hi = hi;
    m.slope = (int16_t)slope;
    return m;
}
#define AM(s, lo, hi) AM_(s, lo, hi, 0)
#define AMS(s, lo, hi, sl) AM_(s, lo, hi, (int32_t)((sl) * 256.0))
#define AC(v) AM_(AS_CONST, v, v, 0)

static uint8_t an_any_source(rnd_t *r)
{
    static const uint8_t SRCS[10] = {AS_X, AS_Y, AS_SIZE, AS_SPEED, AS_DIST, AS_ANGLE, AS_AGE, AS_LFO1, AS_LFO2, AS_GX};
    return SRCS[rnd_below(r, 10)];
}
static int32_t an_mulk(int32_t v, int32_t k16) { return (int32_t)(((int64_t)v * k16) >> 16); }

/* one mapping varied: sometimes another controller, its range stretched or narrowed about its middle,
 * its slope drawn; a range in hertz is varied in octaves */
static void an_vary(an_map *m, rnd_t *r, int hertz, int32_t source_pct)
{
    if (m->src != AS_CONST && rnd_chance(r, source_pct))
        m->src = an_any_source(r);
    if (hertz) {
        int32_t k = (int32_t)exp2_q16(rnd_in(r, R16(-0.8), R16(0.8)));
        m->lo = an_mulk(m->lo, k);
        m->hi = an_mulk(an_mulk(m->hi, k), (int32_t)exp2_q16(rnd_in(r, R16(-0.5), R16(0.5))));
    } else if (m->lo != m->hi) {
        int32_t mid = (m->lo >> 1) + (m->hi >> 1), half = an_mulk((m->hi >> 1) - (m->lo >> 1), rnd_in(r, R16(0.6), R16(1.4)));
        m->lo = mid - half;
        m->hi = mid + half;
    }
    m->slope = (int16_t)clampi(m->slope + rnd_in(r, -154, 154), -256, 256);
}
static void an_scale(an_map *m, int32_t k16)
{
    m->lo = an_mulk(m->lo, k16);
    m->hi = an_mulk(m->hi, k16);
}

/* the variation every design is given on top of its kind's character (AnimaDesign.h varyDesign) */
static void an_vary_design(an_design_t *d, rnd_t *r)
{
    static const uint8_t WARM[8] = {0, 1, 3, 5, 16, 17, 20, 13};
    static const uint8_t BRIGHT[8] = {2, 4, 6, 15, 18, 21, 23, 12};
    static const uint8_t DENSE[8] = {7, 8, 9, 10, 11, 14, 19, 22};
    int kind = d->kind, held = kind == AK_HAZE || kind == AK_VOYAGE || kind == AK_GOLDEN || kind == AK_BREATH;
    int32_t t;
#define PICK(base) do { int32_t x_ = rnd_q15(r); if (x_ >= LV(0.4)) { \
        const uint8_t *p_ = x_ < LV(0.6) ? WARM : x_ < LV(0.8) ? BRIGHT : DENSE; (base) = p_[rnd_below(r, 8)]; } } while (0)
    PICK(d->tableA);
    PICK(d->tableB);
    PICK(d->tableMulti);
    PICK(d->tableGran);
    if (rnd_below(r, 3) == 0)
        d->tableAM = 2;
    if (kind != AK_BELLS && kind != AK_GOLDEN) {
        if (rnd_chance(r, 70))
            d->tableFM = 0;
        else
            PICK(d->tableFM);
    }
#undef PICK
    /* objects in and out, the kind's own kept more often than not */
    if (!d->oscB && rnd_chance(r, 30)) {
        d->oscB = 1;
        d->levelB = AM(an_any_source(r), LV(0.1), LV(0.45));
    }
    if (!d->fm && rnd_chance(r, 30)) {
        d->fm = 1;
        d->levelFM = AM(an_any_source(r), LV(0.1), LV(0.5));
        d->fmIndex = AM(an_any_source(r), IX(0.2), IX(2.5));
    }
    if (!d->multi && rnd_chance(r, held ? 35 : 15)) {
        d->multi = 1;
        d->multiCount = (uint8_t)(2 + rnd_below(r, 3));
        d->levelMulti = AM(an_any_source(r), LV(0.15), LV(0.45));
    }
    if (!d->gran && rnd_chance(r, held ? 40 : 20)) {
        d->gran = 1;
        d->levelGran = AM(an_any_source(r), LV(0.05), LV(0.4));
        d->granRate = AM(an_any_source(r), H8(5.0), H8(40.0));
        d->granLen = AM(an_any_source(r), MS4(20.0), MS4(150.0));
    }
    if (d->noise == 0 && rnd_chance(r, 35)) {
        d->noise = (uint8_t)(1 + rnd_below(r, 4));
        d->levelNoise = AM(an_any_source(r), 0, LV(0.15));
        d->noiseCut = AM(an_any_source(r), HZ(500), HZ(5000));
    }
    if (!d->ampMod && rnd_chance(r, 30)) {
        d->ampMod = 1;
        d->amRate = AM(an_any_source(r), H8(0.5), H8(9.0));
        d->amDepth = AM(an_any_source(r), 0, LV(0.45));
    }
    if (d->filter == 1 && d->cutoff.hi < 6000 && rnd_chance(r, 20))
        d->filter = 2;
    if (d->filter == 0 && rnd_chance(r, 30))
        d->filter = 1;
    d->down = d->down || rnd_chance(r, 15);
    if (d->down && d->downRate.lo >= 40000)
        d->downRate = AM(an_any_source(r), HZ(3000), HZ(26000));
    d->shaper = d->shaper || rnd_chance(r, 30);
    if (d->shaper) {
        d->shaperCurve = (uint8_t)rnd_below(r, 3);
        if (d->shaperDrive.lo == d->shaperDrive.hi)
            d->shaperDrive = AM(an_any_source(r), IX(1.0), rnd_in(r, IX(1.5), IX(3.5)));
    }
    /* a phaser on its own clock is a regular sweep that the hand's movement drowns under: slow, shallow */
    d->phaser = (uint8_t)rnd_chance(r, d->phaser ? 55 : 12);
    if (d->phaser) {
        d->phaserRate = AM(an_any_source(r), H16(0.03), rnd_in(r, H16(0.08), H16(0.2)));
        d->phaserDepth = AM(an_any_source(r), LV(0.15), rnd_in(r, LV(0.3), LV(0.5)));
    }
    d->resonator = (uint8_t)rnd_chance(r, d->resonator ? 85 : 35);
    if (d->resonator && d->resMix.lo == d->resMix.hi && d->resMix.lo == 0) {
        d->resMix = AM(an_any_source(r), LV(0.05), LV(0.4));
        d->resFeedback = AC(rnd_in(r, LV(0.4), LV(0.8)));
    }
    if (d->groupFilter == 0 && rnd_chance(r, 20)) {
        d->groupFilter = 1;
        d->groupCut = AM(an_any_source(r), HZ(1500), HZ(9000));
        d->groupQ = AC(rnd_in(r, IX(0.7), IX(2.5)));
    }
    /* intervals and ratios: whole steps, and the inharmonic ones that suit a bell or a gong */
    {
        static const int8_t IB[7] = {0, 7, 12, 12, 19, -12, 6};
        static const int32_t RATIOS[11] = {R16(1.0), R16(1.5), R16(2.0), R16(3.0), R16(3.5), R16(4.0), R16(1.4),
                                           R16(2.76), R16(5.0), R16(1.41421), R16(7.13)};
        static const int16_t STEPS[5] = {12 * 16, 7 * 16, 5 * 16, 24 * 16, 11 * 16};
        if (d->oscB && rnd_chance(r, 60))
            d->intervalB = IB[rnd_below(r, 7)];
        if (!d->golden && d->fm && rnd_chance(r, 60))
            d->fmRatio = RATIOS[rnd_below(r, 11)];
        if (d->multi && !d->golden && rnd_chance(r, 60))
            d->multiStep = STEPS[rnd_below(r, 5)];
    }
    /* every mapping varied */
    an_vary(&d->levelA, r, 0, 45);
    an_vary(&d->levelB, r, 0, 45);
    an_vary(&d->levelFM, r, 0, 45);
    an_vary(&d->levelMulti, r, 0, 45);
    an_vary(&d->levelGran, r, 0, 45);
    an_vary(&d->levelNoise, r, 0, 45);
    an_vary(&d->fmIndex, r, 0, 45);
    an_vary(&d->granRate, r, 0, 45);
    an_vary(&d->granLen, r, 0, 45);
    an_vary(&d->noiseCut, r, 1, 45);
    an_vary(&d->cutoff, r, 1, 45);
    an_vary(&d->q, r, 0, 45);
    an_vary(&d->amRate, r, 0, 45);
    an_vary(&d->amDepth, r, 0, 45);
    an_vary(&d->amp, r, 0, 15);
    an_vary(&d->shaperDrive, r, 0, 45);
    an_vary(&d->phaserRate, r, 0, 45);
    an_vary(&d->phaserDepth, r, 0, 45);
    d->phaserRate.lo = clampi(d->phaserRate.lo, H16(0.01), H16(0.25));
    d->phaserRate.hi = clampi(d->phaserRate.hi, H16(0.01), H16(0.25));
    d->phaserDepth.lo = clampi(d->phaserDepth.lo, 0, LV(0.55));
    d->phaserDepth.hi = clampi(d->phaserDepth.hi, 0, LV(0.55));
    an_vary(&d->groupCut, r, 1, 45);
    an_vary(&d->groupQ, r, 0, 45);
    an_vary(&d->resMix, r, 0, 45);
    d->q.lo = clampi(d->q.lo, IX(0.5), IX(25.0));
    d->q.hi = clampi(d->q.hi, IX(0.5), IX(12.0));
    d->amp.lo = clampi(d->amp.lo, LV(0.15), LV(1.0));
    d->amp.hi = clampi(d->amp.hi, LV(0.3), LV(1.0));
    /* the envelope's times, stretched or shortened together; and the hand's pace */
    t = (int32_t)exp2_q16(rnd_in(r, R16(-0.8), R16(0.8)));
    d->att = an_mulk(d->att, t);
    d->dec = an_mulk(d->dec, (int32_t)exp2_q16(rnd_in(r, R16(-0.6), R16(0.6))));
    d->rel = an_mulk(d->rel, (int32_t)exp2_q16(rnd_in(r, R16(-0.6), R16(0.6))));
    d->travel = an_mulk(d->travel, (int32_t)exp2_q16(rnd_in(r, R16(-0.7), R16(0.7))));
}

/* the mod matrix: three to seven routes from a gesture to a part of the sound, over a range wide enough to
 * be heard travelling (AnimaDesign.h route) */
static void an_route(an_design_t *d, rnd_t *r)
{
    static const uint8_t VOICE[10] = {AS_X, AS_Y, AS_SIZE, AS_SPEED, AS_DIST, AS_ANGLE, AS_X, AS_Y, AS_COUNT, AS_LFO1};
    static const uint8_t HAND[7] = {AS_GX, AS_GY, AS_COUNT, AS_SIZE, AS_SPEED, AS_GX, AS_LFO2};
    static const uint8_t WHERE[4] = {AS_X, AS_Y, AS_X, AS_GY};
    static const uint8_t LANDING[3] = {AS_SIZE, AS_COUNT, AS_SPEED};
    enum { CUTOFF, PEAK, FM, GRAINS, NOISE, SECOND, TREMOLO, STACK, DRIVE, RING, PHASE, COLOUR, CRUSH, TARGETS };
    static const uint8_t SUDDEN[6] = {FM, DRIVE, RING, SECOND, GRAINS, NOISE};
    uint8_t taken[TARGETS];
    int32_t routes = 4 + rnd_below(r, 4), n = 0, tries = 0;
    memset(taken, 0, sizeof taken);
#define SPAN(m, L_, H_, S_) do { (m) = AM_(S_, L_, H_, rnd_in(r, -128, 128)); if (rnd_chance(r, 30)) { \
        int32_t t_ = (m).lo; (m).lo = (m).hi; (m).hi = t_; } } while (0)
    for (; n < routes && tries < 40; tries++) {
        int32_t t = n == 0 ? CUTOFF : n == 1 ? SUDDEN[rnd_below(r, 6)] : rnd_below(r, TARGETS);
        uint8_t v = VOICE[rnd_below(r, 10)], h = HAND[rnd_below(r, 7)];
        if (t == CRUSH && rnd_chance(r, 50))
            t = DRIVE;
        if (taken[t])
            continue;
        taken[t] = 1;
        if (n == 0)
            v = h = WHERE[rnd_below(r, 4)];
        if (n == 1)
            v = h = LANDING[rnd_below(r, 3)];
        n++;
        switch (t) {
        case CUTOFF: {   /* a low-pass the hand opens and closes */
            int32_t centre = (int32_t)isqrt32((uint32_t)clampi(d->cutoff.lo, 60, 20000) * (uint32_t)clampi(d->cutoff.hi, 60, 20000));
            int32_t oct = rnd_in(r, R16(2.5), R16(3.5));
            centre = clampi(centre, 250, 4000);
            d->filter = 1;
            d->cutoff = AM_(v, clampi((int32_t)(((int64_t)centre * exp2_q16(-oct)) >> 16), 150, 20000),
                            clampi((int32_t)(((int64_t)centre * exp2_q16(oct)) >> 16), 150, 17000), rnd_in(r, -102, 102));
            if (rnd_chance(r, 30)) {
                int32_t t_ = d->cutoff.lo;
                d->cutoff.lo = d->cutoff.hi;
                d->cutoff.hi = t_;
            }
            break;
        }
        case PEAK:
            SPAN(d->q, IX(0.7), rnd_in(r, IX(5.0), IX(14.0)), v);
            break;
        case FM:
            if (!d->fm) {
                d->fm = 1;
                d->tableFM = 0;
                d->levelFM = AC(LV(0.45));
            }
            SPAN(d->fmIndex, 0, rnd_in(r, IX(3.0), IX(7.0)), v);
            break;
        case GRAINS:
            if (!d->gran) {
                d->gran = 1;
                d->granLen = AM(AS_Y, MS4(30.0), MS4(120.0));
            }
            SPAN(d->levelGran, 0, rnd_in(r, LV(0.4), LV(0.7)), v);
            SPAN(d->granRate, H8(4.0), rnd_in(r, H8(25.0), H8(70.0)), VOICE[rnd_below(r, 10)]);
            break;
        case NOISE:
            if (d->noise == 0) {
                d->noise = 3;
                d->noiseCut = AM(AS_X, HZ(1200), HZ(6000));
            }
            SPAN(d->levelNoise, 0, rnd_in(r, LV(0.12), LV(0.28)), v);
            break;
        case SECOND:
            if (!d->oscB) {
                d->oscB = 1;
                d->intervalB = rnd_chance(r, 50) ? 12 : 7;
            }
            SPAN(d->levelB, 0, rnd_in(r, LV(0.4), LV(0.7)), v);
            break;
        case TREMOLO:
            d->ampMod = 1;
            SPAN(d->amDepth, 0, rnd_in(r, LV(0.5), LV(0.9)), v);
            SPAN(d->amRate, rnd_in(r, H8(1.0), H8(3.0)), rnd_in(r, H8(6.0), H8(16.0)), VOICE[rnd_below(r, 10)]);
            break;
        case STACK:
            if (!d->multi) {
                d->multi = 1;
                d->multiCount = 3;
            }
            SPAN(d->levelMulti, 0, rnd_in(r, LV(0.35), LV(0.6)), v);
            break;
        case DRIVE:
            d->shaper = 1;
            SPAN(d->shaperDrive, IX(1.0), rnd_in(r, IX(3.0), IX(7.0)), h);
            break;
        case RING:
            if (!d->resonator) {
                d->resonator = 1;
                d->resFeedback = AC(rnd_in(r, LV(0.55), LV(0.85)));
            }
            SPAN(d->resMix, 0, rnd_in(r, LV(0.35), LV(0.6)), h);
            break;
        case PHASE:      /* the sweep placed by the hand: a finger sliding is the notch moving */
            d->phaser = 1;
            d->phByHand = 1;
            d->phaserDepth = AC(rnd_in(r, LV(0.8), LV(1.0)));
            SPAN(d->phaserPlace, 0, Q15, h);
            break;
        case COLOUR: {
            int32_t centre = rnd_in(r, 1500, 5000), oct = rnd_in(r, R16(1.2), R16(2.0));
            d->groupFilter = 1;
            d->groupQ = AC(rnd_in(r, IX(0.8), IX(2.5)));
            d->groupCut = AM_(h, clampi((int32_t)(((int64_t)centre * exp2_q16(-oct)) >> 16), 80, 17000),
                              clampi((int32_t)(((int64_t)centre * exp2_q16(oct)) >> 16), 80, 17000), rnd_in(r, -102, 102));
            break;
        }
        case CRUSH:
            d->down = 1;
            SPAN(d->downRate, HZ(2500), rnd_in(r, HZ(14000), HZ(36000)), h);
            break;
        }
    }
#undef SPAN
}

/* Anima's eight sliders (AnimaDesign.h applySliders): tone, resonance, fm, grain, texture, space, motion
 * and density, each Q15 with the design as drawn in the middle. Here they are drawn as well. */
static void an_sliders(an_design_t *d, const int32_t *s)
{
#define UP(v) clampi(((v) - Q15 / 2) * 2, 0, Q15)
    int32_t tone, res, floorQ, u;
    if (d->filter == 0) {
        d->filter = 1;
        d->cutoff = AC(HZ(16000));
        d->q = AC(IX(0.7));
    }
    /* tone: the filters, two octaves either way */
    tone = (int32_t)exp2_q16(mulq15(s[0] - Q15 / 2, R16(4.0)) << 0);
    an_scale(&d->cutoff, tone);
    an_scale(&d->groupCut, tone);
    an_scale(&d->noiseCut, tone);
    /* resonance: the filters' Q, and above the middle a floor that rises to a ringing peak */
    res = (int32_t)exp2_q16(mulq15(s[1] - Q15 / 2, R16(3.0)));
    u = UP(s[1]);
    floorQ = IX(0.5) + mulq15(mulq15(u, (int32_t)isqrt32((uint32_t)u << 15)), IX(14.0));
    an_scale(&d->q, res);
    an_scale(&d->groupQ, res);
    d->q.lo = d->q.lo > floorQ ? d->q.lo : floorQ;
    d->q.hi = d->q.hi > floorQ ? d->q.hi : floorQ;
    d->resFeedback.lo = clampi(d->resFeedback.lo + mulq15(s[1] - Q15 / 2, LV(0.4)), 0, LV(0.93));
    d->resFeedback.hi = clampi(d->resFeedback.hi + mulq15(s[1] - Q15 / 2, LV(0.4)), 0, LV(0.93));
    /* fm: the index, from none to twice the design's; brought in above the middle where there is none */
    u = UP(s[2]);
    if (!d->fm && u > 0) {
        d->fm = 1;
        d->tableFM = 0;
        d->levelFM = AC(mulq15(LV(0.6), u));
        d->fmIndex = AM(AS_SIZE, IX(0.4), IX(1.6));
        d->levelA.lo = mulq15(d->levelA.lo, Q15 - (u >> 1));
        d->levelA.hi = mulq15(d->levelA.hi, Q15 - (u >> 1));
    }
    an_scale(&d->fmIndex, s[2] << 2);                     /* x s * 2, Q16 */
    /* grain: the clouds and the stacked copies, thicker and quicker towards the top */
    u = UP(s[3]);
    if (!d->gran && u > 0) {
        d->gran = 1;
        d->levelGran = AC(mulq15(LV(0.5), u));
        d->granRate = AM(AS_SPEED, H8(8.0), H8(30.0));
        d->granLen = AM(AS_Y, MS4(30.0), MS4(120.0));
    }
    an_scale(&d->levelGran, s[3] << 2);
    an_scale(&d->levelMulti, R16(0.5) + (s[3] << 1));
    d->granSpread = clampi(d->granSpread + mulq15(u, LV(0.75)), 0, Q15);
    an_scale(&d->levelA, (Q15 - mulq15(LV(0.55), u)) << 1);
    an_scale(&d->levelB, (Q15 - mulq15(LV(0.55), u)) << 1);
    an_scale(&d->granRate, (int32_t)exp2_q16(mulq15(s[3] - Q15 / 2, R16(1.6))));
    /* texture: noise and the shaper's drive, both brought in above the middle */
    u = UP(s[4]);
    if (d->noise == 0 && u > 0) {
        d->noise = 3;
        d->levelNoise = AM(AS_SIZE, 0, mulq15(LV(0.12), u));
        d->noiseCut = AM(AS_X, HZ(1500), HZ(6000));
    } else
        an_scale(&d->levelNoise, s[4] << 2);
    if (!d->shaper && u > 0) {
        d->shaper = 1;
        d->shaperCurve = 0;
        d->shaperDrive = AC(IX(1.0));
    }
    d->shaperDrive.lo = IX(1.0) + mulq15(d->shaperDrive.lo - IX(1.0), s[4] << 1) + mulq15(u, IX(3.0));
    d->shaperDrive.hi = IX(1.0) + mulq15(d->shaperDrive.hi - IX(1.0), s[4] << 1) + mulq15(u, IX(3.0));
    /* space: the resonator and the phaser, both brought in above the middle */
    u = UP(s[5]);
    if (!d->resonator && u > 0) {
        d->resonator = 1;
        d->resMix = AC(0);
        d->resFeedback = AC(LV(0.6));
    }
    d->resMix.lo = clampi(mulq15(d->resMix.lo, s[5] << 1) + mulq15(u, LV(0.3)), 0, LV(0.65));
    d->resMix.hi = clampi(mulq15(d->resMix.hi, s[5] << 1) + mulq15(u, LV(0.3)), 0, LV(0.65));
    if (!d->phaser && s[5] > LV(0.65)) {
        d->phaser = 1;
        d->phaserRate = AM(AS_LFO2, H16(0.04), H16(0.15));
        d->phaserDepth = AC(0);
    }
    an_scale(&d->phaserDepth, s[5] << 2);
    d->phaserDepth.lo += mulq15(u, LV(0.5));
    d->phaserDepth.hi += mulq15(u, LV(0.5));
    /* motion and density: the hand */
    d->motionScale = (int32_t)exp2_q16(mulq15(s[6] - Q15 / 2, R16(2.6)));
    d->density = R16(0.25) + mulq15(s[7], R16(1.75));
#undef UP
}

/* a slider position: near the middle half the time, anywhere a third, at an end the rest */
static int32_t an_draw_slider(rnd_t *r)
{
    int32_t x = rnd_q15(r);
    if (x < LV(0.5))
        return rnd_in(r, LV(0.3), LV(0.7));
    if (x < LV(0.83))
        return rnd_q15(r);
    return rnd_chance(r, 50) ? rnd_in(r, 0, LV(0.1)) : rnd_in(r, LV(0.9), Q15);
}

/* the kinds' weights on each path */
static const uint8_t AK_W[3][AK_N0] = {
    {1, 1, 1, 1, 1, 1, 1, 1},
    {4, 3, 1, 2, 3, 2, 0, 1},
    {0, 1, 2, 1, 1, 3, 5, 5},
};
static const uint8_t AK_W1[3][AK_N1] = {            /* generations 1 and 2 */
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {4, 3, 1, 2, 3, 2, 0, 1, 4, 4, 0, 3, 2, 0},
    {0, 1, 2, 1, 1, 3, 5, 5, 0, 0, 5, 2, 3, 5},
};
static const uint8_t AK_W3[3][AK_N] = {             /* generation 3: eight kinds more */
    {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
    {4, 3, 1, 2, 3, 2, 0, 1, 4, 4, 0, 3, 2, 0, 2, 2, 5, 3, 0, 5, 3, 0},
    {0, 1, 2, 1, 1, 3, 5, 5, 0, 0, 5, 2, 3, 5, 3, 1, 0, 1, 5, 0, 3, 5},
};
/* the sliders as a path leans them (the open path: as an_draw_slider has them) */
static void an_path_sliders(int32_t *s, uint32_t path, rnd_t *r)
{
    int32_t i;
    for (i = 0; i < 8; i++)
        s[i] = an_draw_slider(r);
    if (path == PATH_AMBIENT) {
        s[0] = rnd_in(r, LV(0.15), LV(0.55));       /* tone: darker */
        s[1] = clampi(s[1], 0, LV(0.65));                  /* resonance: no screaming peaks */
        s[2] = clampi(s[2], 0, LV(0.55));                  /* fm */
        s[4] = rnd_in(r, 0, LV(0.45));               /* texture: clean */
        s[5] = rnd_in(r, LV(0.5), LV(0.95));         /* space */
        s[6] = rnd_in(r, LV(0.15), LV(0.55));        /* motion: a slow hand */
    } else if (path == PATH_EXP) {
        if (rnd_chance(r, 50))
            s[1] = rnd_in(r, LV(0.75), Q15);         /* resonance: screaming, now and then */
        s[2] = rnd_in(r, LV(0.5), Q15);              /* fm */
        s[3] = rnd_in(r, LV(0.45), Q15);             /* grain */
        s[4] = rnd_in(r, LV(0.6), Q15);              /* texture */
        s[6] = rnd_in(r, LV(0.55), Q15);             /* motion */
        s[7] = rnd_in(r, LV(0.6), Q15);              /* density */
    }
}
static void an_path_finish(an_design_t *d, uint32_t path, rnd_t *r)
{
    if (path == PATH_AMBIENT) {
        d->down = 0;
        d->shaperDrive.lo = clampi(d->shaperDrive.lo, IX(1.0), IX(2.0));
        d->shaperDrive.hi = clampi(d->shaperDrive.hi, IX(1.0), IX(2.0));
        d->amDepth.lo = clampi(d->amDepth.lo, 0, LV(0.35));
        d->amDepth.hi = clampi(d->amDepth.hi, 0, LV(0.35));
        if (d->kind != AK_BELLS && d->kind != AK_GLASS && d->att < 120)
            d->att = rnd_in(r, 120, 900);                  /* a swell */
        if (d->rel < 2000)
            d->rel = rnd_in(r, 2000, 5000);                /* a long tail */
    } else if (path == PATH_EXP) {
        if (rnd_chance(r, 70)) {                       /* the rate crushed down, the hand dragging it */
            d->down = 1;
            d->downRate = AM(an_any_source(r), rnd_in(r, HZ(1200), HZ(3500)), rnd_in(r, HZ(9000), HZ(24000)));
        }
        if (rnd_chance(r, 75)) {                       /* folded, mostly */
            d->shaper = 1;
            d->shaperCurve = (uint8_t)(rnd_chance(r, 65) ? 1 : 2);
            d->shaperDrive = AM(an_any_source(r), IX(1.5), rnd_in(r, IX(4.0), IX(7.5)));
        }
        if (rnd_chance(r, 80)) {                       /* a band of noise the hand drags about */
            d->noise = 3;
            d->levelNoise = AM(an_any_source(r), LV(0.08), rnd_in(r, LV(0.25), LV(0.45)));
            d->noiseCut = AM(an_any_source(r), HZ(400), rnd_in(r, HZ(5000), HZ(11000)));
        }
        if (d->gran || rnd_chance(r, 70)) {           /* grains, scattered off the note */
            d->gran = 1;
            if (d->levelGran.hi < LV(0.3))
                d->levelGran = AM(an_any_source(r), LV(0.1), rnd_in(r, LV(0.35), LV(0.6)));
            d->granSpread = clampi(d->granSpread + rnd_in(r, LV(0.4), LV(0.8)), 0, Q15);
            d->granLen = AM(an_any_source(r), MS4(8.0), rnd_in(r, MS4(40.0), MS4(90.0)));
        }
        if (rnd_chance(r, 50)) {                       /* a fast tremolo: a sound torn into pieces */
            d->ampMod = 1;
            d->amRate = AM(an_any_source(r), rnd_in(r, H8(5.0), H8(12.0)), rnd_in(r, H8(18.0), H8(35.0)));
            d->amDepth = AM(an_any_source(r), LV(0.3), rnd_in(r, LV(0.6), LV(0.9)));
        }
        if (d->resonator && rnd_chance(r, 50)) {       /* the comb ringing hard */
            d->resFeedback = AC(rnd_in(r, LV(0.8), LV(0.92)));
            d->resMix = AM(an_any_source(r), LV(0.2), rnd_in(r, LV(0.45), LV(0.65)));
        }
        if (d->att > 60 && rnd_chance(r, 60))
            d->att = rnd_in(r, 2, 40);                 /* it starts at once */
    }
}

/* generation 1's care: the filter follows the keyboard; the second oscillator's interval suits the path; AMBIENT
 * has a space to ring in */
static void an_care1(an_design_t *d, uint32_t path, rnd_t *r)
{
    static const int8_t CONSONANT[6] = {7, 12, -12, 19, 24, 5}, ANY[6] = {6, 1, 11, 13, -1, 18};
    d->kt = (uint8_t)(d->filter ? rnd_in(r, path == PATH_EXP ? 60 : 110, path == PATH_EXP ? 160 : 220) : 0);
    if (d->oscB) {
        if (path == PATH_AMBIENT && (d->intervalB == 6 || d->intervalB == 1 || d->intervalB == 11))
            d->intervalB = CONSONANT[rnd_below(r, 6)];
        else if (path == PATH_EXP && rnd_chance(r, 40))
            d->intervalB = ANY[rnd_below(r, 6)];
    }
    if (path == PATH_AMBIENT && !d->resonator && rnd_chance(r, 70)) {
        d->resonator = 1;
        d->resMix = AM(an_any_source(r), LV(0.08), rnd_in(r, LV(0.25), LV(0.45)));
        d->resFeedback = AC(rnd_in(r, LV(0.55), LV(0.8)));
    }
}

/* generation 3's breadth, on every design (its draws after all the earlier ones'): any of the 24 tables, wider
 * intervals and ratios, a detuning; now and then a plucked envelope or a slow swell, another way for the hand to
 * move; and now and then something brought in -- a pulsing tremolo, a crushed rate, a sharp resonance */
static void an_care3(an_design_t *d, uint32_t path, rnd_t *r)
{
    static const int8_t IB3[10] = {3, 4, 5, 7, 10, 12, 15, 19, 24, -5};
    static const int32_t RAT3[8] = {R16(0.5), R16(1.17), R16(2.41), R16(3.14), R16(4.5), R16(6.0), R16(0.75), R16(9.0)};
    static const int16_t ST3[8] = {3 * 16, 4 * 16, 5 * 16, 10 * 16, 2 * 16, 7 * 16 + 4, 12 * 16 - 3, 17 * 16};
    int struck = d->att < 20 && d->sus < LV(0.3);
    if (rnd_chance(r, 35))
        d->tableA = (uint8_t)rnd_below(r, AN_TABLES);
    if (d->oscB && rnd_chance(r, 35))
        d->tableB = (uint8_t)rnd_below(r, AN_TABLES);
    if (d->multi && rnd_chance(r, 35))
        d->tableMulti = (uint8_t)rnd_below(r, AN_TABLES);
    if (d->gran && rnd_chance(r, 35))
        d->tableGran = (uint8_t)rnd_below(r, AN_TABLES);
    if (d->oscB && rnd_chance(r, 40))
        d->intervalB = IB3[rnd_below(r, path == PATH_AMBIENT ? 9 : 10)];
    if (d->oscB && rnd_chance(r, 40))
        d->detuneB = (int16_t)rnd_in(r, -16 * 16, 16 * 16);
    if (d->fm && !d->golden && rnd_chance(r, 40))
        d->fmRatio = RAT3[rnd_below(r, 8)];
    if (d->multi && !d->golden && rnd_chance(r, 40))
        d->multiStep = ST3[rnd_below(r, 8)];
    if (!struck && rnd_chance(r, path == PATH_AMBIENT ? 8 : 18)) {   /* plucked: it strikes and dies away */
        d->att = rnd_in(r, 1, 8);
        d->hold = 0;
        d->dec = rnd_in(r, 150, 900);
        d->sus = rnd_in(r, 0, LV(0.2));
        d->rel = rnd_in(r, 200, 1200);
        d->amp = AM(AS_SIZE, LV(0.6), LV(1.0));
    } else if (struck && path == PATH_AMBIENT && rnd_chance(r, 30)) {
        d->sus = rnd_in(r, LV(0.3), LV(0.6));          /* (a strike that rings on) */
    } else if (rnd_chance(r, 15)) {                    /* a slow swell */
        d->att = rnd_in(r, 1500, 5000);
        d->sus = rnd_in(r, LV(0.7), LV(0.95));
    }
    if (rnd_chance(r, 25))
        d->motion = (uint8_t)rnd_below(r, 6);
    if (!d->ampMod && rnd_chance(r, path == PATH_AMBIENT ? 10 : 22)) {   /* a pulse: the tremolo square-ish */
        d->ampMod = 1;
        d->tableAM = (uint8_t)(rnd_chance(r, 50) ? 3 : 23);
        d->amRate = AM(an_any_source(r), H8(rnd_in(r, 2, 6)), H8(rnd_in(r, 8, 16)));
        d->amDepth = AM(an_any_source(r), LV(0.3), rnd_in(r, LV(0.6), LV(0.9)));
    }
    if (!d->down && path != PATH_AMBIENT && rnd_chance(r, 18)) {        /* a crushed rate */
        d->down = 1;
        d->downRate = AM(an_any_source(r), HZ(rnd_in(r, 1000, 3000)), HZ(rnd_in(r, 6000, 14000)));
    }
    if (d->filter && rnd_chance(r, path == PATH_AMBIENT ? 8 : 15)) {    /* a sharp resonance the hand plays */
        d->q = AM(an_any_source(r), IX(rnd_in(r, 3, 6)), IX(rnd_in(r, 8, 16)));
        if (d->out > IX(0.5))
            d->out = IX(0.5);
    }
}

/* a new design of kind k (AK_N: any), from seed, on a path */
static void an_design(an_design_t *d, int32_t k, uint32_t seed, uint32_t path, uint32_t gen)
{
    rnd_t rr, *r = &rr;
    int32_t s[8];
    path = path < PATH_N ? path : PATH_OPEN;
    rnd_seed(r, seed);
    if (gen == 0u && (k < 0 || k >= AK_N0))
        k = path == PATH_OPEN ? rnd_below(r, AK_N0) : (int32_t)rnd_weighted(r, AK_W[path], AK_N0);
    else if (gen <= 2u && (k < 0 || k >= AK_N1))
        k = (int32_t)rnd_weighted(r, AK_W1[path], AK_N1);
    else if (k < 0 || k >= AK_N)
        k = (int32_t)rnd_weighted(r, AK_W3[path], AK_N);
    memset(d, 0, sizeof *d);
    d->kind = (uint8_t)k;
    d->path = (uint8_t)path;
    d->gen = (uint8_t)gen;
    d->seed = seed;
    d->oscA = 1;
    d->tableB = 2;
    d->tableMulti = 2;
    d->tableGran = 11;
    d->intervalB = 7;
    d->fmRatio = R16(2.0);
    d->multiCount = 3;
    d->multiStep = 12 * 16;
    d->att = 20;
    d->dec = 400;
    d->sus = LV(0.7);
    d->rel = 900;
    d->levelA = d->levelB = d->levelFM = d->levelMulti = d->levelGran = d->levelNoise = AC(0);
    d->fmIndex = AC(0);
    d->granRate = AC(H8(12.0));
    d->granLen = AC(MS4(60.0));
    d->noiseCut = AC(HZ(2000));
    d->cutoff = AC(HZ(6000));
    d->q = AC(IX(0.8));
    d->amRate = AC(H8(3.0));
    d->amDepth = AC(0);
    d->amp = AC(LV(0.8));
    d->downRate = AC(HZ(44100));
    d->shaperDrive = AC(IX(1.0));
    d->phaserRate = AC(H16(0.2));
    d->phaserDepth = AC(LV(0.5));
    d->groupCut = AC(HZ(9000));
    d->groupQ = AC(IX(0.7));
    d->resFeedback = AC(LV(0.5));
    d->resMix = AC(0);
    d->phaserPlace = AC(LV(0.5));
    d->travel = R16(0.15);
    d->out = IX(0.4);
    switch (k) {
    /* generation 3's kinds */
    case AK_PLUCK:     /* a plucked string: a burst through the comb tuned to the note, dying away */
        d->tableA = rnd_chance(r, 50) ? 2 : 18;
        d->levelA = AM(AS_SIZE, LV(0.25), LV(0.5));
        d->noise = 1;
        d->levelNoise = AM(AS_SIZE, LV(0.15), LV(0.35));
        d->filter = 1;
        d->cutoff = AMS(AS_SIZE, HZ(rnd_in(r, 700, 1600)), HZ(rnd_in(r, 4000, 9000)), -0.3);
        d->q = AC(IX(0.8));
        d->resonator = 1;
        d->resMix = AC(rnd_in(r, LV(0.45), LV(0.7)));
        d->resFeedback = AC(rnd_in(r, LV(0.84), LV(0.93)));
        d->amp = AM(AS_SIZE, LV(0.6), LV(1.0));
        d->att = rnd_in(r, 1, 4);
        d->hold = 0;
        d->dec = rnd_in(r, 120, 700);
        d->sus = rnd_chance(r, 70) ? 0 : LV(0.15);
        d->rel = rnd_in(r, 150, 900);
        d->motion = MO_TAPS;
        d->travel = R16(0.3);
        d->out = IX(0.7);
        break;
    case AK_REED:      /* a reed: nasal, through a formant the hand moves, a breath in it */
        d->tableA = 13;
        d->levelA = AC(LV(0.5));
        d->oscB = 1;
        d->tableB = 3;
        d->intervalB = rnd_chance(r, 60) ? 12 : 0;
        d->detuneB = (int16_t)rnd_in(r, 2 * 16, 9 * 16);
        d->levelB = AM(AS_Y, LV(0.1), LV(0.35));
        d->groupFilter = 2;
        d->groupCut = AMS(AS_X, HZ(rnd_in(r, 500, 900)), HZ(rnd_in(r, 1800, 3200)), -0.2);
        d->groupQ = AC(rnd_in(r, IX(1.5), IX(4.0)));
        d->noise = 3;
        d->levelNoise = AM(AS_SPEED, LV(0.02), LV(0.1));
        d->noiseCut = AM(AS_X, HZ(1200), HZ(4000));
        d->ampMod = 1;
        d->tableAM = 0;
        d->amRate = AM(AS_LFO1, H8(4.0), H8(6.5));
        d->amDepth = AC(rnd_in(r, LV(0.05), LV(0.2)));
        d->amp = AM(AS_SIZE, LV(0.5), LV(0.9));
        d->att = rnd_in(r, 30, 200);
        d->dec = 600;
        d->sus = LV(0.8);
        d->rel = rnd_in(r, 200, 900);
        d->motion = MO_HOLDS;
        d->travel = R16(0.08);
        d->out = IX(0.6);
        break;
    case AK_FOG:       /* a stack under fog: very filtered, a slow phaser drifting through it */
        d->oscA = 1;
        d->tableA = rnd_chance(r, 50) ? 0 : 16;
        d->levelA = AC(LV(0.3));
        d->multi = 1;
        d->tableMulti = rnd_chance(r, 50) ? 17 : 1;
        d->multiCount = 4;
        d->multiStep = rnd_chance(r, 50) ? 7 * 16 : 12 * 16;
        d->levelMulti = AM(AS_DIST, LV(0.25), LV(0.45));
        d->filter = 1;
        d->cutoff = AMS(AS_X, HZ(rnd_in(r, 250, 500)), HZ(rnd_in(r, 1200, 2400)), -0.3);
        d->q = AC(IX(1.1));
        d->noise = 2;
        d->levelNoise = AC(LV(0.04));
        d->noiseCut = AC(HZ(900));
        d->phaser = 1;
        d->phaserRate = AM(AS_LFO2, H16(0.02), H16(0.07));
        d->phaserDepth = AC(LV(0.5));
        d->resonator = 1;
        d->resMix = AM(AS_COUNT, LV(0.1), LV(0.3));
        d->resFeedback = AC(LV(0.7));
        d->att = rnd_in(r, 900, 3000);
        d->dec = 3000;
        d->sus = LV(0.8);
        d->rel = rnd_in(r, 3000, 7000);
        d->motion = MO_DRAGS;
        d->travel = R16(0.04);
        d->out = IX(0.9);
        break;
    case AK_ORGAN:     /* drawbars: an organ's partials, a rotary tremolo, a little drive */
        d->tableA = 5;
        d->levelA = AC(LV(0.5));
        d->multi = 1;
        d->tableMulti = rnd_chance(r, 50) ? 5 : 0;
        d->multiCount = (uint8_t)rnd_in(r, 2, 3);
        d->multiStep = rnd_chance(r, 70) ? 12 * 16 : 19 * 16;
        d->levelMulti = AM(AS_Y, LV(0.15), LV(0.4));
        d->ampMod = 1;
        d->tableAM = 0;
        d->amRate = AM(AS_SPEED, H8(rnd_in(r, 3, 5)), H8(7.0));
        d->amDepth = AC(rnd_in(r, LV(0.12), LV(0.35)));
        d->shaper = 1;
        d->shaperCurve = 0;
        d->shaperDrive = AM(AS_SIZE, IX(1.1), IX(rnd_in(r, 2, 3)));
        d->filter = 1;
        d->cutoff = AM(AS_X, HZ(2500), HZ(8000));
        d->q = AC(IX(0.7));
        d->att = rnd_in(r, 5, 40);
        d->dec = 300;
        d->sus = LV(0.9);
        d->rel = rnd_in(r, 80, 400);
        d->motion = MO_HOLDS;
        d->travel = R16(0.1);
        d->out = IX(0.5);
        break;
    case AK_BITS:      /* pulses in few bits: the rate crushed and dragged about, folded */
        d->tableA = rnd_chance(r, 50) ? 3 : 23;
        d->levelA = AC(LV(0.45));
        d->oscB = 1;
        d->tableB = 4;
        d->intervalB = rnd_chance(r, 50) ? 12 : 7;
        d->levelB = AM(AS_Y, LV(0.1), LV(0.4));
        d->down = 1;
        d->downRate = AMS(AS_X, HZ(rnd_in(r, 500, 1500)), HZ(rnd_in(r, 4000, 9000)), -0.4);
        d->shaper = 1;
        d->shaperCurve = 1;
        d->shaperDrive = AM(AS_SIZE, IX(1.3), IX(rnd_in(r, 3, 5)));
        d->filter = 1;
        d->cutoff = AC(HZ(9000));
        d->att = rnd_in(r, 1, 15);
        d->dec = rnd_in(r, 100, 500);
        d->sus = LV(0.7);
        d->rel = rnd_in(r, 60, 400);
        d->motion = MO_TAPS;
        d->travel = R16(0.35);
        d->out = IX(0.4);
        break;
    case AK_DRIFT:     /* two saws drifting against each other: a slow beat, a chorus the hand moves */
        d->tableA = rnd_chance(r, 50) ? 2 : 1;
        d->levelA = AC(LV(0.4));
        d->oscB = 1;
        d->tableB = d->tableA;
        d->intervalB = rnd_chance(r, 70) ? 0 : 12;
        d->detuneB = (int16_t)rnd_in(r, 6 * 16, 22 * 16);
        d->levelB = AC(LV(0.4));
        d->multi = (uint8_t)rnd_chance(r, 50);
        d->tableMulti = d->tableA;
        d->multiCount = 2;
        d->multiStep = -12 * 16;
        d->levelMulti = AM(AS_DIST, LV(0.1), LV(0.3));
        d->filter = 1;
        d->cutoff = AMS(AS_Y, HZ(rnd_in(r, 600, 1200)), HZ(rnd_in(r, 3000, 6000)), -0.2);
        d->q = AC(IX(rnd_in(r, 1, 2)));
        d->phaser = 1;
        d->phByHand = 1;
        d->phaserDepth = AC(LV(0.6));
        d->phaserPlace = AM(AS_X, 0, Q15);
        d->att = rnd_in(r, 300, 1500);
        d->dec = 2000;
        d->sus = LV(0.85);
        d->rel = rnd_in(r, 2000, 5000);
        d->motion = MO_SWEEPS;
        d->travel = R16(0.05);
        d->out = IX(0.6);
        break;
    case AK_MURMUR:    /* a murmur: a dark cloud of noisy grains */
        d->levelA = AC(LV(0.08));
        d->gran = 1;
        d->tableGran = (uint8_t)(rnd_chance(r, 50) ? 22 : rnd_chance(r, 50) ? 11 : 8);
        d->levelGran = AM(AS_SIZE, LV(0.4), LV(0.75));
        d->granRate = AM(AS_SPEED, H8(rnd_in(r, 12, 25)), H8(rnd_in(r, 40, 70)));
        d->granLen = AM(AS_Y, MS4(30.0), MS4(140.0));
        d->granSpread = rnd_in(r, LV(0.4), LV(0.8));
        d->filter = 1;
        d->cutoff = AMS(AS_Y, HZ(rnd_in(r, 300, 600)), HZ(rnd_in(r, 1500, 3000)), -0.3);
        d->q = AC(IX(1.0));
        d->resonator = 1;
        d->resMix = AM(AS_COUNT, LV(0.15), LV(0.35));
        d->resFeedback = AC(LV(0.7));
        d->att = rnd_in(r, 400, 1500);
        d->dec = 2000;
        d->sus = LV(0.8);
        d->rel = rnd_in(r, 2000, 5000);
        d->motion = MO_SWARM;
        d->travel = R16(0.12);
        d->out = IX(0.8);
        break;
    case AK_HOWL:      /* a howl: noise through a resonance so sharp it whistles, swept by the hand */
        d->levelA = AC(LV(0.1));
        d->noise = 1;
        d->levelNoise = AC(LV(0.35));
        d->filter = 2;
        d->cutoff = AMS(AS_X, HZ(rnd_in(r, 200, 500)), HZ(rnd_in(r, 2500, 6000)), -0.4);
        d->q = AM(AS_SIZE, IX(rnd_in(r, 8, 14)), IX(rnd_in(r, 16, 25)));
        d->shaper = 1;
        d->shaperCurve = 0;
        d->shaperDrive = AC(IX(1.5));
        d->att = rnd_in(r, 80, 500);
        d->dec = 800;
        d->sus = LV(0.85);
        d->rel = rnd_in(r, 400, 2000);
        d->motion = MO_SWEEPS;
        d->travel = R16(0.15);
        d->out = IX(0.3);
        break;
    case AK_HAZE:      /* a breathed, ghostly pad */
        d->tableA = rnd_chance(r, 50) ? 22 : 20;
        d->oscB = 1;
        d->tableB = rnd_chance(r, 50) ? 0 : 17;
        d->intervalB = 12;
        d->detuneB = (int16_t)rnd_in(r, 4 * 16, 9 * 16);
        d->noise = 3;
        d->levelNoise = AM(AS_SPEED, LV(0.02), LV(0.25));
        d->noiseCut = AMS(AS_X, HZ(600), HZ(3000), -0.5);
        d->levelA = AC(LV(0.6));
        d->levelB = AM(AS_DIST, LV(0.15), LV(0.55));
        d->filter = 1;
        d->cutoff = AMS(AS_Y, HZ(400), rnd_in(r, 2500, 4000), -0.6);
        d->q = AM(AS_SIZE, IX(0.8), IX(2.2));
        d->amp = AMS(AS_SIZE, LV(0.35), LV(0.9), 0.3);
        d->att = rnd_in(r, 900, 1800);
        d->dec = 1500;
        d->sus = LV(0.8);
        d->rel = rnd_in(r, 2500, 4000);
        d->phaser = (uint8_t)rnd_chance(r, 70);
        d->phaserRate = AM(AS_LFO2, H16(0.05), H16(0.3));
        d->phaserDepth = AC(LV(0.6));
        d->motion = MO_HOLDS;
        d->travel = R16(0.05);
        d->out = IX(0.5);
        break;
    case AK_VOYAGE:    /* a slowly travelling, grainy stack */
        d->oscA = 0;
        d->multi = 1;
        d->tableMulti = (uint8_t)(8 + rnd_below(r, 3));
        d->multiCount = (uint8_t)(3 + rnd_below(r, 2));
        d->multiStep = rnd_chance(r, 50) ? 12 * 16 : 7 * 16;
        d->levelMulti = AM(AS_DIST, LV(0.6), LV(0.25));
        d->gran = 1;
        d->tableGran = rnd_chance(r, 50) ? 11 : 18;
        d->levelGran = AM(AS_SPEED, LV(0.1), LV(0.6));
        d->granRate = AMS(AS_X, H8(4.0), H8(30.0), -0.5);
        d->granLen = AM(AS_Y, MS4(30.0), MS4(200.0));
        d->filter = 1;
        d->cutoff = AMS(AS_AGE, HZ(700), HZ(5000), -0.4);
        d->q = AC(IX(1.2));
        d->ampMod = (uint8_t)rnd_chance(r, 50);
        d->tableAM = 1;
        d->amRate = AM(AS_LFO1, H8(0.3), H8(2.0));
        d->amDepth = AC(LV(0.25));
        d->amp = AM(AS_SIZE, LV(0.4), LV(0.85));
        d->att = rnd_in(r, 1200, 2500);
        d->dec = 2000;
        d->sus = LV(0.75);
        d->rel = rnd_in(r, 3000, 5000);
        d->shaper = (uint8_t)rnd_chance(r, 40);
        d->shaperDrive = AM(AS_GX, IX(1.0), IX(2.2));
        d->phaser = 1;
        d->phaserRate = AM(AS_LFO2, H16(0.05), H16(0.25));
        d->resonator = (uint8_t)rnd_chance(r, 50);
        d->resMix = AM(AS_LFO2, 0, LV(0.3));
        d->resFeedback = AC(LV(0.6));
        d->motion = rnd_chance(r, 50) ? MO_SWEEPS : MO_ORBITS;
        d->travel = R16(0.08);
        d->out = IX(1.1);
        break;
    case AK_GLASS:     /* bright, crystalline strikes, ringing in a comb */
        d->tableA = rnd_chance(r, 50) ? 6 : 15;
        d->levelA = AC(LV(0.5));
        d->fm = 1;
        d->fmRatio = rnd_chance(r, 50) ? R16(3.0) : R16(3.5);
        d->levelFM = AC(LV(0.6));
        d->fmIndex = AMS(AS_SIZE, IX(0.2), IX(2.5), 0.3);
        d->filter = 1;
        d->cutoff = AMS(AS_X, HZ(2500), HZ(9000), -0.3);
        d->q = AM(AS_Y, IX(1.0), IX(4.0));
        d->amp = AM(AS_SIZE, LV(0.4), LV(0.9));
        d->att = rnd_in(r, 3, 20);
        d->dec = rnd_in(r, 600, 1500);
        d->sus = LV(0.15);
        d->rel = 1500;
        d->resonator = 1;
        d->resMix = AC(rnd_in(r, LV(0.12), LV(0.3)));
        d->resFeedback = AC(LV(0.6));
        d->motion = MO_TAPS;
        d->travel = R16(0.1);
        d->out = IX(0.55);
        break;
    case AK_BELLS:     /* struck FM bells, inharmonic */
        d->fm = 1;
        d->tableFM = 0;
        d->levelFM = AC(LV(0.8));
        {
            static const int32_t RATIOS[5] = {R16(1.4), R16(2.76), R16(3.5), R16(1.5), R16(1.41421)};
            d->fmRatio = RATIOS[rnd_below(r, 5)];
        }
        d->fmIndex = AMS(AS_AGE, IX(4.0), IX(0.4), 0.5);
        d->tableA = 12;
        d->levelA = AC(LV(0.25));
        d->filter = 0;
        d->amp = AM(AS_SIZE, LV(0.4), LV(0.9));
        d->att = 2;
        d->dec = rnd_in(r, 2000, 5000);
        d->sus = 0;
        d->rel = 3000;
        d->phaser = (uint8_t)rnd_chance(r, 30);
        d->motion = MO_TAPS;
        d->travel = R16(0.05);
        d->out = IX(0.6);
        break;
    case AK_GOLDEN:    /* inharmonic FM and a stack, both on the golden ratio */
        d->golden = 1;
        d->oscA = 0;
        d->fm = 1;
        d->tableFM = 0;
        d->levelFM = AC(LV(0.7));
        d->fmIndex = AMS(AS_SIZE, IX(0.5), IX(3.0), -0.3);
        d->multi = 1;
        d->tableMulti = 0;
        d->multiCount = (uint8_t)(3 + rnd_below(r, 2));
        d->levelMulti = AM(AS_DIST, LV(0.2), LV(0.5));
        d->filter = 1;
        d->cutoff = AM(AS_Y, HZ(2000), HZ(9000));
        d->q = AC(IX(0.7));
        d->amp = AM(AS_SIZE, LV(0.4), LV(0.85));
        d->att = rnd_in(r, 1500, 4000);
        d->dec = 3000;
        d->sus = LV(0.6);
        d->rel = rnd_in(r, 4000, 7000);
        d->phaser = (uint8_t)rnd_chance(r, 30);
        d->motion = MO_HOLDS;
        d->travel = R16(0.03);
        d->out = IX(0.4);
        break;
    case AK_BREATH:    /* aspirated noise through a moving band, a voice under it */
        d->tableA = rnd_chance(r, 50) ? 7 : 20;
        d->levelA = AM(AS_DIST, LV(0.05), LV(0.3));
        d->noise = 3;
        d->levelNoise = AMS(AS_SIZE, LV(0.2), LV(0.6), -0.4);
        d->noiseCut = AMS(AS_X, HZ(300), rnd_in(r, 2500, 7000), -0.5);
        d->filter = 2;
        d->cutoff = AMS(AS_Y, HZ(250), rnd_in(r, 1800, 4500), -0.5);
        d->q = AM(AS_SPEED, IX(1.5), IX(7.0));
        d->amp = AMS(AS_SIZE, LV(0.3), LV(0.95), 0.4);
        d->att = rnd_in(r, 400, 2500);
        d->dec = 2500;
        d->sus = LV(0.7);
        d->rel = rnd_in(r, 2000, 6000);
        d->ampMod = (uint8_t)rnd_chance(r, 40);
        d->tableAM = 22;
        d->amRate = AM(AS_SPEED, H8(2.0), H8(14.0));
        d->amDepth = AM(AS_LFO1, 0, LV(0.6));
        d->phaser = 1;
        d->phByHand = (uint8_t)rnd_chance(r, 60);
        d->phaserPlace = AM(AS_GX, 0, Q15);
        d->phaserDepth = AC(LV(0.9));
        d->resonator = (uint8_t)rnd_chance(r, 55);
        d->resMix = AM(AS_GY, LV(0.1), LV(0.45));
        d->resFeedback = AC(rnd_in(r, LV(0.6), LV(0.85)));
        d->motion = MO_DRAGS;
        d->travel = R16(0.12);
        d->out = IX(0.5);
        break;
    case AK_GRIT:      /* bruit: a crushed, folded, scattered body */
        d->tableA = (uint8_t)(8 + rnd_below(r, 3));
        d->levelA = AC(LV(0.5));
        d->oscB = 1;
        d->tableB = 21;
        d->intervalB = rnd_chance(r, 50) ? 0 : 6;
        d->detuneB = (int16_t)rnd_in(r, 10 * 16, 40 * 16);
        d->levelB = AM(AS_Y, LV(0.2), LV(0.5));
        d->gran = 1;
        d->tableGran = 19;
        d->granSpread = rnd_in(r, LV(0.4), LV(0.9));
        d->levelGran = AM(AS_SPEED, LV(0.1), LV(0.5));
        d->granRate = AM(AS_X, H8(8.0), H8(60.0));
        d->granLen = AM(AS_Y, MS4(8.0), MS4(50.0));
        d->noise = 4;
        d->levelNoise = AM(AS_SIZE, 0, LV(0.2));
        d->noiseCut = AM(AS_X, HZ(2000), HZ(9000));
        d->filter = 1;
        d->cutoff = AMS(AS_GY, HZ(400), HZ(8000), -0.5);
        d->q = AM(AS_SIZE, IX(1.0), IX(6.0));
        d->amp = AM(AS_SIZE, LV(0.4), LV(0.85));
        d->att = rnd_in(r, 10, 600);
        d->dec = 900;
        d->sus = LV(0.6);
        d->rel = rnd_in(r, 400, 2500);
        d->shaper = 1;
        d->shaperCurve = (uint8_t)(1 + rnd_below(r, 2));
        d->shaperDrive = AM(AS_GX, IX(1.0), IX(4.5));
        d->down = 1;
        d->downRate = AM(AS_LFO1, HZ(2000), HZ(22000));
        d->motion = MO_SWARM;
        d->travel = R16(0.25);
        d->out = IX(0.35);
        break;
    case AK_TIDE:      /* waves of noise breaking over a low drone */
        d->tableA = rnd_chance(r, 50) ? 0 : 17;
        d->levelA = AM(AS_DIST, LV(0.35), LV(0.55));
        d->noise = 2;
        d->levelNoise = AMS(AS_LFO2, LV(0.02), rnd_in(r, LV(0.3), LV(0.5)), 0.5);
        d->noiseCut = AMS(AS_LFO2, HZ(250), rnd_in(r, 1800, 4000), 0.4);
        d->filter = 1;
        d->cutoff = AM(AS_Y, HZ(400), HZ(2500));
        d->q = AC(IX(0.9));
        d->amp = AM(AS_SIZE, LV(0.5), LV(0.9));
        d->att = rnd_in(r, 1500, 4000);
        d->dec = 3000;
        d->sus = LV(0.8);
        d->rel = rnd_in(r, 4000, 8000);
        d->phaser = 1;
        d->phaserRate = AM(AS_LFO2, H16(0.02), H16(0.08));
        d->phaserDepth = AC(LV(0.5));
        d->resonator = 1;
        d->resMix = AM(AS_LFO2, LV(0.1), LV(0.35));
        d->resFeedback = AC(rnd_in(r, LV(0.6), LV(0.8)));
        d->motion = MO_HOLDS;
        d->travel = R16(0.03);
        d->out = IX(0.8);
        break;
    case AK_CHOIR:     /* voices: two vowels and a stack, a formant the hand moves */
        d->tableA = rnd_chance(r, 50) ? 7 : 20;
        d->levelA = AC(LV(0.45));
        d->oscB = 1;
        d->tableB = d->tableA == 7 ? 20 : 7;
        d->intervalB = rnd_chance(r, 60) ? 12 : 7;
        d->detuneB = (int16_t)rnd_in(r, 4 * 16, 14 * 16);
        d->levelB = AM(AS_Y, LV(0.2), LV(0.45));
        d->multi = 1;
        d->tableMulti = d->tableA;
        d->multiCount = 3;
        d->multiStep = rnd_chance(r, 50) ? 0 : 12 * 16;
        d->levelMulti = AM(AS_DIST, LV(0.15), LV(0.35));
        d->filter = 1;
        d->cutoff = AM(AS_Y, HZ(1200), HZ(5000));
        d->q = AC(IX(0.8));
        d->groupFilter = 1;
        d->groupCut = AMS(AS_X, HZ(450), HZ(2600), -0.2);
        d->groupQ = AC(rnd_in(r, IX(1.5), IX(3.5)));
        d->ampMod = 1;
        d->tableAM = 0;
        d->amRate = AM(AS_LFO1, H8(4.0), H8(6.0));
        d->amDepth = AC(LV(0.08));
        d->amp = AM(AS_SIZE, LV(0.45), LV(0.9));
        d->att = rnd_in(r, 600, 2500);
        d->dec = 2000;
        d->sus = LV(0.85);
        d->rel = rnd_in(r, 2500, 5000);
        d->resonator = (uint8_t)rnd_chance(r, 50);
        d->resMix = AM(AS_COUNT, LV(0.05), LV(0.25));
        d->resFeedback = AC(LV(0.6));
        d->motion = MO_SWEEPS;
        d->travel = R16(0.06);
        d->out = IX(0.6);
        break;
    case AK_SHARDS:    /* glass grains scattered off the note, crushed, ringing */
        d->oscA = 0;
        d->gran = 1;
        d->tableGran = rnd_chance(r, 50) ? 6 : 12;
        d->granSpread = rnd_in(r, LV(0.6), Q15);
        d->levelGran = AM(AS_SIZE, LV(0.4), LV(0.8));
        d->granRate = AMS(AS_SPEED, H8(10.0), H8(70.0), -0.3);
        d->granLen = AM(AS_Y, MS4(4.0), MS4(25.0));
        d->filter = 3;
        d->cutoff = AM(AS_X, HZ(300), HZ(2500));
        d->q = AM(AS_Y, IX(0.8), IX(3.0));
        d->down = 1;
        d->downRate = AM(AS_GX, HZ(3000), HZ(20000));
        d->amp = AM(AS_SIZE, LV(0.45), LV(0.9));
        d->att = rnd_in(r, 2, 60);
        d->dec = 800;
        d->sus = LV(0.7);
        d->rel = rnd_in(r, 600, 2500);
        d->resonator = 1;
        d->resMix = AM(AS_GY, LV(0.15), LV(0.5));
        d->resFeedback = AC(rnd_in(r, LV(0.7), LV(0.9)));
        d->motion = MO_TAPS;
        d->travel = R16(0.2);
        d->out = IX(0.7);
        break;
    case AK_BOWL:      /* a singing bowl: an inharmonic hum, beating, in a ringing comb */
        d->tableA = 0;
        d->levelA = AC(LV(0.5));
        d->fm = 1;
        d->tableFM = 0;
        {
            static const int32_t RATIOS[4] = {R16(2.76), R16(5.40), R16(1.41421), R16(3.17)};
            d->fmRatio = RATIOS[rnd_below(r, 4)];
        }
        d->levelFM = AC(LV(0.45));
        d->fmIndex = AMS(AS_SIZE, IX(0.2), IX(1.2), 0.3);
        d->filter = 1;
        d->cutoff = AM(AS_Y, HZ(1500), HZ(7000));
        d->q = AC(IX(0.8));
        d->ampMod = 1;
        d->tableAM = 0;
        d->amRate = AM(AS_LFO2, H8(0.3), H8(1.6));
        d->amDepth = AC(rnd_in(r, LV(0.2), LV(0.4)));
        d->amp = AM(AS_SIZE, LV(0.5), LV(0.9));
        d->att = rnd_in(r, 30, 400);
        d->dec = rnd_in(r, 3000, 7000);
        d->sus = LV(0.55);
        d->rel = rnd_in(r, 4000, 8000);
        d->resonator = 1;
        d->resMix = AM(AS_COUNT, LV(0.2), LV(0.45));
        d->resFeedback = AC(rnd_in(r, LV(0.75), LV(0.9)));
        d->motion = MO_ORBITS;
        d->travel = R16(0.04);
        d->out = IX(0.6);
        break;
    case AK_DUST:      /* sparse crackle over a dark, hollow drone */
        d->tableA = rnd_chance(r, 50) ? 14 : 1;
        d->levelA = AM(AS_DIST, LV(0.3), LV(0.5));
        d->gran = 1;
        d->tableGran = 11;
        d->granSpread = rnd_in(r, LV(0.7), Q15);
        d->levelGran = AM(AS_SPEED, LV(0.2), LV(0.5));
        d->granRate = AM(AS_X, H8(1.5), H8(9.0));
        d->granLen = AM(AS_Y, MS4(3.0), MS4(14.0));
        d->filter = 1;
        d->cutoff = AM(AS_Y, HZ(350), HZ(1600));
        d->q = AC(IX(1.1));
        d->down = (uint8_t)rnd_chance(r, 40);
        d->downRate = AM(AS_LFO1, HZ(6000), HZ(20000));
        d->amp = AM(AS_SIZE, LV(0.45), LV(0.85));
        d->att = rnd_in(r, 400, 2500);
        d->dec = 2500;
        d->sus = LV(0.75);
        d->rel = rnd_in(r, 3000, 6000);
        d->resonator = (uint8_t)rnd_chance(r, 50);
        d->resMix = AM(AS_LFO2, 0, LV(0.3));
        d->resFeedback = AC(LV(0.65));
        d->motion = MO_HOLDS;
        d->travel = R16(0.05);
        d->out = IX(0.7);
        break;
    default:           /* AK_MACHINE: a pulsing, folded mechanism */
        d->tableA = rnd_chance(r, 50) ? 23 : 4;
        d->levelA = AC(LV(0.55));
        d->oscB = 1;
        d->tableB = 21;
        d->intervalB = rnd_chance(r, 50) ? -12 : 7;
        d->detuneB = (int16_t)rnd_in(r, 0, 20 * 16);
        d->levelB = AM(AS_Y, LV(0.1), LV(0.4));
        d->ampMod = 1;
        d->tableAM = rnd_chance(r, 50) ? 3 : 23;
        d->amRate = AM(AS_SPEED, rnd_in(r, H8(3.0), H8(8.0)), rnd_in(r, H8(10.0), H8(22.0)));
        d->amDepth = AC(rnd_in(r, LV(0.7), LV(1.0)));
        d->shaper = 1;
        d->shaperCurve = 1;
        d->shaperDrive = AM(AS_GX, IX(1.5), IX(5.0));
        d->down = (uint8_t)rnd_chance(r, 60);
        d->downRate = AM(AS_LFO1, HZ(2500), HZ(16000));
        d->filter = 1;
        d->cutoff = AMS(AS_X, HZ(500), HZ(7000), -0.4);
        d->q = AM(AS_Y, IX(1.0), IX(5.0));
        d->amp = AM(AS_SIZE, LV(0.45), LV(0.85));
        d->att = rnd_in(r, 5, 80);
        d->dec = 900;
        d->sus = LV(0.7);
        d->rel = rnd_in(r, 300, 1500);
        d->resonator = (uint8_t)rnd_chance(r, 50);
        d->resMix = AM(AS_COUNT, LV(0.1), LV(0.4));
        d->resFeedback = AC(LV(0.7));
        d->motion = MO_TAPS;
        d->travel = R16(0.15);
        d->out = IX(0.45);
        break;
    case AK_SWARM:     /* iterative clouds, a tremolo of grains in a ring */
        d->oscA = 0;
        d->gran = 1;
        d->tableGran = (uint8_t)(rnd_chance(r, 50) ? 12 : 6);
        d->granSpread = rnd_in(r, LV(0.2), LV(0.7));
        d->levelGran = AM(AS_SIZE, LV(0.3), LV(0.8));
        d->granRate = AMS(AS_SPEED, H8(12.0), H8(90.0), -0.3);
        d->granLen = AM(AS_Y, MS4(6.0), MS4(45.0));
        d->multi = 1;
        d->tableMulti = 15;
        d->multiCount = 3;
        d->multiStep = 7 * 16;
        d->levelMulti = AM(AS_DIST, 0, LV(0.3));
        d->filter = 1;
        d->cutoff = AMS(AS_X, HZ(900), HZ(9000), -0.4);
        d->q = AC(IX(1.5));
        d->ampMod = 1;
        d->tableAM = 4;
        d->amRate = AM(AS_Y, H8(6.0), H8(22.0));
        d->amDepth = AM(AS_LFO1, LV(0.3), LV(0.9));
        d->amp = AM(AS_SIZE, LV(0.45), LV(0.9));
        d->att = rnd_in(r, 200, 1500);
        d->dec = 1200;
        d->sus = LV(0.65);
        d->rel = rnd_in(r, 1500, 4000);
        d->resonator = 1;
        d->resMix = AM(AS_COUNT, LV(0.15), LV(0.5));
        d->resFeedback = AC(LV(0.7));
        d->motion = MO_SWARM;
        d->travel = R16(0.2);
        d->out = IX(0.9);
        break;
    }
    an_vary_design(d, r);
    an_route(d, r);
    an_path_sliders(s, path, r);
    an_sliders(d, s);
    d->lfo1 = rnd_in(r, H16(0.1), H16(0.6));
    d->lfo2 = rnd_in(r, H16(0.03), H16(0.12));
    d->lfo1Shape = (uint8_t)rnd_below(r, 3);
    d->lfo2Shape = 2;
    an_path_finish(d, path, r);
    if (gen >= 1u)
        an_care1(d, path, r);
    if (gen >= 3u)
        an_care3(d, path, r);
    /* a struck kind held is a struck kind: the drone mode of Anima holds it, a key does not */
    if (d->att < 1)
        d->att = 1;
    if (d->dec < 30)
        d->dec = 30;
    if (d->rel < 30)
        d->rel = 30;
}

/* the design's level (its out) set from how loud it usually is -- the 75th percentile of 12 ms windows
 * over 1.5 s held on each of two notes (48 and 69), before the slow leveller -- so it starts about where
 * the leveller would take it, whatever the kind and however far the sliders were drawn. Not its loudest
 * moment: a high resonance the hand sweeps across a partial rings now and then, and that is the limiter's
 * (layers.c), not a reason to make the whole design quiet. Main loop, ~3 s of a voice rendered offline. */
static an_voice_t an_probe POOL;
#define AN_PROBE_BLOCKS 2067u    /* 1.5 s */
#define AN_PROBE_WIN 16u         /* blocks a window */
static void an_measure(an_design_t *d)
{
    static int32_t win[2u * AN_PROBE_BLOCKS / AN_PROBE_WIN];
    int32_t out[CTL];
    uint32_t b, k, nwin = 0, p;
    int64_t acc;
    for (p = 0; p < 2u; p++) {
        an_init(&an_probe, 0x9E37u + p);
        an_note_on(&an_probe, d, pitch_inc((p ? 69 : 48) * 16), 1);
        acc = 0;
        for (b = 0; b < AN_PROBE_BLOCKS; b++) {
            if (!an_render(&an_probe, d, out, CTL))
                break;
            acc += an_probe.blkms;
            if ((b % AN_PROBE_WIN) == AN_PROBE_WIN - 1u) {
                if (nwin < NELEM(win))
                    win[nwin++] = (int32_t)((acc >> 4) > 0x7FFFFFFF ? 0x7FFFFFFF : (acc >> 4));   /* / AN_PROBE_WIN */
                acc = 0;
            }
#ifndef HOST
            if ((b & 255u) == 255u)
                fm1_wdt_feed();
#endif
        }
    }
    for (b = 1; b < nwin; b++)                 /* (a few hundred: an insertion sort) */
        for (k = b; k > 0 && win[k - 1] > win[k]; k--) {
            int32_t t = win[k];
            win[k] = win[k - 1];
            win[k - 1] = t;
        }
    {
        uint32_t root = nwin ? isqrt32((uint32_t)win[nwin * 3u / 4u]) : 0;   /* Q12 */
        int32_t g = root ? (int32_t)(1677722u / root) : 4096 * 8;   /* to 0.1 RMS, Q12 */
        g = clampi(g, 300, 4096 * 8);
        d->out = clampi((int32_t)(((int64_t)d->out * g) >> 12), 40, IX(30.0));
    }
}
