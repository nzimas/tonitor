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
enum { ENG_FM, ENG_ANIMA, ENG_CAL, NENG };
static const char *const ENG_NAME[NENG] = {"FM", "ANIMA", "CALIGULA"};

static fm_patch_t fm_buf[NLAYER][2];
static an_design_t an_buf[NLAYER][2];
static volatile uint8_t fm_pub[NLAYER], fm_use[NLAYER], an_pub[NLAYER], an_use[NLAYER];
/* CALIGULA (cal.c): one organism a layer, played by its notes as the other engines are */
static cal_design_t cal_buf[NLAYER][2];
static volatile uint8_t cal_pub[NLAYER], cal_use[NLAYER];
static cal_t cal_rt[NLAYER];

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
    fm_voice_t fm;
} voice_t;
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
#define LOOP_N 2048u             /* note events a layer's loops hold between them */
#define LOOP_TRK 10u             /* loops a layer */
enum { LR_IDLE, LR_ARMED, LR_REC };
/* an event: t (24 bits, blocks from the loop's start), the note (7 bits), on (bit 31) */
#define LE_T(e) ((e) & 0xFFFFFFu)
#define LE_NOTE(e) (((e) >> 24) & 0x7Fu)
#define LE_ON(e) ((e) >> 31)
#define LE(t, note, on) (((t) & 0xFFFFFFu) | ((uint32_t)((note) & 0x7Fu) << 24) | ((uint32_t)((on) != 0) << 31))
typedef struct {
    uint32_t len, pos;           /* blocks; pos: where it is */
    uint16_t first, n, ix;       /* its events in the looper's ev[]; the next to play */
    uint16_t rsv;
} loop_trk_t;
typedef struct {
    volatile uint8_t rec;        /* LR_*: the screen's and the LEDs' */
    volatile uint8_t play;       /* the loops go round */
    volatile uint8_t ntrk;       /* loops kept */
    volatile uint16_t mute;      /* the loops held out (PLAY held, a white key each): a bit each */
    uint8_t nheld;
    uint8_t held[16];            /* notes on in the recording, not yet off */
    uint32_t start;              /* the block the recording began on */
    uint32_t nev;                /* events used, every loop's */
    loop_trk_t trk[LOOP_TRK];
    uint32_t ev[LOOP_N];
} looper_t;
static looper_t loop[NLAYER] POOL;
static uint32_t blk_clock;       /* blocks since boot (the ISR) */

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
enum { EV_ON, EV_OFF, EV_ENGINE, EV_STOP, EV_LOOP_REC, EV_LOOP_PLAY, EV_LOOP_CLEAR, EV_LOOP_SET, EV_LOOPS, EV_LOOP_TRK };
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
            v->fade = FADE;                      /* out first (on the layer it sounds on), then this note */
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
    cal_release(&cal_rt[l], 2u);
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
    if (y->want_engine != y->engine)
        return 1;
    return y->engine == ENG_FM ? fm_pub[l] != fm_use[l] : y->engine == ENG_ANIMA ? an_pub[l] != an_use[l]
                                                                                  : cal_pub[l] != cal_use[l];
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
        cal_init(c, 0xC0u + l * 131u + lay_age);
        y->engine = y->want_engine;
        fm_use[l] = fm_pub[l];
        an_use[l] = an_pub[l];
        cal_use[l] = cal_pub[l];
        if (y->engine == ENG_CAL)
            for (j = 0; j < n; j++)
                cal_note_on(c, &cal_buf[l][cal_use[l]], nn[j], ns[j]);
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
    uint32_t t = blk_clock - p->start, k;
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
        p->trk[k].pos = p->trk[k].ix = 0;
}
static void loop_cmd(uint32_t l, uint32_t type)
{
    looper_t *p = &loop[l];
    switch (type) {
    case EV_LOOP_REC:
        if (p->rec == LR_REC) {                   /* the end: a loop as long as this */
            loop_trk_t *t = &p->trk[p->ntrk];
            uint32_t len = blk_clock - p->start, i;
            if (len < 2u)
                len = 2u;
            for (i = t->first; i < p->nev; i++)
                if (LE_T(p->ev[i]) > len - 2u)
                    p->ev[i] = LE(len - 2u, LE_NOTE(p->ev[i]), LE_ON(p->ev[i]));
            while (p->nheld)                      /* notes still down: they end with the loop's last block */
                p->ev[p->nev++] = LE(len - 1u, p->held[--p->nheld], 0);
            t->len = len;
            t->n = (uint16_t)(p->nev - t->first);
            t->pos = t->ix = 0;
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
        p->start = blk_clock;
        p->trk[p->ntrk].first = (uint16_t)p->nev;
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
            t->pos = t->ix = 0;
        p->mute &= (uint16_t)~bit;
    } else if (!on && !(p->mute & bit)) {
        if (p->play) {
            uint32_t held[4] = {0, 0, 0, 0}, i, n;
            for (i = 0; i < t->ix; i++) {
                uint32_t e = p->ev[t->first + i], nt = LE_NOTE(e);
                if (LE_ON(e))
                    held[nt >> 5] |= 1u << (nt & 31u);
                else
                    held[nt >> 5] &= ~(1u << (nt & 31u));
            }
            for (n = 0; n < 128u; n++)
                if ((held[n >> 5] >> (n & 31u)) & 1u)
                    note_off(l, n, SRC_LOOP);
        }
        p->mute |= (uint16_t)bit;
    }
}
/* one block of every loop that plays */
static void loops_run(void)
{
    uint32_t l, k;
    for (l = 0; l < NLAYER; l++) {
        looper_t *p = &loop[l];
        if (!p->play)
            continue;
        for (k = 0; k < p->ntrk; k++) {
            loop_trk_t *t = &p->trk[k];
            if ((p->mute >> k) & 1u)
                continue;                         /* (held out: it waits, silent, where it is) */
            while (t->ix < t->n && LE_T(p->ev[t->first + t->ix]) <= t->pos) {
                uint32_t e = p->ev[t->first + t->ix++];
                if (LE_ON(e))
                    note_on(l, LE_NOTE(e), SRC_LOOP);
                else
                    note_off(l, LE_NOTE(e), SRC_LOOP);
            }
            if (++t->pos >= t->len) {
                t->pos = 0;
                t->ix = 0;
            }
        }
    }
}

static void events_take(void)        /* audio ISR, at a block start */
{
    while (ev_r != ev_w) {
        ev_t e = evq[ev_r % EVQ];
        layer_t *y = &lay[e.layer];
        uint32_t k;
        RING_PUBLISH();
        ev_r++;
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
static volatile uint8_t load_pct, load_peak;   /* the render's share of a half buffer, now and at most (main.c) */
static int32_t master_q12 = 2048;                /* the MASTER knob (main loop), Q12 */

static int layer_sounding(uint32_t l)
{
    uint32_t k;
    if (lay[l].engine == ENG_CAL)
        return cal_rt[l].live;
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
    int8_t mac[NMAC];
    for (i = 0; i < NMAC; i++)
        mac[i] = lay_mac[l][i];
    for (i = 0; i < CTL; i++)
        buf[i] = 0;
    if (y->engine == ENG_CAL) {                   /* the organism: one for the layer */
        memcpy(cal_rt[l].mac, mac, NMAC);
        if (cal_render(&cal_rt[l], &cal_buf[l][cal_use[l]], buf))
            any = 1;
    }
    for (k = 0; k < NVOICE && y->engine != ENG_CAL; k++) {
        voice_t *v = &vox[k];
        int alive;
        if (v->layer != l || !v->sounding)
            continue;
        if (y->engine == ENG_FM)
            alive = fm_render(&v->fm, &fm_buf[l][fm_use[l]], vb, mac);
        else {
            memcpy(an_vox[k].mac, mac, NMAC);
            alive = an_render(&an_vox[k], &an_buf[l][an_use[l]], vb, CTL);
        }
        if (!alive) {
            v->sounding = 0;
            if (v->fade && v->pend)               /* (it ended in its fade: the new note now, where it goes) */
                voice_take(k);
            else
                v->fade = 0;
            continue;
        }
        any = 1;
        if (v->fade) {
            int32_t g0 = (int32_t)(v->fade * (uint32_t)Q15 / FADE), g1 = (int32_t)((v->fade - 1u) * (uint32_t)Q15 / FADE);
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
static void shed(void)
{
    uint32_t l;
    if (shed_voice(0))
        return;
    for (l = 0; l < NLAYER; l++)
        if (lay[l].engine == ENG_CAL && cal_rt[l].live && !cal_rt[l].nheld && !cal_rt[l].hush) {
            cal_rt[l].hush = 1;
            return;
        }
    shed_voice(1);
}

/* one block: the events, the loops, the layers, the reverb, the master (Q15 stereo, interleaved) */
static void mix_block(int32_t *out)
{
    int32_t l_[CTL], r_[CTL], rvin[CTL];
    uint32_t i, l;
    events_take();
    loops_run();
    if (shed_req) {                               /* overload: something already heard goes */
        shed_req = 0;
        shed();
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
    blk_clock++;
}

/* ------------------------------------------------ the sounds (main loop) --- */
static void layers_init(void)
{
    uint32_t l, k;
    memset(vox, 0, sizeof vox);
    for (k = 0; k < NVOICE; k++) {
        an_init(&an_vox[k], 0xA11u + k * 131u);
        vox[k].layer = (uint8_t)(k % NLAYER);
    }
    for (l = 0; l < NLAYER; l++) {
        lay[l].lim_g = Q15;
        lay[l].engine = lay[l].want_engine = (uint8_t)(l & 1u ? ENG_ANIMA : ENG_FM);
        cal_init(&cal_rt[l], 0xC0u + l * 131u);
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
    } else {
        if (!sound_slot_free(&cal_pub[l], &cal_use[l]))
            return 0;
        k = cal_use[l] ^ 1u;
        cal_design(&cal_buf[l][k], -1, seed, path, gen);
        cal_buf[l][k].out = level;
        RING_PUBLISH();
        cal_pub[l] = (uint8_t)k;
    }
    return 1;
}
/* the name of what layer l plays on engine e */
static const char *sound_name(uint32_t l, uint32_t e)
{
    if (e == ENG_CAL)
        return CS_NAME[cal_buf[l][cal_pub[l]].kind % CS_N];
    if (e == ENG_FM)
        return FF_NAME[fm_buf[l][fm_pub[l]].family % FF_N];
    return AK_NAME[an_buf[l][an_pub[l]].kind % AK_N];
}
