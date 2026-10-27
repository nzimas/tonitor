/* SPDX-License-Identifier: GPL-3.0-only
 * TONITOR: snapshots (Norilsk's, made for Tonitor) -- the whole machine at a moment, 16 of them in a session. A
 * snapshot holds, for each track: its engine; every engine's sound on it (as what draws it again: seed, level,
 * generation, path, family), each with its macros and its own envelope; its level, pan, sends, filter, granulator
 * (and its character), crush, distortion, octave, own transposition, path, chord mode, modulation; its latched notes
 * and chords; BALLSY's and BRICKLAYER's settings, BRICKLAYER's notes, whether each plays; its loops -- which loops,
 * each where it stood against the others, which are taken out, whether they play, their direction, speed and VARY.
 * And the reverb, the delay, the scale, the root, the loops' transposition and PLAY ALONG (on, its mode, the tracks
 * taken in, its pages' values). Not the track chosen, EDIT and the pages: the panel's, not the machine's.
 *
 * The bulk is kept once however many snapshots hold it: the sounds, the latched notes and BRICKLAYER's notes in
 * tables, the loops as takes -- a loop once recorded stays in its looper's events while a snapshot holds it, and the
 * looper's room is shared with them (layers.c: EV_LOOP_EMPTY, EV_LOOP_PACK, EV_LOOP_TAKES). In RAM, kept in the flash
 * with the session (sessions.c, format 27). Storing and recalling never touch the flash. A table full: the snapshot
 * is not stored (FULL); emptying snapshots makes room.
 *
 * A snapshot recalled with a morph time (KNOB 1 on the SNAPSHOTS page: 0 .. 30 s) moves into place over that time:
 *   gliding        the levels (in fractions of a step), pans, sends, filter, granulator, crush, distortion, the
 *                  macros and envelope of a sound that stays, the modulation's amounts, LFO rates and fades,
 *                  the reverb's DECAY DAMP MIX, the delay's REPEATS DAMP MIX, the sequencers' speed, rate, harmony
 *                  and instability
 *   at the middle  all that has no in-between, and all that would bend the pitch: the scale, the root, the
 *                  transpositions, the octaves (no glide, ever), the delay's TIME and the reverb's SIZE (a moving
 *                  line bends what it holds), the loops, the latched notes, the sequencers and their notes, chord
 *                  mode, the modulation's routes and shapes, the granulator's character, PLAY ALONG
 *   a new sound    the track goes down from a quarter of the way to silence at the middle, where its sound and
 *                  engine change, and back up by three quarters (layers.c lay_duck: no second set of voices)
 * At its end the snapshot is given exactly as stored. Recalled again on the way: a new morph from where it is.
 * The sounds of the engines a track does not play are drawn again one a frame after the middle. All of it in the
 * main loop: no work for the audio */

/* (NSNAP, MORPH_STEPS: ui.c) */
#define SND_N 96u                /* sounds the snapshots hold between them */
#define LAT_N 16u                /* tracks' latched notes .. (0: none latched) */
#define BLN_N 16u                /* BRICKLAYER's notes .. (0: none) */
typedef struct {
    uint32_t seed, word;         /* the sound (sessions.c sound_rec) */
    uint8_t kind, env_on;
    int8_t mac[NMAC], env[4];
} snd_ent_t;
typedef struct {
    uint32_t mask;
    uint8_t note[NKEY], form[NKEY];
} lat_ent_t;
typedef struct {
    uint8_t n;
    int8_t w[BL_MAX], o[BL_MAX];
} bln_ent_t;
typedef struct {
    uint32_t gseed;              /* the granulator's character */
    mod_t mod;
    uint16_t mute;               /* the loops taken out */
    uint16_t ph[LOOP_TRK];       /* where each loop stood (a fraction of its length, Q16) */
    uint8_t engine, path;
    uint8_t chord;               /* chord mode (bit 7), its form */
    uint8_t loop;                /* the loops playing (bit 0), direction (1, 2), speed (3, 4), VARY (5) */
    uint8_t ntrk;
    uint8_t lat, bln;            /* its latched notes, BRICKLAYER's notes (0: none) */
    uint8_t snd[NENG];           /* each engine's sound */
    uint8_t take[LOOP_TRK];      /* each loop */
    int8_t oct, lxt, level, pan, send, dsend, flt, gr, cr, dist;
    uint8_t bb, bb_n;            /* BALLSY: on (bit 0), playing (bit 1); how many */
    int8_t bb_speed, bb_base, bb_unst;
    uint8_t bl, bl_len, bl_dir, bl_spd;   /* BRICKLAYER: on (bit 0), playing (bit 1); its length, direction, speed */
    int8_t bl_rate, bl_harm, bl_unst;
} snap_trk_t;
typedef struct {
    snap_trk_t t[NLAYER];
    int8_t rvp[4], dlp[4];
    uint8_t scale, root;
    int8_t lx;
    uint8_t rx_on, rx_mode, rx_mask;
    int8_t rx_k[4], rx_sk[4], rx_ak[6];
} snap_t;
typedef char snap_t_is_604[sizeof(snap_t) == 604u ? 1 : -1];   /* (sessions keep it as it is: a new layout, a new format) */

static snap_t snap_s[NSNAP] POOL;
static snd_ent_t snd_tab[SND_N];
static lat_ent_t lat_tab[LAT_N + 1u];   /* (entry 0: none) */
static bln_ent_t bln_tab[BLN_N + 1u];
static take_t take_tab[NLAYER][NTAKE];
static struct {
    uint16_t used;               /* a bit a snapshot that holds a state */
    int8_t cur;                  /* the snapshot last stored or recalled, -1 none */
    uint8_t loop_dirty;          /* the loopers with events perhaps no longer held: packed away (a bit a track) */
} snap = {0, -1, 0};
static uint8_t snd_in[SND_N], lat_in[LAT_N + 1u], bln_in[BLN_N + 1u], take_in[NLAYER][NTAKE];   /* (held: snap_mark) */

/* sessions.c */
static void sound_rec(uint32_t l, uint32_t e, uint32_t *seed, uint32_t *word, uint8_t *kind);
static void ses_events_drain(void);

/* ------------------------------------------------------- the tables --- */
static snap_t morph_b POOL;      /* where a morph goes (and what is still to be given: snap_pend) */
static uint8_t snap_pend[NLAYER];   /* engines whose sound is still to be drawn again, a bit each */
static struct {
    uint8_t on, mid, to;
    uint8_t chg;                 /* the tracks whose sound changes: down, swapped at the middle, up */
    uint32_t t0, dur;
} morph;
static int snap_held_b(void)
{
    uint32_t l;
    for (l = 0; l < NLAYER; l++)
        if (snap_pend[l])
            return 1;
    return morph.on;
}
static void mark_one(const snap_t *s)
{
    uint32_t l, k;
    for (l = 0; l < NLAYER; l++) {
        const snap_trk_t *t = &s->t[l];
        for (k = 0; k < NENG; k++)
            if (t->snd[k] < SND_N)
                snd_in[t->snd[k]] = 1;
        lat_in[t->lat <= LAT_N ? t->lat : 0] = 1;
        bln_in[t->bln <= BLN_N ? t->bln : 0] = 1;
        for (k = 0; k < t->ntrk && k < LOOP_TRK; k++)
            if (t->take[k] < NTAKE)
                take_in[l][t->take[k]] = 1;
    }
}
/* what the snapshots hold (but snapshot skip, being stored over), and a morph's target */
static void snap_mark(int skip)
{
    uint32_t i;
    memset(snd_in, 0, sizeof snd_in);
    memset(lat_in, 0, sizeof lat_in);
    memset(bln_in, 0, sizeof bln_in);
    memset(take_in, 0, sizeof take_in);
    for (i = 0; i < NSNAP; i++)
        if (((snap.used >> i) & 1u) && (int)i != skip)
            mark_one(&snap_s[i]);
    if (snap_held_b())
        mark_one(&morph_b);
}
/* an entry the same, or a free one taken (-1: full) */
static int snd_intern(const snd_ent_t *e)
{
    uint32_t i;
    for (i = 0; i < SND_N; i++)
        if (snd_in[i] && !memcmp(&snd_tab[i], e, sizeof *e))
            return (int)i;
    for (i = 0; i < SND_N && snd_in[i]; i++)
        ;
    if (i == SND_N)
        return -1;
    snd_tab[i] = *e;
    snd_in[i] = 1;
    return (int)i;
}
static int lat_intern(const lat_ent_t *e)
{
    uint32_t i;
    if (!e->mask)
        return 0;
    for (i = 1; i <= LAT_N; i++)
        if (lat_in[i] && !memcmp(&lat_tab[i], e, sizeof *e))
            return (int)i;
    for (i = 1; i <= LAT_N && lat_in[i]; i++)
        ;
    if (i > LAT_N)
        return -1;
    lat_tab[i] = *e;
    lat_in[i] = 1;
    return (int)i;
}
static int bln_intern(const bln_ent_t *e)
{
    uint32_t i;
    if (!e->n)
        return 0;
    for (i = 1; i <= BLN_N; i++)
        if (bln_in[i] && !memcmp(&bln_tab[i], e, sizeof *e))
            return (int)i;
    for (i = 1; i <= BLN_N && bln_in[i]; i++)
        ;
    if (i > BLN_N)
        return -1;
    bln_tab[i] = *e;
    bln_in[i] = 1;
    return (int)i;
}
static int take_intern(uint32_t l, const loop_trk_t *t)
{
    uint32_t i;
    for (i = 0; i < NTAKE; i++)
        if (take_in[l][i] && take_same(t, &take_tab[l][i]))
            return (int)i;
    for (i = 0; i < NTAKE && take_in[l][i]; i++)
        ;
    if (i == NTAKE)
        return -1;
    memset(&take_tab[l][i], 0, sizeof take_tab[l][i]);
    take_tab[l][i].first = t->first;
    take_tab[l][i].n = t->n;
    take_tab[l][i].len = t->len;
    take_tab[l][i].xb = t->xb;
    take_in[l][i] = 1;
    return (int)i;
}

/* -------------------------------------------------------- the state --- */
/* the machine now into s; with the tables (intern), else what glides only (a morph's start). 0: a table full */
static int snap_capture(snap_t *s, int intern)
{
    uint32_t l, k;
    memset(s, 0, sizeof *s);
    for (k = 0; k < 4u; k++) {
        s->rvp[k] = ui.rvp[k];
        s->dlp[k] = ui.dlp[k];
    }
    s->scale = ui.scale;
    s->root = ui.root;
    s->lx = ui.lx;
    s->rx_on = rx.on;
    s->rx_mode = rx.mode;
    s->rx_mask = rx.mask;
    memcpy(s->rx_k, rx.k, 4u);
    memcpy(s->rx_sk, rx.sk, 4u);
    memcpy(s->rx_ak, rx.ak, 6u);
    for (l = 0; l < NLAYER; l++) {
        snap_trk_t *t = &s->t[l];
        const looper_t *lp = &loop[l];
        t->engine = ui.engine[l];
        t->path = ui.path[l];
        t->chord = (uint8_t)(ui.chord_on[l] << 7 | ui.chord_form[l]);
        t->loop = (uint8_t)((lp->play ? 1u : 0u) | lp->dmode << 1 | lp->rate << 3 | lp->vary << 5);
        t->mute = lp->mute;
        t->gseed = gran[l].seed;
        t->mod = lay_mod[l];
        t->oct = ui.oct[l];
        t->lxt = ui.lxt[l];
        t->level = ui.level[l];
        t->pan = ui.pan[l];
        t->send = ui.send[l];
        t->dsend = ui.dsend[l];
        t->flt = ui.flt[l];
        t->gr = ui.gr[l];
        t->cr = ui.cr[l];
        t->dist = ui.dist[l];
        t->bb = (uint8_t)(bb.on[l] | bb.run[l] << 1);
        t->bb_n = (uint8_t)bb.n[l];
        t->bb_speed = bb.speed[l];
        t->bb_base = bb.base[l];
        t->bb_unst = bb.unst[l];
        t->bl = (uint8_t)(bl.on[l] | bl.run[l] << 1);
        t->bl_len = bl.len[l];
        t->bl_dir = bl.dir[l];
        t->bl_spd = bl.spd[l];
        t->bl_rate = bl.rate[l];
        t->bl_harm = bl.harm[l];
        t->bl_unst = bl.unst[l];
        if (!intern)
            continue;
        for (k = 0; k < NENG; k++) {             /* every engine's sound, its macros and envelope */
            snd_ent_t e;
            int i;
            memset(&e, 0, sizeof e);
            if (snd_raw[l] >> k & 1u)
                snd_ready(l, k);                 /* (its level, the first time: a moment) */
            sound_rec(l, k, &e.seed, &e.word, &e.kind);
            e.env_on = ui.env_on[l][k];
            memcpy(e.mac, ui.mac[l][k], NMAC);
            memcpy(e.env, ui.env[l][k], 4u);
            if ((i = snd_intern(&e)) < 0)
                return 0;
            t->snd[k] = (uint8_t)i;
        }
        {   /* the latched notes and chords */
            lat_ent_t e;
            int i;
            memset(&e, 0, sizeof e);
            e.mask = ui.latch[l];
            for (k = 0; k < NKEY; k++)
                if ((e.mask >> k) & 1u) {
                    e.note[k] = ui.latch_note[l][k];
                    e.form[k] = ui.latch_form[l][k];
                }
            if ((i = lat_intern(&e)) < 0)
                return 0;
            t->lat = (uint8_t)i;
        }
        {   /* BRICKLAYER's notes */
            bln_ent_t e;
            int i;
            memset(&e, 0, sizeof e);
            e.n = bl.n[l];
            memcpy(e.w, bl.w[l], e.n);
            memcpy(e.o, bl.o[l], e.n);
            if ((i = bln_intern(&e)) < 0)
                return 0;
            t->bln = (uint8_t)i;
        }
        t->ntrk = lp->ntrk;                      /* the loops: each a take, where it stands */
        for (k = 0; k < lp->ntrk; k++) {
            const loop_trk_t *tr = &lp->trk[k];
            int i = take_intern(l, tr);
            if (i < 0)
                return 0;
            t->take[k] = (uint8_t)i;
            t->ph[k] = (uint16_t)frac16((uint32_t)clampi(tr->pos, 0, (int32_t)tr->len - 1), tr->len);
        }
    }
    return 1;
}
/* (read from a session: each value in its range, each reference in its table) */
static void snap_check(snap_t *s)
{
    uint32_t l, k;
    for (k = 0; k < 4u; k++) {
        s->rvp[k] = (int8_t)clampi(s->rvp[k], 0, 100);
        s->dlp[k] = (int8_t)clampi(s->dlp[k], 0, 100);
        s->rx_k[k] = (int8_t)clampi(s->rx_k[k], 0, 100);
        s->rx_sk[k] = (int8_t)clampi(s->rx_sk[k], 0, 100);
    }
    for (k = 0; k < 6u; k++)
        s->rx_ak[k] = (int8_t)clampi(s->rx_ak[k], 0, 100);
    s->rx_on = (uint8_t)(s->rx_on != 0);
    s->rx_mode = (uint8_t)(s->rx_mode < RXM_N ? s->rx_mode : 0u);
    s->rx_mask &= (uint8_t)((1u << NLAYER) - 1u);
    s->scale = (uint8_t)(s->scale < NSCALE ? s->scale : 0u);
    s->root %= 12u;
    s->lx = (int8_t)clampi(s->lx, -14, 14);
    for (l = 0; l < NLAYER; l++) {
        snap_trk_t *t = &s->t[l];
        t->engine = (uint8_t)(t->engine < NENG ? t->engine : 0u);
        t->path = (uint8_t)(t->path < PATH_N ? t->path : PATH_OPEN);
        t->chord = (uint8_t)((t->chord & 0x80u) | (t->chord & 0x7Fu) % CH_N);
        if ((t->loop >> 1 & 3u) > LD_RAND || (t->loop >> 3 & 3u) > LS_DOUBLE)
            t->loop &= 1u;
        t->loop &= 0x3Fu;
        t->mute &= 0x3FFu;
        t->lat = (uint8_t)(t->lat <= LAT_N ? t->lat : 0u);
        t->bln = (uint8_t)(t->bln <= BLN_N ? t->bln : 0u);
        for (k = 0; k < NENG; k++)
            t->snd[k] = (uint8_t)(t->snd[k] < SND_N ? t->snd[k] : 0u);
        t->ntrk = (uint8_t)(t->ntrk <= LOOP_TRK ? t->ntrk : 0u);
        for (k = 0; k < t->ntrk; k++)
            if (t->take[k] >= NTAKE)
                t->ntrk = 0;
        for (k = 0; k < 2u; k++)
            if (t->mod.lfo[k].shape >= MS_N || t->mod.lfo[k].rate > 100u || t->mod.lfo[k].mode >= MM_N || t->mod.lfo[k].fade > 100u)
                memset(&t->mod.lfo[k], 0, sizeof t->mod.lfo[k]);
        for (k = 0; k < NMSLOT; k++)
            if (t->mod.s[k].src >= MSRC_N || t->mod.s[k].dst >= MD_N || t->mod.s[k].amt < -100 || t->mod.s[k].amt > 100)
                memset(&t->mod.s[k], 0, sizeof t->mod.s[k]);
        t->oct = (int8_t)clampi(t->oct, -3, 3);
        t->lxt = (int8_t)clampi(t->lxt, -14, 14);
        t->level = (int8_t)clampi(t->level, 0, LEVEL_MAX);
        t->pan = (int8_t)clampi(t->pan, -PAN_MAX, PAN_MAX);
        t->send = (int8_t)clampi(t->send, 0, 100);
        t->dsend = (int8_t)clampi(t->dsend, 0, 100);
        t->flt = (int8_t)clampi(t->flt, -FLT_MAX, FLT_MAX);
        t->gr = (int8_t)clampi(t->gr, -FLT_MAX, FLT_MAX);
        t->cr = (int8_t)clampi(t->cr, -FLT_MAX, FLT_MAX);
        t->dist = (int8_t)clampi(t->dist, 0, 100);
        t->bb &= 3u;
        t->bb_n = (uint8_t)clampi(t->bb_n, 1, BB_MAX);
        t->bb_speed = (int8_t)clampi(t->bb_speed, 0, 100);
        t->bb_base = (int8_t)clampi(t->bb_base, BB_LO, BB_HI);
        t->bb_unst = (int8_t)clampi(t->bb_unst, 0, 100);
        t->bl &= 3u;
        if ((t->bb & 1u) && (t->bl & 1u))
            t->bl = 0;                           /* (one sequencer a track) */
        t->bl_len = (uint8_t)clampi(t->bl_len, 0, BL_MAX);
        t->bl_dir = (uint8_t)(t->bl_dir <= LD_RAND ? t->bl_dir : LD_FWD);
        t->bl_spd = (uint8_t)(t->bl_spd <= LS_DOUBLE ? t->bl_spd : LS_NORMAL);
        t->bl_rate = (int8_t)clampi(t->bl_rate, 0, 100);
        t->bl_harm = (int8_t)clampi(t->bl_harm, 0, 100);
        t->bl_unst = (int8_t)clampi(t->bl_unst, 0, 100);
    }
}
/* (the tables as read from a session: each value in its range) */
static void snap_tab_check(void)
{
    uint32_t i, k, l;
    for (i = 0; i < SND_N; i++) {
        snd_tab[i].env_on = (uint8_t)(snd_tab[i].env_on != 0);
        for (k = 0; k < NMAC; k++)
            snd_tab[i].mac[k] = (int8_t)clampi(snd_tab[i].mac[k], -MAC_MAX, MAC_MAX);
        for (k = 0; k < 4u; k++)
            snd_tab[i].env[k] = (int8_t)clampi(snd_tab[i].env[k], 0, 100);
    }
    memset(&lat_tab[0], 0, sizeof lat_tab[0]);
    for (i = 1; i <= LAT_N; i++) {
        lat_tab[i].mask &= (1u << NKEY) - 1u;
        for (k = 0; k < NKEY; k++) {
            lat_tab[i].note[k] &= 0x7Fu;
            lat_tab[i].form[k] = (uint8_t)(lat_tab[i].form[k] <= CH_N ? lat_tab[i].form[k] : 0u);
        }
    }
    memset(&bln_tab[0], 0, sizeof bln_tab[0]);
    for (i = 1; i <= BLN_N; i++) {
        bln_tab[i].n = (uint8_t)(bln_tab[i].n <= BL_MAX ? bln_tab[i].n : 0u);
        for (k = 0; k < BL_MAX; k++) {
            bln_tab[i].w[k] = (int8_t)clampi(bln_tab[i].w[k], -32, 48);
            bln_tab[i].o[k] = (int8_t)clampi(bln_tab[i].o[k], -3, 3);
        }
    }
    for (l = 0; l < NLAYER; l++)
        for (i = 0; i < NTAKE; i++)
            if ((uint32_t)take_tab[l][i].first + take_tab[l][i].n > loop[l].nev || !take_tab[l][i].len)
                memset(&take_tab[l][i], 0, sizeof take_tab[l][i]);
}

/* track l's sound on engine e as the table's entry: drawn again from its seed if it is another (published as a new
 * sound is), its macros and envelope. 0: the ISR still holds the other buffer (tried again) */
static int snap_sound(uint32_t l, uint32_t e, const snd_ent_t *s)
{
    uint32_t seed, word, path = s->word >> 24, gen = s->word >> 20 & 15u;
    uint8_t kind;
    sound_rec(l, e, &seed, &word, &kind);
    if ((seed != s->seed || word != s->word || kind != s->kind) &&
        !sound_from_seed(l, e, s->seed, path < PATH_N ? path : PATH_OPEN, gen < GEN_N ? gen : 0u, (int32_t)(s->word & 0xFFFFFu),
                         (int32_t)s->kind))
        return 0;
    memcpy(ui.mac[l][e], s->mac, NMAC);
    memcpy(ui.env[l][e], s->env, 4u);
    ui.env_on[l][e] = s->env_on;
    if (ui.engine[l] == e)
        mac_apply(l);
    return 1;
}
/* track l to the engine the snapshot has it play (its sound given first) */
static void snap_engine(uint32_t l, uint32_t e)
{
    if (ui.engine[l] == e)
        return;
    ui.engine[l] = (uint8_t)e;
    ev_post(EV_ENGINE, l, 0, e);
    mac_apply(l);
}
/* is track l's sound (engine and the sound it plays) the snapshot's? */
static int snap_same_sound(uint32_t l, const snap_trk_t *t)
{
    const snd_ent_t *s = &snd_tab[t->snd[t->engine]];
    uint32_t seed, word;
    uint8_t kind;
    if (ui.engine[l] != t->engine)
        return 0;
    sound_rec(l, t->engine, &seed, &word, &kind);
    return seed == s->seed && word == s->word && kind == s->kind;
}
/* track l's latched notes and chords as the entry's: those that stay sound on, the others let go or struck */
static void snap_latches(uint32_t l, const lat_ent_t *e)
{
    uint32_t k;
    for (k = 0; k < NKEY; k++) {
        int had = (ui.latch[l] >> k) & 1u, want = (e->mask >> k) & 1u;
        if (had && want && ui.latch_note[l][k] == e->note[k] && ui.latch_form[l][k] == e->form[k])
            continue;
        if (had) {
            chord_press(l, ui.latch_note[l][k], ui.latch_form[l][k], 0);
            ui.latch[l] &= ~(1u << k);
            ui.latch_form[l][k] = 0;
        }
        if (want) {
            ui.latch_note[l][k] = e->note[k];
            ui.latch_form[l][k] = e->form[k];
            chord_press(l, e->note[k], e->form[k], 1);
            latch_mark(l, e->note[k], e->form[k]);
            ui.latch[l] |= 1u << k;
        }
    }
}
/* track l's loops as the snapshot's (layers.c loop_takes) */
static void snap_loops(uint32_t l, const snap_trk_t *t)
{
    uint32_t k;
    memset(&lts, 0, sizeof lts);
    lts.ntrk = t->ntrk;
    lts.play = (uint8_t)(t->loop & 1u);
    lts.mute = t->mute;
    for (k = 0; k < t->ntrk; k++) {
        lts.t[k] = take_tab[l][t->take[k]];
        lts.ph[k] = t->ph[k];
    }
    ev_post(EV_LOOP_TAKES, l, 0, 0);
    ses_events_drain();                          /* (lts is the next one's: one at a time) */
    snap.loop_dirty |= (uint8_t)(1u << l);
}

/* what has no in-between, at once: the pitch (scale, root, transpositions, octaves), the delay's TIME and the
 * reverb's SIZE, the loops, the latched notes, the sequencers and their notes, chord mode, the modulation's routes
 * and shapes, the granulator's character, PLAY ALONG; the sound each track plays of the tracks in mask (the others,
 * and the engines not played, after: one a frame) */
static void snap_discrete(const snap_t *b, uint32_t mask)
{
    uint32_t l, k, moved = 0;
    if (ui.lx != b->lx) {                        /* (the latched notes move with the loops, as SHIFT + OCT moves them) */
        for (l = 0; l < NLAYER; l++)
            latch_move(l, b->lx - ui.lx);
        ui.lx = b->lx;
        moved = 1;
    }
    for (l = 0; l < NLAYER; l++)
        if (ui.lxt[l] != b->t[l].lxt) {
            latch_move(l, b->t[l].lxt - ui.lxt[l]);
            ui.lxt[l] = b->t[l].lxt;
            moved = 1;
        }
    if (ui.scale != b->scale || ui.root != b->root) {
        ui.scale = b->scale;
        ui.root = b->root;
        moved = 1;
    }
    if (moved)
        lx_post();                               /* (the loops moved walk the scale) */
    ui.rvp[0] = b->rvp[0];
    ui.dlp[0] = b->dlp[0];
    for (l = 0; l < NLAYER; l++) {               /* the sequencers off first: one a track */
        if (bb.on[l] && !(b->t[l].bb & 1u))
            bb_toggle(l);
        if (bl.on[l] && !(b->t[l].bl & 1u))
            bl_toggle(l);
    }
    for (l = 0; l < NLAYER; l++) {
        const snap_trk_t *t = &b->t[l];
        const bln_ent_t *bn = &bln_tab[t->bln];
        looper_t *lp = &loop[l];
        ui.oct[l] = t->oct;
        ui.path[l] = t->path;
        ui.chord_on[l] = (uint8_t)(t->chord >> 7);
        ui.chord_form[l] = (uint8_t)(t->chord & 0x7Fu);
        if (gran[l].seed != t->gseed)
            gran_profile(l, t->gseed);
        for (k = 0; k < 2u; k++) {
            lay_mod[l].lfo[k].shape = t->mod.lfo[k].shape;
            lay_mod[l].lfo[k].mode = t->mod.lfo[k].mode;
        }
        for (k = 0; k < NMSLOT; k++) {
            lay_mod[l].s[k].src = t->mod.s[k].src;
            lay_mod[l].s[k].dst = t->mod.s[k].dst;
        }
        if (!bb.on[l] && (t->bb & 1u))
            bb_toggle(l);
        if (bb.on[l])
            for (k = (uint32_t)bb.n[l]; k < t->bb_n; k++)
                bb_launch(l, k);                 /* (more balls: the new ones from the middle) */
        bb.n[l] = (int8_t)t->bb_n;
        bb.base[l] = t->bb_base;
        if (bb.run[l] != (t->bb >> 1 & 1u))
            bb_set_run(l, t->bb >> 1 & 1u);
        if (bl.n[l] != bn->n || memcmp(bl.w[l], bn->w, bn->n) || memcmp(bl.o[l], bn->o, bn->n)) {
            bl.n[l] = bn->n;                     /* (its notes: another pattern) */
            memcpy(bl.w[l], bn->w, BL_MAX);
            memcpy(bl.o[l], bn->o, BL_MAX);
            if (bl.pos[l] >= bl.n[l])
                bl.pos[l] = 0;
        }
        bl.rec[l] = 0;
        if (!bl.on[l] && (t->bl & 1u) && !bb.on[l])
            bl_toggle(l);
        bl.len[l] = (uint8_t)clampi(t->bl_len, bl.n[l], BL_MAX);
        bl.dir[l] = t->bl_dir;
        bl.spd[l] = t->bl_spd;
        if (bl.run[l] != (t->bl >> 1 & 1u))      /* (playing on: not from its first step again) */
            bl_set_run(l, t->bl >> 1 & 1u);
        snap_latches(l, &lat_tab[t->lat]);
        snap_loops(l, t);                        /* the loops: which, in or out, playing or not */
        if (lp->dmode != (t->loop >> 1 & 3u))
            ev_post(EV_LOOP_MODE, l, 0, t->loop >> 1 & 3u);
        if (lp->rate != (t->loop >> 3 & 3u))
            ev_post(EV_LOOP_MODE, l, 1, t->loop >> 3 & 3u);
        if (lp->vary != (t->loop >> 5 & 1u))
            ev_post(EV_LOOP_MODE, l, 2, t->loop >> 5 & 1u);
        snap_pend[l] = (uint8_t)((1u << NENG) - 1u);   /* the sounds: every engine's, the one it plays first */
        if ((mask >> l & 1u) && snap_sound(l, t->engine, &snd_tab[t->snd[t->engine]])) {
            snap_pend[l] &= (uint8_t)~(1u << t->engine);
            snap_engine(l, t->engine);
        } else if (!(mask >> l & 1u)) {          /* (the same sound: whether its envelope is its own) */
            ui.env_on[l][ui.engine[l]] = snd_tab[t->snd[ui.engine[l]]].env_on;
            snap_pend[l] &= (uint8_t)~(1u << ui.engine[l]);
            mac_apply(l);
        }
    }
    rx_mode_set(b->rx_mode);                     /* PLAY ALONG: its mode, on or off, the tracks taken in, its pages' values
                                                  * (after: rx_set gives empty pages their first values) */
    if (rx.on != b->rx_on)
        rx_set(b->rx_on);
    rx.mask = b->rx_mask;
    memcpy(rx.k, b->rx_k, 4u);
    memcpy(rx.sk, b->rx_sk, 4u);
    memcpy(rx.ak, b->rx_ak, 6u);
    ses_events_drain();                          /* (taken before anything reads the loops again) */
    ui.force = 1;
}

/* the in-between at u (Q15, 0 .. 1) from a to b: what glides. The macros and envelope of the sound a track plays
 * from morph_ma, morph_ea (the tracks in skip: given with their new sound) */
static int8_t morph_ma[NLAYER][NMAC], morph_ea[NLAYER][4];
static int32_t snap_mix(int32_t a, int32_t b, int32_t u) { return a + (b - a) * u / Q15; }
static void snap_lerp(const snap_t *a, const snap_t *b, int32_t u, uint32_t skip)
{
    uint32_t l, k;
    int32_t mid = u >= Q15 / 2;
    for (k = 1; k < 4u; k++) {
        ui.rvp[k] = (int8_t)snap_mix(a->rvp[k], b->rvp[k], u);
        ui.dlp[k] = (int8_t)snap_mix(a->dlp[k], b->dlp[k], u);
    }
    rv_param(ui.rvp[0], ui.rvp[1], ui.rvp[2], ui.rvp[3]);
    dl_param(ui.dlp[0], ui.dlp[1], ui.dlp[2], ui.dlp[3]);
    for (l = 0; l < NLAYER; l++) {
        const snap_trk_t *ta = &a->t[l], *tb = &b->t[l];
        int32_t lv = ta->level * 256 + (((tb->level - ta->level) * 256 * u) >> 15);
        ui.level[l] = (int8_t)((lv + 128) >> 8);
        ui.pan[l] = (int8_t)snap_mix(ta->pan, tb->pan, u);
        ui.send[l] = (int8_t)snap_mix(ta->send, tb->send, u);
        ui.dsend[l] = (int8_t)snap_mix(ta->dsend, tb->dsend, u);
        ui.flt[l] = (int8_t)snap_mix(ta->flt, tb->flt, u);
        ui.gr[l] = (int8_t)snap_mix(ta->gr, tb->gr, u);
        ui.cr[l] = (int8_t)snap_mix(ta->cr, tb->cr, u);
        ui.dist[l] = (int8_t)snap_mix(ta->dist, tb->dist, u);
        level_pan_apply(l);
        lay_gain[l] = level_gain(lv);            /* (the level between its steps) */
        for (k = 0; k < 2u; k++) {
            lay_mod[l].lfo[k].rate = (uint8_t)snap_mix(ta->mod.lfo[k].rate, tb->mod.lfo[k].rate, u);
            lay_mod[l].lfo[k].fade = (uint8_t)snap_mix(ta->mod.lfo[k].fade, tb->mod.lfo[k].fade, u);
        }
        for (k = 0; k < NMSLOT; k++) {           /* a route that stays: its amount glides; another: out, then in */
            const mslot_t *sa = &ta->mod.s[k], *sb = &tb->mod.s[k];
            if (sa->src == sb->src && sa->dst == sb->dst)
                lay_mod[l].s[k].amt = (int8_t)snap_mix(sa->amt, sb->amt, u);
            else
                lay_mod[l].s[k].amt = (int8_t)(mid ? snap_mix(0, sb->amt, 2 * u - Q15) : snap_mix(sa->amt, 0, 2 * u));
        }
        bb.speed[l] = (int8_t)snap_mix(ta->bb_speed, tb->bb_speed, u);
        bb.unst[l] = (int8_t)snap_mix(ta->bb_unst, tb->bb_unst, u);
        bl.rate[l] = (int8_t)snap_mix(ta->bl_rate, tb->bl_rate, u);
        bl.harm[l] = (int8_t)snap_mix(ta->bl_harm, tb->bl_harm, u);
        bl.unst[l] = (int8_t)snap_mix(ta->bl_unst, tb->bl_unst, u);
        if (!(skip >> l & 1u) && ui.engine[l] == tb->engine) {   /* (the same sound: its macros and envelope glide) */
            const snd_ent_t *s = &snd_tab[tb->snd[tb->engine]];
            uint32_t e = tb->engine;
            for (k = 0; k < NMAC; k++)
                ui.mac[l][e][k] = (int8_t)snap_mix(morph_ma[l][k], s->mac[k], u);
            for (k = 0; k < 4u; k++)
                ui.env[l][e][k] = (int8_t)snap_mix(morph_ea[l][k], s->env[k], u);
            mac_apply(l);
        }
    }
}

/* ------------------------------------------------------------- the morph --- */
static snap_t morph_a POOL;      /* where it came from (the machine when recalled: what glides) */
static int snap_morph_target(void) { return morph.on ? morph.to : -1; }
static void morph_end(void)
{
    uint32_t l;
    morph.on = 0;
    for (l = 0; l < NLAYER; l++)
        lay_duck[l] = 0;
}
/* a looper's events no snapshot holds any more (nor its loops now), packed away: one track a call */
static void loop_gc(void)
{
    uint32_t l, i, k, keep = 0, n = 0;
    uint16_t from[PK_N], len[PK_N];
    for (l = 0; l < NLAYER && !((snap.loop_dirty >> l) & 1u); l++)
        ;
    if (l == NLAYER || loop[l].rec != LR_IDLE)
        return;                                  /* (not while it records: later) */
    snap_mark(-1);
    for (k = 0; k < loop[l].ntrk; k++)           /* what is held: the loops now, the takes the snapshots hold */
        if (loop[l].trk[k].n && n < PK_N) {
            from[n] = loop[l].trk[k].first;
            len[n++] = loop[l].trk[k].n;
        }
    for (i = 0; i < NTAKE; i++)
        if (take_in[l][i] && take_tab[l][i].n && n < PK_N) {
            from[n] = take_tab[l][i].first;
            len[n++] = take_tab[l][i].n;
        }
    for (i = 1; i < n; i++)                      /* in order */
        for (k = i; k > 0 && from[k - 1] > from[k]; k--) {
            uint16_t f = from[k], g = len[k];
            from[k] = from[k - 1], len[k] = len[k - 1];
            from[k - 1] = f, len[k - 1] = g;
        }
    lpk.n = 0;
    for (i = 0; i < n; i++) {                    /* (the same ones, or overlapping: once) */
        if (lpk.n && from[i] < (uint32_t)lpk.from[lpk.n - 1u] + lpk.len[lpk.n - 1u]) {
            uint32_t end = (uint32_t)from[i] + len[i], e0 = (uint32_t)lpk.from[lpk.n - 1u] + lpk.len[lpk.n - 1u];
            if (end > e0)
                lpk.len[lpk.n - 1u] = (uint16_t)(end - lpk.from[lpk.n - 1u]);
            continue;
        }
        lpk.from[lpk.n] = from[i];
        lpk.len[lpk.n++] = len[i];
    }
    for (i = 0; i < lpk.n; i++)
        keep += lpk.len[i];
    if (keep != loop[l].nev) {
        lpk_ok = 0;
        ev_post(EV_LOOP_PACK, l, 0, 0);
        ses_events_drain();
        if (!lpk_ok)
            return;                              /* (it began recording meanwhile: later) */
        for (i = 0; i < NTAKE; i++)
            if (take_in[l][i])
                take_tab[l][i].first = (uint16_t)pk_map(take_tab[l][i].first, take_tab[l][i].n);
    }
    for (i = 0; i < NTAKE; i++)                  /* (the takes no one holds: forgotten) */
        if (!take_in[l][i])
            memset(&take_tab[l][i], 0, sizeof take_tab[l][i]);
    snap.loop_dirty &= (uint8_t)~(1u << l);
}
/* every looper packed (a session saved) */
static void loop_gc_all(void)
{
    uint32_t n;
    snap.loop_dirty = (uint8_t)((1u << NLAYER) - 1u);
    for (n = 0; n < 4u * NLAYER && snap.loop_dirty; n++)
        loop_gc();
}
/* the main loop, every frame: the morph a step on; a sound still to be drawn again, one; a looper packed */
static void morph_poll(void)
{
    uint32_t el, l, e;
    int32_t u;
    if (!morph.on || morph.mid) {
        for (l = 0; l < NLAYER; l++)             /* (the sounds the tracks play first) */
            if ((snap_pend[l] >> morph_b.t[l].engine) & 1u) {
                e = morph_b.t[l].engine;
                if (snap_sound(l, e, &snd_tab[morph_b.t[l].snd[e]])) {
                    snap_pend[l] &= (uint8_t)~(1u << e);
                    snap_engine(l, e);
                }
                break;
            }
        if (l == NLAYER)
            for (l = 0; l < NLAYER; l++)
                if (snap_pend[l]) {
                    for (e = 0; e < NENG && !((snap_pend[l] >> e) & 1u); e++)
                        ;
                    if (snap_sound(l, e, &snd_tab[morph_b.t[l].snd[e]]))
                        snap_pend[l] &= (uint8_t)~(1u << e);
                    break;
                }
    }
    loop_gc();
    if (!morph.on)
        return;
    el = fm1_ms - morph.t0;
    u = el >= morph.dur ? Q15 : (int32_t)((el << 15) / morph.dur);
    if (!morph.mid && u >= Q15 / 2) {
        morph.mid = 1;
        snap_discrete(&morph_b, morph.chg);
    }
    snap_lerp(&morph_a, &morph_b, u, morph.chg);
    for (l = 0; l < NLAYER; l++)                 /* a new sound: down from 1/4 to silence at 1/2, up by 3/4 */
        if (morph.chg >> l & 1u) {
            int32_t d = u < Q15 / 4 || u >= Q15 * 3 / 4 ? 0 : u < Q15 / 2 ? (u - Q15 / 4) * 4 : (Q15 * 3 / 4 - u) * 4;
            lay_duck[l] = (int16_t)clampi(d, 0, Q15 - 1);
        }
    if (u >= Q15) {                              /* the end: the snapshot exactly */
        morph_end();
        for (l = 0; l < NLAYER; l++) {
            const snd_ent_t *s = &snd_tab[morph_b.t[l].snd[morph_b.t[l].engine]];
            memcpy(morph_ma[l], s->mac, NMAC);
            memcpy(morph_ea[l], s->env, 4u);
        }
        snap_lerp(&morph_b, &morph_b, Q15, 0);
        snap.cur = (int8_t)morph.to;
        ui.force = 1;
    }
}

/* ------------------------------------------------------- the panel's --- */
/* the machine now into snapshot i; 0: no room (a table full) */
static int snap_store(uint32_t i)
{
    snap_t s;
    if (i >= NSNAP)
        return 0;
    snap_mark((int)i);                           /* (what it held before: free to take, unless another holds it) */
    if (!snap_capture(&s, 1))
        return 0;
    snap_s[i] = s;
    snap.used |= (uint16_t)(1u << i);
    snap.cur = (int8_t)i;
    snap.loop_dirty = (uint8_t)((1u << NLAYER) - 1u);
    return 1;
}
/* snapshot i recalled: over the morph time (half seconds), or at once */
static void snap_recall(uint32_t i, uint32_t morph_half_s)
{
    uint32_t l;
    if (i >= NSNAP || !((snap.used >> i) & 1u))
        return;
    morph_end();
    memset(snap_pend, 0, sizeof snap_pend);
    morph_b = snap_s[i];                         /* (a copy: stored over on the way, it still goes where it went) */
    for (l = 0; l < NLAYER; l++) {
        memcpy(morph_ma[l], ui.mac[l][ui.engine[l]], NMAC);
        memcpy(morph_ea[l], ui.env[l][ui.engine[l]], 4u);
    }
    if (!morph_half_s) {
        snap_discrete(&morph_b, 0x3Fu);
        for (l = 0; l < NLAYER; l++) {
            const snd_ent_t *s = &snd_tab[morph_b.t[l].snd[morph_b.t[l].engine]];
            memcpy(morph_ma[l], s->mac, NMAC);
            memcpy(morph_ea[l], s->env, 4u);
        }
        snap_lerp(&morph_b, &morph_b, Q15, 0);
        snap.cur = (int8_t)i;
        return;
    }
    snap_capture(&morph_a, 0);                   /* (from where it is now) */
    morph.chg = 0;
    for (l = 0; l < NLAYER; l++)
        if (!snap_same_sound(l, &morph_b.t[l]))
            morph.chg |= (uint8_t)(1u << l);
    morph.on = 1;
    morph.mid = 0;
    morph.to = (uint8_t)i;
    morph.t0 = fm1_ms;
    morph.dur = morph_half_s * 500u;
}
static void snap_clear(uint32_t i)
{
    if (i >= NSNAP)
        return;
    snap.used &= (uint16_t)~(1u << i);
    if (snap.cur == (int8_t)i)
        snap.cur = -1;
    snap.loop_dirty = (uint8_t)((1u << NLAYER) - 1u);
}
static int snap_has(uint32_t i) { return i < NSNAP && ((snap.used >> i) & 1u); }
static int snap_current(void) { return snap.cur; }
/* a session loaded or begun: none */
static void snap_reset(void)
{
    morph_end();
    memset(snap_pend, 0, sizeof snap_pend);
    snap.used = 0;
    snap.cur = -1;
    snap.loop_dirty = 0;
    memset(take_tab, 0, sizeof take_tab);
}
/* the looper of track l emptied (REC held): its events stay while snapshots hold them, packed away later */
static void snap_loop_emptied(uint32_t l)
{
    snap.loop_dirty |= (uint8_t)(1u << l);
}

/* ------------------------------------------------------ the session's --- */
static void ses_out(const void *src, uint32_t n);   /* (sessions.c) */
static void ses_in(void *dst, uint32_t n);
/* the end of the events track l's looper keeps: its loops now, the snapshots' (a loop under way not) */
static uint32_t loop_kept_end(uint32_t l)
{
    uint32_t k, end = 0;
    snap_mark(-1);
    for (k = 0; k < loop[l].ntrk; k++)
        if ((uint32_t)loop[l].trk[k].first + loop[l].trk[k].n > end)
            end = (uint32_t)loop[l].trk[k].first + loop[l].trk[k].n;
    for (k = 0; k < NTAKE; k++)
        if (take_in[l][k] && (uint32_t)take_tab[l][k].first + take_tab[l][k].n > end)
            end = (uint32_t)take_tab[l][k].first + take_tab[l][k].n;
    return end;
}
/* the snapshots into the session (format 27): which hold a state, the morph time, the one last stored or recalled,
 * the sizes of what follows; then, if any, the tables' entries they hold (each with its place), each track's takes,
 * and each snapshot */
static void snap_emit(void)
{
    uint16_t w[8];
    uint32_t i, l;
    uint8_t c;
    w[0] = snap.used;
    w[1] = ui.morph;
    w[2] = (uint16_t)(snap.cur + 1);
    w[3] = (uint16_t)sizeof(snap_t);
    w[4] = (uint16_t)sizeof(snd_ent_t);
    w[5] = (uint16_t)sizeof(lat_ent_t);
    w[6] = (uint16_t)sizeof(bln_ent_t);
    w[7] = (uint16_t)sizeof(take_t);
    ses_out(w, sizeof w);
    if (!snap.used)
        return;
    snap_mark(-1);
#define SNAP_OUT_TAB(in, tab, n0, n1)                                       \
    do {                                                                    \
        for (c = 0, i = n0; i < n1; i++)                                    \
            c = (uint8_t)(c + (in[i] != 0));                                \
        ses_out(&c, 1u);                                                    \
        for (i = n0; i < n1; i++)                                           \
            if (in[i]) {                                                    \
                c = (uint8_t)i;                                             \
                ses_out(&c, 1u);                                            \
                ses_out(&tab[i], sizeof tab[i]);                            \
            }                                                               \
    } while (0)
    SNAP_OUT_TAB(snd_in, snd_tab, 0u, SND_N);
    SNAP_OUT_TAB(lat_in, lat_tab, 1u, LAT_N + 1u);
    SNAP_OUT_TAB(bln_in, bln_tab, 1u, BLN_N + 1u);
    for (l = 0; l < NLAYER; l++)
        SNAP_OUT_TAB(take_in[l], take_tab[l], 0u, NTAKE);
#undef SNAP_OUT_TAB
    for (i = 0; i < NSNAP; i++)
        if ((snap.used >> i) & 1u)
            ses_out(&snap_s[i], sizeof(snap_t));
}
/* .. and back (sessions.c ses_load, after the loopers' events: snap_reset before) */
static void snap_load(void)
{
    uint16_t w[8];
    uint32_t i, l;
    uint8_t c, ix;
    ses_in(w, sizeof w);
    ui.morph = (uint8_t)(w[1] <= MORPH_STEPS ? w[1] : 0u);
    if (!w[0] || w[3] != sizeof(snap_t) || w[4] != sizeof(snd_ent_t) || w[5] != sizeof(lat_ent_t) ||
        w[6] != sizeof(bln_ent_t) || w[7] != sizeof(take_t))
        return;                                  /* (none, or of another layout: left out) */
#define SNAP_IN_TAB(tab, n0, n1)                                            \
    do {                                                                    \
        ses_in(&c, 1u);                                                     \
        for (i = 0; i < c; i++) {                                           \
            ses_in(&ix, 1u);                                                \
            if (ix >= n0 && ix < n1)                                        \
                ses_in(&tab[ix], sizeof tab[ix]);                           \
            else                                                            \
                ses_in(0, sizeof tab[0]);                                   \
        }                                                                   \
    } while (0)
    SNAP_IN_TAB(snd_tab, 0u, SND_N);
    SNAP_IN_TAB(lat_tab, 1u, LAT_N + 1u);
    SNAP_IN_TAB(bln_tab, 1u, BLN_N + 1u);
    for (l = 0; l < NLAYER; l++)
        SNAP_IN_TAB(take_tab[l], 0u, NTAKE);
#undef SNAP_IN_TAB
    snap_tab_check();
    for (i = 0; i < NSNAP; i++)
        if ((w[0] >> i) & 1u) {
            ses_in(&snap_s[i], sizeof(snap_t));
            snap_check(&snap_s[i]);
        }
    snap.used = w[0];
    snap.cur = (int8_t)(w[2] >= 1u && w[2] <= NSNAP && ((w[0] >> (w[2] - 1u)) & 1u) ? w[2] - 1u : -1);
}
