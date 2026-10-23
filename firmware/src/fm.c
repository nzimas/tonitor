/* SPDX-License-Identifier: GPL-3.0-only
 * TONITOR: FM, the six-operator DX7-style engine, one voice a layer.
 *
 * The synthesis is msfa (Dexed's core, Apache-2.0) as Felucca ported it to integer C (fm6_core.c): 32
 * algorithms, six operators with four-rate / four-level envelopes, keyboard scaling, ratio or fixed
 * frequencies, feedback, the LFO and the pitch envelope, in the 155-byte single-voice layout. This file is
 * the layer around it and the patch generator.
 *
 * The generator does not make instruments. It draws a patch from one of nine families of sound objects
 * after the French and German studios -- the GRM's objets sonores and Risset's and Chowning's FM at IRCAM
 * and Stanford -- and then lets chance bend it:
 *   METAL     Risset's bells and gongs: many carriers on inharmonic ratios, the high partials dying first
 *   DRONE     a spectral drone: slow pairs, near-harmonic, detuned to beat, an index that wanders for minutes
 *   BRUIT     noise from feedback and high ratios, scattered by a sample-and-hold LFO (Schaeffer's bruits)
 *   IMPACT    a struck body, its pitch falling, the modulators dying before the carriers
 *   ITERATIF  Schaeffer's iterative objects: a sound pulsed by its own LFO, square or random
 *   GLISSE    Xenakis's and Henry's glissandi: a pitch envelope that travels octaves, slowly
 *   SPECTRE   Risset's arpeggio: rich tones a few cents apart whose beating lifts one harmonic after another
 *   ABYSS     sub-bass rumble, folded by feedback, breathing slowly
 *   FRICTION  Henry's door: creaks from sub-audio fixed-frequency modulators and random pitch
 * Generation 1 (0.21) adds eight more:
 *   CHOIR     vowels: carriers on the harmonics nearest two formants, each turned by the fundamental, a vibrato
 *             that comes in late
 *   BOWED     a string bowed: a saw from feedback, the bow's pressure (the index) rising with the swell
 *   HOLLOW    odd harmonics only (carriers turned by a modulator at twice their frequency): a dark pipe
 *   BLOOM     a tone that opens over seconds and half closes again
 *   TIDE      three stacks crossfading, one fading as the next swells, under a slow tide of level
 *   SHARDS    a tone broken by a fast random step of pitch and chopped by a square
 *   STATIC    ring-modulated bands from modulators nailed to audio frequencies
 *   WIND      breath and gusts from a feedback operator over a soft drone
 * and shapes every patch with more care: modulators on high ratios turned down (their sidebands alias and
 * scream), the brightness scaled down the keyboard so high notes do not turn shrill nor low ones mud, rate
 * scaling for the carriers, and mutations that bend a patch without breaking it. new_fm (layers.c) then draws
 * several and keeps the one whose measured brightness suits the path.
 *
 * About one patch in three is then mutated once more (a fixed partial, an envelope turned over, the
 * feedback pushed, the LFO taken too far, an octave away), so no family is ever quite itself.
 *
 * A layer's path leans the draw (PATH_OPEN: as above, every family alike):
 *   AMBIENT       the drones, Risset's arpeggios, the depths and the bells; then slower, darker and longer:
 *                 soft attacks, long tails, the modulators down, the feedback held, slow LFOs and no sirens,
 *                 the glissandi narrowed, often an octave down; mutated seldom, and only gently
 *   EXPERIMENTAL  bruits, frictions, impacts, iterations and glissandi; the modulators up, mutated more
 *                 often than not, sometimes twice
 *
 * Every patch is measured as it is made (fm_measure: rendered offline in the main loop for two and a half
 * seconds) and given the gain that brings its loudest moment to one level: the families differ by tens of
 * decibels otherwise. */
#include "fm6_core.c"

enum { FF_METAL, FF_DRONE, FF_BRUIT, FF_IMPACT, FF_ITERATIF, FF_GLISSE, FF_SPECTRE, FF_ABYSS, FF_FRICTION,
       FF_N0,                    /* (generation 0 draws these nine only) */
       FF_CHOIR = FF_N0, FF_BOWED, FF_HOLLOW, FF_BLOOM, FF_TIDE, FF_SHARDS, FF_STATIC, FF_WIND,
       FF_N1,                    /* (generations 1 and 2 draw these seventeen) */
       FF_PLUCK = FF_N1, FF_MALLET, FF_WARP, FF_GLITCH, FF_RUBBER,
       FF_N5,                    /* (generations 3 to 5 draw these twenty-two) */
       FF_SHAPER = FF_N5, FF_LADDER, FF_REVERSE, FF_CREEP, FF_CLANG, FF_N };
/* the families' weights on each path: generation 0 (the open path: rnd_below, all alike), generation 1 */
static const uint8_t FF_W[3][FF_N0] = {
    {1, 1, 1, 1, 1, 1, 1, 1, 1},
    {3, 4, 0, 0, 1, 1, 4, 3, 0},
    {2, 1, 4, 3, 3, 2, 0, 1, 4},
};
static const uint8_t FF_W1[3][FF_N1] = {
    {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
    {3, 4, 0, 0, 1, 1, 4, 3, 0, 4, 4, 4, 3, 4, 0, 0, 2},
    {2, 1, 4, 3, 3, 2, 0, 1, 4, 0, 0, 1, 2, 0, 4, 4, 3},
};
static const uint8_t FF_W3[3][FF_N] = {             /* generation 3: plucks, mallets; warps, glitches, rubber */
    {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4, 3, 3, 3, 3},
    {3, 4, 0, 0, 1, 1, 4, 3, 0, 4, 4, 4, 3, 4, 0, 0, 2, 3, 4, 0, 0, 0},
    {2, 1, 4, 3, 3, 2, 0, 1, 4, 0, 0, 1, 2, 0, 4, 4, 3, 2, 2, 5, 5, 4},
};
static const uint8_t FF_W5[3][FF_N] = {             /* generation 5: GLISSE and RUBBER drawn no more -- WARP has them */
    {2, 2, 2, 2, 2, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4, 3, 5, 3, 0},
    {3, 4, 0, 0, 1, 0, 4, 3, 0, 4, 4, 4, 3, 4, 0, 0, 2, 3, 4, 1, 0, 0},
    {2, 1, 4, 3, 3, 0, 0, 1, 4, 0, 0, 1, 2, 0, 4, 4, 3, 2, 2, 8, 5, 0},
};
static const uint8_t FF_W6[3][FF_N] = {             /* generation 6: the five learned from the DX7 collection (fm_dx7.c) */
    {2, 2, 2, 2, 2, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4, 3, 5, 3, 0, 3, 3, 3, 3, 3},
    {3, 4, 0, 0, 1, 0, 4, 3, 0, 4, 4, 4, 3, 4, 0, 0, 2, 3, 4, 1, 0, 0, 3, 4, 4, 4, 2},
    {2, 1, 4, 3, 3, 0, 0, 1, 4, 0, 0, 1, 2, 0, 4, 4, 3, 2, 2, 8, 5, 0, 4, 2, 2, 2, 4},
};
static int fm_retired(int32_t f) { return f == FF_GLISSE || f == FF_RUBBER; }   /* (from generation 5: into WARP) */
static const char *const FF_NAME[FF_N] = {"METAL", "DRONE", "BRUIT", "IMPACT", "ITERATIF", "GLISSE", "SPECTRE",
                                          "ABYSS", "FRICTION", "CHOIR", "BOWED", "HOLLOW", "BLOOM", "TIDE",
                                          "SHARDS", "STATIC", "WIND", "PLUCK", "MALLET", "WARP", "GLITCH", "RUBBER",
                                          "SHAPER", "LADDER", "REVERSE", "CREEP", "CLANG"};

typedef struct {
    uint8_t p[FP_SIZE + 1u];
    uint8_t family;
    uint8_t path;                /* PATH_*: what it was drawn on */
    uint8_t gen;                 /* the generators' generation that drew it */
    int32_t gain;                /* Q12: the measured level brought to the target */
    uint32_t seed;               /* what made it: fm_generate(-1, seed, path, gen) + fm_measure gives it again */
} fm_patch_t;

typedef struct {
    fm6_note_t n;
    fm6_lfo_t lfo;
    int32_t note;                /* MIDI note, transposed */
    int32_t lp;                  /* SATURATION below the middle: its low-pass */
    uint32_t tph;                /* MOTION above the middle: its tremolo's phase */
    uint8_t live;
} fm_voice_t;

#define FM_VEL 100

/* every byte of the 155-byte voice inside its range (from SLOOP's eng_fm6.c, Leo Kuroshita, GPL-3.0) */
static uint32_t fm6_max(uint32_t i)
{
    static const uint8_t OPMAX[21] = {99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 3, 3, 7, 3, 7, 99, 1, 31, 99, 14};
    static const uint8_t VMAX[19] = {99, 99, 99, 99, 99, 99, 99, 99, 31, 7, 1, 99, 99, 99, 99, 1, 5, 7, 48};
    if (i < 126u)
        return OPMAX[i % 21u];
    if (i < FP_NAME)
        return VMAX[i - 126u];
    return 126u;
}
static void fm6_sanitize(uint8_t *v)
{
    uint32_t i;
    for (i = 0; i < FP_SIZE; i++) {
        if (i >= FP_NAME)
            v[i] = v[i] < 32u || v[i] > 126u ? ' ' : v[i];
        else if (v[i] > fm6_max(i))
            v[i] = (uint8_t)fm6_max(i);
    }
    v[FP_SIZE] = 0;
}

/* ------------------------------------------------------------ voice --- */
static void fm_note_on(fm_voice_t *v, const fm_patch_t *pt, int32_t note, int fresh)
{
    const uint8_t *p = pt->p;
    if (!v->live)
        fresh = 1;
    v->note = note + p[FP_TRNSP] - 24;
    if (fresh)
        fm6_lfo_reset(&v->lfo, p);
    fm6_note_init(&v->n, p, v->note, FM_VEL, fresh);
    fm6_lfo_key(&v->lfo);
    v->live = 1;
}
/* the carriers to full and held there: the track's own envelope (layers.c) shapes the voice instead */
static void fm_hold(fm_voice_t *v, const fm_patch_t *pt)
{
    uint32_t car = fm6_carriers(pt->p[FP_ALG] & 31u), k;
    for (k = 0; k < 6u; k++)
        if ((car >> k) & 1u) {
            v->n.env[k].hold = 1;
            fm6_env_advance_fp(&v->n.env[k], pt->p + k * FP_OP, v->n.env[k].ix);
        }
}
/* the patch's own envelope as A D S R (the ENVELOPE page, as drawn): its loudest carrier at middle C, timed --
 * a[0] the first stage, a[1] the second and third, a[2] the third's level (0 .. 100 % of the highest), a[3] the
 * release to 60 dB below it; ms */
static void fm_env_adsr(const fm_patch_t *pt, int32_t *a)
{
    const uint8_t *p = pt->p, *op;
    uint32_t car = fm6_carriers(p[FP_ALG] & 31u), k, best = 0, n;
    int32_t bl = -1, note = 60 + p[FP_TRNSP] - 24, ol, peak = 0, lv;
    fm6_env_t e;
    for (k = 0; k < 6u; k++)
        if ((car >> k) & 1u && (int32_t)p[k * FP_OP + FP_OL] > bl) {
            bl = p[k * FP_OP + FP_OL];
            best = k;
        }
    op = p + best * FP_OP;
    ol = fm6_scaleout(op[FP_OL]) + fm6_scale_level(note, op);
    ol = ((ol > 127 ? 127 : ol) << 5) + fm6_scale_vel(FM_VEL, op[FP_KVS]);
    fm6_env_init(&e, op, ol < 0 ? 0 : ol, fm6_scale_rate(note, op[FP_RS]));
    for (n = 0; e.ix == 0u && n < 14000u; n++)   /* (blocks: 0.7256 ms each) */
        peak = maxi(peak, fm6_env_get_fp(&e, op));
    a[0] = (int32_t)(n * 7256u / 10000u);
    for (n = 0; e.ix < 3u && n < 14000u; n++)
        peak = maxi(peak, fm6_env_get_fp(&e, op));
    a[1] = (int32_t)(n * 7256u / 10000u);
    peak = maxi(peak, e.level);
    a[2] = (int32_t)((100u * exp2_q16(-((peak - e.level) >> 8))) >> 16);
    fm6_env_key(&e, op, 0);
    for (n = 0; e.ix < 4u && n < 28000u; n++) {
        lv = fm6_env_get_fp(&e, op);
        if (lv <= peak - (10 << 24))
            break;
    }
    a[3] = (int32_t)(n * 7256u / 10000u);
}
static void fm_note_off(fm_voice_t *v, const fm_patch_t *pt)
{
    if (v->live && v->n.down)
        fm6_note_key(&v->n, pt->p, 0);
}

static uint32_t fm_carriers_of(const uint8_t *p);
/* EDIT's macros on FM, live under a held note: BRIGHTNESS (the modulators' level -- the index -- about 27 dB either
 * way, the feedback up with it, and the carriers tilted, the higher ones up: so a patch of carriers only brightens
 * too), DETUNE (the carriers but the lowest pulled apart, +-40 cents one way and the other: beating, chorus; and
 * every modulator +-25 cents off its carrier: the partials leave their harmonic places, so even a patch of one
 * carrier beats and roughens) or INHARMONIC for the metallic and noisy families (every operator but the lowest
 * carrier off its ratio: clangour), SATURATION (the feedback, and above the middle a saturation; below it, a
 * low-pass: smoother), MOTION (the LFO's depths, and above the middle a tremolo of its own, quicker and deeper as it
 * turns). Each felt early: the square root of the knob's turn (a quarter of the way: half the change). 0
 * everywhere: the patch as drawn, bit for bit */
AINL int fm_skews(uint32_t f)
{
    return f == FF_METAL || f == FF_BRUIT || f == FF_IMPACT || f == FF_ITERATIF || f == FF_GLISSE || f == FF_FRICTION ||
           f == FF_SHARDS || f == FF_STATIC || f == FF_WIND || f == FF_WARP || f == FF_GLITCH || f == FF_MALLET ||
           f == FF_CLANG;
}
static const char *fm_mac_name(const fm_patch_t *pt, uint32_t j)
{
    static const char *const N[4] = {"BRIGHTNESS", "DETUNE", "SATURATION", "MOTION"};
    return j == 1u && fm_skews(pt->family) ? "INHARMONIC" : N[j & 3u];
}
/* an operator's frequency against the note's, x 100 (fixed ones: against middle C) */
AINL int32_t fm_op_ratio100(const uint8_t *op)
{
    if (!op[FP_MODE])
        return op[FP_FC] ? op[FP_FC] * (100 + op[FP_FF]) : (100 + op[FP_FF]) / 2;
    return (int32_t)((uint32_t)(exp2_q16((op[FP_FC] & 3) * 217706 + (int32_t)(op[FP_FF] % 100u) * 2177) >> 16) *
                     100u / 262u);                       /* 10^x = 2^(3.3219 x), Q16 */
}
/* the macros as the core takes them: a pitch and a level offset an operator, the feedback, the LFO's depths */
static FAST void fm_macros(const fm_patch_t *pt, const int8_t *mac, int32_t *dt, int32_t *lvl, int32_t *fb, uint32_t *lfoq8)
{
    const uint8_t *p = pt->p;
    uint32_t car = fm_carriers_of(p), k, lo = 6, ci = 0, mi = 0;
    int32_t r[6], rmin = 0x7FFFFFFF, rmax = 0;
    for (k = 0; k < 6u; k++) {                           /* (k: the sixth operator first, as the patch lays them) */
        r[k] = fm_op_ratio100(p + k * FP_OP);
        if ((car >> k) & 1u) {
            if (r[k] < rmin)
                rmin = r[k], lo = k;
            if (r[k] > rmax)
                rmax = r[k];
        }
    }
    for (k = 0; k < 6u; k++) {
        int carrier = (int)((car >> k) & 1u);
        /* BRIGHTNESS: +-36 output-level steps (about +-27 dB) on the modulators; the carriers tilted by +-16 */
        if (!carrier)
            lvl[k] = mac[0] * 1179648;
        else if (rmax > rmin)
            lvl[k] = mac[0] * ((r[k] - rmin) * 128 / (rmax - rmin) - 64) * 8192;
        if (fm_skews(pt->family)) {                      /* INHARMONIC: up to 1 .. 2 semitones off, the lowest carrier kept */
            if (k != lo)
                dt[k] = mac[1] * 21845 * (int32_t)(1u + (k & 1u));
        } else if (carrier && k != lo)                   /* DETUNE: the carriers +-40 cents, one way and the other */
            dt[k] = mac[1] * 8738 * ((ci++ & 1u) ? -1 : 1);
        else if (!carrier)                               /* .. the modulators +-25 cents: the partials off their places */
            dt[k] = mac[1] * 5461 * ((mi++ & 1u) ? -1 : 1);
    }
    *fb = clampi(*fb + (mac[2] * 8 + (mac[2] < 0 ? -32 : 32)) / 64 + (mac[0] > 0 ? mac[0] / 32 : 0), 0, 7);   /* (and with BRIGHTNESS up) */
    *lfoq8 = (uint32_t)clampi(256 + mac[3] * 4, 0, 512);
}
static __typeof__(&fm_macros) volatile fm_macros_fp = fm_macros;   /* (the flash's way into the RAM code) */

/* one block (CTL samples), Q15 into out; 0 when the voice is silent for good. mac: EDIT's macros (0: none) */
static FAST int fm_render(fm_voice_t *v, const fm_patch_t *pt, int32_t *out, const int8_t *mac)
{
    const uint8_t *p = pt->p;
    int32_t bus[FM6_N], lv, ld, lf, dt[6] = {0, 0, 0, 0, 0, 0}, lvl[6] = {0, 0, 0, 0, 0, 0}, fb = p[FP_FB] & 7u;
    int32_t trem = Q15, grit = 0;
    uint32_t i, lfoq8 = 256;
    if (!v->live)
        return 0;
    if (!v->n.down && fm6_note_done(&v->n, p, p[FP_ALG] & 31u)) {
        v->live = 0;
        return 0;
    }
    lv = fm6_lfo_sample(&v->lfo);
    ld = fm6_lfo_delay(&v->lfo);
    lf = fm6_note_logfreq(v->note * 16, 0);
    if (mac && (mac[0] | mac[1] | mac[2] | mac[3])) {
        int8_t felt[NMAC];
        for (i = 0; i < NMAC; i++) {                     /* (the knob's turn, felt early: its square root) */
            int32_t m = mac[i], a = m < 0 ? -m : m, q = (int32_t)isqrt32((uint32_t)(a * MAC_MAX));
            felt[i] = (int8_t)(m < 0 ? -q : q);
        }
        mac = felt;
        fm_macros(pt, mac, dt, lvl, &fb, &lfoq8);
        if (mac[3] > 0) {                                /* a tremolo of its own, 1.5 .. 6 Hz, deeper as it turns */
            v->tph += 4380000u + (uint32_t)mac[3] * 205000u;   /* (a block's step: Hz x 2^32 x 32 / FS) */
            trem = Q15 - mulq15(mac[3] * (Q15 * 7 / 10 / MAC_MAX), (sin_ph(v->tph) + Q15) >> 1);
        }
        grit = mac[2];
    }
    if (!fm6_note_compute(&v->n, p, bus, lv, ld, lf, p[FP_ALG] & 31u, fb, dt, lvl, lfoq8)) {
        for (i = 0; i < FM6_N; i++)
            out[i] = 0;
        return 1;                                   /* below the threshold now, but not done */
    }
    for (i = 0; i < FM6_N; i++) {
        int32_t x = bus[i] - v->n.dc;              /* Q24 */
        v->n.dc += x >> 10;                        /* a one-pole high-pass, ~7 Hz: FM puts sidebands at 0 Hz */
        out[i] = (int32_t)(((int64_t)x * pt->gain) >> 21);   /* Q24 x Q12 -> Q15 */
        if (grit > 0)                              /* SATURATION: saturated, about as loud */
            out[i] = mulq15(tanh_q15(out[i] + mulq15(out[i], grit * (6 * Q15 / MAC_MAX))),
                            Q15 - grit * (Q15 / 2 / MAC_MAX));
        else if (grit < 0) {                       /* .. or smoothed: a low-pass from ~8 kHz down to ~300 Hz */
            v->lp += mulq15(out[i] - v->lp, Q15 + grit * (Q15 * 96 / 100 / MAC_MAX));
            out[i] = v->lp;
        }
        if (trem != Q15)
            out[i] = mulq15(out[i], trem);
    }
    return 1;
}
static __typeof__(&fm_render) volatile fm_render_fp = fm_render;   /* (the flash's way into the RAM code) */

/* ---------------------------------------------------------- the patch --- */
/* operator n (1..6, as the DX7 numbers them) of patch p */
static uint8_t *fm_op(uint8_t *p, int n) { return p + (6 - n) * FP_OP; }

static void fm_env(uint8_t *op, int r1, int r2, int r3, int r4, int l1, int l2, int l3, int l4)
{
    op[FP_R1] = (uint8_t)clampi(r1, 0, 99);
    op[FP_R1 + 1] = (uint8_t)clampi(r2, 0, 99);
    op[FP_R1 + 2] = (uint8_t)clampi(r3, 0, 99);
    op[FP_R1 + 3] = (uint8_t)clampi(r4, 0, 99);
    op[FP_L1] = (uint8_t)clampi(l1, 0, 99);
    op[FP_L1 + 1] = (uint8_t)clampi(l2, 0, 99);
    op[FP_L1 + 2] = (uint8_t)clampi(l3, 0, 99);
    op[FP_L1 + 3] = (uint8_t)clampi(l4, 0, 99);
}
/* a ratio as the DX7 has it: coarse (0 = 0.5, 1..31) x (1 + fine / 100); x in hundredths. Generations 0 and 1
 * worked the fine out 100 times too large, so nearly every ratio became (its coarse + 1) x 1.99 -- 1.00 and 1.41
 * both 3.98: the patches two octaves up and most of their ratios lost. Kept so for them (their sounds are drawn
 * again exactly); generation 2 has the ratios as they were meant */
static uint32_t fm_gen_cur;      /* the generation fm_generate draws on now */
static void fm_ratio(uint8_t *op, int32_t x100)
{
    int32_t c, f, k = fm_gen_cur >= 2u ? 1 : 100;
    op[FP_MODE] = 0;
    if (x100 < 100) {
        op[FP_FC] = 0;
        op[FP_FF] = (uint8_t)clampi((x100 - 50) * 2, 0, 99);
        return;
    }
    c = clampi(x100 / 100, 1, 31);
    f = (x100 * k / c) - 100;                       /* the fine that makes c x (1 + f / 100) = x */
    if (f > 99) {
        c = clampi(c + 1, 1, 31);
        f = (x100 * k / c) - 100;
    }
    op[FP_FC] = (uint8_t)c;
    op[FP_FF] = (uint8_t)clampi(f, 0, 99);
}
/* a fixed frequency: 10^decade x 10^(fine / 100) Hz (decade 0..3: 1, 10, 100, 1000 Hz) */
static void fm_fixed(uint8_t *op, int decade, int fine)
{
    op[FP_MODE] = 1;
    op[FP_FC] = (uint8_t)clampi(decade, 0, 3);
    op[FP_FF] = (uint8_t)clampi(fine, 0, 99);
}

AINL uint32_t fm_carriers_of(const uint8_t *p) { return fm6_carriers(p[FP_ALG] & 31u); }
static int fm_is_carrier(const uint8_t *p, int n) { return (fm_carriers_of(p) >> (6 - n)) & 1u; }
static int fm_ncarriers(const uint8_t *p)
{
    int n, c = 0;
    for (n = 1; n <= 6; n++)
        c += fm_is_carrier(p, n);
    return c;
}

static void fm_name(uint8_t *p, const char *s)
{
    int i;
    for (i = 0; i < 10; i++)
        p[FP_NAME + i] = *s ? (uint8_t)*s++ : ' ';
}

/* the voice's defaults: no pitch envelope, a quiet LFO, no transpose */
static void fm_voice_defaults(uint8_t *p)
{
    int i;
    memset(p, 0, FP_SIZE + 1u);
    for (i = 0; i < 4; i++) {
        p[FP_PR1 + i] = 99;
        p[FP_PL1 + i] = 50;
    }
    p[FP_OKS] = 1;
    p[FP_LFS] = 35;
    p[FP_LFW] = 4;
    p[FP_LPMS] = 3;
    p[FP_TRNSP] = 24;
    for (i = 1; i <= 6; i++) {
        uint8_t *op = fm_op(p, i);
        fm_env(op, 99, 99, 99, 99, 99, 99, 99, 0);
        op[FP_BP] = 39;
        op[FP_DET] = 7;
        op[FP_FC] = 1;
        op[FP_OL] = 0;
    }
}

/* algorithm sets (0-based: the DX7's number - 1) */
static const uint8_t ALG_MANY[6] = {31, 30, 24, 23, 21, 28};     /* mostly carriers: additive, bells */
static const uint8_t ALG_PAIRS[6] = {4, 5, 21, 20, 18, 0};       /* pairs and stacks */
static const uint8_t ALG_DEEP[6] = {0, 1, 2, 3, 6, 15};          /* long modulator chains */

static int32_t inharm_ratio(rnd_t *r)              /* hundredths: the ratios of bells, plates and gongs */
{
    static const int16_t R[14] = {141, 173, 200, 224, 276, 300, 317, 350, 411, 541, 593, 713, 864, 1100};
    int32_t x = R[rnd_below(r, 14)];
    return x + rnd_in(r, -8, 8);
}

static void fm_gen_metal(uint8_t *p, rnd_t *r)
{
    int n, k = 0;
    p[FP_ALG] = ALG_MANY[rnd_below(r, 6)];
    p[FP_FB] = (uint8_t)rnd_below(r, 4);
    for (n = 1; n <= 6; n++) {
        uint8_t *op = fm_op(p, n);
        if (fm_is_carrier(p, n)) {
            int32_t ratio = k == 0 ? 100 : inharm_ratio(r) * (rnd_chance(r, 25) ? 2 : 1);
            int32_t dec = 30 + rnd_below(r, 25) - (ratio > 400 ? 12 : 0) - k * 3;   /* high partials die first */
            fm_ratio(op, ratio);
            fm_env(op, 99, dec, dec - rnd_below(r, 15), 20 + rnd_below(r, 30), 99, 70 + rnd_below(r, 25), 0, 0);
            op[FP_OL] = (uint8_t)(k == 0 ? 99 : 80 + rnd_below(r, 18));
            op[FP_RS] = (uint8_t)(2 + rnd_below(r, 5));
            op[FP_DET] = (uint8_t)(4 + rnd_below(r, 7));
            k++;
        } else {
            fm_ratio(op, inharm_ratio(r));
            fm_env(op, 99, 40 + rnd_below(r, 30), 30 + rnd_below(r, 30), 40, 99, 60, 0, 0);
            op[FP_OL] = (uint8_t)(55 + rnd_below(r, 30));
        }
    }
    p[FP_LFS] = (uint8_t)rnd_in(r, 20, 45);
    p[FP_LAMD] = (uint8_t)rnd_below(r, 15);
    p[FP_LPMD] = (uint8_t)rnd_below(r, 6);
}

static void fm_gen_drone(uint8_t *p, rnd_t *r)
{
    static const int16_t NEAR[8] = {50, 100, 100, 200, 300, 100, 400, 150};
    int n;
    p[FP_ALG] = ALG_PAIRS[rnd_below(r, 6)];
    p[FP_FB] = (uint8_t)(2 + rnd_below(r, 5));
    for (n = 1; n <= 6; n++) {
        uint8_t *op = fm_op(p, n);
        int32_t ratio = NEAR[rnd_below(r, 8)] + rnd_below(r, 4);    /* near-harmonic, a little off */
        fm_ratio(op, ratio);
        op[FP_DET] = (uint8_t)rnd_below(r, 15);                    /* detuned: the pairs beat */
        if (fm_is_carrier(p, n)) {
            fm_env(op, 10 + rnd_below(r, 25), 20 + rnd_below(r, 20), 15, 12 + rnd_below(r, 20), 95, 99,
                   85 + rnd_below(r, 14), 0);
            op[FP_OL] = (uint8_t)(88 + rnd_below(r, 11));
            op[FP_AMS] = (uint8_t)rnd_below(r, 2);
        } else {
            /* the index wanders: up slowly, somewhere else even more slowly, held there */
            fm_env(op, 5 + rnd_below(r, 25), 3 + rnd_below(r, 15), 2 + rnd_below(r, 12), 20, 50 + rnd_below(r, 50),
                   rnd_below(r, 99), 40 + rnd_below(r, 59), 0);
            op[FP_OL] = (uint8_t)(60 + rnd_below(r, 32));
        }
    }
    p[FP_LFS] = (uint8_t)rnd_in(r, 2, 18);
    p[FP_LFW] = (uint8_t)(rnd_chance(r, 50) ? 0 : 4);
    p[FP_LPMD] = (uint8_t)rnd_in(r, 2, 12);
    p[FP_LAMD] = (uint8_t)rnd_in(r, 0, 25);
    p[FP_LPMS] = (uint8_t)rnd_in(r, 2, 4);
}

static void fm_gen_bruit(uint8_t *p, rnd_t *r)
{
    int n;
    static const uint8_t FBALG[6] = {31, 5, 3, 16, 0, 7};    /* algorithms whose feedback operator matters */
    p[FP_ALG] = FBALG[rnd_below(r, 6)];
    p[FP_FB] = (uint8_t)(6 + rnd_below(r, 2));
    for (n = 1; n <= 6; n++) {
        uint8_t *op = fm_op(p, n);
        if (rnd_chance(r, 45))
            fm_fixed(op, 2 + rnd_below(r, 2), rnd_below(r, 100));
        else
            fm_ratio(op, rnd_in(r, 300, 2500));
        if (fm_is_carrier(p, n)) {
            int sust = rnd_chance(r, 60);
            fm_env(op, rnd_in(r, 40, 99), rnd_in(r, 20, 60), rnd_in(r, 15, 50), rnd_in(r, 25, 60), 99,
                   rnd_in(r, 60, 95), sust ? rnd_in(r, 60, 90) : 0, 0);
            op[FP_OL] = (uint8_t)rnd_in(r, 85, 99);
        } else {
            fm_env(op, rnd_in(r, 30, 99), rnd_in(r, 10, 50), rnd_in(r, 10, 50), 40, 99, rnd_in(r, 70, 99),
                   rnd_in(r, 70, 99), 0);
            op[FP_OL] = (uint8_t)rnd_in(r, 82, 99);
        }
    }
    p[FP_LFW] = 5;                                  /* sample and hold */
    p[FP_LFS] = (uint8_t)rnd_in(r, 45, 92);
    p[FP_LPMD] = (uint8_t)rnd_in(r, 15, 70);
    p[FP_LPMS] = (uint8_t)rnd_in(r, 4, 7);
    p[FP_LAMD] = (uint8_t)rnd_in(r, 0, 40);
}

static void fm_gen_impact(uint8_t *p, rnd_t *r)
{
    int n;
    p[FP_ALG] = rnd_chance(r, 50) ? ALG_DEEP[rnd_below(r, 6)] : ALG_PAIRS[rnd_below(r, 6)];
    p[FP_FB] = (uint8_t)rnd_below(r, 7);
    for (n = 1; n <= 6; n++) {
        uint8_t *op = fm_op(p, n);
        if (fm_is_carrier(p, n)) {
            fm_ratio(op, rnd_chance(r, 70) ? 100 : inharm_ratio(r));
            fm_env(op, 99, rnd_in(r, 35, 70), rnd_in(r, 20, 50), rnd_in(r, 30, 60), 99, rnd_in(r, 30, 70), 0, 0);
            op[FP_OL] = (uint8_t)rnd_in(r, 90, 99);
        } else {
            if (rnd_chance(r, 40))
                fm_fixed(op, rnd_in(r, 1, 3), rnd_below(r, 100));   /* a thud, a knock, a ping */
            else
                fm_ratio(op, rnd_chance(r, 50) ? inharm_ratio(r) : rnd_in(r, 50, 900));
            fm_env(op, 99, rnd_in(r, 55, 90), rnd_in(r, 40, 70), 50, 99, rnd_in(r, 10, 50), 0, 0);   /* dies first */
            op[FP_OL] = (uint8_t)rnd_in(r, 70, 99);
        }
        op[FP_RS] = (uint8_t)rnd_below(r, 5);
    }
    /* the pitch falls from up there to the note */
    p[FP_PL1 + 3] = (uint8_t)rnd_in(r, 62, 99);
    p[FP_PL1] = p[FP_PL1 + 1] = p[FP_PL1 + 2] = 50;
    p[FP_PR1] = (uint8_t)rnd_in(r, 55, 92);
    p[FP_PR1 + 1] = p[FP_PR1 + 2] = 99;
    p[FP_LAMD] = 0;
    p[FP_LPMD] = 0;
}

static void fm_gen_iteratif(uint8_t *p, rnd_t *r)
{
    int n;
    p[FP_ALG] = rnd_chance(r, 50) ? ALG_MANY[rnd_below(r, 6)] : ALG_PAIRS[rnd_below(r, 6)];
    p[FP_FB] = (uint8_t)rnd_below(r, 6);
    for (n = 1; n <= 6; n++) {
        uint8_t *op = fm_op(p, n);
        fm_ratio(op, rnd_chance(r, 50) ? inharm_ratio(r) : 100 * (1 + rnd_below(r, 5)));
        if (fm_is_carrier(p, n)) {
            fm_env(op, rnd_in(r, 50, 99), rnd_in(r, 15, 40), rnd_in(r, 10, 30), rnd_in(r, 25, 50), 99, 90,
                   rnd_in(r, 60, 95), 0);
            op[FP_OL] = (uint8_t)rnd_in(r, 85, 99);
            op[FP_AMS] = 3;                              /* the LFO pulses it */
        } else {
            fm_env(op, rnd_in(r, 40, 99), rnd_in(r, 10, 40), rnd_in(r, 10, 40), 40, 99, rnd_in(r, 50, 99),
                   rnd_in(r, 40, 90), 0);
            op[FP_OL] = (uint8_t)rnd_in(r, 50, 88);
            op[FP_AMS] = (uint8_t)rnd_below(r, 4);
        }
    }
    {
        static const uint8_t W[4] = {3, 5, 1, 3};       /* square, sample and hold, saw down */
        p[FP_LFW] = W[rnd_below(r, 4)];
    }
    p[FP_LFS] = (uint8_t)rnd_in(r, 20, 72);              /* 1.5 .. 13 Hz */
    p[FP_LAMD] = (uint8_t)rnd_in(r, 55, 99);
    p[FP_LPMD] = (uint8_t)(p[FP_LFW] == 5 ? rnd_in(r, 0, 25) : 0);
    p[FP_LPMS] = (uint8_t)rnd_in(r, 2, 5);
    p[FP_LKS] = 1;
}

static void fm_gen_glisse(uint8_t *p, rnd_t *r)
{
    int n;
    p[FP_ALG] = ALG_PAIRS[rnd_below(r, 6)];
    p[FP_FB] = (uint8_t)rnd_below(r, 6);
    for (n = 1; n <= 6; n++) {
        uint8_t *op = fm_op(p, n);
        fm_ratio(op, 100 * (1 + rnd_below(r, 4)) + rnd_below(r, 3));
        if (fm_is_carrier(p, n)) {
            fm_env(op, rnd_in(r, 20, 60), 30, 20, rnd_in(r, 15, 35), 99, 95, 90, 0);
            op[FP_OL] = (uint8_t)rnd_in(r, 88, 99);
        } else {
            fm_env(op, rnd_in(r, 15, 60), rnd_in(r, 10, 30), rnd_in(r, 10, 30), 30, 99, rnd_in(r, 50, 99),
                   rnd_in(r, 40, 85), 0);
            op[FP_OL] = (uint8_t)rnd_in(r, 55, 85);
        }
    }
    /* the pitch envelope travels: from far below or above, past the note, somewhere else, slowly */
    p[FP_PL1 + 3] = (uint8_t)(rnd_chance(r, 50) ? rnd_in(r, 0, 30) : rnd_in(r, 70, 99));
    p[FP_PL1] = (uint8_t)rnd_in(r, 20, 80);
    p[FP_PL1 + 1] = (uint8_t)rnd_in(r, 0, 99);
    p[FP_PL1 + 2] = (uint8_t)(rnd_chance(r, 50) ? 50 : rnd_in(r, 30, 70));
    p[FP_PR1] = (uint8_t)rnd_in(r, 15, 55);
    p[FP_PR1 + 1] = (uint8_t)rnd_in(r, 8, 40);
    p[FP_PR1 + 2] = (uint8_t)rnd_in(r, 8, 40);
    p[FP_PR1 + 3] = (uint8_t)rnd_in(r, 10, 50);
    /* and a siren of an LFO on top, now and then */
    if (rnd_chance(r, 50)) {
        p[FP_LFW] = (uint8_t)(rnd_chance(r, 50) ? 0 : 4);
        p[FP_LFS] = (uint8_t)rnd_in(r, 3, 25);
        p[FP_LPMD] = (uint8_t)rnd_in(r, 20, 70);
        p[FP_LPMS] = (uint8_t)rnd_in(r, 5, 7);
        p[FP_LFD] = (uint8_t)rnd_in(r, 30, 80);
    }
}

static void fm_gen_spectre(uint8_t *p, rnd_t *r)
{
    int n, k = 0;
    static const uint8_t DET[6] = {7, 0, 14, 3, 11, 7};
    p[FP_ALG] = rnd_chance(r, 70) ? 4 : 5;               /* three pairs (5), or two and a feedback trio (6) */
    p[FP_FB] = (uint8_t)rnd_in(r, 3, 7);
    for (n = 1; n <= 6; n++) {
        uint8_t *op = fm_op(p, n);
        if (fm_is_carrier(p, n)) {
            fm_ratio(op, 100);
            op[FP_DET] = DET[k % 6];                     /* a few cents apart: the beating arpeggio */
            fm_env(op, rnd_in(r, 15, 50) - k * 4, 30, 20, rnd_in(r, 15, 30), 99, 99, 96, 0);
            op[FP_OL] = 95;
            k++;
        } else {
            fm_ratio(op, rnd_chance(r, 75) ? 100 : 200);
            op[FP_DET] = DET[(k + 2) % 6];
            fm_env(op, rnd_in(r, 10, 40), rnd_in(r, 5, 20), 10, 30, rnd_in(r, 80, 99), rnd_in(r, 70, 99),
                   rnd_in(r, 80, 99), 0);
            op[FP_OL] = (uint8_t)rnd_in(r, 72, 90);
        }
    }
    p[FP_LFS] = (uint8_t)rnd_in(r, 2, 10);
    p[FP_LPMD] = (uint8_t)rnd_in(r, 0, 5);
    p[FP_LAMD] = 0;
}

static void fm_gen_abyss(uint8_t *p, rnd_t *r)
{
    int n;
    p[FP_ALG] = rnd_chance(r, 50) ? 0 : ALG_PAIRS[rnd_below(r, 6)];
    p[FP_FB] = (uint8_t)rnd_in(r, 5, 7);
    p[FP_TRNSP] = (uint8_t)(rnd_chance(r, 60) ? 12 : 0);    /* an octave or two down */
    for (n = 1; n <= 6; n++) {
        uint8_t *op = fm_op(p, n);
        fm_ratio(op, rnd_chance(r, 50) ? 50 : 100 + rnd_below(r, 3));
        if (fm_is_carrier(p, n)) {
            fm_env(op, rnd_in(r, 15, 50), 25, 15, rnd_in(r, 10, 30), 99, 97, 92, 0);
            op[FP_OL] = 99;
            op[FP_AMS] = (uint8_t)rnd_in(r, 1, 3);
        } else {
            fm_env(op, rnd_in(r, 10, 40), rnd_in(r, 5, 20), 10, 30, rnd_in(r, 60, 99), rnd_in(r, 30, 90),
                   rnd_in(r, 50, 85), 0);
            op[FP_OL] = (uint8_t)rnd_in(r, 55, 80);
        }
        op[FP_DET] = (uint8_t)rnd_below(r, 15);
    }
    p[FP_LFW] = (uint8_t)(rnd_chance(r, 50) ? 0 : 4);
    p[FP_LFS] = (uint8_t)rnd_in(r, 2, 15);
    p[FP_LAMD] = (uint8_t)rnd_in(r, 25, 60);
    p[FP_LPMD] = (uint8_t)rnd_in(r, 0, 8);
}

static void fm_gen_friction(uint8_t *p, rnd_t *r)
{
    int n;
    p[FP_ALG] = ALG_DEEP[rnd_below(r, 6)];
    p[FP_FB] = (uint8_t)rnd_in(r, 4, 7);
    for (n = 1; n <= 6; n++) {
        uint8_t *op = fm_op(p, n);
        if (fm_is_carrier(p, n)) {
            fm_ratio(op, rnd_chance(r, 60) ? 100 : rnd_in(r, 100, 400));
            fm_env(op, rnd_in(r, 30, 70), rnd_in(r, 20, 50), rnd_in(r, 15, 40), rnd_in(r, 30, 60), 99,
                   rnd_in(r, 50, 95), rnd_in(r, 40, 85), 0);
            op[FP_OL] = (uint8_t)rnd_in(r, 88, 99);
        } else {
            if (rnd_chance(r, 60))
                fm_fixed(op, rnd_below(r, 2), rnd_in(r, 30, 99));   /* 2 .. 98 Hz: a creak, a rattle */
            else
                fm_ratio(op, rnd_in(r, 50, 700));
            /* irregular: up, down, up again */
            fm_env(op, rnd_in(r, 30, 90), rnd_in(r, 20, 70), rnd_in(r, 10, 50), 40, 99, rnd_in(r, 10, 60),
                   rnd_in(r, 50, 99), 0);
            op[FP_OL] = (uint8_t)rnd_in(r, 75, 99);
        }
    }
    p[FP_LFW] = 5;
    p[FP_LFS] = (uint8_t)rnd_in(r, 12, 48);
    p[FP_LPMD] = (uint8_t)rnd_in(r, 25, 99);
    p[FP_LPMS] = (uint8_t)rnd_in(r, 5, 7);
    p[FP_LAMD] = (uint8_t)rnd_in(r, 0, 30);
}

/* one more turn of chance, on any family */
static void fm_mutate(uint8_t *p, rnd_t *r)
{
    int n = 1 + rnd_below(r, 6);
    uint8_t *op = fm_op(p, n);
    switch (rnd_below(r, 6)) {
    case 0:                                             /* a partial nailed to a frequency */
        fm_fixed(op, rnd_in(r, 1, 3), rnd_below(r, 100));
        break;
    case 1: {                                           /* an envelope turned over: it grows as it holds */
        uint8_t t = op[FP_L1];
        op[FP_L1] = (uint8_t)rnd_below(r, 40);
        op[FP_L1 + 2] = t > 60 ? t : 80;
        op[FP_R1 + 2] = (uint8_t)rnd_in(r, 5, 30);
        break;
    }
    case 2:
        p[FP_FB] = 7;
        break;
    case 3:                                             /* the LFO taken too far */
        p[FP_LFS] = (uint8_t)rnd_in(r, 60, 99);
        p[FP_LPMD] = (uint8_t)rnd_in(r, 40, 99);
        p[FP_LPMS] = 7;
        break;
    case 4:
        p[FP_TRNSP] = (uint8_t)clampi(p[FP_TRNSP] + (rnd_chance(r, 50) ? 12 : -12), 0, 48);
        break;
    default:                                            /* an algorithm from another family */
        p[FP_ALG] = (uint8_t)rnd_below(r, 32);
        break;
    }
}


/* ------------------------------------------------- generation 1's families --- */
static void fm_gen_choir(uint8_t *p, rnd_t *r)
{
    static const uint8_t F1[4] = {3, 4, 5, 6}, F2[5] = {7, 8, 9, 11, 13};   /* harmonics near two formants */
    int n;
    p[FP_ALG] = 4;                                       /* three pairs: carriers 1, 3, 5 */
    p[FP_FB] = (uint8_t)rnd_below(r, 3);
    for (n = 1; n <= 6; n++) {
        uint8_t *op = fm_op(p, n);
        if (fm_is_carrier(p, n)) {
            int32_t h = n == 1 ? 1 : n == 3 ? F1[rnd_below(r, 4)] : F2[rnd_below(r, 5)];
            fm_ratio(op, h * 100);
            fm_env(op, rnd_in(r, 18, 40), rnd_in(r, 20, 40), 20, rnd_in(r, 18, 34), 99, 96, 92, 0);
            op[FP_OL] = (uint8_t)(n == 1 ? 99 : n == 3 ? rnd_in(r, 84, 93) : rnd_in(r, 74, 86));
            op[FP_DET] = (uint8_t)rnd_in(r, 5, 9);
        } else {
            fm_ratio(op, 100);                           /* the fundamental: sidebands around each formant */
            fm_env(op, rnd_in(r, 15, 35), 25, 20, 30, rnd_in(r, 80, 99), rnd_in(r, 70, 95), rnd_in(r, 70, 92), 0);
            op[FP_OL] = (uint8_t)rnd_in(r, 50, 70);
        }
    }
    p[FP_LFW] = 4;                                       /* a sine vibrato, late */
    p[FP_LFS] = (uint8_t)rnd_in(r, 28, 40);
    p[FP_LFD] = (uint8_t)rnd_in(r, 40, 75);
    p[FP_LPMD] = (uint8_t)rnd_in(r, 4, 10);
    p[FP_LPMS] = 3;
    p[FP_LAMD] = 0;
}
static void fm_gen_bowed(uint8_t *p, rnd_t *r)
{
    int n;
    p[FP_ALG] = 0;                                       /* 2 > 1, and 6 > 5 > 4 > 3 with the feedback on 6 */
    p[FP_FB] = (uint8_t)rnd_in(r, 5, 7);                 /* a saw */
    for (n = 1; n <= 6; n++) {
        uint8_t *op = fm_op(p, n);
        if (fm_is_carrier(p, n)) {
            fm_ratio(op, n == 1 && rnd_chance(r, 40) ? 200 : 100);
            fm_env(op, rnd_in(r, 22, 45), 30, 25, rnd_in(r, 22, 38), 99, 97, 94, 0);
            op[FP_OL] = (uint8_t)(n == 3 ? 99 : rnd_in(r, 80, 92));
            op[FP_DET] = (uint8_t)(n == 1 ? rnd_in(r, 3, 6) : rnd_in(r, 8, 11));
            op[FP_RS] = 2;
        } else {                                         /* the bow: the index rising with the swell, a little slower */
            fm_ratio(op, n == 2 ? 100 : rnd_chance(r, 70) ? 100 : 200);
            fm_env(op, rnd_in(r, 14, 32), rnd_in(r, 15, 30), 15, 30, rnd_in(r, 85, 99), rnd_in(r, 75, 95), rnd_in(r, 70, 90), 0);
            op[FP_OL] = (uint8_t)(n == 6 ? rnd_in(r, 70, 85) : rnd_in(r, 55, 75));
        }
    }
    p[FP_LFW] = 4;
    p[FP_LFS] = (uint8_t)rnd_in(r, 30, 42);
    p[FP_LFD] = (uint8_t)rnd_in(r, 50, 80);
    p[FP_LPMD] = (uint8_t)rnd_in(r, 4, 9);
    p[FP_LPMS] = 3;
}
static void fm_gen_hollow(uint8_t *p, rnd_t *r)
{
    int n;
    p[FP_ALG] = 4;                                       /* three pairs */
    p[FP_FB] = 0;
    for (n = 1; n <= 6; n++) {
        uint8_t *op = fm_op(p, n);
        if (fm_is_carrier(p, n)) {
            int32_t c = n == 5 && rnd_chance(r, 50) ? 50 : 100;   /* (one an octave down, now and then) */
            fm_ratio(op, c);
            fm_env(op, rnd_in(r, 12, 30), 25, 20, rnd_in(r, 15, 30), 99, 98, 95, 0);
            op[FP_OL] = (uint8_t)(n == 1 ? 99 : rnd_in(r, 78, 92));
            op[FP_DET] = (uint8_t)(n == 1 ? 7 : rnd_chance(r, 50) ? rnd_in(r, 4, 6) : rnd_in(r, 8, 10));
        } else {
            uint8_t *c = fm_op(p, n - 1);
            fm_ratio(op, c[FP_FC] == 0 ? 100 : 200);      /* twice the carrier: odd harmonics only */
            fm_env(op, rnd_in(r, 10, 28), 20, 15, 30, rnd_in(r, 70, 95), rnd_in(r, 60, 90), rnd_in(r, 55, 85), 0);
            op[FP_OL] = (uint8_t)rnd_in(r, 45, 68);
        }
    }
    p[FP_LFW] = 4;
    p[FP_LFS] = (uint8_t)rnd_in(r, 4, 14);
    p[FP_LAMD] = (uint8_t)rnd_in(r, 5, 20);
    p[FP_LPMD] = 0;
}
static void fm_gen_bloom(uint8_t *p, rnd_t *r)
{
    int n;
    p[FP_ALG] = rnd_chance(r, 60) ? ALG_PAIRS[rnd_below(r, 6)] : ALG_DEEP[rnd_below(r, 6)];
    p[FP_FB] = (uint8_t)rnd_below(r, 5);
    for (n = 1; n <= 6; n++) {
        uint8_t *op = fm_op(p, n);
        if (fm_is_carrier(p, n)) {
            fm_ratio(op, rnd_chance(r, 80) ? 100 * (1 + rnd_below(r, 2)) : inharm_ratio(r));
            fm_env(op, rnd_in(r, 25, 55), 30, 20, rnd_in(r, 18, 34), 99, 98, 95, 0);
            op[FP_OL] = (uint8_t)rnd_in(r, 88, 99);
        } else {                                         /* low, opening over seconds, half closing again */
            fm_ratio(op, rnd_chance(r, 70) ? 100 * (1 + rnd_below(r, 3)) : inharm_ratio(r));
            fm_env(op, rnd_in(r, 30, 60), rnd_in(r, 6, 16), rnd_in(r, 5, 14), 30, rnd_in(r, 15, 40), rnd_in(r, 88, 99),
                   rnd_in(r, 55, 80), 0);
            op[FP_OL] = (uint8_t)rnd_in(r, 62, 84);
        }
    }
    p[FP_LFS] = (uint8_t)rnd_in(r, 3, 12);
    p[FP_LPMD] = (uint8_t)rnd_in(r, 0, 4);
}
static void fm_gen_tide(uint8_t *p, rnd_t *r)
{
    static const int16_t RAT[6] = {50, 100, 100, 200, 300, 150};
    int n, k = 0;
    p[FP_ALG] = 4;                                       /* three stacks */
    p[FP_FB] = (uint8_t)rnd_in(r, 2, 5);
    for (n = 1; n <= 6; n++) {
        uint8_t *op = fm_op(p, n);
        if (fm_is_carrier(p, n)) {                       /* one fades as the next swells */
            fm_ratio(op, RAT[rnd_below(r, 6)]);
            if (k == 0)
                fm_env(op, rnd_in(r, 30, 50), rnd_in(r, 6, 14), 10, 25, 99, rnd_in(r, 40, 65), rnd_in(r, 30, 55), 0);
            else if (k == 1)
                fm_env(op, rnd_in(r, 6, 14), 20, 15, 25, 99, 97, 95, 0);
            else
                fm_env(op, rnd_in(r, 4, 10), 20, 15, 25, 99, 96, 92, 0);
            op[FP_OL] = (uint8_t)rnd_in(r, 86, 98);
            op[FP_AMS] = (uint8_t)rnd_in(r, 1, 2);
            op[FP_DET] = (uint8_t)rnd_below(r, 15);
            k++;
        } else {
            fm_ratio(op, RAT[rnd_below(r, 6)] + rnd_below(r, 3));
            fm_env(op, rnd_in(r, 8, 30), rnd_in(r, 5, 15), 10, 30, rnd_in(r, 60, 95), rnd_in(r, 40, 90), rnd_in(r, 50, 85), 0);
            op[FP_OL] = (uint8_t)rnd_in(r, 50, 76);
        }
    }
    p[FP_LFW] = 4;                                       /* the tide */
    p[FP_LFS] = (uint8_t)rnd_in(r, 2, 9);
    p[FP_LAMD] = (uint8_t)rnd_in(r, 18, 40);
    p[FP_LPMD] = 0;
}
static void fm_gen_shards(uint8_t *p, rnd_t *r)
{
    int n;
    p[FP_ALG] = rnd_chance(r, 50) ? ALG_MANY[rnd_below(r, 6)] : ALG_PAIRS[rnd_below(r, 6)];
    p[FP_FB] = (uint8_t)rnd_below(r, 7);
    for (n = 1; n <= 6; n++) {
        uint8_t *op = fm_op(p, n);
        fm_ratio(op, rnd_chance(r, 60) ? inharm_ratio(r) : 100 * (1 + rnd_below(r, 4)));
        if (fm_is_carrier(p, n)) {
            fm_env(op, rnd_in(r, 70, 99), rnd_in(r, 30, 60), 25, rnd_in(r, 40, 70), 99, rnd_in(r, 75, 95), rnd_in(r, 65, 90), 0);
            op[FP_OL] = (uint8_t)rnd_in(r, 84, 99);
            op[FP_AMS] = 3;                              /* chopped */
        } else {
            fm_env(op, rnd_in(r, 60, 99), rnd_in(r, 20, 60), 25, 40, 99, rnd_in(r, 60, 99), rnd_in(r, 50, 90), 0);
            op[FP_OL] = (uint8_t)rnd_in(r, 60, 90);
        }
    }
    p[FP_LFW] = 5;                                       /* sample and hold: a new pitch, a new level, each step */
    p[FP_LFS] = (uint8_t)rnd_in(r, 62, 90);
    p[FP_LPMS] = (uint8_t)rnd_in(r, 4, 6);
    p[FP_LPMD] = (uint8_t)rnd_in(r, 20, 60);
    p[FP_LAMD] = (uint8_t)rnd_in(r, 60, 99);
}
static void fm_gen_static(uint8_t *p, rnd_t *r)
{
    int n;
    p[FP_ALG] = ALG_PAIRS[rnd_below(r, 6)];
    p[FP_FB] = (uint8_t)rnd_in(r, 3, 7);
    for (n = 1; n <= 6; n++) {
        uint8_t *op = fm_op(p, n);
        if (fm_is_carrier(p, n)) {
            fm_ratio(op, rnd_chance(r, 70) ? 100 : inharm_ratio(r));
            fm_env(op, rnd_in(r, 50, 90), 30, 25, rnd_in(r, 30, 60), 99, rnd_in(r, 85, 99), rnd_in(r, 75, 95), 0);
            op[FP_OL] = (uint8_t)rnd_in(r, 85, 99);
        } else {                                         /* nailed to audio frequencies: ring modulation */
            fm_fixed(op, rnd_in(r, 2, 3), rnd_below(r, 100));
            fm_env(op, rnd_in(r, 40, 90), rnd_in(r, 10, 40), 15, 40, 99, rnd_in(r, 70, 99), rnd_in(r, 60, 95), 0);
            op[FP_OL] = (uint8_t)rnd_in(r, 75, 97);
        }
    }
    if (rnd_chance(r, 50)) {                             /* a square tremolo: a stutter */
        p[FP_LFW] = 3;
        p[FP_LFS] = (uint8_t)rnd_in(r, 40, 80);
        p[FP_LAMD] = (uint8_t)rnd_in(r, 50, 99);
        for (n = 1; n <= 6; n++)
            if (fm_is_carrier(p, n))
                fm_op(p, n)[FP_AMS] = 3;
    }
}
static void fm_gen_wind(uint8_t *p, rnd_t *r)
{
    static const int16_t DRONE[5] = {50, 100, 100, 200, 150};
    int n;
    p[FP_ALG] = 31;                                      /* six carriers, the feedback on 6: the breath */
    p[FP_FB] = 7;
    for (n = 1; n <= 6; n++) {
        uint8_t *op = fm_op(p, n);
        if (n == 6) {
            fm_ratio(op, rnd_in(r, 600, 1400));
            fm_env(op, rnd_in(r, 10, 30), 20, 15, rnd_in(r, 15, 30), 99, 97, 95, 0);
            op[FP_OL] = (uint8_t)rnd_in(r, 74, 86);
            op[FP_AMS] = 3;                              /* gusts */
        } else {
            fm_ratio(op, DRONE[rnd_below(r, 5)] + rnd_below(r, 2));
            fm_env(op, rnd_in(r, 10, 28), 20, 15, rnd_in(r, 15, 30), 99, 96, 92, 0);
            op[FP_OL] = (uint8_t)(n == 1 ? rnd_in(r, 78, 90) : rnd_in(r, 55, 78));
            op[FP_DET] = (uint8_t)rnd_below(r, 15);
        }
    }
    p[FP_LFW] = rnd_chance(r, 50) ? 4 : 5;
    p[FP_LFS] = (uint8_t)rnd_in(r, 6, 22);
    p[FP_LAMD] = (uint8_t)rnd_in(r, 35, 75);
    p[FP_LPMD] = 0;
}

/* ------------------------------------------------- generation 3's families --- */
/* plucked strings, bright or dark: the modulators die quickly (the attack's brightness), the carriers die away */
static void fm_gen_pluck(uint8_t *p, rnd_t *r)
{
    static const uint8_t ALG[6] = {4, 5, 0, 1, 15, 21};
    int n, dark = rnd_chance(r, 45);
    p[FP_ALG] = ALG[rnd_below(r, 6)];
    p[FP_FB] = (uint8_t)rnd_in(r, 0, dark ? 3 : 6);
    for (n = 1; n <= 6; n++) {
        uint8_t *op = fm_op(p, n);
        if (fm_is_carrier(p, n)) {
            fm_ratio(op, rnd_chance(r, 75) ? 100 : rnd_chance(r, 50) ? 200 : 50);
            fm_env(op, 99, rnd_in(r, 28, 52), rnd_in(r, 20, 40), rnd_in(r, 35, 60), 99, rnd_in(r, 40, 75), 0, 0);
            op[FP_OL] = (uint8_t)rnd_in(r, 92, 99);
            op[FP_DET] = (uint8_t)rnd_in(r, 5, 9);
        } else {
            static const int16_t BR[6] = {100, 200, 300, 100, 400, 500};
            fm_ratio(op, dark ? (rnd_chance(r, 70) ? 100 : 200) : BR[rnd_below(r, 6)]);
            fm_env(op, 99, rnd_in(r, dark ? 55 : 45, dark ? 85 : 75), rnd_in(r, 30, 60), rnd_in(r, 40, 70), 99,
                   rnd_in(r, 0, dark ? 25 : 45), 0, 0);
            op[FP_OL] = (uint8_t)(dark ? rnd_in(r, 55, 76) : rnd_in(r, 74, 92));
        }
        op[FP_RS] = (uint8_t)rnd_in(r, 3, 6);              /* (higher strings die quicker) */
        op[FP_KVS] = (uint8_t)rnd_in(r, 0, 3);
    }
    p[FP_LAMD] = 0;
    p[FP_LPMD] = (uint8_t)rnd_in(r, 0, 4);
    if (dark && p[FP_TRNSP] >= 12u && rnd_chance(r, 40))
        p[FP_TRNSP] -= 12;
}
/* struck bars: marimba, vibraphone, kalimba -- a high modulator's knock, gone at once; the bar rings on */
static void fm_gen_mallet(uint8_t *p, rnd_t *r)
{
    static const int16_t KNOCK[6] = {350, 400, 700, 1000, 540, 314};
    int n;
    p[FP_ALG] = ALG_PAIRS[rnd_below(r, 6)];
    p[FP_FB] = (uint8_t)rnd_below(r, 3);
    for (n = 1; n <= 6; n++) {
        uint8_t *op = fm_op(p, n);
        if (fm_is_carrier(p, n)) {
            fm_ratio(op, rnd_chance(r, 70) ? 100 : rnd_chance(r, 50) ? 400 : 1000);
            fm_env(op, 99, rnd_in(r, 32, 55), rnd_in(r, 25, 45), rnd_in(r, 35, 60), 99, rnd_in(r, 30, 60), 0, 0);
            op[FP_OL] = (uint8_t)rnd_in(r, 88, 99);
            if (rnd_chance(r, 30))
                op[FP_AMS] = 2;                            /* (a vibraphone's motor) */
        } else {
            fm_ratio(op, KNOCK[rnd_below(r, 6)] + rnd_in(r, -6, 6));
            fm_env(op, 99, rnd_in(r, 75, 95), rnd_in(r, 50, 80), 60, 99, rnd_in(r, 0, 20), 0, 0);
            op[FP_OL] = (uint8_t)rnd_in(r, 68, 90);
        }
        op[FP_RS] = (uint8_t)rnd_in(r, 2, 5);
    }
    p[FP_LFW] = 4;
    p[FP_LFS] = (uint8_t)rnd_in(r, 30, 45);
    p[FP_LAMD] = (uint8_t)rnd_in(r, 0, 30);
    p[FP_LPMD] = 0;
}
/* a warp: ratios anywhere, partials nailed to frequencies, the pitch thrown about by its envelope */
static void fm_gen_warp(uint8_t *p, rnd_t *r)
{
    int n, i;
    p[FP_ALG] = (uint8_t)rnd_below(r, 32);
    p[FP_FB] = (uint8_t)rnd_in(r, 4, 7);
    for (n = 1; n <= 6; n++) {
        uint8_t *op = fm_op(p, n);
        if (fm_is_carrier(p, n)) {                         /* (the carriers heard: their levels up, their ratios low) */
            fm_ratio(op, rnd_in(r, 50, 400));
            fm_env(op, rnd_in(r, 20, 99), rnd_in(r, 5, 60), rnd_in(r, 5, 60), rnd_in(r, 20, 60), rnd_in(r, 80, 99),
                   rnd_in(r, 55, 99), rnd_in(r, 65, 99), 0);
        } else {
            if (rnd_chance(r, 35))
                fm_fixed(op, rnd_in(r, 0, 3), rnd_below(r, 100));
            else
                fm_ratio(op, rnd_in(r, 50, 1200));
            fm_env(op, rnd_in(r, 20, 99), rnd_in(r, 5, 60), rnd_in(r, 5, 60), rnd_in(r, 20, 60), rnd_in(r, 50, 99),
                   rnd_below(r, 99), rnd_in(r, 30, 99), 0);
        }
        op[FP_OL] = (uint8_t)(fm_is_carrier(p, n) ? rnd_in(r, 85, 99) : rnd_in(r, 55, 95));
        op[FP_DET] = (uint8_t)rnd_below(r, 15);
    }
    for (i = 0; i < 4; i++) {                              /* the pitch envelope: anywhere, at any pace */
        p[FP_PR1 + i] = (uint8_t)rnd_in(r, 20, 80);
        p[FP_PL1 + i] = (uint8_t)rnd_in(r, 33, 67);         /* (bent, never thrown out of hearing) */
    }
    p[FP_PL1 + 2] = (uint8_t)rnd_in(r, 40, 60);            /* (it settles near the note) */
    p[FP_LFW] = (uint8_t)rnd_below(r, 6);
    p[FP_LFS] = (uint8_t)rnd_in(r, 5, 60);
    p[FP_LPMD] = (uint8_t)rnd_in(r, 0, 40);
    p[FP_LPMS] = (uint8_t)rnd_in(r, 2, 6);
    p[FP_LAMD] = (uint8_t)rnd_in(r, 0, 40);
}
/* a glitch: a quick sample-and-hold stepping the pitch and the level, envelopes chopped short */
static void fm_gen_glitch(uint8_t *p, rnd_t *r)
{
    int n;
    p[FP_ALG] = rnd_chance(r, 50) ? ALG_PAIRS[rnd_below(r, 6)] : ALG_MANY[rnd_below(r, 6)];
    p[FP_FB] = (uint8_t)rnd_in(r, 2, 7);
    for (n = 1; n <= 6; n++) {
        uint8_t *op = fm_op(p, n);
        fm_ratio(op, 100 * rnd_in(r, 1, 8) / (rnd_chance(r, 25) ? 2 : 1));
        if (fm_is_carrier(p, n)) {
            fm_env(op, 99, rnd_in(r, 40, 80), rnd_in(r, 30, 70), rnd_in(r, 50, 80), 99, rnd_in(r, 60, 99), rnd_in(r, 40, 90), 0);
            op[FP_OL] = (uint8_t)rnd_in(r, 88, 99);
            op[FP_AMS] = (uint8_t)rnd_in(r, 2, 3);
        } else {
            fm_env(op, 99, rnd_in(r, 50, 99), rnd_in(r, 40, 80), 60, 99, rnd_below(r, 99), rnd_below(r, 99), 0);
            op[FP_OL] = (uint8_t)rnd_in(r, 60, 92);
        }
    }
    p[FP_LFW] = 5;
    p[FP_LFS] = (uint8_t)rnd_in(r, 70, 99);
    p[FP_LPMS] = (uint8_t)rnd_in(r, 5, 7);
    p[FP_LPMD] = (uint8_t)rnd_in(r, 25, 99);
    p[FP_LAMD] = (uint8_t)rnd_in(r, 30, 90);
}
/* generation 5's warp: the warp, its pitch now and then GLISSE's journey or RUBBER's bounce instead -- or both, the
 * bounce at the start and the siren after -- and now and then RUBBER's low, plain ratios under it */
static void fm_gen_warp5(uint8_t *p, rnd_t *r)
{
    int n;
    uint32_t how = rnd_below(r, 10);                    /* 0 .. 3 its own; 4 .. 6 a glide; 7 .. 8 a bounce; 9 both */
    fm_gen_warp(p, r);
    if (how >= 4u && how != 7u && how != 8u) {           /* GLISSE: from far below or above, past the note, slowly */
        p[FP_PL1 + 3] = (uint8_t)(rnd_chance(r, 50) ? rnd_in(r, 0, 30) : rnd_in(r, 70, 99));
        p[FP_PL1] = (uint8_t)rnd_in(r, 20, 80);
        p[FP_PL1 + 1] = (uint8_t)rnd_in(r, 0, 99);
        p[FP_PL1 + 2] = (uint8_t)(rnd_chance(r, 50) ? 50 : rnd_in(r, 30, 70));
        p[FP_PR1] = (uint8_t)rnd_in(r, 15, 55);
        p[FP_PR1 + 1] = (uint8_t)rnd_in(r, 8, 40);
        p[FP_PR1 + 2] = (uint8_t)rnd_in(r, 8, 40);
        p[FP_PR1 + 3] = (uint8_t)rnd_in(r, 10, 50);
        if (rnd_chance(r, 50)) {                         /* (a siren on top) */
            p[FP_LFW] = (uint8_t)(rnd_chance(r, 50) ? 0 : 4);
            p[FP_LFS] = (uint8_t)rnd_in(r, 3, 25);
            p[FP_LPMD] = (uint8_t)rnd_in(r, 20, 70);
            p[FP_LPMS] = (uint8_t)rnd_in(r, 5, 7);
            p[FP_LFD] = (uint8_t)rnd_in(r, 30, 80);
        }
    }
    if (how >= 7u) {                                     /* RUBBER: under, up past it, back under, to it -- fast */
        p[FP_PL1 + 3] = (uint8_t)rnd_in(r, 15, 35);
        p[FP_PL1] = (uint8_t)rnd_in(r, 62, 80);
        p[FP_PL1 + 1] = (uint8_t)rnd_in(r, 42, 48);
        p[FP_PL1 + 2] = 50;
        p[FP_PR1] = (uint8_t)rnd_in(r, 70, 90);
        p[FP_PR1 + 1] = (uint8_t)rnd_in(r, 60, 85);
        p[FP_PR1 + 2] = (uint8_t)rnd_in(r, 55, 80);
        p[FP_PR1 + 3] = 99;
    }
    if (rnd_chance(r, how >= 7u ? 60 : 25))             /* (RUBBER's plain ratios under the carriers) */
        for (n = 1; n <= 6; n++)
            if (fm_is_carrier(p, n))
                fm_ratio(fm_op(p, n), rnd_chance(r, 50) ? 100 : rnd_chance(r, 50) ? 50 : 200);
}
/* rubber: low ratios, the pitch bouncing in at every note (up, under, back) */
static void fm_gen_rubber(uint8_t *p, rnd_t *r)
{
    int n;
    p[FP_ALG] = rnd_chance(r, 50) ? ALG_DEEP[rnd_below(r, 6)] : ALG_PAIRS[rnd_below(r, 6)];
    p[FP_FB] = (uint8_t)rnd_in(r, 2, 6);
    for (n = 1; n <= 6; n++) {
        uint8_t *op = fm_op(p, n);
        fm_ratio(op, rnd_chance(r, 50) ? 100 : rnd_chance(r, 50) ? 50 : 200);
        if (fm_is_carrier(p, n)) {
            fm_env(op, 99, rnd_in(r, 25, 50), rnd_in(r, 20, 40), rnd_in(r, 35, 60), 99, rnd_in(r, 50, 85), rnd_in(r, 0, 60), 0);
            op[FP_OL] = (uint8_t)rnd_in(r, 90, 99);
        } else {
            fm_env(op, 99, rnd_in(r, 30, 60), rnd_in(r, 20, 50), 50, 99, rnd_in(r, 40, 80), rnd_in(r, 20, 60), 0);
            op[FP_OL] = (uint8_t)rnd_in(r, 65, 88);
        }
    }
    p[FP_PL1 + 3] = (uint8_t)rnd_in(r, 15, 35);            /* from under the note, */
    p[FP_PL1] = (uint8_t)rnd_in(r, 62, 80);                /* up past it, */
    p[FP_PL1 + 1] = (uint8_t)rnd_in(r, 42, 48);            /* back under, */
    p[FP_PL1 + 2] = 50;                                    /* to it */
    p[FP_PR1] = (uint8_t)rnd_in(r, 70, 90);
    p[FP_PR1 + 1] = (uint8_t)rnd_in(r, 60, 85);
    p[FP_PR1 + 2] = (uint8_t)rnd_in(r, 55, 80);
    p[FP_PR1 + 3] = 99;
    p[FP_LAMD] = 0;
    p[FP_LPMD] = 0;
}
/* generation 3's breadth, on every family (after all the earlier draws): now and then plucked, a modulator moved
 * to another ratio, an operator detuned */
static void fm_care3(uint8_t *p, int32_t f, uint32_t path, rnd_t *r)
{
    static const int16_t R3[8] = {50, 150, 250, 300, 350, 450, 600, 700};
    int n;
    if (f < FF_N1 && rnd_chance(r, path == PATH_AMBIENT ? 6 : 14))   /* plucked: struck, dying away */
        for (n = 1; n <= 6; n++) {
            uint8_t *op = fm_op(p, n);
            op[FP_R1] = 99;
            op[FP_R1 + 1] = (uint8_t)rnd_in(r, fm_is_carrier(p, n) ? 30 : 55, fm_is_carrier(p, n) ? 55 : 85);
            op[FP_L1] = 99;
            op[FP_L1 + 2] = 0;
            op[FP_R1 + 3] = (uint8_t)clampi(op[FP_R1 + 3], 30, 70);
        }
    if (rnd_chance(r, 35)) {
        uint8_t *op = fm_op(p, 1 + rnd_below(r, 6));
        if (!op[FP_MODE] && !fm_is_carrier(p, (int)(6 - (op - p) / FP_OP)))
            fm_ratio(op, R3[rnd_below(r, 8)]);
    }
    if (rnd_chance(r, 30))
        fm_op(p, 1 + rnd_below(r, 6))[FP_DET] = (uint8_t)(rnd_chance(r, 50) ? rnd_in(r, 0, 3) : rnd_in(r, 11, 14));
}

/* generation 1's care, on every patch: no screaming sidebands, the brightness scaled down the keyboard */
static void fm_shape1(uint8_t *p, uint32_t path, rnd_t *r)
{
    int n;
    for (n = 1; n <= 6; n++) {
        uint8_t *op = fm_op(p, n);
        if (!fm_is_carrier(p, n)) {
            int32_t ratio = op[FP_MODE] ? 0 : (op[FP_FC] ? op[FP_FC] : 1) * (100 + op[FP_FF]) / 100;
            if (ratio > 4 && path != PATH_EXP)          /* sidebands far up alias and scream */
                op[FP_OL] = (uint8_t)clampi(op[FP_OL] - clampi((ratio - 4) * 2, 0, 12), 0, 99);
            op[FP_BP] = (uint8_t)rnd_in(r, 36, 46);    /* about C3 .. A3 */
            op[FP_RD] = (uint8_t)rnd_in(r, 12, path == PATH_EXP ? 25 : 40);   /* up the keyboard: less bright */
            op[FP_RC] = 1;                              /* (-EXP) */
            op[FP_LD] = (uint8_t)rnd_in(r, 0, 18);      /* down it: less mud */
            op[FP_LC] = 0;                              /* (-LIN) */
        } else if (op[FP_RS] == 0)
            op[FP_RS] = (uint8_t)rnd_in(r, 1, 3);       /* the higher, the quicker its envelope, a little */
    }
}
/* generation 1's mutations: a patch bent, not broken */
static void fm_mutate1(uint8_t *p, uint32_t path, rnd_t *r)
{
    int n = 1 + rnd_below(r, 6);
    uint8_t *op = fm_op(p, n);
    switch (rnd_below(r, 7)) {
    case 0:                                             /* one operator detuned against the rest: a slow beat */
        op[FP_DET] = (uint8_t)(rnd_chance(r, 50) ? rnd_in(r, 0, 3) : rnd_in(r, 11, 14));
        break;
    case 1: {                                           /* an envelope turned over: it grows as it holds */
        uint8_t t = op[FP_L1];
        op[FP_L1] = (uint8_t)rnd_below(r, 40);
        op[FP_L1 + 2] = t > 60 ? t : 80;
        op[FP_R1 + 2] = (uint8_t)rnd_in(r, 5, 30);
        break;
    }
    case 2:                                             /* the LFO waits, then moves the timbre */
        p[FP_LFD] = (uint8_t)rnd_in(r, 40, 90);
        if (!fm_is_carrier(p, n))
            op[FP_AMS] = (uint8_t)rnd_in(r, 1, 3);
        if (!p[FP_LAMD])
            p[FP_LAMD] = (uint8_t)rnd_in(r, 15, 45);
        break;
    case 3:                                             /* an octave down */
        if (p[FP_TRNSP] >= 12)
            p[FP_TRNSP] -= 12;
        break;
    case 4:                                             /* a partial nailed to a frequency (the edgier paths only) */
        if (path != PATH_AMBIENT)
            fm_fixed(op, rnd_in(r, 1, 3), rnd_below(r, 100));
        break;
    case 5:                                             /* more feedback */
        p[FP_FB] = (uint8_t)clampi(p[FP_FB] + rnd_in(r, 1, 3), 0, path == PATH_AMBIENT ? 5 : 7);
        break;
    default:                                            /* the modulator's ratio moved to a neighbour */
        if (!fm_is_carrier(p, n) && !op[FP_MODE])
            fm_ratio(op, clampi(((op[FP_FC] ? op[FP_FC] : 1) * (100 + op[FP_FF])) + (rnd_chance(r, 50) ? 100 : -50), 50, 3100));
        break;
    }
}

/* AMBIENT: slower, darker, longer, stiller */
static void fm_ambient(uint8_t *p, int32_t f, rnd_t *r)
{
    int n, i;
    for (n = 1; n <= 6; n++) {
        uint8_t *op = fm_op(p, n);
        if (fm_is_carrier(p, n)) {
            if ((f != FF_METAL && f != FF_PLUCK && f != FF_MALLET && f != FF_CLANG) || rnd_chance(r, 50))
                                                              /* swells (a bell, a pluck may keep its strike) */
                op[FP_R1] = (uint8_t)clampi(op[FP_R1], 0, rnd_in(r, 18, 55));
            op[FP_R1 + 3] = (uint8_t)clampi(op[FP_R1 + 3], 0, rnd_in(r, 8, 28));   /* long tails */
        } else
            op[FP_OL] = (uint8_t)clampi(op[FP_OL] - rnd_in(r, 4, 14), 0, 99);      /* darker */
    }
    p[FP_FB] = (uint8_t)clampi(p[FP_FB], 0, 5);
    p[FP_LFS] = (uint8_t)clampi(p[FP_LFS], 0, 28);
    if (p[FP_LPMS] >= 5u) {                                   /* no sirens */
        p[FP_LPMS] = 3;
        p[FP_LPMD] = (uint8_t)clampi(p[FP_LPMD], 0, 10);
    }
    for (i = 0; i < 4; i++)                                   /* the glissandi narrowed to a third */
        p[FP_PL1 + i] = (uint8_t)(50 + (p[FP_PL1 + i] - 50) / 3);
    if (p[FP_TRNSP] >= 24u && rnd_chance(r, 35))
        p[FP_TRNSP] -= 12;                                    /* an octave down */
}
/* EXPERIMENTAL: brighter, harder */
static void fm_experimental(uint8_t *p, rnd_t *r)
{
    int n;
    for (n = 1; n <= 6; n++)
        if (!fm_is_carrier(p, n))
            fm_op(p, n)[FP_OL] = (uint8_t)clampi(fm_op(p, n)[FP_OL] + rnd_in(r, 0, 10), 0, 99);
}

#include "fm_dx7.c"        /* generation 6's families */

/* a patch of family f (FF_N: any), from seed, on a path; then fm_measure */
/* generation 4: IMPACT as struck things of many kinds -- a falling pitch (the old one's every sound) now one kind in
 * ten; the tone moved about (the carriers' ratios, the octave) */
static void fm_gen_impact4(uint8_t *p, rnd_t *r)
{
    static const int16_t SKIN[6] = {100, 159, 214, 230, 265, 292};   /* a drum skin's modes */
    static const int16_t WOOD[5] = {100, 200, 300, 400, 275};
    int n, kind = (int)rnd_below(r, 100), fb;
    kind = kind < 22 ? 0 : kind < 42 ? 1 : kind < 62 ? 2 : kind < 78 ? 3 : kind < 90 ? 4 : 5;
    p[FP_ALG] = kind == 2 || kind == 3 ? ALG_MANY[rnd_below(r, 6)]
                : rnd_chance(r, 50) ? ALG_PAIRS[rnd_below(r, 6)] : ALG_DEEP[rnd_below(r, 6)];
    fb = kind == 4 ? rnd_in(r, 5, 7) : (int)rnd_below(r, 4);
    p[FP_FB] = (uint8_t)fb;
    for (n = 1; n <= 6; n++) {
        uint8_t *op = fm_op(p, n);
        if (fm_is_carrier(p, n)) {
            int dec = kind == 0 ? rnd_in(r, 45, 75) : kind == 2 ? rnd_in(r, 15, 45) : kind == 4 ? rnd_in(r, 50, 80)
                                                                                      : rnd_in(r, 30, 60);
            fm_ratio(op, kind == 0 ? (rnd_chance(r, 50) ? 50 : 100)
                         : kind == 1 ? WOOD[rnd_below(r, 5)] + rnd_in(r, -4, 4)
                         : kind == 2 ? inharm_ratio(r)
                         : kind == 3 ? SKIN[rnd_below(r, 6)] + rnd_in(r, -3, 3)
                         : kind == 4 ? rnd_in(r, 50, 400)
                                     : (rnd_chance(r, 60) ? 100 : inharm_ratio(r)));
            fm_env(op, 99, dec, rnd_in(r, 20, 50), rnd_in(r, 30, 60), 99, rnd_in(r, 0, 40), 0, 0);
            op[FP_OL] = (uint8_t)rnd_in(r, 88, 99);
            op[FP_DET] = (uint8_t)rnd_in(r, 4, 10);
        } else {
            if (kind == 0 || kind == 4 || rnd_chance(r, 25))
                fm_fixed(op, kind == 0 ? rnd_in(r, 1, 2) : rnd_in(r, 2, 3), rnd_below(r, 100));   /* a thud, a crack */
            else
                fm_ratio(op, kind == 1 ? rnd_in(r, 300, 900) : kind == 3 ? rnd_in(r, 100, 300)
                                                              : rnd_chance(r, 50) ? inharm_ratio(r) : rnd_in(r, 50, 700));
            fm_env(op, 99, kind == 1 ? rnd_in(r, 80, 99) : rnd_in(r, 55, 90), rnd_in(r, 40, 70), 50, 99,
                   rnd_in(r, 0, 40), 0, 0);   /* the strike's brightness, gone first */
            op[FP_OL] = (uint8_t)(kind == 2 ? rnd_in(r, 50, 80) : rnd_in(r, 70, 99));
        }
        op[FP_RS] = (uint8_t)rnd_below(r, 5);
    }
    if (rnd_chance(r, 30))                                 /* (an octave down or up) */
        p[FP_TRNSP] = (uint8_t)(rnd_chance(r, 60) ? 12 : 36);
    if (kind == 5) {                                       /* the pitch falling to the note: now and then */
        p[FP_PL1 + 3] = (uint8_t)rnd_in(r, 58, 80);
        p[FP_PL1] = p[FP_PL1 + 1] = p[FP_PL1 + 2] = 50;
        p[FP_PR1] = (uint8_t)rnd_in(r, 60, 92);
        p[FP_PR1 + 1] = p[FP_PR1 + 2] = 99;
    } else if (rnd_chance(r, 15)) {                        /* .. or barely: a strike's slight settling */
        p[FP_PL1 + 3] = (uint8_t)rnd_in(r, 51, 54);
        p[FP_PR1] = (uint8_t)rnd_in(r, 70, 95);
    }
    p[FP_LAMD] = 0;
    p[FP_LPMD] = 0;
}
/* generation 4: GLITCH clunkier and more various -- the sample-and-hold stepping the pitch (the old one's every
 * sound, a toy computer's chatter) now one kind in seven, and gentle; the others: the level chopped by a square or
 * random LFO, a crunch of heavy feedback and fixed-frequency modulators, the pitch thrown between steps by its
 * envelope, a slow wobble on a broken timbre */
static void fm_gen_glitch4(uint8_t *p, rnd_t *r)
{
    int n, kind = (int)rnd_below(r, 100);
    kind = kind < 25 ? 0 : kind < 50 ? 1 : kind < 72 ? 2 : kind < 86 ? 3 : 4;
    p[FP_ALG] = rnd_chance(r, 50) ? ALG_PAIRS[rnd_below(r, 6)] : rnd_chance(r, 50) ? ALG_MANY[rnd_below(r, 6)]
                                                                                    : ALG_DEEP[rnd_below(r, 6)];
    p[FP_FB] = (uint8_t)(kind == 1 ? rnd_in(r, 6, 7) : rnd_in(r, 1, 6));
    for (n = 1; n <= 6; n++) {
        uint8_t *op = fm_op(p, n);
        if (fm_is_carrier(p, n)) {
            fm_ratio(op, rnd_chance(r, 50) ? 100 * rnd_in(r, 1, 4) / (rnd_chance(r, 30) ? 2 : 1) : inharm_ratio(r));
            if (kind == 1)                                 /* (stopped short, a click of a tail) */
                fm_env(op, 99, rnd_in(r, 55, 85), rnd_in(r, 20, 60), rnd_in(r, 60, 90), 99, rnd_in(r, 40, 80),
                       rnd_in(r, 0, 50), 0);
            else
                fm_env(op, 99, rnd_in(r, 30, 70), rnd_in(r, 20, 60), rnd_in(r, 40, 80), 99, rnd_in(r, 60, 99),
                       rnd_in(r, 40, 90), 0);
            op[FP_OL] = (uint8_t)rnd_in(r, 88, 99);
            op[FP_AMS] = (uint8_t)(kind == 0 ? 3 : rnd_in(r, 0, 2));
        } else {
            if (kind == 1 || rnd_chance(r, 30))
                fm_fixed(op, rnd_in(r, 1, 3), rnd_below(r, 100));   /* (mechanical: not following the note) */
            else
                fm_ratio(op, rnd_chance(r, 50) ? inharm_ratio(r) : rnd_in(r, 50, 1100));
            fm_env(op, rnd_in(r, 60, 99), rnd_in(r, 30, 90), rnd_in(r, 20, 80), rnd_in(r, 40, 70), 99,
                   rnd_below(r, 99), rnd_below(r, 99), 0);
            op[FP_OL] = (uint8_t)rnd_in(r, kind == 1 ? 80 : 55, 99);
        }
    }
    p[FP_LPMD] = p[FP_LAMD] = 0;
    if (kind == 0) {                                       /* chopped: the level gated, the pitch left alone */
        p[FP_LFW] = (uint8_t)(rnd_chance(r, 60) ? 3 : 5);
        p[FP_LFS] = (uint8_t)rnd_in(r, 45, 85);
        p[FP_LAMD] = (uint8_t)rnd_in(r, 60, 99);
    } else if (kind == 2) {                                /* thrown: the pitch envelope's abrupt steps */
        int i;
        for (i = 0; i < 3; i++) {
            p[FP_PR1 + i] = (uint8_t)rnd_in(r, 85, 99);
            p[FP_PL1 + i] = (uint8_t)(rnd_chance(r, 50) ? rnd_in(r, 38, 47) : rnd_in(r, 53, 62));
        }
        p[FP_PL1 + 2] = (uint8_t)rnd_in(r, 46, 54);
    } else if (kind == 3) {                                /* a slow, uneven wobble */
        p[FP_LFW] = (uint8_t)rnd_below(r, 3);
        p[FP_LFS] = (uint8_t)rnd_in(r, 15, 45);
        p[FP_LPMS] = (uint8_t)rnd_in(r, 2, 4);
        p[FP_LPMD] = (uint8_t)rnd_in(r, 10, 40);
        p[FP_LAMD] = (uint8_t)rnd_in(r, 0, 40);
    } else if (kind == 4) {                                /* the old chatter, gentler: a slower, shallower step */
        p[FP_LFW] = 5;
        p[FP_LFS] = (uint8_t)rnd_in(r, 40, 75);
        p[FP_LPMS] = (uint8_t)rnd_in(r, 3, 5);
        p[FP_LPMD] = (uint8_t)rnd_in(r, 10, 50);
        p[FP_LAMD] = (uint8_t)rnd_in(r, 10, 60);
    }
}
static void fm_generate(fm_patch_t *pt, int32_t f, uint32_t seed, uint32_t path, uint32_t gen)
{
    rnd_t rr, *r = &rr;
    uint8_t *p = pt->p;
    path = path < PATH_N ? path : PATH_OPEN;
    fm_gen_cur = gen;
    rnd_seed(r, seed ^ 0xF00Du);
    if (gen == 0u && (f < 0 || f >= FF_N0))
        f = path == PATH_OPEN ? rnd_below(r, FF_N0) : (int32_t)rnd_weighted(r, FF_W[path], FF_N0);
    else if (gen <= 2u && (f < 0 || f >= FF_N1))
        f = (int32_t)rnd_weighted(r, FF_W1[path], FF_N1);
    else if (gen <= 4u && (f < 0 || f >= FF_N5))
        f = (int32_t)rnd_weighted(r, FF_W3[path], FF_N5);
    else if (gen <= 5u && (f < 0 || f >= FF_N5))
        f = (int32_t)rnd_weighted(r, FF_W5[path], FF_N5);
    else if (f < 0 || f >= FF_N)
        f = (int32_t)rnd_weighted(r, FF_W6[path], FF_N);
    if (gen >= 5u && fm_retired(f))
        f = FF_WARP;                                     /* (a GLISSE or a RUBBER asked for: a WARP, which has them) */
    fm_voice_defaults(p);
    switch (f) {
    case FF_METAL: fm_gen_metal(p, r); break;
    case FF_DRONE: fm_gen_drone(p, r); break;
    case FF_BRUIT: fm_gen_bruit(p, r); break;
    case FF_IMPACT: gen >= 4u ? fm_gen_impact4(p, r) : fm_gen_impact(p, r); break;
    case FF_ITERATIF: fm_gen_iteratif(p, r); break;
    case FF_GLISSE: fm_gen_glisse(p, r); break;
    case FF_SPECTRE: fm_gen_spectre(p, r); break;
    case FF_ABYSS: fm_gen_abyss(p, r); break;
    case FF_FRICTION: fm_gen_friction(p, r); break;
    case FF_CHOIR: fm_gen_choir(p, r); break;
    case FF_BOWED: fm_gen_bowed(p, r); break;
    case FF_HOLLOW: fm_gen_hollow(p, r); break;
    case FF_BLOOM: fm_gen_bloom(p, r); break;
    case FF_TIDE: fm_gen_tide(p, r); break;
    case FF_SHARDS: fm_gen_shards(p, r); break;
    case FF_STATIC: fm_gen_static(p, r); break;
    case FF_PLUCK: fm_gen_pluck(p, r); break;
    case FF_MALLET: fm_gen_mallet(p, r); break;
    case FF_WARP: gen >= 5u ? fm_gen_warp5(p, r) : fm_gen_warp(p, r); break;
    case FF_GLITCH: gen >= 4u ? fm_gen_glitch4(p, r) : fm_gen_glitch(p, r); break;
    case FF_RUBBER: fm_gen_rubber(p, r); break;
    case FF_SHAPER: fm_gen_shaper(p, r); break;
    case FF_LADDER: fm_gen_ladder(p, r); break;
    case FF_REVERSE: fm_gen_reverse(p, r); break;
    case FF_CREEP: fm_gen_creep(p, r); break;
    case FF_CLANG: fm_gen_clang(p, r); break;
    default: fm_gen_wind(p, r); break;
    }
    if (gen >= 1u) {                                     /* generation 1: shaped, bent with care, then leaned */
        fm_shape1(p, path, r);
        if (rnd_chance(r, path == PATH_EXP ? 70 : path == PATH_AMBIENT ? 30 : 45))
            fm_mutate1(p, path, r);
        if (path == PATH_EXP && rnd_chance(r, 35))
            fm_mutate1(p, path, r);
        if (gen >= 3u)
            fm_care3(p, f, path, r);
        if (path == PATH_AMBIENT)
            fm_ambient(p, f, r);
        else if (path == PATH_EXP)
            fm_experimental(p, r);
    } else if (path == PATH_AMBIENT) {
        if (rnd_chance(r, 15)) {                              /* gently: an envelope turned over */
            uint8_t *op = fm_op(p, 1 + rnd_below(r, 6)), t = op[FP_L1];
            op[FP_L1] = (uint8_t)rnd_below(r, 40);
            op[FP_L1 + 2] = t > 60 ? t : 80;
            op[FP_R1 + 2] = (uint8_t)rnd_in(r, 5, 30);
        }
        fm_ambient(p, f, r);
    } else if (path == PATH_EXP) {
        fm_experimental(p, r);
        if (rnd_chance(r, 60))
            fm_mutate(p, r);
        if (rnd_chance(r, 25))
            fm_mutate(p, r);
    } else if (rnd_chance(r, 33))
        fm_mutate(p, r);
    {   /* every carrier sounding, so a patch is never silent: the loudest at full level */
        int n, any = 0;
        for (n = 1; n <= 6; n++)
            if (fm_is_carrier(p, n) && fm_op(p, n)[FP_OL] >= 80)
                any = 1;
        if (!any)
            for (n = 1; n <= 6; n++)
                if (fm_is_carrier(p, n)) {
                    fm_op(p, n)[FP_OL] = 99;
                    if (fm_op(p, n)[FP_L1] < 90)
                        fm_op(p, n)[FP_L1] = 99;
                    break;
                }
    }
    fm_name(p, FF_NAME[f]);
    fm6_sanitize(p);
    pt->family = (uint8_t)f;
    pt->path = (uint8_t)path;
    pt->gen = (uint8_t)gen;
    pt->gain = 4096;
    pt->seed = seed;
}

/* the loudest 46 ms of the patch held on a middle note for 2.5 s, brought to -20 dBFS RMS (Q12 gain); returns
 * how bright it is, Hz (the slope's energy over the signal's, as a frequency: a sine of f gives about f) */
#define FM_TARGET_RMS 3277       /* 0.1, Q15 */
static int32_t fm_measure(fm_patch_t *pt)
{
    static fm_voice_t v;         /* (static: the main loop only; 260 bytes off the stack) */
    int32_t out[FM6_N], prev = 0;
    uint32_t b, i;
    int64_t acc = 0, e = 0, d = 0;
    int64_t best = 0;
    pt->gain = 4096;
    memset(&v, 0, sizeof v);
    fm_note_on(&v, pt, 60, 1);
    for (b = 0; b < 3445u; b++) {          /* 2.5 s of blocks */
        if (!fm_render_fp(&v, pt, out, 0))
            break;
        for (i = 0; i < FM6_N; i++) {
            int32_t dx = out[i] - prev;
            acc += ((int64_t)out[i] * out[i]) >> 8;
            d += ((int64_t)dx * dx) >> 8;
            prev = out[i];
        }
        if ((b & 63u) == 63u) {           /* windows of 64 blocks (2048 samples) */
            e += acc;
            if (acc > best)
                best = acc;
            acc = 0;
        }
#ifndef HOST
        if ((b & 255u) == 255u)
            fm1_wdt_feed();
#endif
    }
    if (acc > best)
        best = acc;
    {
        uint32_t ms = (uint32_t)(best >> 11);              /* the window's mean square (2048 samples), Q22 */
        uint32_t rms = isqrt32(ms) << 4;                   /* Q11 -> Q15 */
        int32_t g = rms ? (int32_t)((uint32_t)FM_TARGET_RMS * 4096u / rms) : 4096 * 8;
        pt->gain = clampi(g, 300, 4096 * 6);               /* at most +15.6 dB: a slow swell stays a swell */
    }
    e += acc;
    while (e > 0xFFFF) {                                  /* (no 64-bit division: both brought into 32 bits) */
        e >>= 1;
        d >>= 1;
    }
    if (e < 16)
        return 0;                                         /* silent */
    {
        uint32_t q12 = (uint32_t)(d * 4096) / (uint32_t)e;   /* <= 4 x 4096 x 4: no overflow */
        return (int32_t)(7019u * isqrt32(q12 << 8) >> 10);   /* 44100 / 2 pi x sqrt(q12 / 4096): Hz */
    }
}
