/* SPDX-License-Identifier: GPL-3.0-only
 * TONITOR: the six layers, and their loopers.
 *
 * A layer plays one engine (FM, ANIMA or CALIGULA), with a sound of each engine of its own, so turning the
 * engine back finds the sound it had. FM and ANIMA play on voices drawn from one pool of NVOICE for all the
 * layers: a note takes a free voice, whichever layer it is on, and lets the others ring, so a layer has as many
 * voices as its notes need (a chord on one, a single line on another). With none free, a note takes the tail
 * that has been ringing longest (any layer's), else the layer's own oldest held note, else the oldest held note
 * of all -- faded out over FADE blocks (~9 ms) first. CALIGULA is one organism a layer, outside the pool.
 *
 * The main loop owns the choices (which engine, which sounds); the audio ISR owns the voices. Between them:
 *   - events (a ring, main loop -> ISR): a note on or off, an engine change, a stop, the looper's commands;
 *   - the sounds, two buffers a layer and engine: the main loop writes the one the ISR is not using and
 *     publishes it (pub); the ISR takes it at a block start (use = pub). The main loop writes again only
 *     once the ISR has taken the last one (pub == use), so neither ever reads a half-written sound.
 * A change of engine or sound under sounding voices fades the whole layer out over FADE blocks, then makes
 * the change, and plays again, with what is new, the notes still held.
 *
 * Every layer has a looper of up to LOOP_TRK loops, all free and each on its own clock: no tempo, no grid,
 * no two loops in step, not even a layer's own -- each goes round in its own length, so they drift apart.
 *   REC        arms a new loop (the others go on playing); the first note played on the layer starts it;
 *              every note played is kept, to the block (0.73 ms)
 *   REC again  ends it: the loop is as long as from that first note to now, and it plays at once, for ever
 *              (with the layer's loops stopped, they all start again with it)
 *   PLAY       stops the layer's loops, or plays them again, each from its start
 *   REC held   empties the looper: every loop of the layer
 * A loop's notes play on the layer's voices as the keys' do, and stay the loop's: a key lifted never ends a
 * note the loop is playing, nor the loop a note a key holds. The keys go on playing the layer over its loop. */

#define NLAYER 6
#define NVOICE 12                /* the voices of all the layers (FM and ANIMA) */
enum { ENG_FM, ENG_ANIMA, ENG_CAL, ENG_RUS, ENG_DRUID, ENG_BAY, ENG_HIVE, ENG_OB, NENG };
static const char *const ENG_NAME[NENG] = {"FM", "ANIMA", "CALIGULA", "RUSSOLO", "DRUID", "BAYAREA", "HIVE", "OBRADOR"};
#include "midi.c"                /* (the MIDI settings, the notes sent out) */

static fm_patch_t fm_buf[NLAYER][2];
static an_design_t an_buf[NLAYER][2];
static volatile uint8_t fm_pub[NLAYER], fm_use[NLAYER], an_pub[NLAYER], an_use[NLAYER];
/* CALIGULA (cal.c): one organism a layer, played by its notes as the other engines are */
static cal_design_t cal_buf[NLAYER][2];
static volatile uint8_t cal_pub[NLAYER], cal_use[NLAYER];
static cal_t cal_rt[NLAYER] POOL;              /* (11 KB: in the pool, beside the voices -- .bss is nearly full) */
/* RUSSOLO (russolo.c): one organism a layer too */
static rus_design_t rus_buf[NLAYER][2];
static volatile uint8_t rus_pub[NLAYER], rus_use[NLAYER];
static rus_t rus_rt[NLAYER];
/* DRUID (druid.c): strings and resonant bodies, voices of the pool as FM and ANIMA */
static dr_design_t dr_buf[NLAYER][2];
static volatile uint8_t dr_pub[NLAYER], dr_use[NLAYER];
/* BAYAREA (bayarea.c): a West Coast voice, voices of the pool as well */
static ba_design_t ba_buf[NLAYER][2];
static volatile uint8_t ba_pub[NLAYER], ba_use[NLAYER];
/* HIVE (hive.c): a swarm of oscillators, voices of the pool as well */
static hv_design_t hv_buf[NLAYER][2];
static volatile uint8_t hv_pub[NLAYER], hv_use[NLAYER];
/* OBRADOR (obrador.c): an analog polysynth voice, voices of the pool as well */
static ob_design_t ob_buf[NLAYER][2];
static volatile uint8_t ob_pub[NLAYER], ob_use[NLAYER];
/* an organism engine (one for the layer, outside the voices' pool) */
static int eng_organism(uint32_t e) { return e == ENG_CAL || e == ENG_RUS; }

/* the pool's voices' own memory, ~4.8 KiB each: an ANIMA voice (its resonator) or a DRUID one (its strings) --
 * a voice is one engine at a time (voice_t.eng) */
typedef union { an_voice_t an; dr_voice_t dr; ba_voice_t ba; hv_voice_t hv; ob_voice_t ob; } vmem_t;
typedef char dr_fits_an_voice[sizeof(dr_voice_t) <= sizeof(an_voice_t) && sizeof(ba_voice_t) <= sizeof(an_voice_t) &&
                              sizeof(hv_voice_t) <= sizeof(an_voice_t) && sizeof(ob_voice_t) <= sizeof(an_voice_t) ? 1 : -1];
static vmem_t vmem[NVOICE] POOL;
static void dr_measure(dr_design_t *d) { dr_measure_on(d, (dr_voice_t *)(void *)&an_probe); }
static void ba_measure(ba_design_t *d) { ba_measure_on(d, (ba_voice_t *)(void *)&an_probe); }
static void ob_measure(ob_design_t *d) { ob_measure_on(d, (ob_voice_t *)(void *)&an_probe); }
static void hv_measure(hv_design_t *d) { hv_measure_on(d, (hv_voice_t *)(void *)&an_probe); }   /* (ANIMA's probe:
                                                                                               * never both at once) */

#define FADE 12u                 /* blocks of a fade (~9 ms) */
enum { SRC_KEY, SRC_LOOP };      /* who plays a note: the keys (and MIDI), or the looper */
typedef struct {
    uint8_t note, gate, src;     /* the note, held, by whom */
    uint8_t sounding;            /* the voice renders */
    uint8_t fade;                /* blocks of a steal's fade left, 0 = none */
    uint8_t pend;                /* the fade ends in a note-on: note / gate / src are the new note's */
    uint8_t layer;               /* whose it is: it renders on that layer, with its engine */
    uint8_t pend_layer;          /* (pend: whose the new note is -- the layer it goes to once faded) */
    uint32_t age;                /* note-on order */
    uint32_t born;               /* blk_clock at its note-on */
    uint16_t cost;               /* what a block of it took, last time (ticks) */
    uint8_t eng;                 /* the engine its memory (vmem) is playing */
    uint8_t vel;                 /* its note's level, 1 .. 127 (127: full -- the keys, the loops; less: the balls) */
    uint8_t est;                 /* the track's own envelope (AE_*): where this voice is in it, its level Q24 */
    int32_t elv;
    uint8_t fade_n;              /* the fade's length, when not FADE (the budget's quick one) */
    uint16_t quiet, rt;          /* let go: blocks far below its loudest; blocks into this stretch of its tail */
    uint8_t nst;                 /* .. the stretches of its tail so far */
    uint8_t latched;             /* its note latched (SHIFT): the overload guard never lets it go */
    uint32_t lvl;                /* its last block's peak (the quietest let go first, when one must go) */
    uint32_t pk, wmax, rmax;     /* its loudest block (peak); this stretch's loudest; the stretch before's */
    fm_voice_t fm;
} voice_t;
#define TAIL_FLOOR 11u           /* let go, a voice 66 dB (2^11) under its loudest .. */
#define TAIL_QUIET 276u          /* .. for 0.2 s ends; */
#define TAIL_STRETCH 2756u       /* one that has not fallen 1.5 dB in 2 s (stuck ringing: a model's resonance, a
                                  * level held after release) ends too -- each faded out over .. */
#define TAIL_FADE 255u           /* .. 0.19 s; and none rings on more than .. */
#define TAIL_MAX 15u             /* .. 15 stretches: 30 s after it was let go */

/* the tracks' own envelope (the ENVELOPE page): A D S R, 0 .. 100 each, while on (lay_env_on) the track's sounds'
 * own amplitude envelope is held at full and this one shapes them -- attack a line, decay a curve to the sustain,
 * release a curve to -60 dB; on CALIGULA and RUSSOLO the organism held through the release, then let go */
static volatile int8_t lay_env[NLAYER][4];
static volatile uint8_t lay_env_on[NLAYER];
enum { AE_OFF, AE_ATT, AE_DEC, AE_REL, AE_DONE };
static const int32_t ENV_BASE[4] = {1, 5, 0, 5};          /* ms at 0 */
static const int32_t ENV_OCT[4] = {8708, 7187, 0, 7842};  /* log2 of the range (x 10000, 2000, -, 4000) x 2^16 / 100 */
static int32_t env_ms(uint32_t j, int32_t v)   /* A, D, R: ms of a knob position 0 .. 100 */
{
    return (int32_t)(((uint64_t)(uint32_t)ENV_BASE[j] * exp2_q16(clampi(v, 0, 100) * ENV_OCT[j])) >> 16);
}
static int32_t env_val(uint32_t j, int32_t ms)   /* .. and back: the knob position nearest ms */
{
    int32_t d = log2_q16((uint32_t)clampi(ms, 1, 60000)) - log2_q16((uint32_t)ENV_BASE[j]);
    return clampi((d + ENV_OCT[j] / 2) / ENV_OCT[j], 0, 100);
}
static int32_t cal_ex(int32_t ms);
static void env_step(uint8_t *st, int32_t *lv, uint32_t l)   /* a block of it */
{
    int32_t s = lay_env[l][2] * ((1 << 24) / 100);
    switch (*st) {
    case AE_ATT:
        *lv += (int32_t)(12173000u / (uint32_t)clampi(env_ms(0, lay_env[l][0]), 1, 20000));   /* 2^24 x 0.7256 / ms */
        if (*lv >= 1 << 24) {
            *lv = 1 << 24;
            *st = AE_DEC;
        }
        break;
    case AE_DEC:
        *lv = s + (int32_t)(((int64_t)(*lv - s) * cal_ex_fp(env_ms(1, lay_env[l][1]))) >> 16);
        break;
    case AE_REL:
        *lv = (int32_t)(((int64_t)*lv * cal_ex_fp(env_ms(3, lay_env[l][3]))) >> 16);
        if (*lv < (1 << 24) / 1000) {
            *lv = 0;
            *st = AE_DONE;
        }
        break;
    default:
        break;
    }
}
static void env_apply(int32_t *x, int32_t g0, int32_t g1)   /* Q24 levels, a block's line between them */
{
    uint32_t i;
    int32_t a = g0 >> 9, d = ((g1 >> 9) - a) / (int32_t)CTL;
    for (i = 0; i < CTL; i++) {
        a += d;
        x[i] = mulq15(x[i], a);
    }
}
static uint8_t org_est[NLAYER];          /* the organism's place in it, its level */
static int32_t org_elv[NLAYER];
static uint8_t org_defer[NLAYER], org_dsrc[NLAYER];   /* the last note let go, kept held through the release (+ 1) */

/* the budget: what the voices and organisms cost, measured as they render, so that a start that would overrun
 * the block's time makes room first (the oldest release tails let go, quickly) instead of the audio arriving
 * late -- a burst. On the host a model stands in for the clock (tests set it) */
#ifdef HOST
static uint32_t host_cost_clock, host_cost_model[8];
#define COST_NOW() host_cost_clock
#define COST_ADD(e) (host_cost_clock += host_cost_model[(e) & 7u])
#define BLK_TICKS 1000u
#else
#define COST_NOW() fm1_ticks()
#define COST_ADD(e) ((void)0)
#define BLK_TICKS ((uint32_t)CTL * 1000000u / FS * FM1_TICKS_PER_US)   /* a block's time */
#endif
#define BUDGET (BLK_TICKS * 82u / 100u)  /* what a block may be expected to take (the rest: the panel, USB, the screen) */
#define QUICK 8u                         /* blocks of the budget's fade (~6 ms: the quietest tail, so no click) */
static uint32_t eng_cost[8];             /* a voice (an organism) of each engine: its usual cost, ticks */
static uint32_t org_cost[NLAYER];        /* each layer's organism, last block */
static uint32_t blk_last, blk_new;       /* the last block's cost; what has just started since */
static voice_t vox[NVOICE];
/* the layer a voice's note (note, gate, src) is of: while it fades to be taken, the one it goes to */
static uint32_t vox_owner(const voice_t *v) { return v->pend ? v->pend_layer : v->layer; }
typedef struct {
    uint8_t engine;              /* what sounds */
    uint8_t want_engine;
    uint8_t xfade;               /* blocks of the layer's fade left (a change of engine or sound) */
    int32_t meter;               /* |output| peak, decaying (Q15): the screen's */
    int32_t lim_env, lim_g;      /* the limiter: the peak it follows, the gain of the last block (Q15) */
    int32_t g, pl, pr, sd, dsd;  /* the level, the pan and the sends as the last block ended (they glide) */
    svf_t flt;                   /* the DJ filter (KNOB 1) */
    int32_t f_oct, f_q, f_amt;   /* its cutoff (log2 Hz, Q16), Q (Q12) and wet share (Q15) as they glide */
    uint8_t f_hp;                /* high-pass (else low-pass) */
    uint32_t cr_ph;              /* the crush's sample-rate reduction: its phase, the sample it holds */
    int32_t cr_hold;
} layer_t;
static layer_t lay[NLAYER];

/* ------------------------------------------------------------ looper --- */
#define LOOP_N 1024u             /* note events a track's loops hold between them (~500 notes) */
#define LOOP_TRK 10u             /* loops a layer */
enum { LR_IDLE, LR_ARMED, LR_REC };
/* an event: t (24 bits, blocks from the loop's start), the note (7 bits), on (bit 31) */
#define LE_T(e) ((e) & 0xFFFFFFu)
#define LE_NOTE(e) (((e) >> 24) & 0x7Fu)
#define LE_ON(e) ((e) >> 31)
#define LE(t, note, on) (((t) & 0xFFFFFFu) | ((uint32_t)((note) & 0x7Fu) << 24) | ((uint32_t)((on) != 0) << 31))
typedef struct {
    uint32_t len;                /* blocks of loop time */
    int32_t pos;                 /* where it is: the block of loop time it plays next */
    uint16_t first, n;           /* its events in the looper's ev[] */
    int16_t ix;                  /* the next event to play: forwards the first after those played, backwards the last
                                  * before them (-1: none) */
    int8_t xb;                   /* the loops' transposition when it was recorded (it plays at lx_steps - xb) */
    uint8_t wrap;                /* it has just gone round (its notes tied over the end are still sounding) */
    uint8_t rgo;                 /* RANDOM: a note has been struck since the last jump */
    int8_t dir;                  /* 1 forwards, -1 backwards (the looper's direction; PENDULUM turns it at the ends) */
    uint32_t snd[4];             /* the notes it sounds now (as recorded, before the transposition) */
} loop_trk_t;
enum { LD_FWD, LD_REV, LD_PEND, LD_RAND };   /* the direction a looper plays its loops in (RANDOM: forwards, jumping
                                              * to a note drawn at random each time the next is due) */
enum { LS_NORMAL, LS_HALF, LS_DOUBLE };   /* .. and its speed */
typedef struct {
    volatile uint8_t rec;        /* LR_*: the screen's and the LEDs' */
    volatile uint8_t play;       /* the loops go round */
    volatile uint8_t ntrk;       /* loops kept */
    volatile uint16_t mute;      /* the loops held out (PLAY held, a white key each): a bit each */
    uint8_t nheld;
    uint8_t held[16];            /* notes on in the recording, not yet off */
    uint32_t start;              /* the loop clock when the recording began */
    uint32_t lclk;               /* the loop clock: half blocks of loop time, 1 a block at half speed, 2, 4 at double */
    uint8_t dmode, rate;         /* LD_*, LS_* (the 4th .. 8th black keys) */
    uint8_t vary;                /* SHIFT + REC: each note it plays at a level and for a length drawn at random */
    uint32_t rnd;                /* RANDOM's draws */
    uint32_t nev;                /* events used, every loop's */
    loop_trk_t trk[LOOP_TRK];
    uint32_t ev[LOOP_N];
} looper_t;
static looper_t loop[NLAYER] POOL;
#define VQ_N 32u                 /* VARY: a looper's notes waiting to be struck, and struck waiting to be let go */
typedef struct { uint16_t wait, len; uint8_t note, vel; } vq_t;   /* (len 0: a free place) */
static vq_t vq[NLAYER][VQ_N];
static struct { uint8_t note; uint16_t left; } vo[NLAYER][VQ_N];
static uint32_t vs_blk[NLAYER];          /* VARY's strum: the block a loop note was last struck in, the strum's */
static uint8_t vs_idx[NLAYER], vs_on[NLAYER];
static uint16_t vs_gap[NLAYER];
static uint32_t blk_clock;
#if TELEMETRY
static uint32_t tele_ev;                 /* this half: the events taken (bit = type), loop notes (bit 20), wraps (21) */
static uint32_t tele_non;                /* .. notes struck */
static volatile uint32_t tele_trims;     /* tails the budget let go */
#define TELE(b) (tele_ev |= 1u << (b))
#else
#define TELE(b) ((void)0)
#endif
/* a note tied over the loop's end: one note held from its first block to its last (held when the recording ended:
 * ended with the last block, REC's). Going round, it is neither let go at the end nor struck again at the start: a
 * drone stays one note. The same note struck at the start, let go, and struck again to be held at the end is two
 * notes, not one: the first is struck at every round (its own end comes first), the last let go at the end, its
 * release ringing on into the next round */
static int trk_tied(const looper_t *p, const loop_trk_t *t, uint32_t e)
{
    uint32_t i, n = LE_NOTE(e);
    if (LE_ON(e) ? LE_T(e) != 0u : LE_T(e) != t->len - 1u)
        return 0;
    if (LE_ON(e)) {
        for (i = 0; i < t->n; i++) {              /* (its own end: the note's first end) */
            uint32_t f = p->ev[t->first + i];
            if (!LE_ON(f) && LE_NOTE(f) == n)
                return LE_T(f) == t->len - 1u;
        }
    } else
        for (i = t->n; i-- > 0u;) {               /* (its own start: the note's last start) */
            uint32_t f = p->ev[t->first + i];
            if (LE_ON(f) && LE_NOTE(f) == n)
                return LE_T(f) == 0u;
        }
    return 0;
}
/* a loop from its start in its looper's direction (at its end, backwards) */
static void trk_start(const looper_t *p, loop_trk_t *t)
{
    t->dir = (int8_t)(p->dmode == LD_REV ? -1 : 1);
    t->pos = t->dir > 0 ? 0 : (int32_t)t->len - 1;
    t->ix = (int16_t)(t->dir > 0 ? 0 : (int32_t)t->n - 1);
    t->wrap = 0;
    t->rgo = 0;
    memset(t->snd, 0, sizeof t->snd);
}
/* the loops' transposition (SHIFT + OCT): every loop moved by steps of the scale, its notes walked up or down the
 * scale's pitch classes (a mask from C); a note off the scale keeps its distance from the scale note below it.
 * The ISR's; the main loop asks for a change with lx_want_* and EV_LOOP_XP */
static int8_t lx_steps;
static int8_t lx_tr[NLAYER];                     /* each track's own, on top (SHIFT + its button held + OCT) */
static uint16_t lx_mask = 0xFFFu;
static volatile int8_t lx_want_steps;
static volatile int8_t lx_want_tr[NLAYER];
static volatile uint16_t lx_want_mask = 0xFFFu;
static uint32_t lx_map(uint32_t n, int32_t k, uint32_t mask)
{
    int32_t b = (int32_t)n;
    mask &= 0xFFFu;
    if (!k || !mask)
        return n;
    while (b > 0 && !((mask >> (b % 12)) & 1u))   /* the scale note at or below it */
        b--;
    n -= (uint32_t)b;                             /* (how far it is above it) */
    for (; k > 0; k--)
        do
            b++;
        while (!((mask >> (b % 12)) & 1u) && b < 140);
    for (; k < 0; k++)
        do
            b--;
        while (b > 0 && !((mask >> (b % 12)) & 1u));
    return (uint32_t)clampi(b + (int32_t)n, 0, 127);
}       /* blocks since boot (the ISR) */

/* each layer's level and place in the stereo field, set by the main loop (SELECT, ALGORITHM): the gain Q14
 * (1.0 = 16384) and the two sides of an equal-power pan, Q15. The ISR glides to them over a block. */
static volatile int32_t lay_gain[NLAYER], lay_pl[NLAYER], lay_pr[NLAYER];
static volatile int16_t lay_duck[NLAYER];   /* a snapshot's morph: the layer's level lowered by this, Q15 (0: none; snaps.c) */
static volatile int32_t lay_send[NLAYER];   /* the send to the reverb, after the level, Q15 (KNOB 4) */
static volatile int32_t lay_dsend[NLAYER];  /* .. to the delay (SHIFT + KNOB 4) */
/* each layer's DJ filter (KNOB 1), -FLT_MAX .. FLT_MAX: 0 off; left a low-pass coming down from 20 kHz to
 * ~35 Hz, right a high-pass going up from 16 Hz to ~12 kHz, more resonant the further from the middle. A
 * state-variable filter: its pass band stays at full level whatever the resonance (no thinning of the body as
 * the peak rises); the peak itself is the limiter's after it. Near the middle the filter fades in and out, so
 * crossing it never clicks */
#define FLT_MAX 64
static volatile int8_t lay_flt[NLAYER];
static volatile int8_t lay_mac[NLAYER][NMAC];   /* EDIT's macros on the sound the layer plays (ui.c keeps them) */
static volatile int8_t lay_gr[NLAYER];   /* each layer's granulator (KNOB 2), -FLT_MAX .. FLT_MAX: 0 off (gran.c) */
/* each layer's crush (KNOB 3), -FLT_MAX .. FLT_MAX, 0 off: left fewer bits (16 down to 1.5), right a lower
 * sample rate (44.1 kHz down to ~300 Hz), both along exponential curves so the first half of the way stays
 * playable; the rate steps a fractional phase, so it glides rather than jumping between whole dividers */
static volatile int8_t lay_cr[NLAYER];
/* the bits, in tenths, of a crush value (left); the rate, Hz, of one (right) */
static int32_t cr_bits10(int32_t v) { return 160 - (-v) * 145 / FLT_MAX; }
static uint32_t cr_inc(int32_t v)               /* the hold's phase step a sample: 2^32 x rate / FS */
{
    uint32_t r = exp2_q16(-(v * 7 * 65536 + v * 13107) / FLT_MAX);   /* 2^(-7.2 v / 64), Q16 */
    return r >= 65535u ? 0xFFFFFFFFu : r << 16;
}
static int32_t cr_hz(int32_t v) { return (int32_t)(((uint64_t)cr_inc(v) * 44100u) >> 32); }
static void crush_block(layer_t *y, int32_t *x, uint32_t n, int32_t v)
{
    uint32_t i;
    if (v < 0) {                                         /* fewer bits: the step, 2^(16 - bits) */
        uint32_t step = exp2_q16((-v) * 145 * 65536 / (FLT_MAX * 10)) >> 16, inv;
        if (step < 2u)
            return;
        inv = 0xFFFFFFFFu / step;
        for (i = 0; i < n; i++) {
            int32_t q = (int32_t)(((int64_t)x[i] * inv + 0x80000000LL) >> 32);
            x[i] = q * (int32_t)step;
        }
    } else if (v > 0) {                                  /* a lower rate: each sample held */
        uint32_t inc = cr_inc(v);
        for (i = 0; i < n; i++) {
            uint32_t was = y->cr_ph;
            y->cr_ph += inc;
            if (y->cr_ph < was || inc == 0xFFFFFFFFu)
                y->cr_hold = x[i];
            x[i] = y->cr_hold;
        }
    }
}
/* the cutoff of a filter value, log2 Hz Q16 */
static int32_t flt_oct(int32_t v)
{
    if (v < 0)   /* 20 kHz down 9.16 octaves (to ~35 Hz); (x 64 = FLT_MAX: >> 6) */
        return 936005 + ((600310 * v) >> 6);                             /* log2 20000 = 14.288 */
    return 262144 + ((629146 * v) >> 6);                                 /* log2 16 = 4, up 9.6 octaves */
}
/* each track's distortion (SHIFT + KNOB 3), 0 .. 100, 0 off: after the track's limiter, before its level, pan and
 * sends, so before the delay and the reverb. Two gain stages in series, as in a valve preamp: the first
 * symmetric (odd harmonics), the second asymmetric (it clips sooner on one side: even harmonics), both at twice
 * the sample rate (linear interpolation up, a [1/4 1/2 1/4] half-band down: fewer aliases). The highs are lifted
 * 6 dB before the stages and lowered as much after (the mids and highs distort more than the bass: less
 * intermodulation, less mud); then a DC blocker and a low-pass that comes down from 16 kHz to 6.5 kHz as the
 * amount goes up. The level is brought back to the dry signal's: the mean squares of both followed (~0.2 s),
 * the wet scaled by the square root of their ratio, so turning it up adds drive and harmonics, not loudness and
 * not a drop in it. The amount sets the drive into the first stage (x 0.5 .. x 32, +36 dB), the second stage's
 * gain (x 1 .. x 2.5) and bias, and the bias into the first stage, a share (0.15 .. 0.35) of the input's own peak,
 * so that however hard it is driven the clipped wave stays lopsided and keeps its even harmonics; below 6 the
 * amount also fades the stage in */
static volatile int8_t lay_dist[NLAYER];
#define LEVEL_MAX 23
#define LEVEL_UNITY 20
#define PAN_MAX 20
static volatile int8_t lay_lvl[NLAYER], lay_pan[NLAYER];   /* (the level and pan knobs as set: for the modulation) */
#include "mod.c"                 /* (the modulation matrix: per track, a block at a time) */
typedef struct {
    int32_t amt;                 /* the amount as it glides, x 256 */
    int32_t pre, post;           /* the emphasis's low-passes (700 Hz), before and after */
    int32_t dc;                  /* the DC blocker's low-pass, x 256 */
    int32_t lp;                  /* the tone's low-pass */
    int32_t env;                 /* the emphasised input's peak, falling over ~0.1 s (the bias follows it) */
    int32_t ep, ymp, yp, dryp;   /* the last emphasised input; the last mid-point's and sample's stage output; the
                                  * last dry sample (the half-band's one sample of delay) */
    int64_t ein, eout;           /* the mean squares, dry and wet, as they follow (x 256) */
    int32_t g, gp;               /* the level brought back, Q16: wanted, and as the last block ended (it glides) */
} dist_t;
static dist_t dist[NLAYER];
#define DS_EMPH 3110             /* 1 - e^(-2 pi 700 / FS), Q15 */
AINL int32_t ds_asym(int32_t u)  /* the first stage's curve: tanh, clipping at 5/8 below */
{
    return u >= 0 ? tanh_q15(u) : (tanh_q15((u * 8) / 5) * 5) / 8;
}
AINL int32_t ds_stage(int32_t e, int32_t g1, int32_t b, int32_t fb, int32_t g2)   /* the two stages (e biased) */
{
    int32_t u = tanh_q15((int32_t)(((int64_t)e * g1) >> 12));
    return ds_asym(mulq12(u, g2) + b) - fb;
}
static int32_t ds_log2(int64_t v)            /* log2 of a mean square, Q16 */
{
    int32_t e = 0;
    while (v > 0x7FFFFFFFLL) {
        v >>= 1;
        e++;
    }
    return log2_q16((uint32_t)(v > 0 ? v : 1)) + (e << 16);
}
static void dist_block(dist_t *d, int32_t *x, uint32_t n, int32_t want)
{
    int32_t a, g1, b, fb, g2, tone, w, beta, g0 = d->gp, gs, i;
    int64_t sin = 0, sout = 0;
    d->amt += ((want << 8) - d->amt) >> 2;
    if (!want && d->amt < 64) {                  /* (off: from silence when it comes in again) */
        memset(d, 0, sizeof *d);
        d->g = d->gp = Q16;
        return;
    }
    a = d->amt >> 8;
    g1 = (int32_t)((2048u * (uint64_t)exp2_q16(d->amt * 6 / 100 * 256)) >> 16);   /* x 0.5 .. x 32, Q12 */
    b = 3277 + d->amt * 6554 / 25600;            /* the bias, 0.1 .. 0.3 */
    fb = ds_asym(b);
    g2 = 4096 + d->amt * 6144 / 25600;           /* x 1 .. x 2.5, Q12 */
    beta = 4915 + d->amt * 6554 / 25600;         /* the first stage's bias: 0.15 .. 0.35 of the input's peak */
    tone = 29430 - d->amt * (29430 - 19800) / 25600;   /* 16 kHz .. 6.5 kHz */
    w = a >= 6 ? Q15 : d->amt * Q15 / (6 * 256);
    gs = (d->g - g0) / (int32_t)n;
    for (i = 0; i < (int32_t)n; i++) {
        int32_t dry = x[i], e, ym, y, o;
        d->pre += mulq15(dry - d->pre, DS_EMPH);
        e = 2 * dry - d->pre;                    /* the highs +6 dB */
        d->env = absi(e) > d->env ? absi(e) : d->env - (d->env >> 12);
        e += mulq15(d->env, beta);               /* (a bias in step with the level: the clipping stays lopsided --
                                                  * even harmonics -- however hard it is driven) */
        ym = ds_stage((d->ep + e) >> 1, g1, b, fb, g2);
        y = ds_stage(e, g1, b, fb, g2);
        o = (d->ymp + 2 * d->yp + ym) >> 2;      /* (one sample late) */
        d->ep = e;
        d->ymp = ym;
        d->yp = y;
        d->post += mulq15(o - d->post, DS_EMPH);
        o = (o + d->post) >> 1;                  /* the highs -6 dB again */
        d->dc += (((o << 8) - d->dc) * 37) >> 15;   /* (~8 Hz) */
        o -= d->dc >> 8;
        d->lp += mulq15(o - d->lp, tone);
        o = d->lp;
        sin += (int64_t)d->dryp * d->dryp;
        sout += (int64_t)o * o;
        o = (int32_t)(((int64_t)o * (g0 + gs * (i + 1))) >> 16);
        x[i] = d->dryp + mulq15(o - d->dryp, w);
        d->dryp = dry;
    }
    d->gp = d->g;
    d->ein += (sin - d->ein) >> 8;
    d->eout += (sout - d->eout) >> 8;
    if (d->ein > 32 * 1000 && d->eout > 32 * 100)   /* (sounding: else the level as it was) */
        d->g = (int32_t)exp2_q16(clampi((ds_log2(d->ein) - ds_log2(d->eout)) / 2, -(5 << 16), 1 << 16));   /* x 1/32 .. x 2 */
}

#define LAYER_GAIN 7373          /* Q12, x 1.8: one layer at 0.1 RMS reads -15 dBFS */

/* A layer's limiter, after its engine: a resonance the hand finds, an index that swells long after the
 * sound was measured, never take one layer past LIM. The peak of a block sets the gain of the next (instant
 * attack, the block's overshoot left to the master's soft ceiling), released over ~200 ms. */
#define LIM 19661                /* 0.6, Q15 */
static void lim_block(int32_t *env, int32_t *g, int32_t *x, uint32_t n)
{
    int32_t pk = 0, want, g0 = *g, dg;
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t a = absi(x[i]);
        if (a > pk)
            pk = a;
    }
    if (pk > *env)
        *env = pk;
    else
        *env -= *env >> 8;
    want = *env > LIM ? (int32_t)((uint32_t)LIM * 32768u / (uint32_t)*env) : Q15;
    if (want > g0 && g0 < Q15)
        want = g0 + ((want - g0) >> 3);          /* back up gently */
    dg = (want - g0) / (int32_t)n;
    for (i = 0; i < n; i++) {
        g0 += dg;
        x[i] = mulq15(x[i], g0);
    }
    *g = want;
}

/* --------------------------------------------------------- events --- */
enum { EV_ON, EV_OFF, EV_ENGINE, EV_STOP, EV_LOOP_REC, EV_LOOP_PLAY, EV_LOOP_CLEAR, EV_LOOP_SET, EV_LOOPS, EV_LOOP_TRK, EV_LOOP_XP, EV_LOOP_MODE, EV_MIDI_OFF,
       EV_LATCH,                 /* (a key's note latched: its voice is never let go by the overload guard) */
       EV_LOOP_EMPTY, EV_LOOP_PACK, EV_LOOP_TAKES };   /* (the snapshots': snaps.c) */
typedef struct { uint8_t type, layer, note, val; } ev_t;
#define EV_UNHEARD 0x80u         /* (EV_ON, EV_OFF: in val, a sequencer's note -- the looper does not record it) */
#define EVQ 64u
static ev_t evq[EVQ];
static volatile uint32_t ev_w, ev_r;

static void ev_post(uint32_t type, uint32_t layer, uint32_t note, uint32_t val)   /* main loop */
{
    ev_t e;
    if (ev_w - ev_r >= EVQ || layer >= NLAYER)
        return;
    e.type = (uint8_t)type;
    e.layer = (uint8_t)layer;
    e.note = (uint8_t)note;
    e.val = (uint8_t)val;
    evq[ev_w % EVQ] = e;
    RING_PUBLISH();
    ev_w++;
}

/* ------------------------------------------------------- the voices --- */
static uint32_t lay_age;
static void voice_on(uint32_t l, uint32_t k, int fresh)
{
    voice_t *v = &vox[k];
    if (v->layer != l)
        fresh = 1;                                /* (another layer's voice: from silence) */
    if (!v->sounding || v->layer != l)
        blk_new += eng_cost[lay[l].engine & 7u];   /* (a new voice: what it will cost, ahead) */
    v->layer = (uint8_t)l;
    v->age = ++lay_age;
    v->born = blk_clock;
    if (lay[l].engine != v->eng) {
        fresh = 1;                                /* (its memory another engine's: from silence) */
        if (lay[l].engine == ENG_ANIMA)
            an_init(&vmem[k].an, 0xA11u + k * 131u + lay_age);
    }
    v->eng = lay[l].engine;
    if (lay[l].engine == ENG_FM)
        fm_note_on(&v->fm, &fm_buf[l][fm_use[l]], v->note, fresh || !v->sounding);
    else if (lay[l].engine == ENG_DRUID)
        dr_note_on(&vmem[k].dr, &dr_buf[l][dr_use[l]], (int32_t)v->note, fresh || !v->sounding);
    else if (lay[l].engine == ENG_BAY) {
        ba_note_on(&vmem[k].ba, &ba_buf[l][ba_use[l]], (int32_t)v->note, v->vel ? v->vel : 127, fresh || !v->sounding);
        vmem[k].ba.hold = 0;
    } else if (lay[l].engine == ENG_HIVE) {
        hv_note_on(&vmem[k].hv, &hv_buf[l][hv_use[l]], (int32_t)v->note, v->vel ? v->vel : 127, fresh || !v->sounding);
        vmem[k].hv.hold = 0;
    } else if (lay[l].engine == ENG_OB) {
        ob_note_on(&vmem[k].ob, &ob_buf[l][ob_use[l]], (int32_t)v->note, v->vel ? v->vel : 127, fresh || !v->sounding);
        vmem[k].ob.hold = 0;
    }
    else {
        an_note_on(&vmem[k].an, &an_buf[l][an_use[l]], pitch_inc((int32_t)v->note * 16), fresh || !v->sounding);
        vmem[k].an.env_hold = 0;
    }
    v->est = AE_OFF;
    if (lay_env_on[l]) {                          /* the track's own envelope: its own held, this one from 0 */
        if (lay[l].engine == ENG_FM)
            fm_hold(&v->fm, &fm_buf[l][fm_use[l]]);
        else if (lay[l].engine == ENG_ANIMA)
            vmem[k].an.env_hold = 1;
        else if (lay[l].engine == ENG_BAY)
            vmem[k].ba.hold = 1;
        else if (lay[l].engine == ENG_HIVE)
            vmem[k].hv.hold = 1;
        else if (lay[l].engine == ENG_OB)
            vmem[k].ob.hold = 1;
        v->est = AE_ATT;
        v->elv = 0;
    }
    v->sounding = 1;
    v->pk = v->wmax = v->rmax = 0;               /* (its tail watched afresh) */
    v->quiet = v->rt = 0;
    v->nst = 0;
    v->latched = 0;
}
static void voice_off(uint32_t l, uint32_t k)
{
    voice_t *v = &vox[k];
    if (v->est && v->est != AE_DONE) {            /* the track's own envelope: its release, the voice held meanwhile */
        v->est = AE_REL;
        return;
    }
    if (lay[l].engine == ENG_FM)
        fm_note_off(&v->fm, &fm_buf[l][fm_use[l]]);
    else if (v->eng == ENG_DRUID)
        dr_note_off(&vmem[k].dr);
    else if (v->eng == ENG_BAY)
        ba_note_off(&vmem[k].ba, &ba_buf[l][ba_use[l]]);
    else if (v->eng == ENG_HIVE)
        hv_note_off(&vmem[k].hv);
    else if (v->eng == ENG_OB)
        ob_note_off(&vmem[k].ob);
    else
        an_note_off(&vmem[k].an);
}
static void voice_silence(uint32_t k)
{
    voice_t *v = &vox[k];
    v->sounding = 0;
    v->fade = 0;
    v->fade_n = 0;
    v->est = AE_OFF;
    v->fm.live = 0;
    if (v->eng == ENG_DRUID)
        vmem[k].dr.live = 0;
    else if (v->eng == ENG_BAY)
        vmem[k].ba.live = 0;
    else if (v->eng == ENG_HIVE)
        vmem[k].hv.live = 0;
    else if (v->eng == ENG_OB)
        vmem[k].ob.live = 0;
    else
        vmem[k].an.live = 0;
}
/* a voice for a note on layer l: a free one; else the oldest tail (any layer's); else the layer's own oldest held
 * note; else the oldest held of all. own: only free ones or the layer's own (it is fading for a change) */
static uint32_t voice_pick(uint32_t l, int own)
{
    uint32_t k, best = NVOICE, pass;
    for (k = 0; k < NVOICE; k++)
        if (!vox[k].sounding && !vox[k].gate && !vox[k].pend)
            return k;
    for (pass = 0; pass < 4u && best == NVOICE; pass++)   /* (a released one; one of this layer's; any but a latched
                                                           * one; any) */
        for (k = 0; k < NVOICE; k++) {
            const voice_t *v = &vox[k];
            uint32_t o = vox_owner(v);
            if (v->pend || (own && o != l) || (pass == 0u ? v->gate : pass == 1u ? o != l || v->latched : pass == 2u ? v->latched : 0))
                continue;
            if (best == NVOICE || (pass == 0u ? v->lvl < vox[best].lvl : v->age < vox[best].age))
                best = k;                         /* (a tail taken: the quietest; else the oldest) */
        }
    return best;
}

/* a note to CALIGULA, while the layer is (or is becoming) it; else to the voices */
static int cal_note(uint32_t l, uint32_t note, int on, uint32_t src)
{
    if (eng_organism(lay[l].want_engine) && lay_env_on[l]) {   /* the track's own envelope on an organism */
        int cal = lay[l].want_engine == ENG_CAL;
        uint32_t held = cal ? cal_rt[l].nheld : rus_rt[l].nheld;
        if (on) {
            if (!held && !org_defer[l])
                org_elv[l] = 0;
            org_est[l] = AE_ATT;                  /* (from where it is) */
            if (cal)
                cal_note_on(&cal_rt[l], &cal_buf[l][cal_use[l]], note, src == SRC_LOOP ? 2u : 1u);
            else
                rus_note_on(&rus_rt[l], &rus_buf[l][rus_use[l]], note);
            if (org_defer[l]) {                   /* (the note kept through a release: let go now, under the new) */
                if (cal)
                    cal_note_off(&cal_rt[l], org_defer[l] - 1u, org_dsrc[l]);
                else
                    rus_note_off(&rus_rt[l], org_defer[l] - 1u);
                org_defer[l] = 0;
            }
            return 1;
        }
        if (held == 1u && !org_defer[l] && org_est[l]) {   /* the last one: held through the release (its note-on
                                                         * under the envelope: one before it turned on, as it was) */
            org_defer[l] = (uint8_t)(note + 1u);
            org_dsrc[l] = (uint8_t)(src == SRC_LOOP ? 2u : 1u);
            org_est[l] = AE_REL;
            return 1;
        }
    }

    if (on && lay[l].want_engine == lay[l].engine && eng_organism(lay[l].engine)
        && !(lay[l].engine == ENG_CAL ? cal_rt[l].live : rus_rt[l].live))
        blk_new += eng_cost[lay[l].engine & 7u];  /* (the organism wakes: what it will cost, ahead) */
    if (lay[l].want_engine == ENG_RUS) {
        if (on)
            rus_note_on(&rus_rt[l], &rus_buf[l][rus_use[l]], note);
        else
            rus_note_off(&rus_rt[l], note);
        return 1;
    }
    if (lay[l].want_engine != ENG_CAL)
        return 0;
    if (on)
        cal_note_on(&cal_rt[l], &cal_buf[l][cal_use[l]], note, src == SRC_LOOP ? 2u : 1u);
    else
        cal_note_off(&cal_rt[l], note, src == SRC_LOOP ? 2u : 1u);
    return 1;
}

/* REACT (react.c) listens to the guide track: its loops' notes, as they sound, to the main loop (vel 0: let go) */
static volatile uint8_t rx_ear = 0xFFu;          /* (the guide; 0xFF: REACT off) */
static volatile uint8_t rx_lq[16][2];
static volatile uint32_t rx_lw, rx_lr;
static void rx_lq_put(uint32_t l, uint32_t note, uint32_t vel)
{
    if (l != rx_ear || rx_lw - rx_lr >= 16u)
        return;
    rx_lq[rx_lw % 16u][0] = (uint8_t)note;
    rx_lq[rx_lw % 16u][1] = (uint8_t)vel;
    RING_PUBLISH();
    rx_lw++;
}
/* a note on: a silent voice if there is one, else the one ringing longest (released before held) */
static uint8_t note_vel = 127;           /* the level of the note note_on makes (EV_ON's value; else full) */
static void note_on(uint32_t l, uint32_t note, uint32_t src)
{
    layer_t *y = &lay[l];
    uint32_t best;
    mod_note(l, note, note_vel);                  /* (the modulation's sources: this note) */
    if (src == SRC_LOOP)
        rx_lq_put(l, note, note_vel ? note_vel : 1u);
    midi_note_out(l, note, note_vel, 1);          /* (SEND NOTES: on the track's OUT channel) */
    if (cal_note(l, note, 1, src))
        return;
    best = voice_pick(l, y->xfade != 0);
    if (best == NVOICE)
        return;                                  /* (the layer fading for a change, every voice another's) */
    {
        voice_t *v = &vox[best];
        v->note = (uint8_t)note;
        v->gate = 1;
        v->src = (uint8_t)src;
        v->vel = note_vel;
        v->age = ++lay_age;
        if (y->xfade) {                          /* played when the layer's change is made */
            if (!v->sounding)
                v->layer = (uint8_t)l;
            return;
        }
        if (v->sounding) {
            if (!v->fade)
                v->fade = FADE;                  /* out first (on the layer it sounds on), then this note */
            v->pend = 1;
            v->pend_layer = (uint8_t)l;
        } else
            voice_on(l, best, 1);
    }
}
static void note_off(uint32_t l, uint32_t note, uint32_t src)
{
    layer_t *y = &lay[l];
    uint32_t k;
    midi_note_out(l, note, 0, 0);
    if (src == SRC_LOOP)
        rx_lq_put(l, note, 0);
    if (cal_note(l, note, 0, src))
        return;
    for (k = 0; k < NVOICE; k++) {
        voice_t *v = &vox[k];
        if (v->gate && vox_owner(v) == l && v->note == note && v->src == src) {
            v->gate = 0;
            if (!v->pend && !y->xfade)
                voice_off(l, k);                  /* (a pending note sounds briefly, then lets go) */
            return;
        }
    }
}
/* every note the loop plays on layer l, let go */
static void loop_notes_off(uint32_t l)
{
    uint32_t k;
    for (k = 0; k < LOOP_TRK; k++)
        memset(loop[l].trk[k].snd, 0, sizeof loop[l].trk[k].snd);
    cal_release(&cal_rt[l], 2u);
    if (lay[l].engine == ENG_RUS)
        rus_release(&rus_rt[l]);                  /* (its notes are not told apart: all of them) */
    for (k = 0; k < NVOICE; k++)
        if (vox[k].gate && vox_owner(&vox[k]) == l && vox[k].src == SRC_LOOP)
            note_off(l, vox[k].note, SRC_LOOP);
}

/* a change the sounding voices must fade out for: another engine, or a new sound on its own. A new sound
 * of the engine not in use is simply taken. */
static int change_due(uint32_t l)
{
    layer_t *y = &lay[l];
    if (y->engine != ENG_FM && y->want_engine != ENG_FM)
        fm_use[l] = fm_pub[l];
    if (y->engine != ENG_ANIMA && y->want_engine != ENG_ANIMA)
        an_use[l] = an_pub[l];
    if (y->engine != ENG_CAL && y->want_engine != ENG_CAL)
        cal_use[l] = cal_pub[l];
    if (y->engine != ENG_RUS && y->want_engine != ENG_RUS)
        rus_use[l] = rus_pub[l];
    if (y->engine != ENG_DRUID && y->want_engine != ENG_DRUID)
        dr_use[l] = dr_pub[l];
    if (y->engine != ENG_BAY && y->want_engine != ENG_BAY)
        ba_use[l] = ba_pub[l];
    if (y->engine != ENG_HIVE && y->want_engine != ENG_HIVE)
        hv_use[l] = hv_pub[l];
    if (y->engine != ENG_OB && y->want_engine != ENG_OB)
        ob_use[l] = ob_pub[l];
    if (y->want_engine != y->engine)
        return 1;
    return y->engine == ENG_FM ? fm_pub[l] != fm_use[l] : y->engine == ENG_ANIMA ? an_pub[l] != an_use[l]
           : y->engine == ENG_CAL ? cal_pub[l] != cal_use[l] : y->engine == ENG_RUS ? rus_pub[l] != rus_use[l]
           : y->engine == ENG_DRUID ? dr_pub[l] != dr_use[l] : y->engine == ENG_BAY ? ba_pub[l] != ba_use[l]
           : y->engine == ENG_OB ? ob_pub[l] != ob_use[l] : hv_pub[l] != hv_use[l];
}
/* what was asked, now (the voices silent), and the notes still held played again with it */
static void change_apply(uint32_t l)
{
    layer_t *y = &lay[l];
    uint32_t k;
    {   /* the organism, made anew (a new design, or the layer coming to it or leaving it); the notes held go
         * across: to it from the voices, from it to the voices */
        cal_t *c = &cal_rt[l];
        uint8_t nn[8], ns[8], n = 0, j;
        for (k = 0; k < NVOICE; k++)              /* (the voices' first: the organism's are the newer) */
            if (vox[k].gate && vox_owner(&vox[k]) == l && n < 8u) {
                nn[n] = vox[k].note;
                ns[n++] = (uint8_t)(vox[k].src == SRC_LOOP ? 2u : 1u);
            }
        for (k = 0; k < NVOICE; k++) {            /* the layer's voices silent and free */
            voice_t *v = &vox[k];
            if (v->layer == l && (v->sounding || v->pend)) {
                voice_silence(k);
                if (v->pend && v->pend_layer != l) {   /* (fading to be another layer's note: that note now) */
                    v->pend = 0;
                    v->layer = v->pend_layer;
                    if (!lay[v->layer].xfade) {
                        voice_on(v->layer, k, 1);
                        if (!v->gate)
                            voice_off(v->layer, k);
                    }
                    continue;
                }
            }
            if (vox_owner(v) == l) {
                v->gate = 0;
                v->pend = 0;
            }
        }
        for (j = 0; j < c->nheld && n < 8u; j++) {
            nn[n] = c->note[j];
            ns[n++] = c->nsrc[j];
        }
        for (j = 0; j < rus_rt[l].nheld && n < 8u; j++) {
            nn[n] = rus_rt[l].note[j];
            ns[n++] = 1u;
        }
        cal_init(c, 0xC0u + l * 131u + lay_age);
        rus_init(&rus_rt[l], 0x7A5u + l * 131u + lay_age);
        y->engine = y->want_engine;
        fm_use[l] = fm_pub[l];
        an_use[l] = an_pub[l];
        cal_use[l] = cal_pub[l];
        rus_use[l] = rus_pub[l];
        dr_use[l] = dr_pub[l];
        ba_use[l] = ba_pub[l];
        hv_use[l] = hv_pub[l];
        ob_use[l] = ob_pub[l];
        if (y->engine == ENG_CAL)
            for (j = 0; j < n; j++)
                cal_note_on(c, &cal_buf[l][cal_use[l]], nn[j], ns[j]);
        else if (y->engine == ENG_RUS)
            for (j = 0; j < n; j++)
                rus_note_on(&rus_rt[l], &rus_buf[l][rus_use[l]], nn[j]);
        else
            for (j = 0; j < n; j++) {             /* (free voices only: none taken from the other layers) */
                for (k = 0; k < NVOICE && (vox[k].sounding || vox[k].gate || vox[k].pend); k++)
                    ;
                if (k == NVOICE)
                    break;
                vox[k].note = nn[j];
                vox[k].src = (uint8_t)(ns[j] == 2u ? SRC_LOOP : SRC_KEY);
                vox[k].gate = 1;
                voice_on(l, k, 1);
            }
    }
}

/* ------------------------------------------------------ the loopers --- */
static void loop_put(looper_t *p, uint32_t on, uint32_t note)
{
    uint32_t t = (p->lclk - p->start) >> 1, k;   /* (in loop time: played back at this speed, as played) */
    if (on) {
        if (p->nev >= LOOP_N - 16u || p->nheld >= 16u)
            return;                               /* full: no more notes (room is kept for their ends) */
        p->held[p->nheld++] = (uint8_t)note;
    } else {
        for (k = 0; k < p->nheld && p->held[k] != note; k++)
            ;
        if (k == p->nheld)
            return;                               /* its start was not recorded */
        p->held[k] = p->held[--p->nheld];
    }
    p->ev[p->nev++] = LE(t, note, on);
}
static void loop_rewind(looper_t *p)               /* every loop from its start */
{
    uint32_t k;
    for (k = 0; k < p->ntrk; k++)
        trk_start(p, &p->trk[k]);
}
static void loop_cmd(uint32_t l, uint32_t type)
{
    looper_t *p = &loop[l];
    switch (type) {
    case EV_LOOP_REC:
        if (p->rec == LR_REC) {                   /* the end: a loop as long as this */
            loop_trk_t *t = &p->trk[p->ntrk];
            uint32_t len = (p->lclk - p->start) >> 1, i;
            if (len < 2u)
                len = 2u;
            for (i = t->first; i < p->nev; i++)
                if (LE_T(p->ev[i]) > len - 2u)
                    p->ev[i] = LE(len - 2u, LE_NOTE(p->ev[i]), LE_ON(p->ev[i]));
            while (p->nheld)                      /* notes still down: they end with the loop's last block */
                p->ev[p->nev++] = LE(len - 1u, p->held[--p->nheld], 0);
            t->len = len;
            t->n = (uint16_t)(p->nev - t->first);
            if (p->dmode == LD_REV) {             /* recorded while the loops play backwards: kept turned round, so
                                                   * that played backwards it is as it was played */
                uint32_t a = t->first, b = p->nev - 1u;
                for (i = t->first; i < p->nev; i++)
                    p->ev[i] = LE(len - 1u - LE_T(p->ev[i]), LE_NOTE(p->ev[i]), !LE_ON(p->ev[i]));
                for (; a < b; a++, b--) {
                    uint32_t x = p->ev[a];
                    p->ev[a] = p->ev[b];
                    p->ev[b] = x;
                }
            }
            trk_start(p, t);
            if (!p->play)
                loop_rewind(p);                   /* stopped: all of them again, with the new one */
            p->mute &= (uint16_t)~(1u << p->ntrk);   /* (a new one plays) */
            p->ntrk++;
            p->play = 1;
            p->rec = LR_IDLE;
        } else if (p->rec == LR_ARMED) {          /* disarmed */
            p->rec = LR_IDLE;
        } else if (p->ntrk < LOOP_TRK && p->nev < LOOP_N - 32u) {
            p->rec = LR_ARMED;
        }
        break;
    case EV_LOOP_PLAY:
        if (!p->ntrk)
            break;
        if (p->play) {
            loop_notes_off(l);
            p->play = 0;
        } else {
            loop_rewind(p);
            p->play = 1;
        }
        break;
    case EV_LOOP_CLEAR:
        loop_notes_off(l);
        p->dmode = LD_FWD;                        /* (emptied: forwards, at its speed, as played, again) */
        p->rate = LS_NORMAL;
        p->vary = 0;
        memset(vq[l], 0, sizeof vq[l]);
        memset(vo[l], 0, sizeof vo[l]);
        p->play = 0;
        p->mute = 0;
        p->ntrk = 0;
        p->nev = 0;
        p->nheld = 0;
        p->rec = LR_IDLE;
        break;
    }
}
/* loops the main loop has written into an empty looper (a session loaded): from their starts, playing or not */
static void loop_set(uint32_t l, uint32_t play)
{
    looper_t *p = &loop[l];
    if (!p->ntrk || p->rec != LR_IDLE)
        return;
    loop_rewind(p);
    p->nheld = 0;
    p->play = (uint8_t)(play != 0);
}
/* a note from the keys on layer l: kept by its looper when it records (and the first one starts it) */
static void loop_hear(uint32_t l, uint32_t on, uint32_t note)
{
    looper_t *p = &loop[l];
    if (p->rec == LR_ARMED && on) {
        p->rec = LR_REC;
        p->start = p->lclk;
        p->trk[p->ntrk].first = (uint16_t)p->nev;
        p->trk[p->ntrk].xb = (int8_t)(lx_steps + lx_tr[l]);   /* (it plays back as played, whatever the others are
                                                              * moved by) */
        p->nheld = 0;
    }
    if (p->rec == LR_REC)
        loop_put(p, on, note);
}
/* the notes loop t of layer l sounds now, let go */
static void trk_off(uint32_t l, loop_trk_t *t)
{
    uint32_t n;
    for (n = 0; n < 128u; n++)
        if ((t->snd[n >> 5] >> (n & 31u)) & 1u)
            note_off(l, lx_map(n, lx_steps + lx_tr[l] - t->xb, lx_mask), SRC_LOOP);
    memset(t->snd, 0, sizeof t->snd);
}
/* loop k of layer l in or out (PLAY held, its white key). In while they play: from its start, now. Out: the notes
 * it holds now let go (found again from its events up to where it is), the others' left be */
static void loop_trk_set(uint32_t l, uint32_t k, uint32_t on)
{
    looper_t *p = &loop[l];
    loop_trk_t *t = &p->trk[k];
    uint32_t bit = 1u << k;
    if (k >= p->ntrk)
        return;
    if (on && (p->mute & bit)) {
        if (p->play)
            trk_start(p, t);
        p->mute &= (uint16_t)~bit;
    } else if (!on && !(p->mute & bit)) {
        if (p->play)
            trk_off(l, t);
        p->mute |= (uint16_t)bit;
    }
}

/* ------------------------------------------- the snapshots' (snaps.c) --- */
/* A loop once recorded stays in its looper's events while a snapshot holds it (a take): emptying the looper
 * (EV_LOOP_EMPTY) leaves the events be, and the main loop has those no longer held packed away (EV_LOOP_PACK); a
 * snapshot recalled gives the looper its loops (EV_LOOP_TAKES). One layer at a time, the main loop waiting for each */
#define NTAKE 24u                /* the takes a layer's snapshots hold between them */
#define PK_N (LOOP_TRK + NTAKE)
static struct { uint8_t n; uint16_t from[PK_N], len[PK_N]; } lpk;   /* EV_LOOP_PACK: the events kept, in order */
static volatile uint8_t lpk_ok;  /* .. done (not while a loop records) */
typedef struct { uint16_t first, n; uint32_t len; int8_t xb; } take_t;
static struct {
    uint8_t ntrk, play;
    uint16_t mute;
    take_t t[LOOP_TRK];
    uint16_t ph[LOOP_TRK];       /* where each was, a fraction of its length (Q16): where those not playing yet go */
} lts;                           /* EV_LOOP_TAKES */
/* where an event kept goes (a loop with none: anywhere) */
static uint32_t pk_map(uint32_t first, uint32_t n)
{
    uint32_t i, at = 0;
    if (!n)
        return 0;
    for (i = 0; i < lpk.n; i++) {
        if (first >= lpk.from[i] && first < (uint32_t)lpk.from[i] + lpk.len[i])
            return at + first - lpk.from[i];
        at += lpk.len[i];
    }
    return first;
}
static void loop_pack(uint32_t l)
{
    looper_t *p = &loop[l];
    uint32_t i, k, at = 0;
    if (p->rec != LR_IDLE) {
        lpk_ok = 0;
        return;
    }
    for (k = 0; k < p->ntrk; k++)
        p->trk[k].first = (uint16_t)pk_map(p->trk[k].first, p->trk[k].n);
    for (i = 0; i < lpk.n; i++)                   /* (down, in order: a word at a time forwards is safe) */
        for (k = 0; k < lpk.len[i]; k++)
            p->ev[at++] = p->ev[lpk.from[i] + k];
    p->nev = at;
    lpk_ok = 1;
}
/* pos as a fraction of len, Q16 (32 bits: no 64-bit division on the FM-1) */
static uint32_t frac16(uint32_t pos, uint32_t len)
{
    if (!len)
        return 0;
    return len < 65536u ? (pos << 16) / len : (pos << 8) / (len >> 8);
}
/* loop t at its place ph (forwards: its events from there; else from its start) */
static void trk_seek(const looper_t *p, loop_trk_t *t, uint32_t ph)
{
    const uint32_t *ev = p->ev + t->first;
    int32_t j = 0;
    trk_start(p, t);
    if (t->dir < 0 || p->dmode == LD_RAND || t->len < 2u)
        return;
    t->pos = (int32_t)(((uint64_t)ph * t->len) >> 16);
    while (j < (int32_t)t->n && (int32_t)LE_T(ev[j]) < t->pos)
        j++;
    t->ix = (int16_t)j;
}
static int take_same(const loop_trk_t *t, const take_t *k)
{
    return t->first == k->first && t->n == k->n && t->len == k->len && t->xb == k->xb;
}
static void loop_takes(uint32_t l)
{
    looper_t *p = &loop[l];
    uint32_t k, keep = LOOP_TRK, any_same = 0;
    if (p->rec == LR_REC)
        p->nev = p->trk[p->ntrk].first;           /* (a recording under way: given up) */
    p->rec = LR_IDLE;
    p->nheld = 0;
    for (k = 0; k < p->ntrk && k < lts.ntrk; k++)   /* the loops in both, in or out as they were */
        if (take_same(&p->trk[k], &lts.t[k])) {
            any_same = 1;
            if (keep == LOOP_TRK && !(p->mute >> k & 1u) && !(lts.mute >> k & 1u))
                keep = k;                         /* (the one the others are placed by) */
            loop_trk_set(l, k, !(lts.mute >> k & 1u));
        }
    for (k = 0; k < LOOP_TRK; k++) {
        loop_trk_t *t = &p->trk[k];
        int same = k < p->ntrk && k < lts.ntrk && take_same(t, &lts.t[k]);
        if (same)
            continue;
        if (k < p->ntrk && p->play && !(p->mute >> k & 1u))
            trk_off(l, t);                        /* (gone, or another: its notes let go) */
        if (k < lts.ntrk) {
            t->first = lts.t[k].first;
            t->n = lts.t[k].n;
            t->len = lts.t[k].len;
            t->xb = lts.t[k].xb;
            if (keep < LOOP_TRK && t->len) {      /* placed as it was against the one playing on */
                const loop_trk_t *r = &p->trk[keep];
                uint32_t was = (uint32_t)(((uint64_t)lts.ph[keep] * r->len) >> 16);
                uint32_t since = ((uint32_t)r->pos + r->len - was) % r->len;
                uint32_t at = ((uint32_t)(((uint64_t)lts.ph[k] * t->len) >> 16) + since) % t->len;
                trk_seek(p, t, frac16(at, t->len));
            } else
                trk_seek(p, t, any_same ? 0u : lts.ph[k]);   /* (all of them anew: as they stood together) */
        }
    }
    p->ntrk = lts.ntrk;
    p->mute = lts.mute;
    if (!p->ntrk || (!lts.play && p->play)) {
        loop_notes_off(l);
        p->play = 0;
    } else if (lts.play && !p->play) {
        for (k = 0; k < p->ntrk; k++)
            trk_seek(p, &p->trk[k], lts.ph[k]);
        p->play = 1;
    }
}
/* VARY (SHIFT + REC): each loop note struck at a random level (-10 dB .. full), for a random length (50 ms ..
 * 1.5 s, the recorded end not waited for) -- and now and then, at odds of its own, changed or joined:
 *   dropped (5 %), late by 5 .. 40 ms (25 %), an octave up or down (8 %), a neighbouring step of the scale, a
 *   fifth instead or as well (8 %), a grace note a step away just before it (4 %), struck 2 .. 4 times quickly
 *   (4 %), a third or a fifth above with it (10 %), echoed 1 .. 4 times 120 .. 400 ms apart, each quieter, some
 *   climbing or falling a step each (15 %), and the notes of a chord spread out 20 .. 60 ms apart (12 %).
 * Each looper's notes to come wait in vq[], those struck in vo[] until their time is up */
static uint32_t lv_rnd(looper_t *p)
{
    if (!p->rnd)
        p->rnd = 0x9E3779B9u ^ (blk_clock * 2654435761u);
    p->rnd ^= p->rnd << 13;
    p->rnd ^= p->rnd >> 17;
    p->rnd ^= p->rnd << 5;
    return p->rnd;
}
static int lv_odds(looper_t *p, uint32_t pct) { return (lv_rnd(p) >> 8) % 1000u < pct * 10u; }
static uint32_t lv_in(looper_t *p, uint32_t lo, uint32_t hi) { return lo + (lv_rnd(p) >> 8) % (hi - lo + 1u); }
static uint32_t lv_blocks(uint32_t ms) { return (ms * 1378u) >> 10; }
/* a note of track l struck now (the same note still sounding let go first), let go after len blocks */
static void vq_strike(uint32_t l, uint32_t note, uint32_t vel, uint32_t len)
{
    uint32_t i, slot = VQ_N;
    for (i = 0; i < VQ_N; i++) {
        if (vo[l][i].left && vo[l][i].note == note) {
            note_off(l, note, SRC_LOOP);
            vo[l][i].left = 0;
        }
        if (!vo[l][i].left && slot == VQ_N)
            slot = i;
    }
    if (slot == VQ_N) {                           /* (all sounding: the soonest let go now) */
        slot = 0;
        for (i = 1; i < VQ_N; i++)
            if (vo[l][i].left < vo[l][slot].left)
                slot = i;
        note_off(l, vo[l][slot].note, SRC_LOOP);
    }
    note_vel = (uint8_t)clampi((int32_t)vel, 1, 126);
    note_on(l, note, SRC_LOOP);
    note_vel = 127;
    vo[l][slot].note = (uint8_t)note;
    vo[l][slot].left = (uint16_t)clampi((int32_t)len, 1, 0xFFFF);
}
/* .. or in wait blocks */
static void vq_add(uint32_t l, uint32_t wait, uint32_t note, uint32_t vel, uint32_t len)
{
    uint32_t i;
    if (!wait) {
        vq_strike(l, note, vel, len);
        return;
    }
    for (i = 0; i < VQ_N; i++)
        if (!vq[l][i].len) {
            vq[l][i].wait = (uint16_t)clampi((int32_t)wait, 1, 0xFFFF);
            vq[l][i].len = (uint16_t)clampi((int32_t)len, 1, 0xFFFF);
            vq[l][i].note = (uint8_t)note;
            vq[l][i].vel = (uint8_t)vel;
            return;
        }
}
/* a loop note under VARY */
static void vary_strike(uint32_t l, looper_t *p, loop_trk_t *t, uint32_t raw)
{
    uint32_t nt = lx_map(raw, lx_steps + lx_tr[l] - t->xb, lx_mask), wait = 0, vel, len, k;
    if (lv_odds(p, 5))
        return;                                   /* dropped, this time round */
    vel = lv_in(p, 40, 126);
    len = lv_blocks((50u * exp2_q16((int32_t)((lv_rnd(p) >> 8) % 322000u))) >> 16);   /* 50 ms .. 1.5 s */
    if (vs_blk[l] == blk_clock) {                 /* (with another struck at the same moment: a chord) */
        if (vs_on[l])
            wait += (uint32_t)++vs_idx[l] * vs_gap[l];   /* strummed */
    } else {
        vs_blk[l] = blk_clock;
        vs_idx[l] = 0;
        vs_on[l] = (uint8_t)lv_odds(p, 12);
        vs_gap[l] = (uint16_t)lv_blocks(lv_in(p, 20, 60));
    }
    if (lv_odds(p, 25))                           /* late */
        wait += lv_blocks(lv_in(p, 5, 40));
    if (lv_odds(p, 8)) {                          /* an octave up or down */
        int32_t o = lv_odds(p, 50) ? 12 : -12;
        nt = (uint32_t)((int32_t)nt + o >= 12 && (int32_t)nt + o <= 120 ? (int32_t)nt + o : (int32_t)nt - o);
    }
    if (lv_odds(p, 8)) {                          /* a note not recorded: a step or two away, or a fifth */
        static const int8_t ST[5] = {-2, -1, 1, 2, 4};
        uint32_t f = lx_map(nt, ST[lv_in(p, 0, 4)], lx_mask);
        if (lv_odds(p, 60))
            nt = f;
        else
            vq_add(l, wait, f, vel * 4u / 5u, len);
    }
    if (lv_odds(p, 4)) {                          /* a grace note just before */
        vq_add(l, wait, lx_map(nt, lv_odds(p, 50) ? 1 : -1, lx_mask), vel * 7u / 10u, lv_blocks(lv_in(p, 40, 80)));
        wait += lv_blocks(lv_in(p, 50, 90));
    }
    if (lv_odds(p, 4)) {                          /* ratchet: struck 2 .. 4 times, quickly */
        uint32_t n = lv_in(p, 2, 4), sp = lv_blocks(lv_in(p, 50, 110));
        for (k = 0; k < n; k++)
            vq_add(l, wait + k * sp, nt, vel, sp * 4u / 5u);
    } else
        vq_add(l, wait, nt, vel, len);
    if (lv_odds(p, 10))                           /* a third or a fifth above, with it */
        vq_add(l, wait, lx_map(nt, lv_odds(p, 50) ? 2 : 4, lx_mask), vel * 4u / 5u, len);
    if (lv_odds(p, 15)) {                         /* echoes, quieter each, some climbing or falling */
        uint32_t n = lv_in(p, 1, 4), d = lv_blocks(lv_in(p, 120, 400)), v = vel;
        int32_t dir = lv_odds(p, 40) ? (lv_odds(p, 50) ? 1 : -1) : 0;
        for (k = 1; k <= n; k++) {
            v = v * 3u / 5u;
            vq_add(l, wait + k * d, lx_map(nt, dir * (int32_t)k, lx_mask), v > 8u ? v : 8u, len < d ? len : d);
        }
    }
}
/* a loop note struck: as recorded, or under VARY */
static void loop_strike(uint32_t l, looper_t *p, loop_trk_t *t, uint32_t raw)
{
    if (p->vary) {
        vary_strike(l, p, t, raw);
        return;
    }
    note_on(l, lx_map(raw, lx_steps + lx_tr[l] - t->xb, lx_mask), SRC_LOOP);
    t->snd[raw >> 5] |= 1u << (raw & 31u);
}
/* a block: VARY's notes due struck, those whose time is up let go (the loops stopped: nothing more struck) */
static void lv_tick(uint32_t l)
{
    uint32_t i;
    for (i = 0; i < VQ_N; i++) {
        vq_t *q = &vq[l][i];
        if (q->len && (!loop[l].play || !--q->wait)) {
            if (loop[l].play)
                vq_strike(l, q->note, q->vel, q->len);
            q->len = 0;
        }
        if (vo[l][i].left && !--vo[l][i].left)
            note_off(l, vo[l][i].note, SRC_LOOP);
    }
}
/* a note tied over the loop's end, going round: still sounding (a drone, a pad: it goes on, one note), or died
 * away -- more than 18 dB under its loudest (a pluck, a bell, held to the end: it is struck again, as the loop's
 * first note). An organism's (CALIGULA, RUSSOLO) goes on */
static int tied_alive(uint32_t l, uint32_t note)
{
    uint32_t k;
    if (eng_organism(lay[l].engine))
        return 1;
    for (k = 0; k < NVOICE; k++) {
        const voice_t *v = &vox[k];
        if (v->gate && v->sounding && !v->fade && !v->pend && vox_owner(v) == l && v->src == SRC_LOOP &&
            v->note == note && v->lvl * 8u >= v->pk)
            return 1;
    }
    return 0;
}
/* an event of loop t played: as recorded, or (backwards) turned round -- its start an end, its end a start. A note
 * tied over the loop's end is never let go there, nor struck again just after (forwards: its start is the loop's
 * first block, its end the last; backwards the other way round) */
static void trk_play(uint32_t l, loop_trk_t *t, looper_t *p, uint32_t e, int as_on)
{
    uint32_t raw = LE_NOTE(e), nt = lx_map(raw, lx_steps + lx_tr[l] - t->xb, lx_mask);
    int tied = trk_tied(p, t, e);
    if (as_on) {
        if (t->wrap && tied && !p->vary) {
            if (tied_alive(l, nt))
                return;                           /* (still sounding from the round before: one note) */
            note_off(l, nt, SRC_LOOP);            /* (died away -- a pluck, a bell: let go, its tail fading on, and
                                                   * struck anew) */
        }
        (void)nt;
        loop_strike(l, p, t, raw);
        TELE(20);
#if TELEMETRY
        tele_non++;
#endif
    } else if (p->vary ? (t->snd[raw >> 5] >> (raw & 31u)) & 1u : !tied) {
        /* (VARY: its notes have their own lengths -- but one struck as recorded before VARY came on, tied over the
         * loop's end or not, is let go here, at its recorded end: else it would sound on for ever) */
        note_off(l, nt, SRC_LOOP);
        t->snd[raw >> 5] &= ~(1u << (raw & 31u));
    }
}
/* RANDOM: the notes sounding let go, and on from a note of the loop drawn at random (with any struck at its time) */
static void trk_jump(uint32_t l, looper_t *p, loop_trk_t *t)
{
    const uint32_t *ev = p->ev + t->first;
    uint32_t n, j, k;
    for (n = 0; n < 128u; n++)
        if ((t->snd[n >> 5] >> (n & 31u)) & 1u)
            note_off(l, lx_map(n, lx_steps + lx_tr[l] - t->xb, lx_mask), SRC_LOOP);
    memset(t->snd, 0, sizeof t->snd);
    t->rgo = 0;
    t->wrap = 0;
    if (!p->rnd)                                  /* (a session loaded in RANDOM: seeded at its first draw) */
        p->rnd = 0x9E3779B9u ^ (blk_clock * 2654435761u) ^ (uint32_t)l;
    p->rnd ^= p->rnd << 13;
    p->rnd ^= p->rnd >> 17;
    p->rnd ^= p->rnd << 5;
    j = t->n ? (uint32_t)(((uint64_t)p->rnd * t->n) >> 32) : 0;
    for (k = 0; k < t->n && !LE_ON(ev[(j + k) % t->n]); k++)
        ;                                         /* (the next note's start from there) */
    if (k == t->n) {                              /* (no notes: from the start) */
        t->pos = 0;
        t->ix = 0;
        return;
    }
    j = (j + k) % t->n;
    while (j > 0 && LE_T(ev[j - 1]) == LE_T(ev[j]))
        j--;                                      /* (the first event at its time) */
    t->pos = (int32_t)LE_T(ev[j]);
    t->ix = (int16_t)j;
}
/* one block of loop time of loop t, in its direction; at an end, round again -- or, PENDULUM, back the other way;
 * RANDOM: forwards, but when the next note is due (or the loop ends), a jump to a note drawn at random */
static void trk_step(uint32_t l, looper_t *p, loop_trk_t *t)
{
    const uint32_t *ev = p->ev + t->first;
    if (p->dmode == LD_RAND && t->dir > 0) {
        if (t->pos >= (int32_t)t->len || (t->rgo && t->ix < (int32_t)t->n && LE_ON(ev[t->ix]) && (int32_t)LE_T(ev[t->ix]) <= t->pos))
            trk_jump(l, p, t);
        while (t->ix < (int32_t)t->n && (int32_t)LE_T(ev[t->ix]) <= t->pos) {
            uint32_t e = ev[t->ix++];
            if (LE_ON(e)) {
                loop_strike(l, p, t, LE_NOTE(e));
                t->rgo = 1;
            } else if ((t->snd[LE_NOTE(e) >> 5] >> (LE_NOTE(e) & 31u)) & 1u) {   /* (only what it struck as
                                                                                    * recorded -- VARY's, never) */
                note_off(l, lx_map(LE_NOTE(e), lx_steps + lx_tr[l] - t->xb, lx_mask), SRC_LOOP);
                t->snd[LE_NOTE(e) >> 5] &= ~(1u << (LE_NOTE(e) & 31u));
            }
        }
        t->pos++;
        return;
    }
    if (t->dir > 0) {
        while (t->ix < (int32_t)t->n && (int32_t)LE_T(ev[t->ix]) <= t->pos) {
            uint32_t e = ev[t->ix++];
            trk_play(l, t, p, e, (int)LE_ON(e));
        }
        t->wrap = 0;
        if (++t->pos >= (int32_t)t->len) {
            if (p->dmode == LD_PEND) {
                t->dir = -1;
                t->pos = (int32_t)t->len - 1;
                t->ix = (int16_t)(t->n - 1u);
            } else {
                t->pos = 0;
                t->ix = 0;
            }
            t->wrap = 1;
            TELE(21);
        }
    } else {
        while (t->ix >= 0 && (int32_t)LE_T(ev[t->ix]) >= t->pos) {
            uint32_t e = ev[t->ix--];
            trk_play(l, t, p, e, !LE_ON(e));
        }
        t->wrap = 0;
        if (--t->pos < 0) {
            if (p->dmode == LD_PEND) {
                t->dir = 1;
                t->pos = 0;
                t->ix = 0;
            } else {
                t->pos = (int32_t)t->len - 1;
                t->ix = (int16_t)(t->n - 1u);
            }
            t->wrap = 1;
            TELE(21);
        }
    }
}
/* loop t turned round where it is: what is next forwards is next backwards, and the other way (the events of the
 * block it is at played again, turned: what had just started ends, what had just ended starts) */
static void trk_turn(loop_trk_t *t, int dir)
{
    if (dir == t->dir)
        return;
    t->dir = (int8_t)dir;
    if (dir < 0) {
        t->pos--;
        t->ix--;
        if (t->pos < 0) {                         /* (at its very start: from its end) */
            t->pos = (int32_t)t->len - 1;
            t->ix = (int16_t)(t->n - 1u);
            t->wrap = 1;
        }
    } else {
        t->pos++;
        t->ix++;
        if (t->pos >= (int32_t)t->len) {
            t->pos = 0;
            t->ix = 0;
            t->wrap = 1;
        }
    }
}
/* the direction or the speed of layer l's loops (the black keys): forwards, backwards, PENDULUM; half, double */
static void loop_mode(uint32_t l, uint32_t what, uint32_t val)
{
    looper_t *p = &loop[l];
    uint32_t k;
    if (what == 2u) {                             /* VARY on or off */
        p->vary = (uint8_t)(val != 0);
        return;
    }
    if (what) {
        p->rate = (uint8_t)(val <= LS_DOUBLE ? val : LS_NORMAL);
        return;
    }
    if (p->dmode == LD_RAND && val != LD_RAND)    /* (out of RANDOM: on from where it is, in range) */
        for (k = 0; k < p->ntrk; k++)
            if (p->trk[k].pos >= (int32_t)p->trk[k].len) {
                p->trk[k].pos = 0;
                p->trk[k].ix = 0;
            }
    p->dmode = (uint8_t)(val <= LD_RAND ? val : LD_FWD);
    if (p->dmode != LD_PEND)
        for (k = 0; k < p->ntrk; k++)
            trk_turn(&p->trk[k], p->dmode == LD_REV ? -1 : 1);
    if (!p->rnd)
        p->rnd = 0x9E3779B9u ^ (blk_clock * 2654435761u) ^ l;
}
/* VARY's time: the loops' pace no longer steady -- a slow undulation (a sine, its rate and depth redrawn now and
 * then), and every ~6 s on average one of: a halt (stopped dead 0.15 .. 1 s, then on at once), a burst (2 .. 3
 * times as fast for 0.2 .. 0.8 s), a grind (slowing to a stop over 0.4 .. 1.5 s, held up to 0.4 s, then back at
 * once), a swoop (rising to 2 .. 2.5 times over 0.8 .. 2 s, then dropping to 0.4 for 0.3 s). Not while REC records
 * (its pace steady, to play in time to) */
enum { VT_RUN, VT_HALT, VT_BURST, VT_GRIND, VT_SWOOP };
static struct {
    uint8_t ev, phase;
    uint32_t left, len;          /* blocks of the event (its phase) left, and its length */
    int32_t top;                 /* the event's speed, Q16 */
    uint32_t lph, lrate;         /* the undulation: phase, step a block */
    int32_t depth;               /* .. its depth, Q16 */
    uint32_t acc;                /* blocks of loop time to come, Q16 */
} vt[NLAYER];
static int32_t vt_speed(uint32_t l, looper_t *p)   /* a block on: the pace, Q16 (1.0 = as recorded) */
{
    int32_t m = Q16;
    if (!vt[l].lrate || (lv_rnd(p) >> 8) % 4000u == 0u) {   /* (the undulation redrawn, every ~3 s) */
        vt[l].lrate = lv_in(p, 70, 350) * 3117u;            /* 0.07 .. 0.35 Hz: x 2^32 / 1378 / 1000 */
        vt[l].depth = (int32_t)lv_in(p, 9830, 26214);       /* 0.15 .. 0.4 */
    }
    vt[l].lph += vt[l].lrate;
    m += (int32_t)(((int64_t)sin_ph(vt[l].lph) * vt[l].depth) >> 15);
    if (vt[l].ev == VT_RUN && (lv_rnd(p) >> 8) % 8268u == 0u) {   /* (one a block in 6 x 1378: every ~6 s) */
        uint32_t w = lv_in(p, 0, 99);
        vt[l].ev = (uint8_t)(w < 30u ? VT_HALT : w < 55u ? VT_BURST : w < 80u ? VT_GRIND : VT_SWOOP);
        vt[l].phase = 0;
        if (vt[l].ev == VT_HALT)
            vt[l].left = lv_blocks(lv_in(p, 150, 1000));
        else if (vt[l].ev == VT_BURST) {
            vt[l].left = lv_blocks(lv_in(p, 200, 800));
            vt[l].top = (int32_t)lv_in(p, 131072, 196608);  /* x 2 .. 3 */
        } else if (vt[l].ev == VT_GRIND)
            vt[l].left = lv_blocks(lv_in(p, 400, 1500));
        else {
            vt[l].left = lv_blocks(lv_in(p, 800, 2000));
            vt[l].top = (int32_t)lv_in(p, 131072, 163840);  /* x 2 .. 2.5 */
        }
        vt[l].len = vt[l].left ? vt[l].left : 1u;
    }
    if (vt[l].ev != VT_RUN) {
        uint32_t done = vt[l].len - vt[l].left;             /* (blocks of this phase gone) */
        int32_t f = (int32_t)((done << 16) / vt[l].len);   /* 0 .. 1, Q16 (done < 2^15: 32 bits) */
        switch (vt[l].ev) {
        case VT_HALT:
            m = 0;
            break;
        case VT_BURST:
            m = vt[l].top;
            break;
        case VT_GRIND:                                     /* down to a stop, then held */
            m = vt[l].phase ? 0 : (int32_t)(((int64_t)m * (Q16 - f)) >> 16);
            break;
        default:                                           /* up, then the drop */
            m = vt[l].phase ? 26214 : m + (int32_t)(((int64_t)(vt[l].top - m) * f) >> 16);
            break;
        }
        if (!vt[l].left || !--vt[l].left) {                /* the phase over: the next, or back to running */
            if (vt[l].phase == 0 && (vt[l].ev == VT_GRIND || vt[l].ev == VT_SWOOP)) {
                vt[l].phase = 1;
                vt[l].left = vt[l].ev == VT_GRIND ? lv_blocks(lv_in(p, 0, 400)) : lv_blocks(300);
                vt[l].len = vt[l].left ? vt[l].left : 1u;
                if (!vt[l].left)
                    vt[l].ev = VT_RUN;
            } else
                vt[l].ev = VT_RUN;
        }
    }
    return m < 0 ? 0 : m;
}

/* one block of every loop that plays: as many blocks of loop time as the speed has passed (at half speed every
 * other block, at double two; VARY's time on top) */
static void loops_run(void)
{
    uint32_t l, k;
    for (l = 0; l < NLAYER; l++) {
        looper_t *p = &loop[l];
        uint32_t was = p->lclk, steps;
        p->lclk += p->rate == LS_HALF ? 1u : p->rate == LS_DOUBLE ? 4u : 2u;
        steps = (p->lclk >> 1) - (was >> 1);
        lv_tick(l);
        if (!p->play)
            continue;
        if (p->vary && p->rec != LR_REC) {                 /* VARY's time: the steps through its pace */
            uint32_t base = p->rate == LS_HALF ? 32768u : p->rate == LS_DOUBLE ? 131072u : 65536u;
            vt[l].acc += (uint32_t)(((uint64_t)base * (uint32_t)vt_speed(l, p)) >> 16);
            steps = vt[l].acc >> 16;
            vt[l].acc &= 0xFFFFu;
        }
        for (k = 0; k < p->ntrk; k++) {
            loop_trk_t *t = &p->trk[k];
            uint32_t s;
            if ((p->mute >> k) & 1u)
                continue;                         /* (held out: it waits, silent, where it is) */
            for (s = 0; s < steps; s++)
                trk_step(l, p, t);
        }
    }
}

/* the loops' transposition changed (its steps, or the scale it walks): the loop notes sounding move to their new
 * notes at once -- let go at the old, struck at the new */
static void loop_retune(void)
{
    int32_t s = lx_want_steps, tr[NLAYER];
    uint32_t m = lx_want_mask, l, k, pass, same = s == lx_steps, still = !s;
    for (l = 0; l < NLAYER; l++) {
        tr[l] = lx_want_tr[l];
        same &= tr[l] == lx_tr[l];
        still &= !tr[l] && !lx_tr[l];
    }
    if (same && (m == lx_mask || still))
        return;
    for (pass = 0; pass < 2u; pass++) {           /* every old note off first, then every new one on */
        for (l = 0; l < NLAYER; l++) {
            looper_t *p = &loop[l];
            if (!p->play)
                continue;
            for (k = 0; k < p->ntrk; k++) {
                loop_trk_t *t = &p->trk[k];
                uint32_t n;
                if ((p->mute >> k) & 1u)
                    continue;
                for (n = 0; n < 128u; n++)
                    if ((t->snd[n >> 5] >> (n & 31u)) & 1u) {
                        uint32_t was = lx_map(n, lx_steps + lx_tr[l] - t->xb, lx_mask), now = lx_map(n, s + tr[l] - t->xb, m);
                        if (was == now)
                            continue;
                        if (!pass)
                            note_off(l, was, SRC_LOOP);
                        else
                            note_on(l, now, SRC_LOOP);
                    }
            }
        }
    }
    lx_steps = (int8_t)s;
    for (l = 0; l < NLAYER; l++)
        lx_tr[l] = (int8_t)tr[l];
    lx_mask = (uint16_t)m;
}

static void events_take(void)        /* audio ISR, at a block start */
{
    while (ev_r != ev_w) {
        ev_t e = evq[ev_r % EVQ];
        layer_t *y = &lay[e.layer];
        uint32_t k;
        RING_PUBLISH();
        ev_r++;
        TELE(e.type);
        switch (e.type) {
        case EV_ON:
            if (!(e.val & EV_UNHEARD))
                loop_hear(e.layer, 1, e.note);
            e.val &= 0x7Fu;
            note_vel = (uint8_t)(e.val && e.val < 127u ? e.val : 127u);
            note_on(e.layer, e.note, SRC_KEY);
            note_vel = 127;
            break;
        case EV_OFF:
            if (!(e.val & EV_UNHEARD))
                loop_hear(e.layer, 0, e.note);
            note_off(e.layer, e.note, SRC_KEY);
            break;
        case EV_ENGINE:
            y->want_engine = (uint8_t)(e.val < NENG ? e.val : 0);
            break;
        case EV_STOP:
            midi_layer_off(e.layer);
            org_defer[e.layer] = 0;
            org_est[e.layer] = AE_OFF;
            cal_init(&cal_rt[e.layer], 0xC0u + e.layer);
            rus_init(&rus_rt[e.layer], 0x7A5u + e.layer);
            for (k = 0; k < NVOICE; k++) {
                voice_t *v = &vox[k];
                if (vox_owner(v) == e.layer) {
                    v->gate = 0;
                    if (v->pend && v->pend_layer == e.layer)
                        v->pend = 0;              /* (its fade goes on, to nothing) */
                }
                if (v->layer == e.layer && v->sounding && !v->fade)
                    v->fade = FADE;
            }
            (void)y;
            break;
        case EV_LOOP_SET:
            loop_set(e.layer, e.val);
            break;
        case EV_LOOP_EMPTY:                       /* (REC held, snapshots kept: the events stay, packed later) */
            {
                uint32_t keep = loop[e.layer].nev;
                loop_cmd(e.layer, EV_LOOP_CLEAR);
                loop[e.layer].nev = keep;
            }
            break;
        case EV_LOOP_PACK:
            loop_pack(e.layer);
            break;
        case EV_LOOP_TAKES:
            loop_takes(e.layer);
            break;
        case EV_LATCH:                            /* (a key's note latched: its voices kept from the overload guard) */
            for (k = 0; k < NVOICE; k++)
                if (vox[k].gate && vox_owner(&vox[k]) == e.layer && vox[k].note == e.note && vox[k].src == SRC_KEY)
                    vox[k].latched = 1;
            break;
        case EV_MIDI_OFF:                         /* (SEND NOTES turned off: what was sent on, off) */
            for (k = 0; k < NLAYER; k++)
                midi_layer_off(k);
            break;
        case EV_LOOP_MODE:                        /* (note: 0 the direction, 1 the speed, 2 VARY; val: which) */
            loop_mode(e.layer, e.note, e.val);
            break;
        case EV_LOOP_XP:                          /* SHIFT and OCT: the loops moved along the scale */
            loop_retune();
            break;
        case EV_LOOP_TRK:                         /* (note: the loop, val: in or out) */
            loop_trk_set(e.layer, e.note, e.val);
            break;
        case EV_LOOPS:                            /* SHIFT and PLAY: every loop at once, in this block */
            for (k = 0; k < NLAYER; k++) {
                looper_t *p = &loop[k];
                if (e.val && !p->play && p->ntrk) {
                    loop_rewind(p);
                    p->play = 1;
                } else if (!e.val && p->play) {
                    loop_notes_off(k);
                    p->play = 0;
                }
            }
            break;
        default:
            loop_cmd(e.layer, e.type);
            break;
        }
    }
}

/* ---------------------------------------------------------- the mix --- */
static volatile uint8_t shed_req;
static volatile uint8_t load_pct;   /* the render's share of a half buffer, the last one (main.c) */
static int32_t master_q12 = 2048;                /* the MASTER knob (main loop), Q12 */

static int layer_sounding(uint32_t l)
{
    uint32_t k;
    if (lay[l].engine == ENG_CAL)
        return cal_rt[l].live;
    if (lay[l].engine == ENG_RUS)
        return rus_rt[l].live;
    for (k = 0; k < NVOICE; k++)
        if (vox[k].layer == l && vox[k].sounding)
            return 1;
    return 0;
}

#include "gran.c"
#include "delay.c"

/* voice k's fade over: the note it waited for, on the layer it goes to (unless that layer is fading for a change:
 * then its change plays it) */
static void voice_take(uint32_t k)
{
    voice_t *v = &vox[k];
    uint32_t to = v->pend_layer;
    voice_silence(k);
    v->pend = 0;
    v->layer = (uint8_t)to;
    if (lay[to].xfade)
        return;
    voice_on(to, k, 1);
    if (!v->gate)
        voice_off(to, k);
}

/* ------------------------------------------- the block's work, on two cores --- */
/* Each block the work that needs no other is dealt to the two cores (hal/fm1_multicore.h, after BIGORNA build 32
 * and Melodee 1.4.5): every sounding voice of the pool, every layer's organism (CALIGULA, RUSSOLO), and the master
 * delay and reverb -- the dearest first, each to the core with less so far (CPU1 starts with the effects). Each
 * renders into a buffer of its own; then CPU0 takes each layer's buffers in turn as before (the track's envelope,
 * the fades, the filter, the grains, the crush, the level, the sends). The effects run a block behind, on the
 * sends of the block before (their returns 32 samples, 0.73 ms, later); on one core in the same order, so the
 * sound is the same with one core or two. Only CPU0 writes the voices' state outside their own memory */
static int8_t blk_mac[NLAYER][NMAC], blk_felt[NLAYER][NMAC];   /* each layer's macros for the block (as turned, felt) */
static int32_t vr_buf[NVOICE][CTL] POOL __attribute__((aligned(32)));   /* each voice's sound, this block */
static int32_t or_buf[NLAYER][CTL] POOL __attribute__((aligned(32)));   /* each layer's organism's */
static int32_t fx_dl[CTL], fx_rv[CTL], fx_l[CTL], fx_r[CTL];   /* the effects: the last block's sends, their returns */
static uint32_t vr_ticks[NVOICE], or_ticks[NLAYER], fx_ticks;
static uint8_t vr_alive[NVOICE], vr_lay[NVOICE], or_live[NLAYER];   /* (vr_lay: the layer it was rendered for) */
static uint8_t vr_core[NVOICE], or_core[NLAYER], fx_core;   /* CPU1's share */

static void voice_render(uint32_t k)
{
    voice_t *v = &vox[k];
    uint32_t l = v->layer, e = lay[l].engine, c0 = COST_NOW();
    int32_t *vb = vr_buf[k];
    int alive;
    if (e == ENG_FM)
        alive = fm_render_fp(&v->fm, &fm_buf[l][fm_use[l]], vb, blk_mac[l]);
    else if (e == ENG_DRUID) {
        memcpy(vmem[k].dr.mac, blk_felt[l], NMAC);
        alive = dr_render(&vmem[k].dr, &dr_buf[l][dr_use[l]], vb, CTL);
    } else if (e == ENG_BAY) {
        memcpy(vmem[k].ba.mac, blk_felt[l], NMAC);
        alive = ba_render_fp(&vmem[k].ba, &ba_buf[l][ba_use[l]], vb, CTL);
    } else if (e == ENG_HIVE) {
        memcpy(vmem[k].hv.mac, blk_felt[l], NMAC);
        alive = hv_render_fp(&vmem[k].hv, &hv_buf[l][hv_use[l]], vb, CTL);
    } else if (e == ENG_OB) {
        memcpy(vmem[k].ob.mac, blk_felt[l], NMAC);
        alive = ob_render_fp(&vmem[k].ob, &ob_buf[l][ob_use[l]], vb, CTL);
    } else {
        memcpy(vmem[k].an.mac, blk_mac[l], NMAC);
        alive = an_render_fp(&vmem[k].an, &an_buf[l][an_use[l]], vb, CTL);
    }
    COST_ADD(e);
    vr_ticks[k] = COST_NOW() - c0;
    vr_alive[k] = (uint8_t)alive;
    vr_lay[k] = (uint8_t)l;
}
static void org_render(uint32_t l)               /* the layer's organism */
{
    uint32_t c0 = COST_NOW();
    int32_t *b = or_buf[l];
    memset(b, 0, CTL * sizeof b[0]);
    if (lay[l].engine == ENG_CAL) {
        memcpy(cal_rt[l].mac, blk_felt[l], NMAC);
        or_live[l] = (uint8_t)cal_render_fp(&cal_rt[l], &cal_buf[l][cal_use[l]], b);
    } else {
        memcpy(rus_rt[l].mac, blk_felt[l], NMAC);
        or_live[l] = (uint8_t)rus_render(&rus_rt[l], &rus_buf[l][rus_use[l]], b);
    }
    if (or_live[l])
        COST_ADD(lay[l].engine);                 /* (the host's model: a silent organism costs nothing) */
    or_ticks[l] = COST_NOW() - c0;
}
static volatile uint8_t diag_nofx;               /* (a diagnostic build's benchmark: the delay and reverb skipped) */
static uint32_t fx_quiet;                        /* (blocks the effects have had nothing in and nothing out) */
static FAST void fx_render(void)                 /* the master delay, then the reverb (its echoes into it too) */
{
    uint32_t c0 = COST_NOW(), i, any = 0, pk = 0;
    for (i = 0; i < CTL; i++)                    /* (cleared here: RAM code cannot call the flash's memset) */
        fx_l[i] = fx_r[i] = 0;
    if (diag_nofx) {
        fx_ticks = 0;
        return;
    }
    for (i = 0; i < CTL; i++)
        any |= (uint32_t)(fx_dl[i] | fx_rv[i]);
    if (!any && fx_quiet >= 64u) {               /* nothing sent, their tails long gone: not run (no work, no sound) */
        fx_ticks = 0;
        return;
    }
    dl_block(fx_dl, fx_l, fx_r, fx_rv);
    rv_block(fx_rv, fx_l, fx_r, CTL);            /* (its tail goes on with no layer sounding) */
    for (i = 0; i < CTL; i++) {
        uint32_t a = (uint32_t)(fx_l[i] < 0 ? -fx_l[i] : fx_l[i]) | (uint32_t)(fx_r[i] < 0 ? -fx_r[i] : fx_r[i]);
        pk = a > pk ? a : pk;
    }
    fx_quiet = !any && pk < 8u ? fx_quiet + 1u : 0u;   /* (out below -72 dB, nothing in: quiet a while) */
    fx_ticks = COST_NOW() - c0;
}
static __typeof__(&fx_render) volatile fx_render_p = fx_render;   /* (the flash's way in: RAM code is out of a call's reach) */
static void blk_job(void *ctx)                   /* CPU1's share */
{
    uint32_t k;
    (void)ctx;
    if (fx_core)
        fx_render_p();
    for (k = 0; k < NVOICE; k++)
        if (vr_core[k])
            voice_render(k);
    for (k = 0; k < NLAYER; k++)
        if (or_core[k])
            org_render(k);
}
/* the changes due, the macros; then the work dealt, rendered on both cores, and CPU1's joined */
static void blk_render(void)
{
    uint32_t cst[NVOICE + NLAYER + 1u], s0 = 0, s1, l, k, j, n = 0, i;
    uint8_t ord[NVOICE + NLAYER + 1u];           /* (0 .. NVOICE-1 a voice, then the organisms, then the effects) */
    int any1 = 0;
    for (l = 0; l < NLAYER; l++) {
        layer_t *y = &lay[l];
        mod_block(l);                            /* (the modulation: its LFOs a step on, its offsets) */
        if (!y->xfade && change_due(l)) {
            if (layer_sounding(l))
                y->xfade = FADE;                  /* out first, then the change */
            else
                change_apply(l);
        }
        for (i = 0; i < NMAC; i++) {
            int32_t m = mod_val(l, MD_MAC1 + i, lay_mac[l][i], -MAC_MAX, MAC_MAX), a = m < 0 ? -m : m, q = (int32_t)isqrt32((uint32_t)(a * MAC_MAX));
            blk_mac[l][i] = (int8_t)m;
            blk_felt[l][i] = (int8_t)(m < 0 ? -q : q);   /* (the organisms' macros felt early: the square root of the
                                                          * turn; FM and ANIMA take theirs as turned) */
        }
    }
    audio_worker_check();
    {   /* several HIVE voices at once: each thinned (hive.c hv_thin) */
        uint32_t nh = 0;
        for (k = 0; k < NVOICE; k++)
            nh += vox[k].sounding && lay[vox[k].layer].engine == ENG_HIVE;
        hv_thin = (uint8_t)(nh >= 3u);
    }
    for (k = 0; k < NVOICE + NLAYER + 1u; k++) {  /* (the work by its last cost, the dearest first ..) */
        uint32_t c;
        if (k < NVOICE) {
            vr_core[k] = 0;
            vr_lay[k] = 0xFFu;
            if (!vox[k].sounding || eng_organism(lay[vox[k].layer].engine))
                continue;
            c = vox[k].cost ? vox[k].cost : eng_cost[lay[vox[k].layer].engine & 7u];
        } else if (k < NVOICE + NLAYER) {
            l = k - NVOICE;
            or_core[l] = or_live[l] = 0;
            if (!eng_organism(lay[l].engine))
                continue;
            c = org_cost[l] ? org_cost[l] : eng_cost[lay[l].engine];
        } else {
            fx_core = 0;
            c = fx_ticks;
        }
        cst[k] = c;
        for (j = n; j > 0u && cst[ord[j - 1u]] < c; j--)
            ord[j] = ord[j - 1u];
        ord[j] = (uint8_t)k;
        n++;
    }
    if (audio_worker_online) {
        s1 = 0;
        for (j = 0; j < n; j++) {                /* (.. each to the core with less so far) */
            k = ord[j];
            if (k == NVOICE + NLAYER || s1 <= s0) {   /* (the effects on CPU1) */
                if (k < NVOICE)
                    vr_core[k] = 1;
                else if (k < NVOICE + NLAYER)
                    or_core[k - NVOICE] = 1;
                else
                    fx_core = 1;
                s1 += cst[k];
            } else
                s0 += cst[k];
        }
        any1 = audio_worker_submit(blk_job, 0);
    }
    if (!any1) {                                 /* (CPU1 off, or not free: all here) */
        memset(vr_core, 0, sizeof vr_core);
        memset(or_core, 0, sizeof or_core);
        fx_core = 0;
    }
    if (!fx_core)
        fx_render_p();
    for (k = 0; k < NVOICE; k++)
        if (vox[k].sounding && !vr_core[k] && !eng_organism(lay[vox[k].layer].engine))
            voice_render(k);
    for (l = 0; l < NLAYER; l++)
        if (eng_organism(lay[l].engine) && !or_core[l])
            org_render(l);
    if (any1 && !audio_worker_join()) {          /* (CPU1 lost: retired; its share silent this block, kept going) */
        for (k = 0; k < NVOICE; k++)
            if (vr_core[k]) {
                memset(vr_buf[k], 0, sizeof vr_buf[k]);
                vr_alive[k] = 1;
                vr_ticks[k] = 0;
                vr_lay[k] = vox[k].layer;
            }
        for (l = 0; l < NLAYER; l++)
            if (or_core[l]) {
                memset(or_buf[l], 0, sizeof or_buf[l]);
                or_live[l] = 1;
                or_ticks[l] = 0;
            }
        if (fx_core) {
            memset(fx_l, 0, sizeof fx_l);
            memset(fx_r, 0, sizeof fx_r);
        }
    }
    an_cache_fill();                             /* (a wavetable missed this block, copied in now: no core rendering) */
}

#if defined(FELUCCA_UAC) && FELUCCA_UAC && UAC_NBLK > 2u
static int32_t usb_trk[NLAYER][CTL];             /* (USB audio: each track's block, mono, after its level) */
#endif
static void layer_block(uint32_t l, int32_t *mixl, int32_t *mixr, int32_t *rvin, int32_t *dlin)
{
    layer_t *y = &lay[l];
    int32_t buf[CTL], *vb, pk = 0;
    uint32_t i, k, any = 0;
    for (i = 0; i < CTL; i++)
        buf[i] = 0;
    org_cost[l] = 0;
    if (y->engine == ENG_CAL || y->engine == ENG_RUS) {   /* the organism: one for the layer */
        uint32_t live = or_live[l];
        memcpy(buf, or_buf[l], sizeof buf);
        if (org_defer[l] && (!lay_env_on[l] || org_est[l] == AE_DONE)) {   /* (released: the note kept, let go) */
            if (y->engine == ENG_CAL) {
                cal_note_off(&cal_rt[l], org_defer[l] - 1u, org_dsrc[l]);
                cal_rt[l].hush = 1;
            } else
                rus_note_off(&rus_rt[l], org_defer[l] - 1u);
            org_defer[l] = 0;
            org_est[l] = lay_env_on[l] ? AE_DONE : AE_OFF;   /* (its own release, silent: the envelope's is over) */
        }
        if (live && lay_env_on[l] && org_est[l]) {   /* the track's own envelope over the organism */
            int32_t e0 = org_elv[l];
            if (org_est[l] == AE_DONE)
                memset(buf, 0, sizeof buf);
            else {
                env_step(&org_est[l], &org_elv[l], l);
                env_apply(buf, e0, org_elv[l]);
            }
        }
        if (live) {
            any = 1;
            org_cost[l] = or_ticks[l];
            eng_cost[y->engine] = (eng_cost[y->engine] * 7u + org_cost[l] + 7u) / 8u;
        }
    }
    for (k = 0; k < NVOICE && !eng_organism(y->engine); k++) {
        voice_t *v = &vox[k];
        int alive;
        if (v->layer != l || !v->sounding)
            continue;
        if (vr_lay[k] != l)                      /* (taken for this layer since the deal: rendered now, here) */
            voice_render(k);
        vb = vr_buf[k];
        alive = vr_alive[k];
        {
            uint32_t c = vr_ticks[k];
            if (c) {                             /* (0: CPU1 lost it, nothing measured) */
                v->cost = (uint16_t)(c > 0xFFFFu ? 0xFFFFu : c);
                if (alive)
                    eng_cost[y->engine & 7u] = (eng_cost[y->engine & 7u] * 7u + c + 7u) / 8u;
            }
        }
        if (alive && v->est) {                     /* the track's own envelope over the voice */
            int32_t e0 = v->elv;
            env_step(&v->est, &v->elv, l);
            env_apply(vb, e0, v->elv);
            if (v->est == AE_DONE) {               /* (its release over: the voice ends) */
                alive = 0;
                v->est = AE_OFF;
                v->fm.live = 0;
                if (v->eng == ENG_DRUID)
                    vmem[k].dr.live = 0;
                else if (v->eng == ENG_BAY)
                    vmem[k].ba.live = 0;
                else if (v->eng == ENG_HIVE)
                    vmem[k].hv.live = 0;
                else if (v->eng == ENG_OB)
                    vmem[k].ob.live = 0;
                else
                    vmem[k].an.live = 0;
            }
        }
        if (!alive) {
            v->sounding = 0;
            if (v->fade && v->pend)               /* (it ended in its fade: the new note now, where it goes) */
                voice_take(k);
            else
                v->fade = v->fade_n = 0;
            continue;
        }
        any = 1;
        if (v->vel && v->vel < 127u) {             /* (a softer note: the balls') */
            int32_t vg = v->vel * 258;
            for (i = 0; i < CTL; i++)
                vb[i] = mulq15(vb[i], vg);
        }
        if (!v->fade && !v->pend) {               /* a voice let go must end: far under its loudest, or no longer falling */
            uint32_t bp = 0;
            for (i = 0; i < CTL; i++) {
                uint32_t a = (uint32_t)(vb[i] < 0 ? -vb[i] : vb[i]);
                bp = a > bp ? a : bp;
            }
            if (bp > v->pk)
                v->pk = bp;
            v->lvl = bp;
            if (!v->gate && v->pk) {
                v->quiet = (uint16_t)(bp < v->pk >> TAIL_FLOOR ? v->quiet + 1u : 0u);
                if (bp > v->wmax)
                    v->wmax = bp;
                if (++v->rt >= TAIL_STRETCH) {    /* (a stretch done: fallen at least 1.5 dB since the one before?) */
                    int stuck = v->rmax && (uint64_t)v->wmax * 100u > (uint64_t)v->rmax * 84u;
                    v->rmax = v->wmax;
                    v->wmax = 0;
                    v->rt = 0;
                    if (stuck || ++v->nst >= TAIL_MAX)
                        v->quiet = TAIL_QUIET;
                }
                if (v->quiet >= TAIL_QUIET) {
                    v->fade = (uint8_t)TAIL_FADE;
                    v->fade_n = (uint8_t)TAIL_FADE;
                }
            }
        }
        if (v->fade) {
            uint32_t fn = v->fade_n ? v->fade_n : FADE;
            int32_t g0 = (int32_t)(v->fade * (uint32_t)Q15 / fn), g1 = (int32_t)((v->fade - 1u) * (uint32_t)Q15 / fn);
            for (i = 0; i < CTL; i++)
                buf[i] += mulq15(vb[i], g0 + (((g1 - g0) * (int32_t)i) >> 5));
            if (--v->fade == 0) {
                if (v->pend)
                    voice_take(k);
                else
                    voice_silence(k);
            }
        } else
            for (i = 0; i < CTL; i++)
                buf[i] += vb[i];
    }
    if (y->xfade) {
        int32_t g0 = (int32_t)(y->xfade * (uint32_t)Q15 / FADE), g1 = (int32_t)((y->xfade - 1u) * (uint32_t)Q15 / FADE);
        for (i = 0; i < CTL; i++)
            buf[i] = mulq15(buf[i], g0 + (((g1 - g0) * (int32_t)i) >> 5));
        if (--y->xfade == 0 || !any) {
            y->xfade = 0;
            change_apply(l);
        }
    }
    int32_t m_gr = mod_val(l, MD_GR, lay_gr[l], -FLT_MAX, FLT_MAX);   /* (the knobs as the modulation moves them) */
    if (!any && !(gran[l].tail && (m_gr || gran[l].amt))) {   /* (grains ring on a while after the voices) */
        y->meter -= y->meter >> 4;
        y->g = lay_gain[l];
        y->pl = lay_pl[l];
        y->pr = lay_pr[l];
        y->sd = lay_send[l];
        y->dsd = lay_dsend[l];
        return;
    }
    {   /* the DJ filter */
        int32_t v = mod_val(l, MD_FLT, lay_flt[l], -FLT_MAX, FLT_MAX), a = v < 0 ? -v : v;
        int32_t amt1 = a >= 4 ? Q15 : a * (Q15 / 4);
        int32_t q1 = 2896 + (a >= 24 ? 13488 : a * 13488 / 24);         /* Q 0.707 .. 4, Q12 */
        if (v)
            y->f_hp = (uint8_t)(v > 0);
        if (amt1 || y->f_amt) {
            svf_c c;
            int32_t a0 = y->f_amt, da = (amt1 - a0) / CTL;
            if (!y->f_amt) {                      /* coming in: from where the knob is */
                y->f_oct = flt_oct(v ? v : (y->f_hp ? 1 : -1));
                y->f_q = q1;
                y->flt.ic1 = y->flt.ic2 = 0;
            }
            if (v)
                y->f_oct += (flt_oct(v) - y->f_oct) >> 2;   /* (glides: a knob's click is a step) */
            y->f_q += (q1 - y->f_q) >> 2;
            svf_coef(&c, (int32_t)exp2_q16(y->f_oct - (16 << 16)), y->f_q);   /* 2^oct Hz */
            for (i = 0; i < CTL; i++) {
                int32_t f = svf_run(&y->flt, &c, buf[i], y->f_hp ? 3 : 1);
                a0 += da;
                buf[i] += mulq15(f - buf[i], a0);
            }
            y->f_amt = amt1;
        }
    }
    gran_block(l, buf, CTL, m_gr, (int)any);             /* the granulator */
    crush_block(y, buf, CTL, mod_val(l, MD_CR, lay_cr[l], -FLT_MAX, FLT_MAX));   /* the crush */
    for (i = 0; i < CTL; i++)
        buf[i] = mulq12(buf[i], LAYER_GAIN);
    lim_block(&y->lim_env, &y->lim_g, buf, CTL);
    dist_block(&dist[l], buf, CTL, mod_val(l, MD_DIST, lay_dist[l], 0, 100));   /* the distortion (before the level, sends) */
    {   /* the layer's level, then its pan, gliding from the last block's to the knobs' */
        int32_t g1 = lay_gain[l], pl1 = lay_pl[l], pr1 = lay_pr[l], sd1 = lay_send[l], dd1 = lay_dsend[l];
        if (mst[l].off[MD_LVL]) {                 /* (the modulation's level: 1.5 dB a step, as the knob's) */
            int32_t lv = mod_val(l, MD_LVL, lay_lvl[l], 0, LEVEL_MAX);
            g1 = lv <= 0 ? 0 : (int32_t)(exp2_q16((lv - LEVEL_UNITY) * 16328) >> 2);
        }
        if (lay_duck[l])                          /* (a morph changing the sound: down, then up again) */
            g1 -= mulq15(g1, lay_duck[l]);
        if (mst[l].off[MD_PAN]) {                 /* (.. its pan) */
            int32_t ph = (int32_t)((uint32_t)(mod_val(l, MD_PAN, lay_pan[l], -PAN_MAX, PAN_MAX) + PAN_MAX) *
                                   (0x40000000u / (2u * PAN_MAX)));
            pl1 = sin_ph((uint32_t)ph + 0x40000000u);
            pr1 = sin_ph((uint32_t)ph);
        }
        if (mst[l].off[MD_RV])                    /* (.. its sends) */
            sd1 = clampi(sd1 + mst[l].off[MD_RV] * 327, 0, 32767);
        if (mst[l].off[MD_DL])
            dd1 = clampi(dd1 + mst[l].off[MD_DL] * 327, 0, 32767);
        int32_t dg = (g1 - y->g) / CTL, dl = (pl1 - y->pl) / CTL, dr = (pr1 - y->pr) / CTL, ds = (sd1 - y->sd) / CTL;
        int32_t dds = (dd1 - y->dsd) / CTL, g = y->g, pl = y->pl, pr = y->pr, sd = y->sd, dsd = y->dsd;
        for (i = 0; i < CTL; i++) {
            int32_t x, a;
            g += dg;
            pl += dl;
            pr += dr;
            sd += ds;
            dsd += dds;
            x = (int32_t)(((int64_t)buf[i] * g) >> 14);
            a = absi(x);
            if (a > pk)
                pk = a;
#if defined(FELUCCA_UAC) && FELUCCA_UAC && UAC_NBLK > 2u
            usb_trk[l][i] = x;                    /* (USB audio: the track alone, mono) */
#endif
            mixl[i] += mulq15(x, pl);
            mixr[i] += mulq15(x, pr);
            rvin[i] += mulq15(x, sd);
            dlin[i] += mulq15(x, dsd);
        }
        y->g = g1;
        y->pl = pl1;
        y->pr = pr1;
        y->sd = sd1;
        y->dsd = dd1;
    }
    y->meter = pk > y->meter ? pk : y->meter - (y->meter >> 5);
}

/* above half scale, a soft knee into a ceiling at full scale */
static inline int32_t ceiling(int32_t x)
{
    int32_t a = absi(x);
    if (a <= Q15 / 2)
        return x;
    a = Q15 / 2 + (tanh_q15((a - Q15 / 2) * 2) >> 1);
    return x < 0 ? -a : a;
}

/* overload (main.c: two halves in a row over 85 %): the oldest tail of a released note fades; else a CALIGULA's
 * tail (no key on it) is hushed; else the oldest held note that has sounded 1.5 s. A note just played never
 * goes: under it, nothing does */
#define SHED_HELD_BLOCKS 2067u   /* 1.5 s */
static int shed_voice(int held)  /* the oldest released voice (held: the oldest held 1.5 s) faded; 0 if none */
{
    uint32_t k, ok = NVOICE;
    for (k = 0; k < NVOICE; k++) {
        const voice_t *v = &vox[k];
        if (!v->sounding || v->fade || (held ? !v->gate || v->latched || blk_clock - v->born < SHED_HELD_BLOCKS : v->gate))
            continue;                             /* (a latched note is never let go: a drone held on purpose) */
        if (ok == NVOICE || (held ? v->age < vox[ok].age : v->lvl < vox[ok].lvl))
            ok = k;                               /* (a tail: the quietest; a held note: the oldest) */
    }
    if (ok == NVOICE)
        return 0;
    vox[ok].fade = FADE;                          /* (its key still held: it comes back with the next note) */
    return 1;
}
/* near the edge (1): a released tail goes, else CALIGULA's ring-out; late (2): if none, a held note too */
static void shed(uint32_t late)
{
    uint32_t l;
    if (shed_voice(0))
        return;
    for (l = 0; l < NLAYER; l++)
        if (lay[l].engine == ENG_CAL && cal_rt[l].live && !cal_rt[l].nheld && !cal_rt[l].hush) {
            cal_rt[l].hush = 1;
            return;
        }
    if (late)
        shed_voice(1);
}

/* ahead of the block: what it will take (the last one, and what has started since) over the budget -- the oldest
 * release tails let go quickly (then CALIGULA's ring-out) until it fits. A held note is never let go here (the
 * overload guard does that, when the audio is late after all) */
static void budget_guard(void)
{
    uint32_t pred = blk_last + (audio_worker_online ? blk_new / 2u : blk_new), l, k;   /* (two cores: half each) */
    blk_new = 0;
    for (k = 0; k < NVOICE; k++)                  /* (those already fading out are as good as gone) */
        if (vox[k].sounding && vox[k].fade && !vox[k].pend)
            pred -= pred < vox[k].cost ? pred : vox[k].cost;
    for (l = 0; l < NLAYER; l++)
        if (lay[l].engine == ENG_CAL && cal_rt[l].hush)
            pred -= pred < org_cost[l] ? pred : org_cost[l];
    while (pred > BUDGET) {
        uint32_t ok = NVOICE;
        for (k = 0; k < NVOICE; k++) {
            const voice_t *v = &vox[k];
            if (v->sounding && !v->gate && !v->fade && !v->pend && (ok == NVOICE || v->lvl < vox[ok].lvl))
                ok = k;                           /* (the quietest tail first: the least heard) */
        }
        if (ok < NVOICE) {
#if TELEMETRY
            tele_trims++;
#endif
            vox[ok].fade = QUICK;
            vox[ok].fade_n = QUICK;
            pred -= pred < vox[ok].cost ? pred : vox[ok].cost;
            continue;
        }
        for (l = 0; l < NLAYER; l++)
            if (lay[l].engine == ENG_CAL && cal_rt[l].live && !cal_rt[l].nheld && !cal_rt[l].hush)
                break;
        if (l == NLAYER)
            return;                               /* (nothing more to let go: only what is held) */
        cal_rt[l].hush = 1;
        pred -= pred < org_cost[l] ? pred : org_cost[l];
    }
}

#if defined(FELUCCA_UAC) && FELUCCA_UAC
static int32_t usb_blk[UAC_NBLK * CTL];           /* (the block for USB audio: the master before the MASTER knob, at
                                                  * unity -- a recording does not follow the headphones' level; then
                                                  * each track, mono: after its level, before its pan and the master
                                                  * delay and reverb) */
#endif
/* one block: the events, the loops, the work (two cores), the layers, the effects' returns, the master (Q15 stereo,
 * interleaved) */
static void mix_block(int32_t *out)
{
    int32_t l_[CTL], r_[CTL], rvin[CTL], dlin[CTL];
    uint32_t i, l;
    uint32_t c0 = COST_NOW();
    an_cache_now = blk_clock + 1u;                /* (ANIMA's wavetables from RAM while the ISR renders) */
    midi_main_drain();                            /* (the main loop's MIDI out: the CCs) */
    events_take();
    loops_run();
    budget_guard();
    if (shed_req) {                               /* overload: something already heard goes */
        shed(shed_req > 1u);
        shed_req = 0;
    }
    blk_render();
    for (i = 0; i < CTL; i++)
        l_[i] = r_[i] = rvin[i] = dlin[i] = 0;
#if defined(FELUCCA_UAC) && FELUCCA_UAC && UAC_NBLK > 2u
    memset(usb_trk, 0, sizeof usb_trk);           /* (a silent track: silence) */
#endif
    for (l = 0; l < NLAYER; l++)
        layer_block(l, l_, r_, rvin, dlin);
    for (i = 0; i < CTL; i++) {
        l_[i] += fx_l[i];                         /* the delay's and the reverb's returns (on the last block's sends) */
        r_[i] += fx_r[i];
#if defined(FELUCCA_UAC) && FELUCCA_UAC
        usb_blk[UAC_NBLK * i] = ceiling(l_[i]);
        usb_blk[UAC_NBLK * i + 1u] = ceiling(r_[i]);
#if UAC_NBLK > 2u
        for (l = 0; l < NLAYER && l + 2u < UAC_NBLK; l++)
            usb_blk[UAC_NBLK * i + 2u + l] = ceiling(usb_trk[l][i]);
#endif
#endif
        out[2u * i] = ceiling(mulq12(l_[i], master_q12));
        out[2u * i + 1u] = ceiling(mulq12(r_[i], master_q12));
    }
    memcpy(fx_dl, dlin, sizeof fx_dl);            /* (this block's sends, for the effects next block) */
    memcpy(fx_rv, rvin, sizeof fx_rv);
    blk_last = COST_NOW() - c0;
    an_cache_now = 0;
    blk_clock++;
}

/* ------------------------------------------------ the sounds (main loop) --- */
/* the tracks' engines at the start (and in an empty session): FM, ANIMA, CALIGULA, HIVE, DRUID, BAYAREA (as on the
 * SMK-37 port, its build 29) */
static const uint8_t ENG_START[NLAYER] = {ENG_FM, ENG_ANIMA, ENG_CAL, ENG_HIVE, ENG_DRUID, ENG_BAY};
static void layers_init(void)
{
    uint32_t l, k;
    memset(vox, 0, sizeof vox);
#ifndef HOST
    eng_cost[ENG_FM] = BLK_TICKS * 5u / 100u;     /* (first guesses, until they are measured) */
    eng_cost[ENG_ANIMA] = BLK_TICKS * 9u / 100u;
    eng_cost[ENG_CAL] = BLK_TICKS * 20u / 100u;
    eng_cost[ENG_RUS] = BLK_TICKS * 4u / 100u;
    eng_cost[ENG_DRUID] = BLK_TICKS * 6u / 100u;
    eng_cost[ENG_BAY] = BLK_TICKS * 6u / 100u;
    eng_cost[ENG_HIVE] = BLK_TICKS * 8u / 100u;
    eng_cost[ENG_OB] = BLK_TICKS * 6u / 100u;
#else
    memcpy(eng_cost, host_cost_model, sizeof eng_cost);
#endif
    memset(dist, 0, sizeof dist);
    for (l = 0; l < NLAYER; l++)
        dist[l].g = dist[l].gp = Q16;
    blk_last = blk_new = 0;
    for (k = 0; k < NVOICE; k++) {
        an_init(&vmem[k].an, 0xA11u + k * 131u);
        vox[k].eng = ENG_ANIMA;
        vox[k].layer = (uint8_t)(k % NLAYER);
    }
    for (l = 0; l < NLAYER; l++) {
        lay[l].lim_g = Q15;
        lay[l].engine = lay[l].want_engine = ENG_START[l];
        cal_init(&cal_rt[l], 0xC0u + l * 131u);
        rus_init(&rus_rt[l], 0x7A5u + l * 131u);
    }
    rv_init();
    gran_init();
    dl_init();
    memset(fx_dl, 0, sizeof fx_dl);               /* (the effects' sends from the block before, and their returns) */
    memset(fx_rv, 0, sizeof fx_rv);
    memset(fx_l, 0, sizeof fx_l);
    memset(fx_r, 0, sizeof fx_r);
}

/* the sounds drawn but not measured yet (bit e: engine e). Measuring renders seconds of the sound (a tenth to a
 * third of a second each, here): at start and for an empty session only the engine each layer plays is measured;
 * the others when the layer first comes to them (snd_ready). A session keeps them unmeasured (level 0) */
static uint8_t snd_raw[NLAYER];
/* layer l's sound on engine e measured -- drawn again from what made it first, its family too (the same sound, at its design's own
 * level, which ANIMA's measure scales). snd_ready: if it was not yet, in place (the ISR renders it only once the
 * layer is on engine e): before the layer is moved to e */
static void snd_measure(uint32_t l, uint32_t e, uint32_t k)   /* (its buffer k) */
{
    snd_raw[l] &= (uint8_t)~(1u << e);
    if (e == ENG_FM) {
        fm_patch_t *d = &fm_buf[l][k];
        fm_generate(d, d->family, d->seed, d->path, d->gen);
        fm_measure(d);
    } else if (e == ENG_ANIMA) {
        an_design_t *d = &an_buf[l][k];
        an_design(d, d->kind, d->seed, d->path, d->gen);
        an_measure(d);
    } else if (e == ENG_CAL) {
        cal_design_t *d = &cal_buf[l][k];
        cal_design(d, d->kind, d->seed, d->path, d->gen);
        cal_measure(d);
    } else if (e == ENG_RUS) {
        rus_design_t *d = &rus_buf[l][k];
        rus_design(d, d->kind, d->seed, d->path, d->gen);
        rus_measure(d);
    } else if (e == ENG_DRUID) {
        dr_design_t *d = &dr_buf[l][k];
        dr_design(d, d->kind, d->seed, d->path, d->gen);
        dr_measure(d);
    } else if (e == ENG_BAY) {
        ba_design_t *d = &ba_buf[l][k];
        ba_design(d, d->kind, d->seed, d->path, d->gen);
        ba_measure(d);
    } else if (e == ENG_OB) {
        ob_design_t *d = &ob_buf[l][k];
        ob_design(d, d->kind, d->seed, d->path, d->gen);
        ob_measure(d);
    } else {
        hv_design_t *d = &hv_buf[l][k];
        hv_design(d, d->kind, d->seed, d->path, d->gen);
        hv_measure(d);
    }
}
static void snd_ready(uint32_t l, uint32_t e)
{
    if (snd_raw[l] >> e & 1u)
        snd_measure(l, e, e == ENG_FM ? fm_pub[l] : e == ENG_ANIMA ? an_pub[l] : e == ENG_CAL ? cal_pub[l]
                          : e == ENG_RUS ? rus_pub[l] : e == ENG_DRUID ? dr_pub[l] : e == ENG_BAY ? ba_pub[l]
                          : e == ENG_OB ? ob_pub[l] : hv_pub[l]);
}

/* the ISR has taken the last sound published for this layer and engine (or waits ~20 ms for it) */
static int sound_slot_free(volatile uint8_t *pub, volatile uint8_t *use)
{
    uint32_t t0 = fm1_ms;
    while (*pub != *use) {
        if ((uint32_t)(fm1_ms - t0) > 40u)
            return 0;
#ifndef HOST
        fm1_wdt_feed();
#else
        host_pump();
#endif
    }
    return 1;
}

/* a new FM patch for layer l, not of the family it had (main loop: rendered to be measured, tens of ms each).
 * Generation 1 draws up to four and keeps the first of another family whose brightness suits the path (darker
 * for AMBIENT, brighter for EXPERIMENTAL), else the nearest */
static int fm_misfit(int32_t hz, uint32_t path, int same)
{
    static const int32_t LO[PATH_N] = {150, 120, 1300}, HI[PATH_N] = {6000, 1100, 7000};
    int32_t m = hz < LO[path] ? LO[path] - hz : hz > HI[path] ? hz - HI[path] : 0;
    return (hz <= 0 ? 1000000 : m) + (same ? 4000 : 0);
}
/* the family (kind, species) the next new sound is to be of (SHIFT + HOME: the one the track plays); -1, any but
 * that one (HOME) */
static int32_t new_kind = -1;
static int new_fm(uint32_t l, uint32_t seed, uint32_t path)
{
    static fm_patch_t cand;
    uint32_t k, tries;
    int32_t was = fm_buf[l][fm_use[l]].family, best = 0x7FFFFFFF;
    if (!sound_slot_free(&fm_pub[l], &fm_use[l]))
        return 0;
    k = fm_use[l] ^ 1u;
    for (tries = 0; tries < 4u; tries++) {
        int32_t m;
        fm_generate(&cand, new_kind, seed + tries * 7919u, path, GEN_NOW);
        m = fm_misfit(fm_measure(&cand), path, new_kind < 0 && cand.family == was);
        if (m < best) {
            best = m;
            fm_buf[l][k] = cand;
        }
        if (!m)
            break;
    }
    RING_PUBLISH();
    fm_pub[l] = (uint8_t)k;
    snd_raw[l] &= (uint8_t)~(1u << ENG_FM);   /* (measured) */
    return 1;
}
static int new_anima(uint32_t l, uint32_t seed, uint32_t path)
{
    uint32_t k, tries;
    int32_t was = an_buf[l][an_use[l]].kind;
    if (!sound_slot_free(&an_pub[l], &an_use[l]))
        return 0;
    k = an_use[l] ^ 1u;
    for (tries = 0; tries < 4u; tries++) {
        an_design(&an_buf[l][k], new_kind, seed + tries * 7919u, path, GEN_NOW);
        if (new_kind >= 0 || an_buf[l][k].kind != was)
            break;
    }
    an_measure(&an_buf[l][k]);
    RING_PUBLISH();
    an_pub[l] = (uint8_t)k;
    snd_raw[l] &= (uint8_t)~(1u << ENG_ANIMA);   /* (measured) */
    return 1;
}
static int new_cal(uint32_t l, uint32_t seed, uint32_t path)
{
    uint32_t k, tries;
    int32_t was = cal_buf[l][cal_use[l]].kind;
    if (!sound_slot_free(&cal_pub[l], &cal_use[l]))
        return 0;
    k = cal_use[l] ^ 1u;
    for (tries = 0; tries < 4u; tries++) {
        cal_design(&cal_buf[l][k], new_kind, seed + tries * 7919u, path, GEN_NOW);
        if (new_kind >= 0 || cal_buf[l][k].kind != was)
            break;
    }
    cal_measure(&cal_buf[l][k]);
    RING_PUBLISH();
    cal_pub[l] = (uint8_t)k;
    snd_raw[l] &= (uint8_t)~(1u << ENG_CAL);   /* (measured) */
    return 1;
}
/* layer l's sound on engine e drawn again from what made it (a preset): its seed, path and generation, and the
 * level it was measured at, and its family (-1: drawn from the seed); published as a new sound is. 0: the ISR still
 * holds the other buffer */
static int sound_from_seed(uint32_t l, uint32_t e, uint32_t seed, uint32_t path, uint32_t gen, int32_t level, int32_t kind)
{
    uint32_t k;
    if (e == ENG_FM) {
        if (!sound_slot_free(&fm_pub[l], &fm_use[l]))
            return 0;
        k = fm_use[l] ^ 1u;
        fm_generate(&fm_buf[l][k], kind, seed, path, gen);
        fm_buf[l][k].gain = level;
        RING_PUBLISH();
        fm_pub[l] = (uint8_t)k;
        snd_raw[l] &= (uint8_t)~(1u << ENG_FM);   /* (measured) */
    } else if (e == ENG_ANIMA) {
        if (!sound_slot_free(&an_pub[l], &an_use[l]))
            return 0;
        k = an_use[l] ^ 1u;
        an_design(&an_buf[l][k], kind, seed, path, gen);
        an_buf[l][k].out = level;
        RING_PUBLISH();
        an_pub[l] = (uint8_t)k;
        snd_raw[l] &= (uint8_t)~(1u << ENG_ANIMA);   /* (measured) */
    } else if (e == ENG_CAL) {
        if (!sound_slot_free(&cal_pub[l], &cal_use[l]))
            return 0;
        k = cal_use[l] ^ 1u;
        cal_design(&cal_buf[l][k], kind, seed, path, gen);
        cal_buf[l][k].out = level;
        RING_PUBLISH();
        cal_pub[l] = (uint8_t)k;
        snd_raw[l] &= (uint8_t)~(1u << ENG_CAL);   /* (measured) */
    } else if (e == ENG_HIVE) {
        if (!sound_slot_free(&hv_pub[l], &hv_use[l]))
            return 0;
        k = hv_use[l] ^ 1u;
        hv_design(&hv_buf[l][k], kind, seed, path, gen);
        hv_buf[l][k].out = level;
        RING_PUBLISH();
        hv_pub[l] = (uint8_t)k;
        snd_raw[l] &= (uint8_t)~(1u << ENG_HIVE);   /* (measured) */
    } else if (e == ENG_BAY) {
        if (!sound_slot_free(&ba_pub[l], &ba_use[l]))
            return 0;
        k = ba_use[l] ^ 1u;
        ba_design(&ba_buf[l][k], kind, seed, path, gen);
        ba_buf[l][k].out = level;
        RING_PUBLISH();
        ba_pub[l] = (uint8_t)k;
        snd_raw[l] &= (uint8_t)~(1u << ENG_BAY);   /* (measured) */
    } else if (e == ENG_OB) {
        if (!sound_slot_free(&ob_pub[l], &ob_use[l]))
            return 0;
        k = ob_use[l] ^ 1u;
        ob_design(&ob_buf[l][k], kind, seed, path, gen);
        ob_buf[l][k].out = level;
        RING_PUBLISH();
        ob_pub[l] = (uint8_t)k;
        snd_raw[l] &= (uint8_t)~(1u << ENG_OB);   /* (measured) */
    } else if (e == ENG_DRUID) {
        if (!sound_slot_free(&dr_pub[l], &dr_use[l]))
            return 0;
        k = dr_use[l] ^ 1u;
        dr_design(&dr_buf[l][k], kind, seed, path, gen);
        dr_buf[l][k].out = level;
        RING_PUBLISH();
        dr_pub[l] = (uint8_t)k;
        snd_raw[l] &= (uint8_t)~(1u << ENG_DRUID);   /* (measured) */
    } else {
        if (!sound_slot_free(&rus_pub[l], &rus_use[l]))
            return 0;
        k = rus_use[l] ^ 1u;
        rus_design(&rus_buf[l][k], kind, seed, path, gen);
        rus_buf[l][k].out = level;
        RING_PUBLISH();
        rus_pub[l] = (uint8_t)k;
        snd_raw[l] &= (uint8_t)~(1u << ENG_RUS);   /* (measured) */
    }
    return 1;
}
static int new_rus(uint32_t l, uint32_t seed, uint32_t path)
{
    uint32_t k, tries;
    int32_t was = rus_buf[l][rus_use[l]].kind;
    if (!sound_slot_free(&rus_pub[l], &rus_use[l]))
        return 0;
    k = rus_use[l] ^ 1u;
    for (tries = 0; tries < 4u; tries++) {
        rus_design(&rus_buf[l][k], new_kind, seed + tries * 7919u, path, GEN_NOW);
        if (new_kind >= 0 || rus_buf[l][k].kind != was)
            break;
    }
    rus_measure(&rus_buf[l][k]);
    RING_PUBLISH();
    rus_pub[l] = (uint8_t)k;
    snd_raw[l] &= (uint8_t)~(1u << ENG_RUS);   /* (measured) */
    return 1;
}
static int new_druid(uint32_t l, uint32_t seed, uint32_t path)
{
    uint32_t k, tries;
    int32_t was = dr_buf[l][dr_use[l]].kind;
    if (!sound_slot_free(&dr_pub[l], &dr_use[l]))
        return 0;
    k = dr_use[l] ^ 1u;
    for (tries = 0; tries < 4u; tries++) {
        dr_design(&dr_buf[l][k], new_kind, seed + tries * 7919u, path, GEN_NOW);
        if (new_kind >= 0 || dr_buf[l][k].kind != was)
            break;
    }
    dr_measure(&dr_buf[l][k]);
    RING_PUBLISH();
    dr_pub[l] = (uint8_t)k;
    snd_raw[l] &= (uint8_t)~(1u << ENG_DRUID);   /* (measured) */
    return 1;
}
static int new_bay(uint32_t l, uint32_t seed, uint32_t path)
{
    uint32_t k, tries;
    int32_t was = ba_buf[l][ba_use[l]].kind;
    if (!sound_slot_free(&ba_pub[l], &ba_use[l]))
        return 0;
    k = ba_use[l] ^ 1u;
    for (tries = 0; tries < 4u; tries++) {
        ba_design(&ba_buf[l][k], new_kind, seed + tries * 7919u, path, GEN_NOW);
        if (new_kind >= 0 || ba_buf[l][k].kind != was)
            break;
    }
    ba_measure(&ba_buf[l][k]);
    RING_PUBLISH();
    ba_pub[l] = (uint8_t)k;
    snd_raw[l] &= (uint8_t)~(1u << ENG_BAY);   /* (measured) */
    return 1;
}
static int new_hive(uint32_t l, uint32_t seed, uint32_t path)
{
    uint32_t k, tries;
    int32_t was = hv_buf[l][hv_use[l]].kind;
    if (!sound_slot_free(&hv_pub[l], &hv_use[l]))
        return 0;
    k = hv_use[l] ^ 1u;
    for (tries = 0; tries < 4u; tries++) {
        hv_design(&hv_buf[l][k], new_kind, seed + tries * 7919u, path, GEN_NOW);
        if (new_kind >= 0 || hv_buf[l][k].kind != was)
            break;
    }
    hv_measure(&hv_buf[l][k]);
    RING_PUBLISH();
    hv_pub[l] = (uint8_t)k;
    snd_raw[l] &= (uint8_t)~(1u << ENG_HIVE);   /* (measured) */
    return 1;
}
static int new_ob(uint32_t l, uint32_t seed, uint32_t path)
{
    uint32_t k, tries;
    int32_t was = ob_buf[l][ob_use[l]].kind;
    if (!sound_slot_free(&ob_pub[l], &ob_use[l]))
        return 0;
    k = ob_use[l] ^ 1u;
    for (tries = 0; tries < 4u; tries++) {
        ob_design(&ob_buf[l][k], new_kind, seed + tries * 7919u, path, GEN_NOW);
        if (new_kind >= 0 || ob_buf[l][k].kind != was)
            break;
    }
    ob_measure(&ob_buf[l][k]);
    RING_PUBLISH();
    ob_pub[l] = (uint8_t)k;
    snd_raw[l] &= (uint8_t)~(1u << ENG_OB);   /* (measured) */
    return 1;
}
/* the name of what layer l plays on engine e */
static const char *sound_name(uint32_t l, uint32_t e)
{
    if (e == ENG_CAL)
        return CS_NAME[cal_buf[l][cal_pub[l]].kind % CS_N];
    if (e == ENG_RUS)
        return RK_NAME[rus_buf[l][rus_pub[l]].kind % RK_N];
    if (e == ENG_DRUID)
        return DK_NAME[dr_buf[l][dr_pub[l]].kind % DK_N];
    if (e == ENG_BAY)
        return BK_NAME[ba_buf[l][ba_pub[l]].kind % BK_N];
    if (e == ENG_HIVE)
        return HK_NAME[hv_buf[l][hv_pub[l]].kind % HK_N];
    if (e == ENG_OB)
        return OBK_NAME[ob_buf[l][ob_pub[l]].kind % OBK_N];
    if (e == ENG_FM)
        return FF_NAME[fm_buf[l][fm_pub[l]].family % FF_N];
    return AK_NAME[an_buf[l][an_pub[l]].kind % AK_N];
}
