/* SPDX-License-Identifier: GPL-3.0-only
 * TONITOR: REACT, the other tracks improvising with the one played -- with SHADOW and ARPEGGIO, the PLAY ALONG page
 * (SEQ's fourth: PRESETS the mode, PLAY on or off). The chosen track is the guide (its button lit); on the page the
 * other tracks' buttons take them in or out -- in a glow, out dark; the guide's says GUIDE TRACK (it cannot follow
 * itself). Choosing another track (off the page) makes it the guide. After the SMK-37 port's react.c (its build 32).
 *
 * After the systems that do this well -- Pachet's Continuator, IRCAM's OMax and Somax, Lewis's Voyager:
 *
 * MEMORY. Everything the guide plays is kept, in order, as its own vocabulary: each event (a note, or a chord struck
 * together) with its level, its length, and the gap before it -- its steps of the scale, so it can be moved along
 * the scale and keep its shape (the last RX_H events). A gap longer than a breath begins a phrase.
 *
 * MUSICIANS. Each track taken in improvises from that memory, as a factor oracle is navigated: from the event it
 * played last, it goes on as the guide once went on (continuity), or jumps to another place in the memory where the
 * guide played the same intervals (the longest same context first, the same rhythm too; then shorter, looser), and
 * goes on from there. So its lines are the guide's own, recombined: new phrases in the guide's style, never a pattern
 * of its own. Its rhythm is the guide's -- the gaps and lengths of the events it plays, as played -- slowed or
 * quickened by its own factor. Each choice is weighed by how it fits the harmony now: what the guide holds, and the
 * pitch classes it played lately (fading over a second or two); its notes moved along the scale (none, a third up
 * or down) to fit. When the guide's harmony moves, a musician's long note that no longer fits lets go.
 *
 * THE ENSEMBLE. At a phrase of the guide's, after some seconds, the musicians regroup: each takes a way to play,
 * suited to its sound -- a texture (CALIGULA, RUSSOLO, the drones and washes: only slow, low, single notes), a
 * sustained sound (the guide's chords too), a plucked one -- and a register of its own, an octave or two from the
 * guide's:
 *   FOLLOWS    with the guide, as fast or half as fast, its harmony followed closely; quick to react to a new chord
 *   CONTRASTS  against it: slow and long under a busy guide, moving under one holding long chords; the other side of
 *              the keyboard, softer
 *   ECHOES     the guide's line a few events behind, as the guide plays them (so in its rhythm), a third or a sixth
 *              or an octave away; at a rest, the rest of it
 *   ANSWERS    in the guide's rest: its phrase continued as the guide might have (the continuation of its last
 *              notes, found in the memory), until the guide plays again
 *   RESTS      sits out this time
 * The guide's intensity sets how much each plays; its silence silences them (ANSWERS apart, in the rest).
 *
 * Its page: SELECT and ALGORITHM stay the track's level and pan; KNOB 1 .. 4
 *   DENSITY     how much they play: how often a musician sounds a step, how long it waits (from twice to half
 *               the guide's gaps), how many sit out
 *   COMPLEXITY  the structure: low, single notes in plain rhythms (the guide's gaps rounded to halves and doubles of
 *               its usual one); high, harmonised notes (a third or a sixth under), notes moved further along the
 *               scale, steps split in two or three, notes pushed off the beat, paces in threes against twos
 *   CONTINUITY  low, they jump about the guide's memory, recombining it; high, they keep to its phrases as played
 *   SPREAD      how far from the guide's register: low, around its hands; high, from deep bass to high air
 * Its notes go as a sequencer's (the looper does not record them), never on the guide. It runs in the main loop. */

#define RX_H 128                 /* the memory: the guide's last events */
#define RX_Q 32                  /* notes to come and sounding */
#define RX_HD 10                 /* the guide's notes down */
#define RX_NONE 0xFFFFFFFFu
enum { RC_PLUCK, RC_SUST, RC_TEX };
enum { RM_REST, RM_FOLLOW, RM_CONTRA, RM_ECHO, RM_ANSWER, RM_N };
static const char *const RM_NAME[RM_N] = {"RESTS", "FOLLOWS", "CONTRASTS", "ECHOES", "ANSWERS"};

/* the families by name: textures, plucked, low */
static const char *const RX_TEXN[] = {"DRONE", "HAZE", "FOG", "WASH", "TIDE", "TIDES", "WIND", "STATIC", "SPECTRE",
                                      "SPECTRA", "BREATH", "DUST", "MURMUR", "SWARM", "DRIFT", "HOWL", "AIR", "CLUSTER",
                                      "SWELL", "ABYSS", "FRICTION", "BRUIT", "WARP", "GRIT"};
static const char *const RX_PLKN[] = {"PLUCK", "MALLET", "IMPACT", "BONGO", "STRUCK", "HARP", "SITAR", "CHIME", "BELL",
                                      "BELLS", "STAB", "GLASS", "SHARDS", "RATTLE", "PLATE", "RUBBER", "GLITCH", "BITS",
                                      "ITERATIF", "METAL", "MACHINE", "CLANG"};
static const char *const RX_BASSN[] = {"SUB", "GROUND", "ABYSS", "UNDERTOW"};

typedef struct {
    uint8_t n[3], nn;            /* its notes, low to high (a chord: up to three) */
    uint8_t vel, start, dg;      /* its level; a phrase's first; its top note's step of the scale (from MIDI 0) */
    uint16_t ioi, dur;           /* the gap before it, how long it was held (0: not yet let go), ms */
    uint32_t t;                  /* when it was played (fm1_ms) */
} rx_item_t;
typedef struct {
    uint8_t chr, bassy, mode;
    uint8_t tf;                  /* its pace: x tf / 4 the guide's times (2 half .. 16 four times as long) */
    uint8_t cont, pplay, pct;    /* how often it goes on as the guide went on; plays a step; its level, % */
    uint8_t lag;                 /* ECHOES: events behind */
    int8_t oct, tx, sh;          /* its register (octaves from the guide's); its steps moved; this line's shift */
    uint32_t pos;                /* the event it played last (from the first ever kept), RX_NONE: none */
    uint32_t next_at, ans_end, echoed;
    uint32_t taboo[3];
} rx_ag_t;

/* a SHADOW: where it is in the guide's time, which way it goes, how fast, how far behind, moved how far */
enum { SD_FWD, SD_REV, SD_PING };
typedef struct {
    uint8_t dir, paused;
    int8_t sgn, tx, oct;
    uint32_t rate;               /* its pace against the guide's, Q16 */
    uint32_t lag;                /* how far behind the guide it means to be, ms */
    uint32_t vt;                 /* the moment of the guide's playing it is at (fm1_ms) */
    uint32_t ci;                 /* the event it meets next, its way (RX_NONE: none) */
    uint32_t until, last, lo, hi;   /* (its way kept till; its last pass; PING's two ends) */
} rx_sv_t;
enum { RXM_REACT, RXM_SHADOW, RXM_ARP, RXM_N };
static const char *const RXM_NAME[RXM_N] = {"REACT", "SHADOW", "ARPEGGIO"};   /* (ARPEGGIO: the FM-1's ARP is the SCALE page) */
static const char *const RXM_OFF[RXM_N] = {"REACT OFF", "SHADOW OFF", "ARPEGGIO OFF"};
/* an ARP track: its pattern over the chord's notes (spread over its octaves), its pace, its way */
#define AR_SEQ 32
#define AR_REST 0xFFu
#define AR_ANY 0xFEu
typedef struct {
    uint8_t dir, n, len, pos, mut, rank;
    uint8_t e[24];               /* the notes it plays: the chord, from its register up its octaves */
    uint8_t seq[AR_SEQ];         /* its pattern: which of them, in turn (AR_REST: a rest; AR_ANY: any, drawn) */
    uint16_t mult;               /* its pace against RATE's, Q8 */
    int8_t oct;
    uint16_t ver;                /* (the chord it plays: rx.a_ver when it took it) */
    uint8_t fresh;               /* (started from silence: its first step takes the chord as it has come by then) */
    uint32_t next_at;
} rx_av_t;
#define AR_SETTLE 90u            /* a chord taken once it has stood this long (ms), at a track's next step */

static struct {
    uint8_t on, mask, guide;
    uint8_t mode;                /* RXM_REACT, RXM_SHADOW, RXM_ARP (on its page: < and >) */
    int8_t sk[4];                /* SHADOW's page: DELAY DRIFT VARIATION TRANSPOSE, 0 .. 100 */
    int8_t ak[6];                /* ARP's page: RATE RELATION MOTION MUTATE RANGE CHANCE, 0 .. 100 */
    rx_av_t av[NLAYER];
    uint8_t an[8], ann;          /* ARP: the chord, in the order its notes came */
    uint32_t a_on;               /* (its last note's coming) */
    uint16_t a_ver;              /* (the chord's changes, counted) .. */
    uint32_t a_chg;              /* .. the last one's time */
    uint8_t a_idle;              /* (the arpeggios quiet: the next chord starts them, on its own time) */
    rx_sv_t sv[NLAYER];
    struct { uint8_t note; uint32_t t, it; } hd[RX_HD];
    uint8_t nhd;
    uint16_t ch[12];             /* each pitch class lately (fading) */
    rx_item_t h[RX_H];
    uint32_t hn;                 /* events kept so far (the last RX_H in h) */
    uint32_t last_on, last_ev, typ, form_t, decay_t, ph0;
    uint8_t vel, mid, ans_done, turn;
    rx_ag_t a[NLAYER];
    uint32_t red_until, rt0;
    uint32_t flash;              /* the guide's last note struck, + 1 (0: none): PLAY flashes (ui_leds) */
    uint8_t rdown, rdone;
    int8_t k[4];                 /* the page's DENSITY COMPLEXITY CONTINUITY SPREAD, 0 .. 100 */
    struct { uint8_t layer, note, vel, on; uint32_t at, len; } q[RX_Q];   /* on: 1 to strike, 2 sounding */
    rnd_t r;
} rx;

static void env_native(uint32_t l, int8_t *v);
static int32_t cur_kind(uint32_t l);
static const char *kind_name(uint32_t e, int32_t k);

static uint32_t rx_rnd(void)
{
    if (!rx.r.s)
        rnd_seed(&rx.r, fm1_ms ^ 0x5EAC7u);
    return rnd_next(&rx.r);
}
static int rx_odds(int32_t pct) { return (int32_t)((rx_rnd() >> 8) % 100u) < pct; }
static uint32_t rx_in(uint32_t lo, uint32_t hi) { return lo + (rx_rnd() >> 8) % (hi - lo + 1u); }
static uint32_t rx_rest_ms(void) { return (uint32_t)clampi((int32_t)rx.typ * 2, 350, 2500); }   /* a breath */
static uint32_t rx_ans_ms(void) { return (uint32_t)clampi((int32_t)rx.typ * 3 / 2, 300, 2000); }
#define RX_DENS rx.k[0]
#define RX_CPLX rx.k[1]
#define RX_CONT rx.k[2]
#define RX_SPRD rx.k[3]
static const char *const RX_KNAME[4] = {"DENSITY", "COMPLEXITY", "CONTINUITY", "SPREAD"};
static int rx_in_ens(uint32_t l) { return l < NLAYER && l != rx.guide && ((rx.mask >> l) & 1u); }
static int rx_role(uint32_t l) { return rx_in_ens(l) ? rx.a[l].mode : -1; }   /* (the host tests) */

/* ---- the queue: a note of track l, struck in wait ms, held len ms */
static uint32_t rx_snap(uint32_t note);
static void rx_play(uint32_t l, uint32_t note, uint32_t vel, uint32_t len, uint32_t wait)
{
    uint32_t i, slot = RX_Q;
    if (note > 127u || !rx_in_ens(l))
        return;
    note = rx_snap(note);                           /* (in the scale, whatever came: the last word) */
    for (i = 0; i < RX_Q; i++)
        if (!rx.q[i].on) {
            slot = i;
            break;
        }
    if (slot == RX_Q) {                             /* (all taken: the soonest to end let go, its place taken) */
        slot = 0;
        for (i = 1; i < RX_Q; i++)
            if (rx.q[i].on == 2u && (rx.q[slot].on != 2u || (int32_t)(rx.q[i].at - rx.q[slot].at) < 0))
                slot = i;
        if (rx.q[slot].on == 2u)
            seq_press(rx.q[slot].layer, rx.q[slot].note, 0, 0);
    }
    rx.q[slot].layer = (uint8_t)l;
    rx.q[slot].note = (uint8_t)note;
    rx.q[slot].vel = (uint8_t)clampi((int32_t)vel, 8, 124);
    rx.q[slot].len = len < 40u ? 40u : len;
    rx.q[slot].at = fm1_ms + wait;
    rx.q[slot].on = 1;
}
static void rx_cut(uint32_t l)                      /* track l's notes (all if l is NLAYER) let go, none to come */
{
    uint32_t i;
    for (i = 0; i < RX_Q; i++)
        if (rx.q[i].on && (l >= NLAYER || rx.q[i].layer == l)) {
            if (rx.q[i].on == 2u)
                seq_press(rx.q[i].layer, rx.q[i].note, 0, 0);
            rx.q[i].on = 0;
        }
}

/* ---- the scale: steps of it counted from MIDI note 0, and back */
static uint32_t rx_n(void) { return SCALE_N[ui.scale]; }
static uint32_t rx_pcs(uint8_t *pc)                 /* the scale's pitch classes, low to high from C */
{
    uint32_t m = scale_mask(), k, n = 0;
    for (k = 0; k < 12u; k++)
        if ((m >> k) & 1u)
            pc[n++] = (uint8_t)k;
    return n;
}
static uint32_t rx_dg(uint32_t note)                /* note's step (a note off the scale: the step below it) */
{
    uint8_t pc[12];
    uint32_t n = rx_pcs(pc), k = 0;
    while (k + 1u < n && pc[k + 1u] <= note % 12u)
        k++;
    return (note / 12u) * n + k;
}
static uint32_t rx_note(int32_t dg)
{
    uint8_t pc[12];
    int32_t n = (int32_t)rx_pcs(pc);
    dg = clampi(dg, 0, 10 * n);
    return (uint32_t)clampi(dg / n * 12 + pc[dg % n], 0, 127);
}
static uint32_t rx_snap(uint32_t note)              /* the nearest note of the scale (below first), in range */
{
    uint32_t m = scale_mask(), d;
    for (d = 0; d < 6u; d++) {
        if (note >= d && ((m >> ((note - d) % 12u)) & 1u))
            return note - d;
        if (note + d <= 127u && ((m >> ((note + d) % 12u)) & 1u))
            return note + d;
    }
    return note;
}
/* how well pitch class pc fits the harmony now, 0 .. 256: what the guide holds counts most, then what it played lately */
static uint32_t rx_fit(uint32_t pc)
{
    uint32_t k, w[12], mx = 1;
    for (k = 0; k < 12u; k++)
        w[k] = rx.ch[k];
    for (k = 0; k < rx.nhd; k++)
        w[rx.hd[k].note % 12u] += 3000u;
    for (k = 0; k < 12u; k++)
        if (w[k] > mx)
            mx = w[k];
    return w[pc % 12u] * 256u / mx;
}

/* a note that clashes with the harmony now (in the scale, but not in it): the nearest one along the scale that fits */
static uint32_t rx_settle(uint32_t note)
{
    uint32_t best = note, bf, d, f;
    if ((bf = rx_fit(note % 12u)) >= 60u)
        return note;
    for (d = 1; d <= 2u; d++) {
        int32_t s;
        for (s = -1; s <= 1; s += 2) {
            uint32_t m = rx_note((int32_t)rx_dg(note) + s * (int32_t)d);
            if ((f = rx_fit(m % 12u)) > bf + 20u) {
                bf = f;
                best = m;
            }
        }
        if (best != note)
            break;
    }
    return best;
}

/* ---- the memory */
static rx_item_t *rx_it(uint32_t i) { return &rx.h[i % RX_H]; }
static int rx_kept(uint32_t i) { return i != RX_NONE && i < rx.hn && rx.hn - i <= RX_H; }
static int32_t rx_ival(uint32_t i)                  /* the step from the event before (99: a phrase's first) */
{
    if (!rx_kept(i) || !rx_kept(i - 1u) || rx_it(i)->start)
        return 99;
    return clampi((int32_t)rx_it(i)->dg - (int32_t)rx_it(i - 1u)->dg, -12, 12);
}
static uint32_t rx_dc(uint32_t i)                   /* its gap: short, usual, long */
{
    uint32_t g = rx_it(i)->ioi;
    return g * 10u < rx.typ * 7u ? 0u : g * 2u < rx.typ * 3u ? 1u : 2u;
}
/* do the events before and at i and j match, k back: lvl 0 the steps and the gaps, 1 the steps, 2 the direction */
static int rx_same(uint32_t i, uint32_t j, uint32_t k, uint32_t lvl)
{
    uint32_t m;
    for (m = 0; m < k; m++) {
        int32_t a, b;
        if (!rx_kept(i - m) || !rx_kept(j - m))
            return 0;
        a = rx_ival(i - m);
        b = rx_ival(j - m);
        if (lvl == 2u ? (a > 0) - (a < 0) != (b > 0) - (b < 0) || (a == 99) != (b == 99) : a != b)
            return 0;
        if (lvl == 0u && rx_dc(i - m) != rx_dc(j - m))
            return 0;
    }
    return 1;
}
/* event e's notes for musician l, moved tx steps and sh semitones: how many (into n) */
static uint32_t rx_render(uint32_t l, uint32_t e, int32_t tx, int32_t sh, uint32_t *n)
{
    const rx_item_t *it = rx_it(e);
    const rx_ag_t *a = &rx.a[l];
    uint32_t k, c = 0, lo = 0, hi = it->nn;
    if (a->chr == RC_TEX)                           /* a texture: the lowest note only; plucked: the top (now and then two) */
        hi = 1;
    else if (RX_CPLX < 35)                          /* (plain: one note, the top) */
        lo = it->nn - 1u;
    else if (a->chr == RC_PLUCK)
        lo = it->nn > 1u && rx_odds(75) ? it->nn - 1u : it->nn > 2u ? it->nn - 2u : 0u;
    for (k = lo; k < hi; k++)
        n[c++] = (uint32_t)clampi((int32_t)rx_note((int32_t)rx_dg(it->n[k]) + tx) + sh, 24, 108);
    if (a->chr != RC_TEX && c == 1u && RX_CPLX > 55 && rx_odds(RX_CPLX - (a->chr == RC_SUST ? 35 : 55)))
        n[c++] = (uint32_t)clampi((int32_t)rx_note((int32_t)rx_dg(it->n[hi - 1u]) + tx - (rx_odds(70) ? 2 : 5)) + sh, 24, 108);
    return c;                                       /* (rich: harmonised, a third or a sixth under) */
}
static uint32_t rx_fits(uint32_t l, uint32_t e, int32_t tx)   /* how well event e fits, moved tx steps */
{
    uint32_t n[3], c = rx_render(l, e, tx, 0, n), k, f = 0;
    for (k = 0; k < c; k++)
        f += rx_fit(n[k] % 12u);
    return c ? f / c : 0u;
}
/* the shift that puts event e (moved tx) in musician l's register */
static int32_t rx_reg(uint32_t l, uint32_t e, int32_t tx)
{
    int32_t c = clampi((int32_t)rx.mid + 12 * rx.a[l].oct, 30, 100), t = (int32_t)rx_note((int32_t)rx_dg(rx_it(e)->n[rx_it(e)->nn - 1u]) + tx), s = 0;
    if (rx.a[l].chr == RC_TEX || rx.a[l].bassy)
        c = clampi(c, RX_SPRD > 60 ? 26 : 30, 60);
    c = clampi(c, RX_SPRD > 60 ? 26 : 30, RX_SPRD > 60 ? 104 : 96);
    while (t + s > c + 6)
        s -= 12;
    while (t + s < c - 6)
        s += 12;
    return s;
}

/* ---- the musicians: what each is (by its sound) */
static int rx_listed(const char *s, const char *const *t, uint32_t n)
{
    while (n--)
        if (str_eq(s, t[n]))
            return 1;
    return 0;
}
static void rx_class(uint32_t l)
{
    rx_ag_t *a = &rx.a[l];
    uint32_t e = ui.engine[l], k;
    const char *nm = kind_name(e, cur_kind(l));
    int own = ui.env_on[l][e] != 0;
    int8_t v[4];
    int32_t att, dec;
    if (own)
        for (k = 0; k < 4u; k++)
            v[k] = ui.env[l][e][k];
    else
        env_native(l, v);
    att = env_ms(0, v[0]);
    dec = env_ms(1, v[1]);
    a->bassy = (uint8_t)rx_listed(nm, RX_BASSN, sizeof RX_BASSN / sizeof RX_BASSN[0]);
    if (eng_organism(e) || (!own && rx_listed(nm, RX_TEXN, sizeof RX_TEXN / sizeof RX_TEXN[0])))
        a->chr = RC_TEX;                            /* (CALIGULA, RUSSOLO: organisms, never note by note) */
    else if (!own && rx_listed(nm, RX_PLKN, sizeof RX_PLKN / sizeof RX_PLKN[0]))
        a->chr = RC_PLUCK;
    else
        a->chr = v[2] < 40 && att < 120 && dec < 1500 ? RC_PLUCK : RC_SUST;
}
/* the ensemble regrouped: each musician a way to play (suited to its sound, unlike the others' where it can be), a
 * pace, a register */
static void rx_form(void)
{
    static const uint8_t W[3][RM_N] = {          /* (RESTS FOLLOWS CONTRASTS ECHOES ANSWERS, by plucked, sustained, texture) */
        {12, 22, 20, 20, 26}, {12, 34, 26, 18, 10}, {14, 50, 36, 0, 0}};
    uint32_t l, used = 0, nact = 0, nin = 0, octs = 0, nrest = 0, nall = 0;
    rx.form_t = fm1_ms + rx_in(0, 3000);         /* (the next, some seconds on: not on a clock) */
    for (l = 0; l < NLAYER; l++)
        nall += (uint32_t)rx_in_ens(l);
    for (l = 0; l < NLAYER; l++) {
        rx_ag_t *a = &rx.a[l];
        uint32_t m, tot = 0, d, was;
        uint8_t w[RM_N];
        if (!rx_in_ens(l))
            continue;
        nin++;
        rx_class(l);
        was = a->mode;
        for (m = 0; m < RM_N; m++) {
            w[m] = W[a->chr][m];
            if (m == RM_REST)                       /* (sparser, more sit out: one at most at the middle, none dense) */
                w[m] = (uint8_t)(nall < 3u || RX_DENS >= 80 || (nrest && RX_DENS >= 35) ? 0 : w[m] * (uint32_t)(160 - RX_DENS) / 100u);
            if (m == RM_ECHO || m == RM_ANSWER)     /* (more structure: more counterpoint) */
                w[m] = (uint8_t)(w[m] * (uint32_t)(50 + RX_CPLX) / 90u);
            if (m == RM_ANSWER && rx.typ > 900u)
                w[m] = (uint8_t)(w[m] / 4u);        /* (a guide holding long leaves few rests to answer) */
            if ((used >> m) & 1u && m != RM_REST)
                w[m] = (uint8_t)(w[m] / 3u);        /* (another plays so already: less likely) */
            if (m == was && m != RM_REST)
                w[m] = (uint8_t)(w[m] / 2u);        /* (and a change is welcome) */
            tot += w[m];
        }
        d = rx_in(0, tot - 1u);
        for (m = 0; m < RM_N - 1u && d >= w[m]; m++)
            d -= w[m];
        a->mode = (uint8_t)m;
        used |= 1u << m;
        nrest += m == RM_REST;
        nact += m != RM_REST;
        if (m != was)
            a->pos = RX_NONE;
        a->tx = 0;
        a->cont = 70;
        a->pplay = 85;
        if (RX_CPLX > 65 && rx_odds(RX_CPLX - 40))   /* (structure: a pace in threes against the guide's twos) */
            a->tf = 3;
        switch (m) {
        case RM_FOLLOW:
            a->tf = (uint8_t)(a->chr == RC_TEX ? 4u * rx_in(2, 4) : a->tf == 3u ? (rx_odds(50) ? 3u : 6u) : rx_odds(60) ? 4u : 8u);
            a->pct = 70;
            break;
        case RM_CONTRA:                         /* slow under a busy guide; moving under one holding long */
            a->tf = (uint8_t)(rx.typ < 350u ? rx_in(12, 16) : rx.typ > 900u ? (a->chr == RC_TEX ? 8u : rx_in(2, 4))
                              : rx_odds(50) ? 8u : RX_CPLX > 50 ? 3u : 2u);
            a->cont = 50;
            a->pplay = 70;
            a->pct = 58;
            break;
        case RM_ECHO: {
            static const int8_t TX[4] = {2, -2, -5, 0};
            a->tx = TX[rx_in(0, 3)];
            a->lag = (uint8_t)rx_in(1, 3);
            a->pct = 62;
            break;
        }
        case RM_ANSWER:
            a->tf = (uint8_t)(rx_odds(70) ? 4u : 6u);
            a->cont = 85;
            a->pplay = 95;
            a->pct = 76;
            break;
        default:
            break;
        }
        {   /* its register, as SPREAD has it -- around the guide's hands, an octave or two off, or up to three -- low
             * for a texture or a bass, the other side for a contrast, unlike the others' */
            static const int8_t O[3][4] = {{0, -1, 1, 0}, {-1, 1, -2, 2}, {-2, 2, -3, 3}};
            const int8_t *o = O[RX_SPRD < 20 ? 0 : RX_SPRD < 60 ? 1 : 2];
            uint32_t k = rx_in(0, 3), t;
            int down = a->chr == RC_TEX || a->bassy || (m == RM_CONTRA && rx.mid > 64u), up = m == RM_CONTRA && !down;
            for (t = 0; t < 8u; t++, k = (k + 1u) % 4u)
                if ((!down || o[k] <= 0) && (!up || o[k] >= 0) && !((octs >> (o[k] + 3)) & 1u))
                    break;
            a->oct = o[k];
            octs |= 1u << (o[k] + 3);
        }
        a->cont = (uint8_t)clampi(a->cont + (RX_CONT - 60) * 4 / 5, 5, 98);   /* (CONTINUITY: kept to the phrases, or not) */
        a->next_at = fm1_ms + rx_in(0, 200);
    }
    if (nin && !nact)                               /* (never all resting) */
        for (l = 0; l < NLAYER; l++)
            if (rx_in_ens(l)) {
                rx.a[l].mode = RM_FOLLOW;
                rx.a[l].tf = 4;
                rx.a[l].pct = 70;
                break;
            }
}

/* ---- a musician's next event: on as the guide went on, or a jump to where it played the same; fitted to the harmony */
static uint32_t rx_choose(uint32_t l, int32_t *tx)
{
    rx_ag_t *a = &rx.a[l];
    uint32_t cand[12], w[12], nc = 0, i, k, lvl, tot = 0, d, lo = rx.hn > RX_H ? rx.hn - RX_H : 0u, hi = rx.hn - 1u, on = 0;
    static const int8_t TXS[5] = {0, 2, -2, 4, -3};
    uint32_t ntx = RX_CPLX < 35 ? 1u : RX_CPLX > 65 ? 5u : 3u;   /* (moved along the scale: plain, none; rich, further) */
    if (a->mode != RM_ANSWER)
        hi = rx.hn > 2u ? rx.hn - 2u : 0u;          /* (not the guide's very last: that is an echo, not a line) */
    if (rx_kept(a->pos) && a->pos + 1u <= hi) {     /* on as it went on */
        cand[nc++] = a->pos + 1u;
        on = 1;
    }
    if (rx_kept(a->pos))                            /* .. or where the guide played the same: the longest same first */
        for (lvl = 0; lvl < 5u && nc < 2u; lvl++) {
            static const uint8_t K[5] = {3, 2, 2, 1, 1}, L[5] = {0, 0, 1, 1, 2};
            for (i = lo + K[lvl]; i < hi && nc < 12u; i++)
                if (i != a->pos && i + 1u != a->pos + 1u && rx_same(i, a->pos, K[lvl], L[lvl]))
                    cand[nc++] = i + 1u;
        }
    while (nc < 4u && hi >= lo) {                   /* (nothing like it: anywhere, a phrase's first preferred) */
        uint32_t j = lo + rx_in(0, hi - lo);
        for (k = 0; k < 6u && !rx_it(j)->start && j > lo; k++)
            j--;
        cand[nc++] = j;
        if (nc >= 4u || rx.hn < 2u)
            break;
    }
    for (i = 0; i < nc; i++) {                      /* each: how it fits (moved as fits best), the line going on, taboo */
        uint32_t best = 0, f, t;
        int32_t bt = a->tx;
        if (i == 0u && on) {
            best = rx_fits(l, cand[i], a->tx);
            w[i] = (1u + best * 4u / 64u) * (uint32_t)a->cont / 10u;
        } else {
            for (t = 0; t < ntx; t++) {
                int32_t x = a->mode == RM_ECHO ? a->tx : TXS[t];
                f = rx_fits(l, cand[i], x) + (x == 0 ? 20u : 0u);
                if (f > best) {
                    best = f;
                    bt = x;
                }
            }
            w[i] = (1u + best * 4u / 64u) * (100u - a->cont) / 10u / (nc > 1u ? nc - 1u : 1u) + 1u;
            cand[i] |= (uint32_t)(bt + 8) << 24;   /* (its move, kept with it: a jump) */
        }
        for (k = 0; k < 3u; k++)
            if (a->taboo[k] == (cand[i] & 0xFFFFFFu))
                w[i] = w[i] / 4u + 1u;
        tot += w[i];
    }
    if (!nc)
        return RX_NONE;
    d = rx_in(0, tot - 1u);
    for (i = 0; i + 1u < nc && d >= w[i]; i++)
        d -= w[i];
    if (cand[i] >> 24) {
        *tx = (int32_t)(cand[i] >> 24) - 8;
        a->sh = 0x7F;                               /* (a jump: its register found again) */
    } else
        *tx = a->tx;
    return cand[i] & 0xFFFFFFu;
}
/* a musician plays its next event, and waits as the guide waited after it */
static void rx_step(uint32_t l)
{
    rx_ag_t *a = &rx.a[l];
    int32_t tx = a->tx;
    uint32_t e = rx_choose(l, &tx), n[3], c, k, gap, len, vel;
    if (e == RX_NONE || !rx_kept(e)) {
        a->next_at = fm1_ms + rx.typ;
        return;
    }
    a->tx = (int8_t)tx;
    if (a->sh == 0x7F || !rx_kept(a->pos))
        a->sh = (int8_t)rx_reg(l, e, tx);
    a->pos = e;
    a->taboo[2] = a->taboo[1];
    a->taboo[1] = a->taboo[0];
    a->taboo[0] = e;
    gap = rx_kept(e + 1u) ? rx_it(e + 1u)->ioi : rx.typ;   /* (the guide's own wait after it: a breath, a phrase's end) */
    if (RX_CPLX < 35 && gap < rx.typ * 3u) {       /* (plain: rounded to a half, one, two or four of its usual gap) */
        uint32_t g = rx.typ / 2u;
        while (g * 3u < gap * 2u && g < rx.typ * 4u)
            g *= 2u;
        gap = g;
    }
    len = rx_it(e)->dur ? rx_it(e)->dur : gap * 9u / 10u;
    gap = (uint32_t)(((uint64_t)gap * a->tf / 4u * exp2_q16((50 - RX_DENS) * 1311)) >> 16);   /* (DENSITY: x 2 .. x 1/2) */
    gap = (uint32_t)clampi((int32_t)gap, 70, 9000);
    len = (uint32_t)clampi((int32_t)(len * a->tf / 4u) + (a->chr == RC_SUST ? 40 : 0), 50, 12000);
    vel = rx_it(e)->vel * a->pct / 100u;
    if (rx_odds(clampi(a->pplay * (25 + 3 * RX_DENS / 2) / 100, 3, 98))) {
        uint32_t at = 0;
        if (RX_CPLX > 70 && a->chr != RC_TEX && gap > 200u && rx_odds(RX_CPLX - 70))
            at = gap / (rx_odds(50) ? 4u : 3u);     /* (pushed off the beat) */
        c = rx_render(l, e, tx, a->sh, n);
        if (rx.nhd && len >= 350u)                  /* a long note over what the guide holds: onto its harmony */
            for (k = 0; k < c; k++)
                n[k] = rx_settle(n[k]);
        if (RX_CPLX > 60 && a->chr != RC_TEX && gap >= 240u && rx_odds((RX_CPLX - 60) * 3 / 2)) {
            /* split: the step in two or three -- the event, then the next as the guide went on, sooner */
            uint32_t parts = rx_odds(50) ? 2u : 3u, p, m[3], cm = rx_kept(e + 1u) ? rx_render(l, e + 1u, tx, a->sh, m) : 0u;
            uint32_t d = (gap - at) / parts;
            if (parts == 2u && rx_odds(50))
                d = (gap - at) * 2u / 3u;           /* (unevenly: long, short) */
            for (k = 0; k < c; k++)
                rx_play(l, n[k], vel - k * 6u, d * 9u / 10u, at + k * rx_in(0, 18));
            for (p = 1; p < parts && cm; p++)
                rx_play(l, p == 2u ? rx_note((int32_t)rx_dg(m[cm - 1u]) + (n[c - 1u] > m[cm - 1u] ? 1 : -1)) : m[cm - 1u], vel - 8u,
                        ((parts == 2u ? gap - at - d : d) * 9u) / 10u, at + p * d);
        } else
            for (k = 0; k < c; k++)
                rx_play(l, n[k], vel - k * 6u, len, at + k * rx_in(0, 18));
    }
    a->next_at = fm1_ms + gap;
}

/* ---- the guide heard */
static void rx_off(uint32_t note)
{
    uint32_t k;
    for (k = 0; k < rx.nhd; k++)
        if (rx.hd[k].note == note) {
            uint32_t d = fm1_ms - rx.hd[k].t, it = rx.hd[k].it;
            if (rx_kept(it) && rx_it(it)->dur < d)
                rx_it(it)->dur = (uint16_t)(d > 12000u ? 12000u : d);
            rx.hd[k] = rx.hd[--rx.nhd];
            break;
        }
    rx.last_ev = fm1_ms;
}
static void ar_hear(uint32_t note, int on);
static void ar_form(void);
static void rx_hear(uint32_t note, uint32_t vel, int on)
{
    uint32_t now = fm1_ms, ioi, gap, l, i;
    rx_item_t *it;
    if (!rx.on || note > 127u)
        return;
    if (!on) {
        rx_off(note);
        if (rx.mode == RXM_ARP)
            ar_hear(note, 0);
        return;
    }
    vel = vel ? vel : 100u;
    rx.flash = now + 1u;
    ioi = rx.last_on ? now - rx.last_on : 100000u;
    gap = rx.last_ev ? now - rx.last_ev : 100000u;
    rx.ch[note % 12u] = (uint16_t)(rx.ch[note % 12u] + 500u > 4000u ? 4000u : rx.ch[note % 12u] + 500u);
    rx.mid = (uint8_t)((rx.mid * 7u + note) / 8u);
    if (ioi < 45u && rx.hn) {                       /* (struck with the one before: a chord, one event) */
        it = rx_it(rx.hn - 1u);
        if (it->nn < 3u) {
            for (i = it->nn; i > 0u && it->n[i - 1u] > note; i--)
                it->n[i] = it->n[i - 1u];
            it->n[i] = (uint8_t)note;
            it->nn++;
            it->dg = (uint8_t)rx_dg(it->n[it->nn - 1u]);
        }
    } else {
        if (ioi >= 60u && ioi < 3000u && ioi < rx.typ * 3u)   /* its usual gap */
            rx.typ = (uint32_t)clampi((int32_t)rx.typ + ((int32_t)ioi - (int32_t)rx.typ) / 5, 90, 2500);
        it = rx_it(rx.hn);
        it->n[0] = (uint8_t)note;
        it->nn = 1;
        it->vel = (uint8_t)vel;
        it->dg = (uint8_t)rx_dg(note);
        it->ioi = (uint16_t)(ioi > 8000u ? 8000u : ioi);
        it->dur = 0;
        it->start = (uint8_t)(!rx.hn || (rx.nhd == 0u && gap > rx_rest_ms()));
        it->t = now;
        rx.hn++;
        rx.vel = (uint8_t)((rx.vel * 3u + vel) / 4u);
        rx.last_on = now;
        rx.ans_done = 0;
        if (it->start) {
            rx.ph0 = rx.hn - 1u;
            if (rx.mode == RXM_REACT && (int32_t)(now - rx.form_t) > 4500)   /* a phrase begins, some seconds on: the
                                                                             * ensemble regroups */
                rx_form();
        }
        for (l = 0; l < NLAYER && rx.mode == RXM_REACT; l++) {
            rx_ag_t *a = &rx.a[l];
            if (!rx_in_ens(l))
                continue;
            if (a->mode == RM_ANSWER && (int32_t)(a->ans_end - now) > 0) {
                rx_cut(l);                          /* (the guide plays on: the answer stops) */
                a->ans_end = now;
            } else if (a->mode == RM_ECHO && rx.hn > a->lag && rx_odds(clampi(30 + RX_DENS, 5, 98))) {   /* the line, events behind */
                uint32_t e = rx.hn - 1u - a->lag, n[3], c, k;
                if (a->sh == 0x7F || it->start)
                    a->sh = (int8_t)rx_reg(l, e, a->tx);
                c = rx_render(l, e, a->tx, a->sh, n);
                for (k = 0; k < c; k++)
                    rx_play(l, n[k], rx_it(e)->vel * a->pct / 100u, rx_it(e)->dur ? rx_it(e)->dur : rx.typ, rx_in(0, 15));
                a->echoed = e + 1u;
            } else if (a->mode == RM_FOLLOW && (int32_t)(a->next_at - now) > 150 && rx_odds(40))
                a->next_at = now + rx_in(30, 120);  /* (with the guide: quick to come in) */
        }
    }
    if (rx.nhd < RX_HD) {
        rx.hd[rx.nhd].note = (uint8_t)note;
        rx.hd[rx.nhd].t = now;
        rx.hd[rx.nhd].it = rx.hn - 1u;
        rx.nhd++;
    }
    for (i = 0; i < RX_Q && rx.mode == RXM_REACT; i++)   /* the harmony moved: a long note that no longer fits lets go */
        if (rx.q[i].on == 2u && (int32_t)(rx.q[i].at - now) > 300 && rx_fit(rx.q[i].note % 12u) < 30u && rx_odds(70))
            rx.q[i].at = now + rx_in(0, 60);
    rx.last_ev = now;
    if (rx.mode == RXM_ARP)
        ar_hear(note, 1);
}

static void rx_reset(void)                          /* nothing heard yet (on, or a new guide) */
{
    uint32_t l;
    rx_cut(NLAYER);
    rx.nhd = 0;
    memset(rx.ch, 0, sizeof rx.ch);
    rx.hn = 0;
    rx.last_on = rx.last_ev = rx.form_t = 0;
    rx.typ = 500;
    rx.vel = 96;
    rx.mid = 60;
    rx.ans_done = 1;
    rx.guide = ui.sel;
    rx_ear = rx.on ? ui.sel : 0xFFu;
    for (l = 0; l < NLAYER; l++) {
        rx.a[l].pos = rx.a[l].echoed = RX_NONE;
        rx.a[l].mode = RM_REST;
        rx.a[l].sh = 0x7F;
        rx.sv[l].until = 0;
    }
    rx.ann = 0;
    rx.a_idle = 1;
    rx_form();
    ar_form();
}
static void rx_defaults(void)                      /* (the pages' values, the first time) */
{
    static const int8_t KD[4] = {50, 40, 60, 40}, SD[4] = {30, 30, 40, 30}, AD[6] = {50, 40, 40, 20, 30, 10};
    if (!rx.k[0] && !rx.k[1] && !rx.k[2] && !rx.k[3]) {
        memcpy(rx.k, KD, sizeof rx.k);
        memcpy(rx.sk, SD, sizeof rx.sk);
    }
    if (!rx.ak[0] && !rx.ak[1] && !rx.ak[2] && !rx.ak[3] && !rx.ak[4] && !rx.ak[5])
        memcpy(rx.ak, AD, sizeof rx.ak);
}
static void rx_set(int on)
{
    rx_defaults();
    rx.on = (uint8_t)on;
    rx_reset();
}

/* ---- SHADOW: the guide's own playing, shadowed by each track taken in -- a little or a long way behind, at its own
 * pace, now and then backwards or to and fro, resting and coming in again, moved along the scale. Its page:
 *   DELAY      how far behind: from right behind (a note or two) to 15 s, each shadow at its own distance
 *   DRIFT      how far each one's pace strays from the guide's: from a little loose to half, two thirds, three
 *              quarters, one and a third, one and a half, twice -- in and out of step
 *   VARIATION  how often each changes its way: low, forwards and steady for long; high, backwards, to and fro,
 *              resting and coming in again, its rhythm loosened (notes late, left out, struck twice)
 *   TRANSPOSE  how far it is moved: low, as played (or an octave); high, a third, a fourth, a fifth, a sixth away
 * It shadows what the guide sounds -- keys, chords, loops, BALLSY, BRICKLAYER -- and stops when that does. */
#define SH_DELAY rx.sk[0]
#define SH_DRIFT rx.sk[1]
#define SH_VAR rx.sk[2]
#define SH_TRANS rx.sk[3]
static const char *const SH_KNAME[4] = {"DELAY", "DRIFT", "VARIATION", "TRANSPOSE"};
static uint32_t sh_lag(void)                        /* 150 ms .. 15 s, this shadow's own, about DELAY's */
{
    uint32_t d = 150u + (uint32_t)SH_DELAY * (uint32_t)SH_DELAY * 3u / 2u;
    return d * rx_in(70, 130) / 100u;
}
static uint32_t sh_rate(void)
{
    static const uint32_t R[7] = {32768, 43691, 49152, 65536, 87381, 98304, 131072};   /* 1/2 2/3 3/4 1 4/3 3/2 2 */
    if (SH_DRIFT < 25)                              /* (a little loose: within 6 %) */
        return 65536u + (uint32_t)((int32_t)rx_in(0, 2u * (uint32_t)SH_DRIFT) - SH_DRIFT) * 65536u / 400u;
    if (rx_odds(100 - SH_DRIFT))
        return 65536u;
    return SH_DRIFT < 70 ? R[rx_in(1, 5)] : R[rx_in(0, 6)];
}
static void sh_move(rx_sv_t *v)                     /* how far it is moved, as TRANSPOSE has it */
{
    static const int8_t T[7] = {0, 2, -2, -5, 4, -3, 5};
    uint32_t t = (uint32_t)SH_TRANS;
    v->tx = t < 40u ? 0 : T[rx_odds((int32_t)t) ? rx_in(0, t < 70u ? 3u : 6u) : 0u];
    v->oct = (int8_t)(t >= 15u && rx_odds((int32_t)t / 2) ? (rx_odds(50) ? 1 : -1) : 0);
}
static uint32_t sh_old(void) { return rx.hn > RX_H ? rx.hn - RX_H : 0u; }
static void sh_find(rx_sv_t *v)                     /* the event it meets next, its way */
{
    uint32_t i;
    if (v->sgn > 0) {
        for (i = sh_old(); i < rx.hn && (int32_t)(rx_it(i)->t - v->vt) <= 0; i++)
            ;
        v->ci = i;                                  /* (rx.hn: the guide's next, to come) */
    } else {
        v->ci = RX_NONE;
        for (i = rx.hn; i-- > sh_old();)
            if ((int32_t)(rx_it(i)->t - v->vt) < 0) {
                v->ci = i;
                break;
            }
    }
}
static void sh_vary(uint32_t l, int first)          /* its way, for a while: as VARIATION has it */
{
    rx_sv_t *v = &rx.sv[l];
    uint32_t now = fm1_ms, V = (uint32_t)SH_VAR, r;
    v->until = now + rx_in(2500, 4000) + (100u - V) * rx_in(60, 160);
    if (first || v->paused) {                       /* (in, or in again: behind the guide by its own delay) */
        if (first) {
            v->rate = sh_rate();
            sh_move(v);
        }
        v->paused = 0;
        v->lag = sh_lag();
        v->vt = now - v->lag;
        v->dir = SD_FWD;
        v->sgn = 1;
        sh_find(v);
        return;
    }
    if (V && rx_odds((int32_t)V / 4)) {             /* a rest */
        v->paused = 1;
        rx_cut(l);
        v->until = now + rx_in(800, 4000);
        return;
    }
    r = rx_in(0, 99);
    if (r < V * 30u / 100u) {                       /* backwards */
        v->dir = SD_REV;
        v->sgn = -1;
    } else if (r < V * 55u / 100u) {                /* to and fro, over a stretch behind it */
        v->dir = SD_PING;
        v->sgn = -1;
        v->hi = v->vt;
        v->lo = v->vt - rx_in(1500, 6000);
    } else {
        v->dir = SD_FWD;
        v->sgn = 1;
    }
    if (rx_odds(20 + SH_DRIFT / 2))
        v->rate = sh_rate();
    if (rx_odds(SH_TRANS))
        sh_move(v);
    if (rx_odds((int32_t)V / 3))
        v->lag = sh_lag();
    sh_find(v);
}
static void sh_play(uint32_t l, uint32_t e)         /* the guide's event e, as this shadow plays it */
{
    const rx_item_t *it = rx_it(e);
    const rx_sv_t *v = &rx.sv[l];
    uint32_t V = (uint32_t)SH_VAR, k, lo = 0, hi = it->nn, len, wait = 0, vel = it->vel * 4u / 5u;
    if (V > 20u && rx_odds((int32_t)V / 10))
        return;                                     /* (left out) */
    if (rx.a[l].chr == RC_TEX)
        hi = 1;                                     /* (a texture: the lowest; plucked: the top) */
    else if (rx.a[l].chr == RC_PLUCK)
        lo = it->nn - 1u;
    len = it->dur ? it->dur : rx_kept(e + 1u) ? rx_it(e + 1u)->ioi : rx.typ;
    len = (uint32_t)clampi((int32_t)(len * 65536u / v->rate), 50, 10000);   /* (len <= 65535: fits 32 bits) */
    if (V > 30u)                                    /* (late, a little: its rhythm loosened) */
        wait = rx_in(0, V * (rx.typ < 600u ? rx.typ : 600u) / 400u);
    for (k = lo; k < hi; k++) {
        uint32_t n = (uint32_t)clampi((int32_t)rx_note((int32_t)rx_dg(it->n[k]) + v->tx) + 12 * v->oct, 24, 108);
        if (V > 40u && len > 200u && rx_odds((int32_t)V / 6)) {   /* struck twice */
            rx_play(l, n, vel, len / 2u, wait);
            rx_play(l, n, vel * 4u / 5u, len / 2u, wait + len / 2u);
        } else
            rx_play(l, n, vel, len, wait);
    }
}
static void sh_tick(void)
{
    uint32_t now = fm1_ms, l;
    for (l = 0; l < NLAYER; l++) {
        rx_sv_t *v = &rx.sv[l];
        uint32_t dt = now - v->last, step;
        v->last = now;
        if (!rx_in_ens(l) || !rx.hn || !rx.last_on)
            continue;
        if (!v->until)
            sh_vary(l, 1);
        if (!rx.nhd && now - rx.last_ev > v->lag + 4000u) {
            v->until = 0;                           /* (the guide silent, all of it shadowed: waiting, to come in afresh) */
            continue;
        }
        if ((int32_t)(now - v->until) >= 0)
            sh_vary(l, 0);
        if (v->paused)
            continue;
        if (dt > 200u)
            dt = 200u;
        step = (uint32_t)(((uint64_t)dt * v->rate) >> 16);
        v->vt = v->sgn > 0 ? v->vt + step : v->vt - step;
        if ((int32_t)(v->vt - (now - 80u)) > 0) {   /* (never ahead of the guide: right behind it, then) */
            v->vt = now - 80u;
            if (v->dir == SD_PING) {
                v->sgn = -1;
                sh_find(v);
            }
        }
        if (v->dir == SD_PING && ((v->sgn < 0 && (int32_t)(v->vt - v->lo) < 0) || (v->sgn > 0 && (int32_t)(v->vt - v->hi) > 0))) {
            v->sgn = (int8_t)-v->sgn;               /* (to and fro: turned at either end) */
            sh_find(v);
        }
        if ((int32_t)(v->vt - rx_it(sh_old())->t) < 0 || (v->sgn < 0 && v->ci == RX_NONE)) {
            if (v->sgn < 0) {                       /* (back past what is kept: forwards again) */
                v->sgn = 1;
                v->dir = v->dir == SD_PING ? SD_PING : SD_FWD;
                if ((int32_t)(v->vt - rx_it(sh_old())->t) < 0)
                    v->vt = rx_it(sh_old())->t;
            } else
                v->vt = now - v->lag;
            sh_find(v);
        }
        if (v->dir == SD_FWD && now - v->vt > v->lag + 8000u) {   /* (fallen far behind: caught up) */
            v->vt = now - v->lag;
            sh_find(v);
        }
        if (v->sgn > 0) {
            if (v->ci != RX_NONE && v->ci < rx.hn && !rx_kept(v->ci))
                sh_find(v);
            while (v->ci < rx.hn && rx_kept(v->ci) && (int32_t)(rx_it(v->ci)->t - v->vt) <= 0)
                sh_play(l, v->ci++);
        } else
            while (rx_kept(v->ci) && (int32_t)(rx_it(v->ci)->t - v->vt) >= 0) {
                sh_play(l, v->ci);
                v->ci = v->ci ? v->ci - 1u : RX_NONE;
            }
    }
}
/* ---- ARP: the tracks taken in break the chord the guide plays into arpeggios -- each at its own pace, its own way,
 * in its own octaves -- and follow it: a new chord, once it has stood AR_SETTLE ms, is taken by each track at its
 * next step, from its pattern's start -- never between two of its notes, and their pulse unbroken (a hand a little
 * early or late on the change does not jolt them; from silence, the first chord starts them at once). What the
 * guide holds is the chord (a finger lifted, a note less); let go, the last chord goes on until the guide has been
 * silent a while; a single note is arpeggiated over its octaves. Whatever the guide sounds leads: its keys, its loops,
 * BALLSY, BRICKLAYER. Its page:
 *   RATE      the pace: a step every 2 s (0) .. every 62 ms (100), steady
 *   RELATION  how the tracks' paces relate: all one at 0; higher, twice, half, three in two, two in three, four in
 *             three, three times as fast -- each track its own, interlocking
 *   MOTION    which way: all up at 0; higher, each its own -- down, up and down, down and up, the order played, from
 *             the outside in, from the middle out, at random -- the second against the first; high, a track turns
 *             another way now and then
 *   MUTATE    how much a pattern changes as it repeats: fixed at 0; higher, steps swapped, a rest, a step struck
 *             again, the pattern turned on
 *   RANGE     how many octaves each spans, 1 .. 4, and how far apart the tracks sit
 *   CHANCE    tight at 0; higher, steps left out and steps struck twice */
#define AR_RATE rx.ak[0]
#define AR_REL rx.ak[1]
#define AR_MOT rx.ak[2]
#define AR_MUT rx.ak[3]
#define AR_RNG rx.ak[4]
#define AR_CHN rx.ak[5]
static const char *const AR_KNAME[6] = {"RATE", "RELATION", "MOTION", "MUTATE", "RANGE", "CHANCE"};
enum { AD_UP, AD_DOWN, AD_UPDN, AD_DNUP, AD_PLAYED, AD_IN, AD_OUT, AD_RAND, AD_N };
static uint32_t ar_base(void)                       /* RATE's step, ms: 2000 .. 62 */
{
    return (uint32_t)(((uint64_t)2000u * exp2_q16(-(int32_t)AR_RATE * 3277)) >> 16);
}
static uint32_t ar_step_ms(uint32_t l)
{
    const rx_av_t *v = &rx.av[l];
    uint32_t s = ar_base() * 256u / (v->mult ? v->mult : 256u);
    if (rx.a[l].chr == RC_TEX)
        s *= 4u;                                    /* (a texture: slowly) */
    return s < 40u ? 40u : s;
}
/* its notes: the chord (sorted, or as played), its lowest moved to the track's register, up RANGE's octaves */
static void ar_notes(uint32_t l)
{
    rx_av_t *v = &rx.av[l];
    uint8_t b[8];
    uint32_t n = rx.ann, i, j, o, span = 1u + (uint32_t)AR_RNG * 3u / 100u, lo = 127;
    int32_t start;
    for (i = 0; i < n; i++) {
        b[i] = rx.an[i];
        lo = b[i] < lo ? b[i] : lo;
    }
    if (v->dir != AD_PLAYED)                        /* (low to high, but as played for PLAYED) */
        for (i = 1; i < n; i++)
            for (j = i; j > 0u && b[j - 1u] > b[j]; j--) {
                uint8_t t = b[j];
                b[j] = b[j - 1u];
                b[j - 1u] = t;
            }
    start = (int32_t)lo + 12 * v->oct;
    while (start + 12 * (int32_t)span > 104)
        start -= 12;
    while (start < 28)
        start += 12;
    v->n = 0;
    for (o = 0; o < span; o++)
        for (i = 0; i < n && v->n < 24u; i++) {
            int32_t m = start + (int32_t)b[i] - (int32_t)lo + 12 * (int32_t)o;
            for (j = 0; j < v->n && v->e[j] != m; j++)
                ;
            if (m >= 24 && m <= 108 && j == v->n)
                v->e[v->n++] = (uint8_t)m;
        }
}
static void ar_mutate(rx_av_t *v)                   /* the pattern changed a little, as MUTATE has it */
{
    uint32_t a = rx_in(0, v->len - 1u), b = rx_in(0, v->len - 1u), k, rests = 0;
    for (k = 0; k < v->len; k++)
        rests += v->seq[k] == AR_REST;
    switch (rx_in(0, 3)) {
    case 0: {                                       /* two steps swapped */
        uint8_t t = v->seq[a];
        v->seq[a] = v->seq[b];
        v->seq[b] = t;
        break;
    }
    case 1:                                         /* a rest, or a rest filled */
        if (v->seq[a] == AR_REST)
            v->seq[a] = (uint8_t)rx_in(0, v->n - 1u);
        else if (rests * 3u < v->len && v->len > 2u)
            v->seq[a] = AR_REST;
        break;
    case 2:                                         /* a step struck again */
        if (v->len < AR_SEQ) {
            for (k = v->len; k > a; k--)
                v->seq[k] = v->seq[k - 1u];
            v->len++;
        }
        break;
    default: {                                      /* turned on by one */
        uint8_t t = v->seq[0];
        for (k = 0; k + 1u < v->len; k++)
            v->seq[k] = v->seq[k + 1u];
        v->seq[v->len - 1u] = t;
        break;
    }
    }
}
static void ar_build(uint32_t l)                    /* its pattern, its way over its notes */
{
    rx_av_t *v = &rx.av[l];
    uint32_t n, i, k = 0;
    ar_notes(l);
    n = v->n;
    v->len = 0;
    v->pos = 0;
    if (!n)
        return;
    switch (v->dir) {
    case AD_DOWN:
        for (i = 0; i < n; i++)
            v->seq[k++] = (uint8_t)(n - 1u - i);
        break;
    case AD_UPDN:
    case AD_DNUP:
        for (i = 0; i < n && k < AR_SEQ; i++)
            v->seq[k++] = (uint8_t)(v->dir == AD_UPDN ? i : n - 1u - i);
        for (i = n >= 2u ? n - 2u : 0u; i >= 1u && n > 2u && k < AR_SEQ; i--)
            v->seq[k++] = (uint8_t)(v->dir == AD_UPDN ? i : n - 1u - i);
        break;
    case AD_IN:
        for (i = 0; i < (n + 1u) / 2u; i++) {
            v->seq[k++] = (uint8_t)i;
            if (n - 1u - i != i)
                v->seq[k++] = (uint8_t)(n - 1u - i);
        }
        break;
    case AD_OUT:
        for (i = 0; i < n; i++)                     /* the middle, then out: m, m-1, m+1, m-2 .. */
            v->seq[k++] = (uint8_t)(n / 2u + (i & 1u ? -(int32_t)((i + 1u) / 2u) : (int32_t)(i / 2u)));
        break;
    case AD_RAND:
        for (i = 0; i < n; i++)
            v->seq[k++] = AR_ANY;
        break;
    default:                                        /* up, and as played (its notes kept in that order) */
        for (i = 0; i < n; i++)
            v->seq[k++] = (uint8_t)i;
        break;
    }
    v->len = (uint8_t)(k < AR_SEQ ? k : AR_SEQ);
    for (i = 0; i < v->mut && i < 6u; i++)          /* (what it has mutated, again) */
        ar_mutate(v);
}
static void ar_restart(void)                        /* from silence (or the tracks dealt again): every pattern from its
                                                     * start, a moment from now */
{
    uint32_t l;
    for (l = 0; l < NLAYER; l++)
        if (rx_in_ens(l)) {
            ar_build(l);
            rx.av[l].ver = rx.a_ver;
            rx.av[l].fresh = 1;
            rx.av[l].next_at = fm1_ms + 40u;        /* (the chord's other notes, a moment to come) */
        }
}
/* the tracks' paces, ways and registers, as RELATION, MOTION and RANGE have them: each unlike the others */
static void ar_form(void)
{
    static const uint16_t MULT[8] = {256, 512, 128, 384, 171, 341, 768, 192};   /* x1 2 1/2 3/2 2/3 4/3 3 3/4 */
    static const int8_t OCT[3][6] = {{0, 1, 0, 1, 0, 1}, {0, 1, -1, 1, -1, 0}, {0, 1, -1, 2, -2, 1}};
    uint32_t k, rank = 0, um = 0, ud = 0, nm = 1u + (uint32_t)AR_REL * 7u / 100u, nd = 1u + (uint32_t)AR_MOT * 7u / 100u;
    for (k = rx.guide + 1u; k < rx.guide + 1u + NLAYER; k++) {
        uint32_t l = k % NLAYER, mi = 0, d = AD_UP, t;
        rx_av_t *v = &rx.av[l];
        if (!rx_in_ens(l))
            continue;
        rx_class(l);
        if (rank && nm > 1u) {                      /* (the first at RATE's pace; the others drawn, unlike) */
            mi = rx_in(1, nm - 1u);
            for (t = 0; t < nm && ((um >> mi) & 1u); t++)
                mi = 1u + mi % (nm - 1u);
        }
        um |= 1u << mi;
        if (nd > 1u) {
            d = rank == 1u ? AD_DOWN : rx_in(0, nd - 1u);   /* (the second against the first) */
            for (t = 0; t < nd && ((ud >> d) & 1u); t++)
                d = (d + 1u) % nd;
            if (rank == 0u && rx_odds(50))
                d = AD_UP;
        }
        ud |= 1u << d;
        v->mult = MULT[mi];
        v->dir = (uint8_t)d;
        v->oct = OCT[AR_RNG < 25 ? 0 : AR_RNG < 60 ? 1 : 2][rank % 6u];
        v->rank = (uint8_t)rank;
        v->mut = 0;
        rank++;
    }
    ar_restart();
}
static void ar_hear(uint32_t note, int on)          /* the guide's chord, as it comes and goes */
{
    uint32_t now = fm1_ms, k, ch = 0;
    if (on) {
        if (rx.nhd <= 1u && now - rx.a_on > 120u) { /* (the hand down again, alone: a new chord) */
            rx.ann = 0;
            ch = 1;
        }
        for (k = 0; k < rx.ann && rx.an[k] != note; k++)
            ;
        if (k == rx.ann && rx.ann < 8u) {
            rx.an[rx.ann++] = (uint8_t)note;
            ch = 1;
        }
        rx.a_on = now;
    } else if (rx.nhd) {                            /* a finger lifted, the others held: a note less */
        for (k = 0; k < rx.ann && rx.an[k] != note; k++)
            ;
        if (k < rx.ann) {
            for (; k + 1u < rx.ann; k++)
                rx.an[k] = rx.an[k + 1u];
            rx.ann--;
            ch = 1;
        }
    }
    if (ch && rx.ann) {
        rx.a_ver++;
        rx.a_chg = now;
        if (rx.a_idle) {                            /* (from silence: at once, the pulse from this chord) */
            rx.a_idle = 0;
            ar_restart();
        }
    }
}
static void ar_step(uint32_t l)                     /* a step: its note (or a rest, a skip, a note struck twice) */
{
    rx_av_t *v = &rx.av[l];
    uint32_t sm = ar_step_ms(l), s, pct = rx.a[l].chr == RC_PLUCK ? 55u : rx.a[l].chr == RC_TEX ? 100u : 85u, vel;
    if ((int32_t)(fm1_ms - v->next_at) > (int32_t)sm)
        v->next_at = fm1_ms;                        /* (fallen behind: on from now) */
    v->next_at += sm;
    if (v->ver != rx.a_ver && (v->fresh || fm1_ms - rx.a_chg >= AR_SETTLE)) {   /* a new chord, settled: taken now,
                                                                                 * on this step */
        ar_build(l);
        v->ver = rx.a_ver;
    }
    v->fresh = 0;
    if (!v->len || !v->n)
        return;
    s = v->seq[v->pos];
    if (s == AR_ANY)
        s = rx_in(0, v->n - 1u);
    vel = rx.vel * 3u / 4u + (v->pos ? 0u : 12u);
    if (AR_CHN)
        vel = (uint32_t)clampi((int32_t)vel + (int32_t)rx_in(0, (uint32_t)AR_CHN / 3u) - AR_CHN / 6, 10, 124);
    if (s < v->n && !rx_odds(AR_CHN * 2 / 5)) {
        if (sm > 120u && rx_odds(AR_CHN / 4)) {     /* struck twice */
            rx_play(l, v->e[s], vel, sm * pct / 200u, 0);
            rx_play(l, v->e[s], vel * 4u / 5u, sm * pct / 200u, sm / 2u);
        } else
            rx_play(l, v->e[s], vel, sm * pct / 100u, 0);
    }
    if (++v->pos >= v->len) {                       /* a round done: now and then changed, or turned another way */
        v->pos = 0;
        if (AR_MUT && rx_odds(AR_MUT)) {
            ar_mutate(v);
            v->mut = (uint8_t)(v->mut < 250u ? v->mut + 1u : v->mut);
        }
        if (AR_MOT > 50 && rx_odds((AR_MOT - 50) / 2)) {
            v->dir = (uint8_t)rx_in(0, AD_N - 1u);
            ar_build(l);
        }
    }
}
static void ar_tick(void)
{
    uint32_t now = fm1_ms, l, hold = ar_base() * 4u;
    if (!rx.ann || (!rx.nhd && now - rx.last_ev > (hold > 1500u ? hold : 1500u))) {
        rx.a_idle = 1;                              /* (no chord, or the guide silent a while: quiet) */
        return;
    }
    for (l = 0; l < NLAYER; l++)
        if (rx_in_ens(l) && (int32_t)(now - rx.av[l].next_at) >= 0)
            ar_step(l);
}

static void rx_mode_set(uint32_t m)                 /* REACT, SHADOW or ARPEGGIO (PRESETS on the page): the notes let go */
{
    uint32_t l;
    if (m == rx.mode || m >= RXM_N)
        return;
    rx_cut(NLAYER);
    rx.mode = (uint8_t)m;
    for (l = 0; l < NLAYER; l++) {
        rx.sv[l].until = 0;
        rx.a[l].pos = rx.a[l].echoed = RX_NONE;
    }
    rx.ann = 0;
    rx.a_idle = 1;
    rx_form();
    ar_form();
}

/* the main loop, every pass: the guide's loops heard; the musicians' steps as they fall due; its rests answered,
 * its silence kept; the notes due */
static void rx_tick(void)
{
    uint32_t now = fm1_ms, i, l;
    if (rx.on && rx.guide != ui.sel)
        rx_reset();                                 /* (another track chosen: it leads now) */
    while (rx_lr != rx_lw) {                        /* the guide's loops' notes (the ISR's) */
        uint32_t n = rx_lq[rx_lr % 16u][0], v = rx_lq[rx_lr % 16u][1];
        RING_PUBLISH();
        rx_lr++;
        rx_hear(n, v, v != 0u);
    }
    if (rx.on && now - rx.decay_t >= 100u) {        /* the harmony lately, fading (a second or two) */
        rx.decay_t = now;
        for (i = 0; i < 12u; i++)
            rx.ch[i] = (uint16_t)(rx.ch[i] - rx.ch[i] / 14u);
    }
    if (rx.on && rx.mode == RXM_SHADOW)
        sh_tick();
    if (rx.on && rx.mode == RXM_ARP)
        ar_tick();
    if (rx.on && rx.mode == RXM_REACT && rx.last_on && rx.hn >= 2u) {
        uint32_t gap = now - rx.last_ev, playing = rx.nhd || gap < rx_rest_ms() * 2u;
        if (!rx.nhd && !rx.ans_done && gap > rx_ans_ms()) {   /* a rest: the echoes finish the line, one answers */
            uint32_t na = 0, k, j = 0, phl = 0;
            rx.ans_done = 1;
            for (l = 0; l < NLAYER; l++) {
                rx_ag_t *a = &rx.a[l];
                if (!rx_in_ens(l))
                    continue;
                if (a->mode == RM_ECHO && rx_kept(a->echoed)) {
                    uint32_t t = rx.typ / 2u, e;
                    for (e = a->echoed; e < rx.hn; e++) {
                        uint32_t n[3], c = rx_render(l, e, a->tx, a->sh == 0x7F ? 0 : a->sh, n);
                        if (e > a->echoed)
                            t += (uint32_t)clampi(rx_it(e)->ioi, 70, 1500);
                        for (k = 0; k < c; k++)
                            rx_play(l, n[k], rx_it(e)->vel * a->pct / 100u, rx_it(e)->dur ? rx_it(e)->dur : rx.typ, t);
                    }
                    a->echoed = RX_NONE;
                }
                na += a->mode == RM_ANSWER;
            }
            for (i = rx.ph0 + 1u; i < rx.hn && rx_kept(i); i++)
                phl += rx_it(i)->ioi;
            if (na)                                 /* (the answers take turns) */
                for (l = 0; l < NLAYER; l++)
                    if (rx_in_ens(l) && rx.a[l].mode == RM_ANSWER && j++ == rx.turn % na) {
                        rx_ag_t *a = &rx.a[l];
                        uint32_t best = RX_NONE, lvl;
                        rx.turn++;
                        for (lvl = 0; lvl < 3u && best == RX_NONE; lvl++)   /* where the guide once went on from here */
                            for (i = rx.hn > RX_H ? rx.hn - RX_H + 3u : 3u; i + 1u < rx.hn; i++)
                                if (rx_same(i, rx.hn - 1u, 3u - lvl, lvl) && (best == RX_NONE || rx_odds(50)))
                                    best = i;
                        a->pos = best != RX_NONE ? best : rx.ph0 ? rx.ph0 - 1u : RX_NONE;
                        a->sh = 0x7F;
                        a->ans_end = now + (uint32_t)clampi((int32_t)phl * 3 / 2, 1500, 8000);
                        a->next_at = now + (uint32_t)clampi((int32_t)rx.typ / 3, 60, 400);
                        break;
                    }
        }
        for (l = 0; l < NLAYER; l++) {              /* each musician, on its own time */
            rx_ag_t *a = &rx.a[l];
            int go;
            if (!rx_in_ens(l) || a->mode == RM_REST || a->mode == RM_ECHO)
                continue;
            go = a->mode == RM_ANSWER ? (int32_t)(a->ans_end - now) > 0 && !rx.nhd : playing;
            if (!go) {
                if ((int32_t)(a->next_at - now) < 0)
                    a->next_at = now;               /* (back in at once when the guide is) */
                continue;
            }
            if ((int32_t)(now - a->next_at) >= 0)
                rx_step(l);
        }
        if (playing && (int32_t)(now - rx.form_t) > 6000)
            rx_form();                              /* (playing on without a rest: the ensemble regroups all the same) */
        if (!rx.nhd && gap > (rx.typ * 10u > 8000u ? rx.typ * 10u : 8000u)) {   /* silence: all let go, waiting */
            rx_cut(NLAYER);
            rx.last_on = 0;
        }
    }
    for (i = 0; i < RX_Q; i++) {
        if (rx.q[i].on == 1u && (int32_t)(now - rx.q[i].at) >= 0) {
            uint32_t pv = press_vel;
            press_vel = rx.q[i].vel;
            seq_press(rx.q[i].layer, rx.q[i].note, 0, 1);
            press_vel = (uint8_t)pv;
            rx.q[i].on = 2;
            rx.q[i].at = now + rx.q[i].len;
        } else if (rx.q[i].on == 2u && (int32_t)(now - rx.q[i].at) >= 0) {
            seq_press(rx.q[i].layer, rx.q[i].note, 0, 0);
            rx.q[i].on = 0;
        }
    }
}

static int8_t *rx_kv(void) { return rx.mode == RXM_ARP ? rx.ak : rx.mode == RXM_SHADOW ? rx.sk : rx.k; }   /* (the page's, of the mode) */
static const char *const *rx_kn(void) { return rx.mode == RXM_ARP ? AR_KNAME : rx.mode == RXM_SHADOW ? SH_KNAME : RX_KNAME; }
static uint32_t rx_nk(void) { return rx.mode == RXM_ARP ? 6u : 4u; }   /* (KNOB 1 .. 4; ARPEGGIO: SELECT, ALGORITHM too) */
static void rx_knob(uint32_t j, int32_t s)        /* the page: KNOB 1 .. 4 (ARPEGGIO: SELECT, ALGORITHM too) */
{
    rx_defaults();
    rx_kv()[j] = (int8_t)clampi(rx_kv()[j] + 2 * s, 0, 100);
    if (rx.mode == RXM_ARP && (j == 1u || j == 2u || j == 4u))
        ar_form();                                  /* (RELATION, MOTION, RANGE: the tracks dealt again) */
    if (rx.mode == RXM_REACT && (j == 3u || j == 2u))
        rx.form_t = fm1_ms - 5000u;                 /* (SPREAD, CONTINUITY: the ensemble regroups a second after) */
}
/* the page: track j's button, it in or out; the guide's, GUIDE TRACK */
static void rx_pad_hit(uint32_t j)
{
    static char m[24];
    if (j >= NLAYER)
        return;
    if (j == ui.sel) {
        rx.red_until = fm1_ms + 1200u;
        say("GUIDE TRACK", 1200);
        return;
    }
    rx.mask = (uint8_t)(rx.mask ^ (1u << j));
    rx_cut(j);
    rx.a[j].pos = rx.a[j].echoed = RX_NONE;
    rx.sv[j].until = 0;
    if (rx.mode == RXM_ARP)
        ar_form();
    rx_form();                                      /* (the ensemble regrouped with it, or without) */
    m[0] = (char)('1' + j);
    m[1] = ' ';
    str_cpy(m + 2, (rx.mask >> j) & 1u ? "IN" : "OUT", sizeof m - 2u);
    say(m, 1200);
}
