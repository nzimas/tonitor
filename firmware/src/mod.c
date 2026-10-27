/* SPDX-License-Identifier: GPL-3.0-only
 * TONITOR: the modulation matrix (0.53). Each track has two LFOs and four slots; a slot takes a source (an LFO, the
 * last note's velocity or pitch, a random value drawn at each note) to a destination (one of the sound's four macros,
 * the filter, the grains, the crush, the distortion, the reverb and delay sends, the level, the pan, or an LFO's rate)
 * by an amount, -100 .. +100 of the destination's whole swing. It moves the track's own values, which the engines read
 * every block: nothing is rebuilt, so it costs next to nothing. Worked out in the audio ISR once a block (1378 Hz),
 * before the work is dealt (layers.c blk_render).
 *
 * LFO: SHAPE (SINE, TRI, SAW UP, SAW DN, SQUARE, STEPS -- a new random value each cycle --, DRIFT -- smooth random),
 * RATE (0 .. 100: 0.01 Hz .. 20 Hz, a cycle in 100 s to 20 a second), MODE (FREE; NOTE: from the start at each note;
 * ONCE: one cycle from each note, then held -- an envelope), FADE (0 .. 100: in over 0 .. 10 s from each note).
 *
 * HOME draws a modulation with some sounds (mod_draw): with the slow, sustained families of FM, HIVE, BAYAREA and
 * DRUID mostly, now and then with the others (a velocity on their brightness), never with ANIMA and CALIGULA, whose
 * sound already moves by itself. */

enum { MS_SINE, MS_TRI, MS_SAWU, MS_SAWD, MS_SQR, MS_STEPS, MS_DRIFT, MS_N };
enum { MM_FREE, MM_NOTE, MM_ONCE, MM_N };
enum { MSRC_OFF, MSRC_L1, MSRC_L2, MSRC_VEL, MSRC_KEY, MSRC_RAND, MSRC_N };
enum { MD_MAC1, MD_MAC2, MD_MAC3, MD_MAC4, MD_FLT, MD_GR, MD_CR, MD_DIST, MD_RV, MD_DL, MD_LVL, MD_PAN, MD_RATE1, MD_RATE2,
       MD_N };
static const char *const MS_NAME[MS_N] = {"SINE", "TRI", "SAW UP", "SAW DN", "SQUARE", "STEPS", "DRIFT"};
static const char *const MM_NAME[MM_N] = {"FREE", "NOTE", "ONCE"};
static const char *const MSRC_NAME[MSRC_N] = {"OFF", "LFO 1", "LFO 2", "VELOCITY", "KEY", "RANDOM"};
static const char *const MD_NAME[MD_N] = {"", "", "", "", "FILTER", "GRAINS", "CRUSH", "DRIVE", "REVERB", "DELAY", "LEVEL",
                                          "PAN", "LFO 1 RATE", "LFO 2 RATE"};   /* (the macros: by the sound's names) */
/* each destination's whole swing (amount 100), in its own units */
static const int16_t MD_SPAN[MD_N] = {MAC_MAX, MAC_MAX, MAC_MAX, MAC_MAX, FLT_MAX, FLT_MAX, FLT_MAX, 100, 100, 100, 12, 20,
                                      50, 50};

typedef struct { uint8_t shape, rate, mode, fade; } mlfo_t;
typedef struct { uint8_t src, dst; int8_t amt; } mslot_t;
#define NMSLOT 4u
typedef struct {
    mlfo_t lfo[2];
    mslot_t s[NMSLOT];
} mod_t;                                         /* (a track's: 20 bytes; sessions keep it) */
static mod_t lay_mod[NLAYER];                    /* (ui.c sets it, the ISR reads it) */

static struct {
    uint32_t ph[2];
    int32_t a[2], b[2];                          /* STEPS' and DRIFT's values: the last, the next */
    int32_t fade[2];                             /* each LFO's fade in since the last note, Q16 0 .. 1 */
    int32_t val[2];                              /* each LFO now, Q15 -1 .. 1 */
    uint8_t held[2];                             /* ONCE: done */
    uint8_t vel, key;
    int16_t rnd;
    int16_t off[MD_N];                           /* each destination's offset, its units */
} mst[NLAYER];
static uint32_t mod_rs = 0x9E3779B9u;
static int32_t mod_rand(void)                    /* -Q15 .. Q15 */
{
    mod_rs = mod_rs * 1664525u + 1013904223u;
    return (int32_t)(mod_rs >> 16) - 32768;
}
static uint32_t mod_inc(int32_t rate)            /* 0.01 Hz x 2000^(rate / 100), a block's phase step */
{
    return (uint32_t)(((uint64_t)31166u * exp2_q16(clampi(rate, 0, 100) * 7189)) >> 16);
}
/* a note on track l (the ISR's note_on): what it holds for the sources, and the LFOs started again as their modes say */
static void mod_note(uint32_t l, uint32_t note, uint32_t vel)
{
    uint32_t k;
    mst[l].vel = (uint8_t)(vel ? vel : 127u);
    mst[l].key = (uint8_t)note;
    mst[l].rnd = (int16_t)mod_rand();
    mst[l].fade[0] = mst[l].fade[1] = 0;
    for (k = 0; k < 2u; k++)
        if (lay_mod[l].lfo[k].mode != MM_FREE) {
            mst[l].ph[k] = 0;
            mst[l].held[k] = 0;
        }
}
static int mod_on(uint32_t l)                    /* (any slot in use) */
{
    uint32_t i;
    for (i = 0; i < NMSLOT; i++)
        if (lay_mod[l].s[i].src && lay_mod[l].s[i].amt)
            return 1;
    return 0;
}
/* the block: track l's LFOs a step on, the offsets worked out */
static void mod_block(uint32_t l)
{
    const mod_t *m = &lay_mod[l];
    uint32_t k, i;
    int32_t ro[2] = {mst[l].off[MD_RATE1], mst[l].off[MD_RATE2]};   /* (the LFOs' rates as the last block moved them) */
    memset(mst[l].off, 0, sizeof mst[l].off);
    if (!mod_on(l))
        return;
    for (k = 0; k < 2u; k++) {
        const mlfo_t *f = &m->lfo[k];
        uint32_t p0 = mst[l].ph[k], inc = mod_inc((int32_t)f->rate + ro[k]), p1 = p0 + inc, t;
        int wrap = p1 < p0;
        int32_t v;
        if (mst[l].held[k])
            p1 = p0;
        else if (wrap && f->mode == MM_ONCE) {
            p1 = 0xFFFFFFFFu;                    /* (one cycle: held at its end) */
            mst[l].held[k] = 1;
        }
        if (wrap && !mst[l].held[k]) {           /* (a new cycle: STEPS' and DRIFT's next value) */
            mst[l].a[k] = mst[l].b[k];
            mst[l].b[k] = mod_rand();
        }
        mst[l].ph[k] = p1;
        t = p1 >> 16;
        switch (f->shape) {
        case MS_TRI:
            v = t < 32768u ? (int32_t)t * 2 - 32768 : 32767 - ((int32_t)t - 32768) * 2;
            break;
        case MS_SAWU:
            v = (int32_t)t - 32768;
            break;
        case MS_SAWD:
            v = 32767 - (int32_t)t;
            break;
        case MS_SQR:
            v = t < 32768u ? 32767 : -32768;
            break;
        case MS_STEPS:
            v = mst[l].b[k];
            break;
        case MS_DRIFT:
            v = mst[l].a[k] + (int32_t)(((int64_t)(mst[l].b[k] - mst[l].a[k]) * (int32_t)t) >> 16);
            break;
        default:
            v = sin_ph(p1);
            break;
        }
        if (mst[l].fade[k] < Q16)                /* (FADE: in over 0.1 s a step, 138 blocks) */
            mst[l].fade[k] += f->fade ? (int32_t)(65536u / (f->fade * 138u) + 1u) : Q16;
        if (mst[l].fade[k] > Q16)
            mst[l].fade[k] = Q16;
        mst[l].val[k] = (int32_t)(((int64_t)v * mst[l].fade[k]) >> 16);
    }
    for (i = 0; i < NMSLOT; i++) {
        const mslot_t *s = &m->s[i];
        int32_t v, o;
        if (!s->src || !s->amt || s->dst >= MD_N)
            continue;
        if (s->src == MSRC_L1 || s->src == MSRC_L2)
            v = mst[l].val[s->src - MSRC_L1];
        else if (s->src == MSRC_VEL)
            v = ((int32_t)mst[l].vel - 127) * 258;   /* (0 at full, down to -1 for the softest: softer, less) */
        else if (s->src == MSRC_KEY)
            v = clampi(((int32_t)mst[l].key - 60) * 683, -Q15, Q15);   /* (+- 4 octaves around C4: +-1) */
        else
            v = mst[l].rnd;
        o = v * s->amt * MD_SPAN[s->dst] / (Q15 * 100);   /* (32 bits: at most 32768 x 100 x 100) */
        mst[l].off[s->dst] = (int16_t)clampi(mst[l].off[s->dst] + o, -400, 400);
    }
}
static int32_t mod_val(uint32_t l, uint32_t d, int32_t base, int32_t lo, int32_t hi)   /* a value, moved */
{
    return mst[l].off[d] ? clampi(base + mst[l].off[d], lo, hi) : base;
}

static void mod_check(void)                      /* (as read from a session: each value in its range, or off) */
{
    uint32_t l, k;
    for (l = 0; l < NLAYER; l++) {
        mod_t *m = &lay_mod[l];
        for (k = 0; k < 2u; k++)
            if (m->lfo[k].shape >= MS_N || m->lfo[k].rate > 100u || m->lfo[k].mode >= MM_N || m->lfo[k].fade > 100u)
                memset(&m->lfo[k], 0, sizeof m->lfo[k]);
        for (k = 0; k < NMSLOT; k++)
            if (m->s[k].src >= MSRC_N || m->s[k].dst >= MD_N || m->s[k].amt < -100 || m->s[k].amt > 100)
                memset(&m->s[k], 0, sizeof m->s[k]);
    }
}

/* ------------------------------------------------------- HOME's draws --- */
/* how likely a sound of each family is to come with a modulation (%): the slow and sustained most */
static const uint8_t MODP_FM[FF_N] = {10, 80, 10, 5, 10, 30, 70, 70, 40, 70, 60, 50, 70, 40, 10, 40, 50, 10, 10, 15, 10, 10,
                                     60, 50, 50, 70, 10};
static const uint8_t MODP_HV[HK_N] = {20, 25, 10, 70, 80, 70, 50, 20, 40, 60, 50, 30};
static const uint8_t MODP_BA[BK_N] = {5, 10, 20, 30, 70, 80};
static const uint8_t MODP_DR[DK_N] = {10, 60, 10, 20, 20, 15, 80, 20, 50, 30};
static const uint8_t MODP_OB[OBK_N] = {60, 30, 70, 10, 40, 50, 20, 70, 10, 60};
/* each engine's macros for the tone (the first) and the texture (the second) */
static const uint8_t MOD_TONE[NENG][2] = {{0, 1}, {0, 1}, {0, 1}, {0, 1}, {1, 3}, {0, 1}, {3, 0}, {0, 1}};
static int mod_sustained(uint32_t e, uint32_t kind)   /* (a family that holds: slow modulation; else velocity) */
{
    return e == ENG_FM ? MODP_FM[kind % FF_N] >= 40 : e == ENG_HIVE ? MODP_HV[kind % HK_N] >= 40
           : e == ENG_BAY ? kind >= 4u : e == ENG_DRUID ? MODP_DR[kind % DK_N] >= 50
           : e == ENG_OB ? MODP_OB[kind % OBK_N] >= 40 : 0;
}
/* track l's modulation, drawn with its new sound (engine e, family kind, its seed and path): or none */
static void mod_draw(uint32_t l, uint32_t e, uint32_t kind, uint32_t seed, uint32_t path)
{
    rnd_t r;
    mod_t m;
    uint32_t p = e == ENG_FM ? MODP_FM[kind % FF_N] : e == ENG_HIVE ? MODP_HV[kind % HK_N] : e == ENG_BAY ? MODP_BA[kind % BK_N]
                 : e == ENG_DRUID ? MODP_DR[kind % DK_N] : e == ENG_OB ? MODP_OB[kind % OBK_N] : 0u;
    memset(&m, 0, sizeof m);
    rnd_seed(&r, seed ^ 0x5EEDu * 31u ^ kind * 977u);
    if (path == PATH_AMBIENT && p)
        p = p + (100u - p) / 4u;                 /* (ambient: a little more often) */
    if ((uint32_t)rnd_below(&r, 100) >= p) {
        lay_mod[l] = m;
        return;
    }
    if (mod_sustained(e, kind)) {                /* slow: LFO 1 on the tone, sometimes LFO 2 on the place in the room */
        int32_t slow = path == PATH_AMBIENT ? 0 : path == PATH_EXP ? 25 : 10;
        static const uint8_t SH[4] = {MS_SINE, MS_TRI, MS_DRIFT, MS_DRIFT};
        m.lfo[0].shape = path == PATH_EXP && rnd_chance(&r, 30) ? MS_STEPS : SH[rnd_below(&r, 4)];
        m.lfo[0].rate = (uint8_t)rnd_in(&r, 10 + slow, 32 + slow);   /* (~0.02 .. 0.2 Hz; quicker on EXPERIMENTAL) */
        m.lfo[0].mode = MM_FREE;
        m.lfo[0].fade = (uint8_t)(rnd_chance(&r, 40) ? rnd_in(&r, 20, 60) : 0);
        m.s[0].src = MSRC_L1;
        m.s[0].dst = MOD_TONE[e][rnd_chance(&r, 70) ? 0 : 1];
        m.s[0].amt = (int8_t)(rnd_in(&r, 15, 40) * (rnd_chance(&r, 50) ? 1 : -1));
        if (rnd_chance(&r, 45)) {
            m.lfo[1].shape = MS_DRIFT;
            m.lfo[1].rate = (uint8_t)rnd_in(&r, 5, 20);
            m.s[1].src = MSRC_L2;
            m.s[1].dst = rnd_chance(&r, 60) ? MD_PAN : MD_RV;
            m.s[1].amt = (int8_t)rnd_in(&r, 20, 50);
        }
        if (path == PATH_EXP && rnd_chance(&r, 30)) {   /* (and now and then LFO 1's pace wanders) */
            m.s[2].src = MSRC_RAND;
            m.s[2].dst = MD_RATE1;
            m.s[2].amt = (int8_t)rnd_in(&r, 20, 50);
        }
    } else {                                     /* struck, plucked: the softer, the duller */
        m.s[0].src = MSRC_VEL;
        m.s[0].dst = MOD_TONE[e][0];
        m.s[0].amt = (int8_t)rnd_in(&r, 20, 45);
    }
    lay_mod[l] = m;
}

/* SHIFT + SEQ: track l's modulation thrown anew -- both LFOs and two to four slots, from any source to any
 * destination, by amounts that may go far: what HOME draws stays near the sound; this takes risks (seed: a new one) */
static void mod_random(uint32_t l, uint32_t seed)
{
    static const uint8_t DW[MD_N] = {10, 10, 10, 10, 12, 6, 5, 6, 7, 7, 4, 8, 3, 3};   /* (the destinations' odds) */
    static const uint8_t SW[MSRC_N] = {0, 35, 30, 10, 10, 15};
    rnd_t r;
    mod_t m;
    uint32_t k, n, i;
    memset(&m, 0, sizeof m);
    rnd_seed(&r, seed);
    for (k = 0; k < 2u; k++) {
        mlfo_t *f = &m.lfo[k];
        f->shape = (uint8_t)rnd_below(&r, MS_N);
        f->rate = (uint8_t)(k ? rnd_in(&r, 0, 55) : rnd_in(&r, 5, rnd_chance(&r, 25) ? 90 : 70));   /* (LFO 2 slower) */
        f->mode = (uint8_t)(rnd_chance(&r, 60) ? MM_FREE : rnd_chance(&r, 60) ? MM_NOTE : MM_ONCE);
        f->fade = (uint8_t)(rnd_chance(&r, 60) ? 0 : rnd_in(&r, 10, 60));
    }
    n = 2u + (uint32_t)rnd_below(&r, 3);
    for (k = 0; k < n; k++) {
        mslot_t *s = &m.s[k];
        uint32_t tries, d = 0;
        int32_t mag;
        s->src = (uint8_t)rnd_weighted(&r, SW, MSRC_N);
        for (tries = 0; tries < 6u; tries++) {   /* (a destination not yet taken, and no LFO on its own rate) */
            d = rnd_weighted(&r, DW, MD_N);
            if ((d == MD_RATE1 && s->src == MSRC_L1) || (d == MD_RATE2 && s->src == MSRC_L2))
                continue;
            for (i = 0; i < k && m.s[i].dst != d; i++)
                ;
            if (i == k)
                break;
        }
        s->dst = (uint8_t)d;
        mag = d == MD_LVL ? rnd_in(&r, 10, 40) : d == MD_PAN ? rnd_in(&r, 20, 70) : rnd_in(&r, 15, 70);
        s->amt = (int8_t)(mag * (rnd_chance(&r, 50) ? 1 : -1));
    }
    lay_mod[l] = m;
}
