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
enum { ENG_FM, ENG_ANIMA, ENG_CAL, ENG_RUS, NENG };
static const char *const ENG_NAME[NENG] = {"FM", "ANIMA", "CALIGULA", "RUSSOLO"};

static fm_patch_t fm_buf[NLAYER][2];
static an_design_t an_buf[NLAYER][2];
static volatile uint8_t fm_pub[NLAYER], fm_use[NLAYER], an_pub[NLAYER], an_use[NLAYER];
/* CALIGULA (cal.c): one organism a layer, played by its notes as the other engines are */
static cal_design_t cal_buf[NLAYER][2];
static volatile uint8_t cal_pub[NLAYER], cal_use[NLAYER];
static cal_t cal_rt[NLAYER];
/* RUSSOLO (russolo.c): one organism a layer too */
static rus_design_t rus_buf[NLAYER][2];
static volatile uint8_t rus_pub[NLAYER], rus_use[NLAYER];
static rus_t rus_rt[NLAYER];
/* an organism engine (one for the layer, outside the voices' pool) */
static int eng_organism(uint32_t e) { return e == ENG_CAL || e == ENG_RUS; }

static an_voice_t an_vox[NVOICE] POOL;   /* the pool's ANIMA voices, ~4.8 KiB each: the resonator */

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
    uint8_t fade_n;              /* the fade's length, when not FADE (the budget's quick one) */
    fm_voice_t fm;
} voice_t;

/* the budget: what the voices and organisms cost, measured as they render, so that a start that would overrun
 * the block's time makes room first (the oldest release tails let go, quickly) instead of the audio arriving
 * late -- a burst. On the host a model stands in for the clock (tests set it) */
#ifdef HOST
static uint32_t host_cost_clock, host_cost_model[4];
#define COST_NOW() host_cost_clock
#define COST_ADD(e) (host_cost_clock += host_cost_model[(e) & 3u])
#define BLK_TICKS 1000u
#else
#define COST_NOW() fm1_ticks()
#define COST_ADD(e) ((void)0)
#define BLK_TICKS ((uint32_t)CTL * 1000000u / FS * FM1_TICKS_PER_US)   /* a block's time */
#endif
#define BUDGET (BLK_TICKS * 82u / 100u)  /* what a block may be expected to take (the rest: the panel, USB, the screen) */
#define QUICK 3u                         /* blocks of the budget's fade (~2 ms) */
static uint32_t eng_cost[4];             /* a voice (an organism) of each engine: its usual cost, ticks */
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
    int32_t g, pl, pr, sd;       /* the level, the pan and the send as the last block ended (they glide) */
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
    int8_t dir;                  /* 1 forwards, -1 backwards (the looper's direction; PENDULUM turns it at the ends) */
    uint32_t snd[4];             /* the notes it sounds now (as recorded, before the transposition) */
} loop_trk_t;
enum { LD_FWD, LD_REV, LD_PEND };   /* the direction a looper plays its loops in */
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
    uint8_t dmode, rate;         /* LD_*, LS_* (the 4th .. 7th black keys) */
    uint32_t nev;                /* events used, every loop's */
    loop_trk_t trk[LOOP_TRK];
    uint32_t ev[LOOP_N];
} looper_t;
static looper_t loop[NLAYER] POOL;
static uint32_t blk_clock;
#if TELEMETRY
static uint32_t tele_ev;                 /* this half: the events taken (bit = type), loop notes (bit 20), wraps (21) */
static uint32_t tele_non;                /* .. notes struck */
static volatile uint32_t tele_trims;     /* tails the budget let go */
#define TELE(b) (tele_ev |= 1u << (b))
#else
#define TELE(b) ((void)0)
#endif
/* a note tied over the loop's end: held from its first block to its last (held when the recording ended: ended
 * with the last block, REC's; a key let go is never later than the one before it). Going round, it is neither let
 * go at the end nor struck again at the start: a drone stays one note */
static int trk_tied(const looper_t *p, const loop_trk_t *t, uint32_t e)
{
    uint32_t i;
    if (LE_ON(e) ? LE_T(e) != 0u : LE_T(e) != t->len - 1u)
        return 0;
    if (LE_ON(e))
        for (i = t->n; i-- > 0u;) {               /* (its end: among the last events) */
            uint32_t f = p->ev[t->first + i];
            if (LE_T(f) != t->len - 1u)
                return 0;
            if (!LE_ON(f) && LE_NOTE(f) == LE_NOTE(e))
                return 1;
        }
    else
        for (i = 0; i < t->n; i++) {              /* (its start: among the first) */
            uint32_t f = p->ev[t->first + i];
            if (LE_T(f) != 0u)
                return 0;
            if (LE_ON(f) && LE_NOTE(f) == LE_NOTE(e))
                return 1;
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
static volatile int32_t lay_send[NLAYER];   /* the send to the reverb, after the level, Q15 (KNOB 4) */
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
enum { EV_ON, EV_OFF, EV_ENGINE, EV_STOP, EV_LOOP_REC, EV_LOOP_PLAY, EV_LOOP_CLEAR, EV_LOOP_SET, EV_LOOPS, EV_LOOP_TRK, EV_LOOP_XP, EV_LOOP_MODE };
typedef struct { uint8_t type, layer, note, val; } ev_t;
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
        blk_new += eng_cost[lay[l].engine & 3u];   /* (a new voice: what it will cost, ahead) */
    v->layer = (uint8_t)l;
    v->age = ++lay_age;
    v->born = blk_clock;
    if (lay[l].engine == ENG_FM)
        fm_note_on(&v->fm, &fm_buf[l][fm_use[l]], v->note, fresh || !v->sounding);
    else
        an_note_on(&an_vox[k], &an_buf[l][an_use[l]], pitch_inc((int32_t)v->note * 16), fresh || !v->sounding);
    v->sounding = 1;
}
static void voice_off(uint32_t l, uint32_t k)
{
    voice_t *v = &vox[k];
    if (lay[l].engine == ENG_FM)
        fm_note_off(&v->fm, &fm_buf[l][fm_use[l]]);
    else
        an_note_off(&an_vox[k]);
}
static void voice_silence(uint32_t k)
{
    voice_t *v = &vox[k];
    v->sounding = 0;
    v->fade = 0;
    v->fade_n = 0;
    v->fm.live = 0;
    an_vox[k].live = 0;
}
/* a voice for a note on layer l: a free one; else the oldest tail (any layer's); else the layer's own oldest held
 * note; else the oldest held of all. own: only free ones or the layer's own (it is fading for a change) */
static uint32_t voice_pick(uint32_t l, int own)
{
    uint32_t k, best = NVOICE, pass;
    for (k = 0; k < NVOICE; k++)
        if (!vox[k].sounding && !vox[k].gate && !vox[k].pend)
            return k;
    for (pass = 0; pass < 3u && best == NVOICE; pass++)
        for (k = 0; k < NVOICE; k++) {
            const voice_t *v = &vox[k];
            uint32_t o = vox_owner(v);
            if (v->pend || (own && o != l) || (pass == 0u ? v->gate : pass == 1u ? o != l : 0))
                continue;
            if (best == NVOICE || v->age < vox[best].age)
                best = k;
        }
    return best;
}

/* a note to CALIGULA, while the layer is (or is becoming) it; else to the voices */
static int cal_note(uint32_t l, uint32_t note, int on, uint32_t src)
{
    if (on && lay[l].want_engine == lay[l].engine && eng_organism(lay[l].engine)
        && !(lay[l].engine == ENG_CAL ? cal_rt[l].live : rus_rt[l].live))
        blk_new += eng_cost[lay[l].engine & 3u];  /* (the organism wakes: what it will cost, ahead) */
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

/* a note on: a silent voice if there is one, else the one ringing longest (released before held) */
static void note_on(uint32_t l, uint32_t note, uint32_t src)
{
    layer_t *y = &lay[l];
    uint32_t best;
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
    if (y->want_engine != y->engine)
        return 1;
    return y->engine == ENG_FM ? fm_pub[l] != fm_use[l] : y->engine == ENG_ANIMA ? an_pub[l] != an_use[l]
           : y->engine == ENG_CAL ? cal_pub[l] != cal_use[l] : rus_pub[l] != rus_use[l];
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
        p->dmode = LD_FWD;                        /* (emptied: forwards, at its speed, again) */
        p->rate = LS_NORMAL;
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
        if (p->play) {
            uint32_t n;
            for (n = 0; n < 128u; n++)
                if ((t->snd[n >> 5] >> (n & 31u)) & 1u)
                    note_off(l, lx_map(n, lx_steps + lx_tr[l] - t->xb, lx_mask), SRC_LOOP);
            memset(t->snd, 0, sizeof t->snd);
        }
        p->mute |= (uint16_t)bit;
    }
}
/* an event of loop t played: as recorded, or (backwards) turned round -- its start an end, its end a start. A note
 * tied over the loop's end is never let go there, nor struck again just after (forwards: its start is the loop's
 * first block, its end the last; backwards the other way round) */
static void trk_play(uint32_t l, loop_trk_t *t, const looper_t *p, uint32_t e, int as_on)
{
    uint32_t raw = LE_NOTE(e), nt = lx_map(raw, lx_steps + lx_tr[l] - t->xb, lx_mask);
    int tied = trk_tied(p, t, e);
    if (as_on) {
        if (t->wrap && tied)
            return;                               /* (still sounding from the round before) */
        note_on(l, nt, SRC_LOOP);
        t->snd[raw >> 5] |= 1u << (raw & 31u);
        TELE(20);
#if TELEMETRY
        tele_non++;
#endif
    } else if (!tied) {
        note_off(l, nt, SRC_LOOP);
        t->snd[raw >> 5] &= ~(1u << (raw & 31u));
    }
}
/* one block of loop time of loop t, in its direction; at an end, round again -- or, PENDULUM, back the other way */
static void trk_step(uint32_t l, looper_t *p, loop_trk_t *t)
{
    const uint32_t *ev = p->ev + t->first;
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
    if (what) {
        p->rate = (uint8_t)(val <= LS_DOUBLE ? val : LS_NORMAL);
        return;
    }
    p->dmode = (uint8_t)(val <= LD_PEND ? val : LD_FWD);
    if (p->dmode != LD_PEND)
        for (k = 0; k < p->ntrk; k++)
            trk_turn(&p->trk[k], p->dmode == LD_REV ? -1 : 1);
}
/* one block of every loop that plays: as many blocks of loop time as the speed has passed (at half speed every
 * other block, at double two) */
static void loops_run(void)
{
    uint32_t l, k;
    for (l = 0; l < NLAYER; l++) {
        looper_t *p = &loop[l];
        uint32_t was = p->lclk, steps;
        p->lclk += p->rate == LS_HALF ? 1u : p->rate == LS_DOUBLE ? 4u : 2u;
        steps = (p->lclk >> 1) - (was >> 1);
        if (!p->play)
            continue;
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
            loop_hear(e.layer, 1, e.note);
            note_on(e.layer, e.note, SRC_KEY);
            break;
        case EV_OFF:
            loop_hear(e.layer, 0, e.note);
            note_off(e.layer, e.note, SRC_KEY);
            break;
        case EV_ENGINE:
            y->want_engine = (uint8_t)(e.val < NENG ? e.val : 0);
            break;
        case EV_STOP:
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
        case EV_LOOP_MODE:                        /* (note: 0 the direction, 1 the speed; val: which) */
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

static void layer_block(uint32_t l, int32_t *mixl, int32_t *mixr, int32_t *rvin)
{
    layer_t *y = &lay[l];
    int32_t buf[CTL], vb[CTL], pk = 0;
    uint32_t i, k, any = 0;
    if (!y->xfade && change_due(l)) {
        if (layer_sounding(l))
            y->xfade = FADE;                      /* out first, then the change */
        else
            change_apply(l);
    }
    int8_t mac[NMAC], felt[NMAC];
    for (i = 0; i < NMAC; i++) {
        int32_t m = lay_mac[l][i], a = m < 0 ? -m : m, q = (int32_t)isqrt32((uint32_t)(a * MAC_MAX));
        mac[i] = (int8_t)m;
        felt[i] = (int8_t)(m < 0 ? -q : q);       /* (the organisms' macros felt early: the square root of the turn;
                                                   * FM and ANIMA take theirs as turned, and feel them so themselves) */
    }
    for (i = 0; i < CTL; i++)
        buf[i] = 0;
    org_cost[l] = 0;
    if (y->engine == ENG_CAL || y->engine == ENG_RUS) {   /* the organism: one for the layer */
        uint32_t c0 = COST_NOW(), live;
        if (y->engine == ENG_CAL) {
            memcpy(cal_rt[l].mac, felt, NMAC);
            live = (uint32_t)cal_render(&cal_rt[l], &cal_buf[l][cal_use[l]], buf);
        } else {
            memcpy(rus_rt[l].mac, felt, NMAC);
            live = (uint32_t)rus_render(&rus_rt[l], &rus_buf[l][rus_use[l]], buf);
        }
        if (live) {
            any = 1;
            COST_ADD(y->engine);
            org_cost[l] = COST_NOW() - c0;
            eng_cost[y->engine] = (eng_cost[y->engine] * 7u + org_cost[l] + 7u) / 8u;
        }
    }
    for (k = 0; k < NVOICE && !eng_organism(y->engine); k++) {
        voice_t *v = &vox[k];
        int alive;
        if (v->layer != l || !v->sounding)
            continue;
        {
            uint32_t c0 = COST_NOW(), c;
            if (y->engine == ENG_FM)
                alive = fm_render(&v->fm, &fm_buf[l][fm_use[l]], vb, mac);
            else {
                memcpy(an_vox[k].mac, mac, NMAC);
                alive = an_render(&an_vox[k], &an_buf[l][an_use[l]], vb, CTL);
            }
            COST_ADD(y->engine);
            c = COST_NOW() - c0;
            v->cost = (uint16_t)(c > 0xFFFFu ? 0xFFFFu : c);
            if (alive)
                eng_cost[y->engine & 3u] = (eng_cost[y->engine & 3u] * 7u + c + 7u) / 8u;
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
    if (!any && !(gran[l].tail && (lay_gr[l] || gran[l].amt))) {   /* (grains ring on a while after the voices) */
        y->meter -= y->meter >> 4;
        y->g = lay_gain[l];
        y->pl = lay_pl[l];
        y->pr = lay_pr[l];
        y->sd = lay_send[l];
        return;
    }
    {   /* the DJ filter */
        int32_t v = lay_flt[l], a = v < 0 ? -v : v;
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
    gran_block(l, buf, CTL, lay_gr[l], (int)any);       /* the granulator */
    crush_block(y, buf, CTL, lay_cr[l]);                 /* the crush */
    for (i = 0; i < CTL; i++)
        buf[i] = mulq12(buf[i], LAYER_GAIN);
    lim_block(&y->lim_env, &y->lim_g, buf, CTL);
    {   /* the layer's level, then its pan, gliding from the last block's to the knobs' */
        int32_t g1 = lay_gain[l], pl1 = lay_pl[l], pr1 = lay_pr[l], sd1 = lay_send[l];
        int32_t dg = (g1 - y->g) / CTL, dl = (pl1 - y->pl) / CTL, dr = (pr1 - y->pr) / CTL, ds = (sd1 - y->sd) / CTL;
        int32_t g = y->g, pl = y->pl, pr = y->pr, sd = y->sd;
        for (i = 0; i < CTL; i++) {
            int32_t x, a;
            g += dg;
            pl += dl;
            pr += dr;
            sd += ds;
            x = (int32_t)(((int64_t)buf[i] * g) >> 14);
            a = absi(x);
            if (a > pk)
                pk = a;
            mixl[i] += mulq15(x, pl);
            mixr[i] += mulq15(x, pr);
            rvin[i] += mulq15(x, sd);
        }
        y->g = g1;
        y->pl = pl1;
        y->pr = pr1;
        y->sd = sd1;
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
        if (!v->sounding || v->fade || (held ? !v->gate || blk_clock - v->born < SHED_HELD_BLOCKS : v->gate))
            continue;
        if (ok == NVOICE || v->age < vox[ok].age)
            ok = k;
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
    uint32_t pred = blk_last + blk_new, l, k;
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
            if (v->sounding && !v->gate && !v->fade && !v->pend && (ok == NVOICE || v->age < vox[ok].age))
                ok = k;
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

/* one block: the events, the loops, the layers, the reverb, the master (Q15 stereo, interleaved) */
static void mix_block(int32_t *out)
{
    int32_t l_[CTL], r_[CTL], rvin[CTL];
    uint32_t i, l;
    uint32_t c0 = COST_NOW();
    an_cache_now = blk_clock + 1u;                /* (ANIMA's wavetables from RAM while the ISR renders) */
    an_cache_filled = 0;
    events_take();
    loops_run();
    budget_guard();
    if (shed_req) {                               /* overload: something already heard goes */
        shed(shed_req > 1u);
        shed_req = 0;
    }
    for (i = 0; i < CTL; i++)
        l_[i] = r_[i] = rvin[i] = 0;
    for (l = 0; l < NLAYER; l++)
        layer_block(l, l_, r_, rvin);
    rv_block(rvin, l_, r_, CTL);                  /* the master reverb (its tail goes on with no layer sounding) */
    for (i = 0; i < CTL; i++) {
        out[2u * i] = ceiling(mulq12(l_[i], master_q12));
        out[2u * i + 1u] = ceiling(mulq12(r_[i], master_q12));
    }
    blk_last = COST_NOW() - c0;
    an_cache_now = 0;
    blk_clock++;
}

/* ------------------------------------------------ the sounds (main loop) --- */
static void layers_init(void)
{
    uint32_t l, k;
    memset(vox, 0, sizeof vox);
#ifndef HOST
    eng_cost[ENG_FM] = BLK_TICKS * 5u / 100u;     /* (first guesses, until they are measured) */
    eng_cost[ENG_ANIMA] = BLK_TICKS * 9u / 100u;
    eng_cost[ENG_CAL] = BLK_TICKS * 20u / 100u;
    eng_cost[ENG_RUS] = BLK_TICKS * 4u / 100u;
#else
    memcpy(eng_cost, host_cost_model, sizeof eng_cost);
#endif
    blk_last = blk_new = 0;
    for (k = 0; k < NVOICE; k++) {
        an_init(&an_vox[k], 0xA11u + k * 131u);
        vox[k].layer = (uint8_t)(k % NLAYER);
    }
    for (l = 0; l < NLAYER; l++) {
        lay[l].lim_g = Q15;
        lay[l].engine = lay[l].want_engine = (uint8_t)(l & 1u ? ENG_ANIMA : ENG_FM);
        cal_init(&cal_rt[l], 0xC0u + l * 131u);
        rus_init(&rus_rt[l], 0x7A5u + l * 131u);
    }
    rv_init();
    gran_init();
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
        fm_generate(&cand, -1, seed + tries * 7919u, path, GEN_NOW);
        m = fm_misfit(fm_measure(&cand), path, cand.family == was);
        if (m < best) {
            best = m;
            fm_buf[l][k] = cand;
        }
        if (!m)
            break;
    }
    RING_PUBLISH();
    fm_pub[l] = (uint8_t)k;
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
        an_design(&an_buf[l][k], -1, seed + tries * 7919u, path, GEN_NOW);
        if (an_buf[l][k].kind != was)
            break;
    }
    an_measure(&an_buf[l][k]);
    RING_PUBLISH();
    an_pub[l] = (uint8_t)k;
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
        cal_design(&cal_buf[l][k], -1, seed + tries * 7919u, path, GEN_NOW);
        if (cal_buf[l][k].kind != was)
            break;
    }
    cal_measure(&cal_buf[l][k]);
    RING_PUBLISH();
    cal_pub[l] = (uint8_t)k;
    return 1;
}
/* layer l's sound on engine e drawn again from what made it (a preset): its seed, path and generation, and the
 * level it was measured at; published as a new sound is. 0: the ISR still holds the other buffer */
static int sound_from_seed(uint32_t l, uint32_t e, uint32_t seed, uint32_t path, uint32_t gen, int32_t level)
{
    uint32_t k;
    if (e == ENG_FM) {
        if (!sound_slot_free(&fm_pub[l], &fm_use[l]))
            return 0;
        k = fm_use[l] ^ 1u;
        fm_generate(&fm_buf[l][k], -1, seed, path, gen);
        fm_buf[l][k].gain = level;
        RING_PUBLISH();
        fm_pub[l] = (uint8_t)k;
    } else if (e == ENG_ANIMA) {
        if (!sound_slot_free(&an_pub[l], &an_use[l]))
            return 0;
        k = an_use[l] ^ 1u;
        an_design(&an_buf[l][k], -1, seed, path, gen);
        an_buf[l][k].out = level;
        RING_PUBLISH();
        an_pub[l] = (uint8_t)k;
    } else if (e == ENG_CAL) {
        if (!sound_slot_free(&cal_pub[l], &cal_use[l]))
            return 0;
        k = cal_use[l] ^ 1u;
        cal_design(&cal_buf[l][k], -1, seed, path, gen);
        cal_buf[l][k].out = level;
        RING_PUBLISH();
        cal_pub[l] = (uint8_t)k;
    } else {
        if (!sound_slot_free(&rus_pub[l], &rus_use[l]))
            return 0;
        k = rus_use[l] ^ 1u;
        rus_design(&rus_buf[l][k], -1, seed, path, gen);
        rus_buf[l][k].out = level;
        RING_PUBLISH();
        rus_pub[l] = (uint8_t)k;
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
        rus_design(&rus_buf[l][k], -1, seed + tries * 7919u, path, GEN_NOW);
        if (rus_buf[l][k].kind != was)
            break;
    }
    rus_measure(&rus_buf[l][k]);
    RING_PUBLISH();
    rus_pub[l] = (uint8_t)k;
    return 1;
}
/* the name of what layer l plays on engine e */
static const char *sound_name(uint32_t l, uint32_t e)
{
    if (e == ENG_CAL)
        return CS_NAME[cal_buf[l][cal_pub[l]].kind % CS_N];
    if (e == ENG_RUS)
        return RK_NAME[rus_buf[l][rus_pub[l]].kind % RK_N];
    if (e == ENG_FM)
        return FF_NAME[fm_buf[l][fm_pub[l]].family % FF_N];
    return AK_NAME[an_buf[l][an_pub[l]].kind % AK_N];
}
