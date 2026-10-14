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
       FF_CHOIR = FF_N0, FF_BOWED, FF_HOLLOW, FF_BLOOM, FF_TIDE, FF_SHARDS, FF_STATIC, FF_WIND, FF_N };
/* the families' weights on each path: generation 0 (the open path: rnd_below, all alike), generation 1 */
static const uint8_t FF_W[3][FF_N0] = {
    {1, 1, 1, 1, 1, 1, 1, 1, 1},
    {3, 4, 0, 0, 1, 1, 4, 3, 0},
    {2, 1, 4, 3, 3, 2, 0, 1, 4},
};
static const uint8_t FF_W1[3][FF_N] = {
    {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
    {3, 4, 0, 0, 1, 1, 4, 3, 0, 4, 4, 4, 3, 4, 0, 0, 2},
    {2, 1, 4, 3, 3, 2, 0, 1, 4, 0, 0, 1, 2, 0, 4, 4, 3},
};
static const char *const FF_NAME[FF_N] = {"METAL", "DRONE", "BRUIT", "IMPACT", "ITERATIF", "GLISSE", "SPECTRE",
                                          "ABYSS", "FRICTION", "CHOIR", "BOWED", "HOLLOW", "BLOOM", "TIDE",
                                          "SHARDS", "STATIC", "WIND"};

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
static void fm_note_off(fm_voice_t *v, const fm_patch_t *pt)
{
    if (v->live && v->n.down)
        fm6_note_key(&v->n, pt->p, 0);
}

/* one block (CTL samples), Q15 into out; 0 when the voice is silent for good */
static int fm_render(fm_voice_t *v, const fm_patch_t *pt, int32_t *out)
{
    const uint8_t *p = pt->p;
    int32_t bus[FM6_N], lv, ld, lf;
    static const int32_t ZERO[6] = {0, 0, 0, 0, 0, 0};
    uint32_t i;
    if (!v->live)
        return 0;
    if (!v->n.down && fm6_note_done(&v->n, p, p[FP_ALG] & 31u)) {
        v->live = 0;
        return 0;
    }
    lv = fm6_lfo_sample(&v->lfo);
    ld = fm6_lfo_delay(&v->lfo);
    lf = fm6_note_logfreq(v->note * 16, 0);
    if (!fm6_note_compute(&v->n, p, bus, lv, ld, lf, p[FP_ALG] & 31u, p[FP_FB] & 7u, ZERO, 0)) {
        for (i = 0; i < FM6_N; i++)
            out[i] = 0;
        return 1;                                   /* below the threshold now, but not done */
    }
    for (i = 0; i < FM6_N; i++) {
        int32_t x = bus[i] - v->n.dc;              /* Q24 */
        v->n.dc += x >> 10;                        /* a one-pole high-pass, ~7 Hz: FM puts sidebands at 0 Hz */
        out[i] = (int32_t)(((int64_t)x * pt->gain) >> 21);   /* Q24 x Q12 -> Q15 */
    }
    return 1;
}

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
/* a ratio as the DX7 has it: coarse (0 = 0.5, 1..31) x (1 + fine / 100); x in hundredths */
static void fm_ratio(uint8_t *op, int32_t x100)
{
    int32_t c, f;
    op[FP_MODE] = 0;
    if (x100 < 100) {
        op[FP_FC] = 0;
        op[FP_FF] = (uint8_t)clampi((x100 - 50) * 2, 0, 99);
        return;
    }
    c = clampi(x100 / 100, 1, 31);
    f = (x100 * 100 / c) - 100;                     /* the fine that makes c x (1 + f / 100) = x */
    if (f > 99) {
        c = clampi(c + 1, 1, 31);
        f = (x100 * 100 / c) - 100;
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

static uint32_t fm_carriers_of(const uint8_t *p) { return fm6_carriers(p[FP_ALG] & 31u); }
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
            if (f != FF_METAL || rnd_chance(r, 50))           /* swells (a bell may keep its strike) */
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

/* a patch of family f (FF_N: any), from seed, on a path; then fm_measure */
static void fm_generate(fm_patch_t *pt, int32_t f, uint32_t seed, uint32_t path, uint32_t gen)
{
    rnd_t rr, *r = &rr;
    uint8_t *p = pt->p;
    path = path < PATH_N ? path : PATH_OPEN;
    rnd_seed(r, seed ^ 0xF00Du);
    if (gen == 0u && (f < 0 || f >= FF_N0))
        f = path == PATH_OPEN ? rnd_below(r, FF_N0) : (int32_t)rnd_weighted(r, FF_W[path], FF_N0);
    else if (f < 0 || f >= FF_N)
        f = (int32_t)rnd_weighted(r, FF_W1[path], FF_N);
    fm_voice_defaults(p);
    switch (f) {
    case FF_METAL: fm_gen_metal(p, r); break;
    case FF_DRONE: fm_gen_drone(p, r); break;
    case FF_BRUIT: fm_gen_bruit(p, r); break;
    case FF_IMPACT: fm_gen_impact(p, r); break;
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
    default: fm_gen_wind(p, r); break;
    }
    if (gen >= 1u) {                                     /* generation 1: shaped, bent with care, then leaned */
        fm_shape1(p, path, r);
        if (rnd_chance(r, path == PATH_EXP ? 70 : path == PATH_AMBIENT ? 30 : 45))
            fm_mutate1(p, path, r);
        if (path == PATH_EXP && rnd_chance(r, 35))
            fm_mutate1(p, path, r);
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
        if (!fm_render(&v, pt, out))
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
