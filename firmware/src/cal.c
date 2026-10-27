/* SPDX-License-Identifier: GPL-3.0-only
 * TONITOR: CALIGULA, the third engine -- an organism.
 *
 * After the architecture of SOMA's LYRA-8 as Mike Moreno's LIRA-8 emulates it in Pure Data (BSD licence,
 * Copyright (C) Miguel Moreno: https://github.com/MikeMorenoDSP/LIRA-8), rebuilt in fixed point and bent
 * towards this firmware: eight oscillators in four pairs on two sides; each oscillator a triangle pushed
 * towards a square by its pair's SHARP; each with its sensor and an envelope, slow (a long swell and a longer
 * fall) or fast; each pair frequency-modulated at audio rate by a source -- its partner, the other pair of its
 * side, a pair of the other side, or the TOTAL FEEDBACK (the whole organism's output, saturated, fed back); a
 * HYPER LFO of two LFOs combined (alone, AND, OR, XOR) moving the modulation's depth and gating chosen pairs
 * into irregular rhythms; a short modulated delay (two lines, ~35 ms, swept by the hyper LFO or warped at audio
 * rate by the upper pairs) with feedback; a drive.
 *
 * Bent: the hyper LFO moves timbre (the FM's depth) and gates, never pitch -- no sirens, no laser guns; and
 * the organism is played as the other engines are: a key held, it sounds, at the key's note (its root: the
 * intervals of its oscillators are from root-centred sets); no key, it rings out and falls silent. A new key
 * while one is held retunes it there (a quick snap, not a glide); letting go returns it to a key still held.
 *
 * What a player's fingers would do on the eight touch plates, a simulated HAND does while a key is held: it
 * lands on one to three plates at once, then fingers come and go -- held for seconds or tapped, on the plates
 * it favours or anywhere, never more at once than it has fingers, never none -- as each species' hand does
 * (slow and few for SPORE and VEIL, quick and many for HIVE and THORN). And while it plays, the organism stirs:
 * the depths and the sharpness wander, quick taps come in BURSTS at irregular times, and now and then the
 * feedback and the drive SURGE for a moment. Every design is drawn by cal_design from one of six species.
 *
 * Generation 1 (0.21) adds four species -- LITANY (a slow choir of unisons beating), UNDERTOW (deep feedback and a
 * warped delay pulling it under), WIRE (high, sharp, a quick hand tapping), MOTH (soft triangles fluttering through
 * the gates) -- and four tunings (fourths, a minor seventh, open fifths, a quarter-tone cluster); and it gives
 * the hand a phrase: two to four plates it comes back to, varied now and then, often on a pulse of its own, so it
 * plays like someone rather than like chance. A pair the gates never close stays under it all.
 *
 * A layer's path leans the draw (PATH_OPEN: every species and tuning alike):
 *   AMBIENT       SPORE and VEIL most, CARRION, now and then RUST; tuned on pillars, depths and unisons; slow
 *                 swells, softer edges and drive, fewer bursts and surges, a slower hand that stays longer
 *   EXPERIMENTAL  THORN, HIVE and RUST most, CARRION; clusters and tritones; sharper, driven, bursting and
 *                 surging more, a quicker hand */

#define CAL_DLN 768u             /* each delay line: samples at FS / 2 (~35 ms), mu-law */

enum { CS_SPORE, CS_THORN, CS_HIVE, CS_CARRION, CS_VEIL, CS_RUST,
       CS_N0,                    /* (generation 0 draws these six only) */
       CS_LITANY = CS_N0, CS_UNDERTOW, CS_WIRE, CS_MOTH, CS_N };
static const char *const CS_NAME[CS_N] = {"SPORE", "THORN", "HIVE", "CARRION", "VEIL", "RUST", "LITANY", "UNDERTOW",
                                          "WIRE", "MOTH"};
enum { CSRC_PARTNER, CSRC_SIDE, CSRC_CROSS, CSRC_TOTAL };
enum { CLNK_A, CLNK_AND, CLNK_OR, CLNK_XOR };

typedef struct {
    uint32_t seed;               /* what made it: cal_design(kind, seed, path, gen) + cal_measure give it again */
    uint8_t kind;
    uint8_t src[4];              /* each pair's FM source */
    uint8_t link, lfo_sq, gate_pairs, del_src;   /* hyper LFO; its shape; the pairs it gates; the delay's mod */
    int16_t tune[8];             /* each oscillator, from the root: 1/16 semitone */
    int32_t att[4], rel[4];      /* each pair's envelope: ms */
    int32_t sharp[4], mod[4];    /* Q15 */
    int32_t lfo_idx[4];          /* how much the hyper LFO moves each pair's FM depth, Q15 */
    int32_t hold[2];             /* each side: the floor its oscillators hold at while a sensor of it is down */
    int32_t lfo_a, lfo_b;        /* Hz Q16 */
    int32_t gate_depth;          /* Q15 */
    int32_t vib;                 /* vibrato, Q15 of a semitone */
    int32_t del_t[2];            /* each line's time: samples at FS / 2 */
    int32_t del_mod, del_fb, del_mix;   /* Q15 */
    int32_t drive;               /* Q12 */
    int32_t total_fb;            /* Q15 */
    int32_t burst;               /* bursts a second while awake, Q8 */
    int32_t surge;               /* surges a minute while awake, Q8 */
    int32_t drift;               /* how far depth and sharpness wander, Q15 */
    int32_t out;                 /* Q12: the measured level brought to the target */
    /* the hand */
    int32_t touch;               /* fingers landing a second, Q8 (on average: the waits are irregular) */
    int32_t hold_lo, hold_hi;    /* how long a finger stays, ms */
    int32_t tap;                 /* the share of touches that are quick taps, Q15 */
    uint8_t fingers;             /* at most this many down at once */
    uint8_t home;                /* the plates it favours, a bit each */
    uint8_t first;               /* plates it lands on at once, 1 .. 3 */
    uint8_t path;                /* PATH_*: what it was drawn on */
    /* (generation 1 on: fields after here, so generation 0's stay where they were) */
    uint8_t gen;                 /* the generators' generation that drew it */
    uint8_t motif[4];            /* the hand's phrase: the plates it comes back to (generation 1) */
    uint8_t mlen;                /* .. how many (0: no phrase, every plate by chance) */
    uint8_t mfollow;             /* .. how often it follows it, Q8 */
    uint8_t rhythm;              /* .. its touches on a pulse (the phrase's own) rather than at random */
    uint8_t rsv1;
} cal_design_t;

typedef struct {
    uint32_t ph[8], inc[8];
    int32_t env[8];              /* Q23 (Q15 and 8 bits more: a swell of seconds moves less than 1 a sample) */
    uint16_t burst[8];           /* blocks of a burst left */
    uint8_t gate[8];             /* the plates the hand holds down */
    uint16_t ftime[8];           /* blocks each finger stays (0xFFFF: until the key is let go) */
    uint8_t ftap[8];             /* that finger is a quick tap (a fast envelope) */
    uint8_t note[8], nsrc[8], nheld;   /* the keys held (the last on top), and who holds them: 1 keys, 2 loop */
    uint32_t touch_wait;         /* blocks to the next finger */
    int32_t osc[8];              /* each oscillator's last output (Q15) */
    int32_t pair[4];             /* each pair's last output */
    int32_t fb;                  /* the total feedback: the output, saturated, low-passed */
    uint32_t la, lb, vph;        /* the hyper LFO's two, the vibrato */
    int32_t gate_s;              /* the gate, slewed */
    int32_t wd[4], ws[4];        /* the wanderings of depth and sharpness, Q15 */
    int32_t wd_t[4], ws_t[4];
    uint32_t awake;              /* blocks it stays awake */
    uint32_t surge_left;
    int32_t base, base_t;        /* the note it sounds (MIDI note x 16), and where it is going (a quick snap) */
    uint8_t dl[2][CAL_DLN];
    uint32_t dw;
    int32_t dhalf[2], dlast[2];
    uint8_t odd, live;
    uint8_t hush;                /* overload (layers.c): its tail, no key on it, let go in ~30 ms */
    uint8_t mstep;               /* where the hand is in its phrase */
    int8_t mac[NMAC];            /* EDIT's macros (layers.c sets them each block; 0: the design as drawn) */
    rnd_t r;
} cal_t;

static FAST int32_t cal_ex(int32_t ms)            /* a per-block factor: e^(-4.6 x 0.7256 / ms), Q16 */
{
    return an_expneg((int32_t)(218760u / (uint32_t)clampi(ms, 1, 600000)));
}
static __typeof__(&cal_ex) volatile cal_ex_fp = cal_ex;   /* (the flash's way into the RAM code) */

AINL int32_t cal_dec(uint8_t b) { int32_t m = MULAW_DEC[b & 0x7Fu]; return b & 0x80u ? -m : m; }
AINL uint8_t cal_enc(int32_t x)
{
    uint32_t a = (uint32_t)(x < 0 ? -x : x);
    if (a > 32767u)
        a = 32767u;
    return (uint8_t)((x < 0 ? 0x80u : 0u) | MULAW_ENC[a >> 4]);
}

static void cal_init(cal_t *c, uint32_t seed)
{
    memset(c, 0, sizeof *c);
    rnd_seed(&c->r, seed);
    {
        uint32_t i;
        for (i = 0; i < CAL_DLN; i++)
            c->dl[0][i] = c->dl[1][i] = 0;
    }
}

AINL int cal_fingers_down(const cal_t *c)
{
    uint32_t k, n = 0;
    for (k = 0; k < 8u; k++)
        n += c->gate[k] != 0;
    return (int)n;
}
/* a finger lands on plate k: for good (until the key is let go), held a while, or tapped */
static FAST void cal_land(cal_t *c, const cal_design_t *d, uint32_t k, int for_good)
{
    c->gate[k] = 1;
    c->ftap[k] = 0;
    if (for_good)
        c->ftime[k] = 0xFFFFu;
    else if (rnd_q15(&c->r) < d->tap) {
        c->ftap[k] = 1;
        c->ftime[k] = (uint16_t)rnd_in(&c->r, 28, 340);           /* 20 .. 250 ms */
    } else
        c->ftime[k] = (uint16_t)clampi(rnd_in(&c->r, d->hold_lo, d->hold_hi) * 1378 / 1000, 30, 0xFFFE);
}
static __typeof__(&cal_land) volatile cal_land_fp = cal_land;   /* (the flash's way into the RAM code) */
/* a plate for the next finger: the next of its phrase, if it has one, more often than not (now and then a
 * neighbour instead: a variation); else one it favours more often than not; never one already down */
static FAST int32_t cal_pick(cal_t *c, const cal_design_t *d)
{
    uint32_t tries, k;
    if (d->mlen && (rnd_next(&c->r) & 255u) < d->mfollow) {
        k = d->motif[c->mstep++ % d->mlen] & 7u;
        if (rnd_chance(&c->r, 12))
            k ^= 1u;                                  /* (the plate beside it) */
        if (!c->gate[k])
            return (int32_t)k;
    }
    for (tries = 0; tries < 12u; tries++) {
        k = (uint32_t)rnd_below(&c->r, 8);
        if (c->gate[k])
            continue;
        if (tries < 6u && !((d->home >> k) & 1u) && rnd_chance(&c->r, 60))
            continue;
        return (int32_t)k;
    }
    return -1;
}
static __typeof__(&cal_pick) volatile cal_pick_fp = cal_pick;   /* (the flash's way into the RAM code) */
/* EDIT's macros on CALIGULA: EDGE (the oscillators from triangles to squares), DEPTH (how deep the pairs modulate
 * each other), CHAOS (the total feedback and the drive) or, for the gentler species, ECHO (the delay's mix and
 * feedback), and HAND (how busy the hand is: its touches and the bursts) or, for the gated ones, PULSE (the hyper
 * LFO's speed) */
AINL int cal_echoes(uint32_t kind)
{
    return kind == CS_SPORE || kind == CS_VEIL || kind == CS_LITANY || kind == CS_MOTH;
}
AINL int cal_pulses(uint32_t kind) { return kind == CS_HIVE || kind == CS_MOTH || kind == CS_WIRE; }
static const char *cal_mac_name(const cal_design_t *d, uint32_t j)
{
    return j == 0u ? "WAVEFORM" : j == 1u ? "MOD DEPTH" : j == 2u ? (cal_echoes(d->kind) ? "DELAY" : "FEEDBACK")
                                                                  : (cal_pulses(d->kind) ? "GATE RATE" : "TOUCHES");
}
/* 2^(m / MAC_MAX x oct), Q16 */
AINL int32_t cal_mac_exp(int32_t m, int32_t oct_q16) { return (int32_t)exp2_q16(m * (oct_q16 / MAC_MAX)); }

/* the wait to the next finger: irregular, around 1 / touch (exponential: -ln u / rate) */
static FAST uint32_t cal_wait(cal_t *c, const cal_design_t *d)
{
    int32_t mean = (int32_t)(1378u * 256u / (uint32_t)clampi(d->touch, 16, 256 * 40));   /* blocks */
    if (c->mac[3] && !cal_pulses(d->kind))            /* HAND: busier (shorter waits) or calmer */
        mean = clampi(mulq16(mean, cal_mac_exp(-c->mac[3], R16(1.5))), 4, 1378 * 20);
    if (d->rhythm && d->mlen) {                       /* on the phrase's pulse: long, short, ... a little loose */
        static const uint8_t PULSE[4] = {2, 1, 1, 4};   /* x mean / 2 */
        int32_t w = mean * PULSE[c->mstep % d->mlen] / 2;
        return (uint32_t)clampi(w + mulq15(w, rnd_bi(&c->r) / 6), 8, 1378 * 20);
    }
    int32_t ln = (int32_t)(((int64_t)(R16(15.0) - log2_q16((uint32_t)rnd_in(&c->r, 64, Q15))) * 45426) >> 16);
    return (uint32_t)clampi(mulq16(mean, ln), 8, 1378 * 20);
}
static __typeof__(&cal_wait) volatile cal_wait_fp = cal_wait;   /* (the flash's way into the RAM code) */

/* a key (or the loop: who 2) down on the note: it sounds there; from silence, the hand lands */
static void cal_note_on(cal_t *c, const cal_design_t *d, uint32_t note, uint32_t who)
{
    uint32_t k, n;
    int fresh = !c->nheld;
    for (k = 0; k < c->nheld; k++)                        /* (already held: to the top) */
        if (c->note[k] == note && c->nsrc[k] == who) {
            for (; k + 1u < c->nheld; k++) {
                c->note[k] = c->note[k + 1u];
                c->nsrc[k] = c->nsrc[k + 1u];
            }
            c->nheld--;
            break;
        }
    if (c->nheld == 8u) {
        for (k = 0; k + 1u < 8u; k++) {
            c->note[k] = c->note[k + 1u];
            c->nsrc[k] = c->nsrc[k + 1u];
        }
        c->nheld--;
    }
    c->note[c->nheld] = (uint8_t)note;
    c->nsrc[c->nheld++] = (uint8_t)who;
    c->base_t = (int32_t)note * 16;
    c->live = 1;
    c->awake = 1;
    c->hush = 0;
    if (fresh) {
        c->base = c->base_t;
        n = d->first ? d->first : 1u;
        for (k = 0; k < n; k++) {
            int32_t p = cal_pick_fp(c, d);
            if (p >= 0)
                cal_land_fp(c, d, (uint32_t)p, k == 0u);     /* the first for good: it always sounds */
        }
        c->touch_wait = cal_wait_fp(c, d);
    }
}
/* everything the hand holds let go: the organism rings out */
static void cal_lift(cal_t *c)
{
    memset(c->gate, 0, sizeof c->gate);
    memset(c->burst, 0, sizeof c->burst);
    c->awake = 0;
    c->surge_left = 0;
}
static void cal_note_off(cal_t *c, uint32_t note, uint32_t who)
{
    uint32_t k;
    for (k = 0; k < c->nheld; k++)
        if (c->note[k] == note && c->nsrc[k] == who) {
            for (; k + 1u < c->nheld; k++) {
                c->note[k] = c->note[k + 1u];
                c->nsrc[k] = c->nsrc[k + 1u];
            }
            c->nheld--;
            if (c->nheld)
                c->base_t = (int32_t)c->note[c->nheld - 1u] * 16;   /* back to the key still held */
            else
                cal_lift(c);
            return;
        }
}
/* every note one holder holds (the loop, stopped), let go */
static void cal_release(cal_t *c, uint32_t who)
{
    uint32_t k = 0;
    while (k < c->nheld) {
        if (c->nsrc[k] == who)
            cal_note_off(c, c->note[k], who);
        else
            k++;
    }
}

/* the hand, a block: fingers lift when their time is up, others land (never more than it has, never none) */
static FAST void cal_hand(cal_t *c, const cal_design_t *d)
{
    uint32_t k;
    int down;
    for (k = 0; k < 8u; k++)
        if (c->gate[k] && c->ftime[k] != 0xFFFFu && !--c->ftime[k])
            c->gate[k] = 0;
    down = cal_fingers_down(c);
    if (!down) {                                          /* (never none: one lands at once) */
        int32_t p = cal_pick(c, d);
        if (p >= 0)
            cal_land(c, d, (uint32_t)p, 0);
    }
    if (c->touch_wait && --c->touch_wait)
        return;
    c->touch_wait = cal_wait(c, d);
    if (down < d->fingers) {
        int32_t p = cal_pick(c, d);
        if (p >= 0)
            cal_land(c, d, (uint32_t)p, 0);
    } else {                                              /* all down: one lifts (not the last) */
        for (k = 0; k < 8u; k++)
            if (c->gate[k] && rnd_chance(&c->r, 40)) {
                c->gate[k] = 0;
                break;
            }
    }
}
static __typeof__(&cal_hand) volatile cal_hand_fp = cal_hand;   /* (the flash's way into the RAM code) */

/* the hyper LFO: 0 .. Q15 (a gate when square), from its two */
static FAST int32_t cal_hyper(cal_t *c, const cal_design_t *d)
{
    int32_t a, b;
    if (d->lfo_sq) {
        a = c->la < 0x80000000u ? Q15 : 0;
        b = c->lb < 0x80000000u ? Q15 : 0;
    } else {
        a = (int32_t)(c->la < 0x80000000u ? c->la >> 16 : (0xFFFFFFFFu - c->la) >> 16);
        b = (int32_t)(c->lb < 0x80000000u ? c->lb >> 16 : (0xFFFFFFFFu - c->lb) >> 16);
    }
    switch (d->link) {
    case CLNK_AND: return mulq15(a, b);
    case CLNK_OR: return a > b ? a : b;
    case CLNK_XOR: return absi(a - b);
    default: return a;
    }
}
static __typeof__(&cal_hyper) volatile cal_hyper_fp = cal_hyper;   /* (the flash's way into the RAM code) */

/* one block (CTL samples), Q15 into out; 0 when it has gone quiet for good */
static FAST int cal_render(cal_t *c, const cal_design_t *d, int32_t *out)
{
    uint32_t i, k;
    int32_t h, gate, dmod_lfo, sharp_drv[4], depth[4], gp, total_fb = d->total_fb, drive = d->drive;
    int32_t del_fb = d->del_fb, del_mix = d->del_mix;
    int32_t env_t[8], env_k[8], denv[8], env_e[8], del_tq[2];
    int any = 0;
    if (!c->live)
        return 0;
    if (c->mac[2]) {                                      /* ECHO: the delay wetter and longer; CHAOS: feedback, drive */
        if (cal_echoes(d->kind)) {
            del_mix = clampi(del_mix + c->mac[2] * (LV(0.45) / MAC_MAX), 0, LV(0.9));
            del_fb = clampi(del_fb + c->mac[2] * (LV(0.3) / MAC_MAX), 0, LV(0.85));
        } else {
            total_fb = clampi(total_fb + c->mac[2] * (LV(0.45) / MAC_MAX), 0, LV(0.92));
            drive = c->mac[2] > 0 ? drive + c->mac[2] * (IX(4.0) / MAC_MAX)
                                  : IX(1.0) + mulq15(drive - IX(1.0), (MAC_MAX + c->mac[2]) * (Q15 / MAC_MAX));
        }
    }
    /* the hyper LFO and what it moves */
    {
        int32_t pul = c->mac[3] && cal_pulses(d->kind) ? cal_mac_exp(c->mac[3], R16(1.5)) : Q16;   /* PULSE */
        c->la += (uint32_t)(((uint64_t)(uint32_t)mulq16(d->lfo_a, pul) * 3116605u) >> 16);   /* Hz Q16 -> a block's step */
        c->lb += (uint32_t)(((uint64_t)(uint32_t)mulq16(d->lfo_b, pul) * 3116605u) >> 16);
    }
    h = cal_hyper(c, d);
    gate = d->lfo_sq ? h : (h > Q15 / 2 ? Q15 : 0);
    c->gate_s += (gate - c->gate_s) >> 2;                 /* (a few ms: no clicks) */
    dmod_lfo = h - Q15 / 2;
    /* while a key is held: the hand, the wandering, the bursts, the surges */
    if (c->awake && c->nheld) {
        cal_hand(c, d);
        for (k = 0; k < 4u; k++) {
            if ((rnd_next(&c->r) & 1023u) == 0u) {         /* a new place to wander to, now and then */
                c->wd_t[k] = mulq15(rnd_bi(&c->r), d->drift);
                c->ws_t[k] = mulq15(rnd_bi(&c->r), d->drift);
            }
        }
        if (rnd_q15(&c->r) < (mulq16(d->burst, c->mac[3] && !cal_pulses(d->kind) ? cal_mac_exp(c->mac[3], R16(1.5)) : Q16) * 95) >> 10) {
            /* (bursts: Q8 a second -> a block's chance, Q15; HAND makes them more or fewer) */
            k = (uint32_t)rnd_below(&c->r, 8);
            c->burst[k] = (uint16_t)rnd_in(&c->r, 20, 520);   /* 15 .. 380 ms */
        }
        if (!c->surge_left && rnd_q15(&c->r) < (d->surge * 13) >> 13)   /* surges: Q8 a minute -> a block's, Q15 */
            c->surge_left = (uint32_t)rnd_in(&c->r, 400, 2800);   /* 0.3 .. 2 s */
    }
    for (k = 0; k < 4u; k++) {
        c->wd[k] += (c->wd_t[k] - c->wd[k]) >> 9;
        c->ws[k] += (c->ws_t[k] - c->ws[k]) >> 9;
        sharp_drv[k] = IX(1.0) + mulq15(clampi(d->sharp[k] + c->ws[k] + c->mac[0] * (LV(0.7) / MAC_MAX), 0, Q15), IX(7.0));
        depth[k] = clampi(d->mod[k] + c->wd[k] + c->mac[1] * (LV(0.6) / MAC_MAX), 0, Q15);
        depth[k] = mulq15(depth[k], Q15 - mulq15(d->lfo_idx[k], Q15 - h));   /* the hyper LFO opens and closes it */
    }
    if (c->surge_left) {                                  /* a surge: feedback and drive up, then back */
        int32_t s = c->surge_left > 200u ? Q15 : (int32_t)c->surge_left * (Q15 / 200);
        c->surge_left--;
        total_fb = total_fb + mulq15(s, LV(0.85) - total_fb);
        drive = drive + mulq15(s, IX(8.0));
    }
    c->base += (c->base_t - c->base) / 2;                 /* a new note: a quick snap, not a glide */
    c->vph += 27000000u;                                  /* the vibrato: ~5.5 Hz, small */
    {
        uint32_t j;
        int32_t v16 = (mulq15(sin_ph(c->vph), d->vib) * 16) >> 15;
        for (j = 0; j < 8u; j++)
            c->inc[j] = pitch_inc(c->base + d->tune[j] + v16);
    }
    /* the envelopes: towards 1 (a sensor, a burst, a side's hold) or 0, slow or fast */
    for (k = 0; k < 8u; k++) {
        uint32_t p = k >> 1, side = k >> 2;
        int on = c->gate[k] || c->burst[k];
        int32_t floor_ = 0;
        if (c->burst[k])
            c->burst[k]--;
        if (!on) {                                       /* a side's hold: the others of a side touched */
            uint32_t j;
            for (j = side * 4u; j < side * 4u + 4u; j++)
                if (c->gate[j])
                    floor_ = d->hold[side];
        }
        env_t[k] = (on ? Q15 : floor_) << 8;
        env_k[k] = c->burst[k] || (c->gate[k] && c->ftap[k]) ? cal_ex_fp(8)
                   : env_t[k] > c->env[k] ? cal_ex_fp(d->att[p]) : cal_ex_fp(c->hush ? 30 : d->rel[p]);
        {
            int32_t e1 = env_t[k] + mulq16(c->env[k] - env_t[k], env_k[k]);
            if (absi(e1 - env_t[k]) < 32 << 8)           /* (-60 dB: there) */
                e1 = env_t[k];
            denv[k] = (e1 - c->env[k]) / (int32_t)CTL;
            env_e[k] = e1;
        }
        if (c->env[k] || denv[k] || env_t[k])
            any = 1;
    }
    if (!any && !c->awake && absi(c->dlast[0]) < 16 && absi(c->dlast[1]) < 16 && absi(c->fb) < 16) {
        c->live = 0;
        for (i = 0; i < CTL; i++)
            out[i] = 0;
        return 0;
    }
    gp = Q15 - mulq15(d->gate_depth, Q15 - c->gate_s);    /* the gated pairs' level now */
    {   /* the delay's sweep by the hyper LFO: once a block (warped by the voices, it is worked out a sample) */
        int32_t m = mulq15(dmod_lfo, d->del_mod);
        for (k = 0; k < 2u; k++)
            del_tq[k] = (d->del_t[k] << 8) + mulq15(m, d->del_t[k] << 7);
    }
    for (i = 0; i < CTL; i++) {
        int32_t y = 0, s, mod_in[4], dy;
        for (k = 0; k < 4u; k++) {                        /* each pair's source, a sample late */
            switch (d->src[k]) {
            case CSRC_SIDE: mod_in[k] = c->pair[k ^ 1u]; break;
            case CSRC_CROSS: mod_in[k] = c->pair[k ^ 2u]; break;
            case CSRC_TOTAL: mod_in[k] = c->fb; break;
            default: mod_in[k] = 0; break;               /* the partner: below */
            }
        }
        for (k = 0; k < 4u; k++) {
            int32_t pa = 0;
            uint32_t j;
            for (j = k * 2u; j < k * 2u + 2u; j++) {
                int32_t m, t, e;
                uint32_t inc = c->inc[j];
                c->env[j] += denv[j];
                e = c->env[j] >> 8;
                if (!e) {
                    c->osc[j] = 0;
                    continue;
                }
                m = d->src[k] == CSRC_PARTNER ? c->osc[j ^ 1u] : mod_in[k];
                m = mulq15(m, depth[k]);                  /* FM: the increment x (1 + m), m within +-0.95 */
                m = clampi(m, -31130, 31130);
                inc = (uint32_t)((int64_t)inc + (((int64_t)inc * m) >> 15));
                c->ph[j] += inc;
                t = (int32_t)(c->ph[j] >> 16);            /* a triangle, -Q15 .. Q15 */
                t = (t < 32768 ? t : 65535 - t) * 2 - Q15;
                t = tanh_q15(mulq12(t, sharp_drv[k]));    /* pushed towards a square */
                c->osc[j] = t;
                pa += mulq15(t, e);
            }
            pa >>= 1;
            if ((d->gate_pairs >> k) & 1u)
                pa = mulq15(pa, gp);
            c->pair[k] = pa;
            y += pa;
        }
        y >>= 1;
        /* the delay: two lines at FS / 2, swept by the hyper LFO or warped by the upper pairs */
        {
            int32_t mod = d->del_src ? mulq15((c->pair[2] + c->pair[3]) >> 1, d->del_mod) : 0;
            for (k = 0; k < 2u; k++) {
                int32_t tq = d->del_src ? (d->del_t[k] << 8) + mulq15(mod, d->del_t[k] << 7) : del_tq[k];   /* Q8 */
                int32_t ti = tq >> 8, fr = tq & 255, r0, r1, a0, a1;
                ti = clampi(ti, 2, (int32_t)CAL_DLN - 3);
                r0 = (int32_t)c->dw - ti;
                if (r0 < 0)
                    r0 += CAL_DLN;
                r1 = r0 - 1 < 0 ? (int32_t)CAL_DLN - 1 : r0 - 1;
                a0 = cal_dec(c->dl[k][r0]);
                a1 = cal_dec(c->dl[k][r1]);
                c->dlast[k] = a0 + (((a1 - a0) * fr) >> 8);
            }
            if (c->odd) {
                for (k = 0; k < 2u; k++)
                    c->dl[k][c->dw] = cal_enc((c->dhalf[k] + y + mulq15(c->dlast[k ^ 1u], del_fb)) >> 1);
                if (++c->dw >= CAL_DLN)
                    c->dw = 0;
            } else
                for (k = 0; k < 2u; k++)
                    c->dhalf[k] = y + mulq15(c->dlast[k ^ 1u], del_fb);
            c->odd ^= 1u;
            dy = (c->dlast[0] + c->dlast[1]) >> 1;
            y += mulq15(dy - y, del_mix);
        }
        /* the drive, and the total feedback (the output, saturated, low-passed, back into the modulation) */
        s = tanh_q15(mulq12(y, drive));
        c->fb += (mulq15(s, total_fb) - c->fb) >> 3;
        out[i] = mulq12(s, d->out);
    }
    for (k = 0; k < 8u; k++)
        c->env[k] = env_e[k];                            /* (exactly where the block was going: never stuck short) */
    return 1;
}
static __typeof__(&cal_render) volatile cal_render_fp = cal_render;   /* (the flash's way into the RAM code) */

/* ------------------------------------------------------------- designs --- */
/* the intervals a species tunes its eight oscillators to, from the root, semitones */
#define CSET_N1 10u              /* the tunings of generation 1 (generation 0: the first six) */
static const int8_t CAL_SETS[CSET_N1][8] = {
    {0, 0, 12, 12, 7, 7, 19, 24},        /* pillars: root, octave, fifth */
    {0, 1, 12, 13, -12, -11, 7, 8},      /* clusters: each a semitone off itself */
    {-12, -12, 0, 0, -24, -24, 7, 12},   /* the depths */
    {0, 0, 7, 7, 12, 19, 24, 31},        /* the spectrum: harmonics, roughly */
    {0, 0, 0, 12, 12, 12, -12, 7},       /* unisons, beating */
    {0, 6, 12, 18, -12, -6, 7, 13},      /* tritones */
    /* (generation 1) */
    {0, 0, 5, 5, 12, 12, 17, 24},        /* fourths */
    {0, 0, 3, 3, 7, 7, 10, 12},          /* a minor seventh: melancholy */
    {-12, -12, 0, 0, 7, 7, 14, 19},      /* open fifths, stacked */
    {0, 0, 0, 0, 12, 12, -12, 7},        /* a quarter-tone cluster (the odd ones a quarter tone up) */
};

/* the species' and the tunings' weights on each path (the open path: all alike) */
static const uint8_t CS_W[3][CS_N0] = {{1, 1, 1, 1, 1, 1}, {4, 0, 0, 2, 4, 1}, {1, 3, 3, 2, 0, 3}};
static const uint8_t CSET_W[3][6] = {{1, 1, 1, 1, 1, 1}, {3, 1, 3, 2, 3, 0}, {1, 3, 1, 1, 1, 3}};
/* generation 1: ten species, ten tunings */
static const uint8_t CS_W1[3][CS_N] = {{1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {4, 0, 0, 2, 4, 1, 4, 3, 0, 2},
                                       {1, 3, 3, 2, 0, 3, 0, 1, 4, 2}};
static const uint8_t CSET_W1[3][CSET_N1] = {{1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {3, 1, 3, 2, 3, 0, 2, 3, 3, 1},
                                            {1, 3, 1, 1, 1, 3, 1, 0, 1, 3}};

/* generation 1: the four new species made what they are; a phrase for the hand; a bed that never goes; no harsh
 * pair on AMBIENT */
static void cal_care1(cal_design_t *d, uint32_t kind, uint32_t path, rnd_t *r)
{
    uint32_t k;
    switch (kind) {
    case CS_LITANY:                                   /* a slow choir: unisons beating, nothing sharp */
        for (k = 0; k < 4u; k++) {
            d->att[k] = rnd_in(r, 800, 3000);
            d->rel[k] = rnd_in(r, 4000, 9000);
            d->sharp[k] = rnd_in(r, 0, LV(0.2));
            d->mod[k] = rnd_in(r, 0, LV(0.25));
        }
        d->hold[0] = rnd_in(r, LV(0.3), LV(0.6));
        d->hold[1] = rnd_in(r, LV(0.3), LV(0.6));
        d->gate_pairs = 0;
        d->lfo_a = rnd_in(r, H16(0.05), H16(0.4));
        d->burst = rnd_in(r, 0, 128);
        d->surge = rnd_in(r, 0, 256);
        d->drive = rnd_in(r, IX(1.0), IX(1.5));
        d->total_fb = rnd_in(r, LV(0.05), LV(0.25));
        d->del_mix = rnd_in(r, LV(0.2), LV(0.4));
        d->touch = rnd_in(r, 40, 110);
        d->hold_lo = 3000, d->hold_hi = 9000, d->tap = 0, d->fingers = (uint8_t)rnd_in(r, 3, 4);
        break;
    case CS_UNDERTOW:                                 /* deep, slow, the feedback and the delay pulling it under */
        for (k = 0; k < 4u; k++) {
            d->att[k] = rnd_in(r, 400, 2500);
            d->rel[k] = rnd_in(r, 3000, 9000);
            d->sharp[k] = rnd_in(r, LV(0.2), LV(0.5));
        }
        d->total_fb = rnd_in(r, LV(0.4), LV(0.75));
        d->del_src = 1;
        d->del_mod = rnd_in(r, LV(0.15), LV(0.4));
        d->del_fb = rnd_in(r, LV(0.5), LV(0.75));
        d->drive = rnd_in(r, IX(1.5), IX(3.0));
        d->surge = rnd_in(r, 256 * 2, 256 * 5);
        d->burst = rnd_in(r, 0, 128);
        d->touch = rnd_in(r, 50, 120);
        d->hold_lo = 2000, d->hold_hi = 8000, d->tap = LV(0.05), d->fingers = (uint8_t)rnd_in(r, 2, 3);
        break;
    case CS_WIRE:                                     /* high, sharp, a quick hand tapping */
        for (k = 0; k < 4u; k++) {
            d->att[k] = rnd_in(r, 5, 60);
            d->rel[k] = rnd_in(r, 80, 600);
            d->sharp[k] = rnd_in(r, LV(0.7), Q15);
            d->mod[k] = rnd_in(r, LV(0.6), Q15);
        }
        d->lfo_a = rnd_in(r, H16(3.0), H16(11.0));
        d->lfo_sq = 1;
        d->gate_pairs = (uint8_t)(1 + rnd_below(r, 14));
        d->gate_depth = rnd_in(r, LV(0.6), Q15);
        d->burst = rnd_in(r, 256 * 3, 256 * 8);
        d->touch = rnd_in(r, 1000, 2500);
        d->hold_lo = 40, d->hold_hi = 300, d->tap = LV(0.8), d->fingers = (uint8_t)rnd_in(r, 4, 6);
        break;
    case CS_MOTH:                                     /* soft wings: triangles fluttering through the gates */
        for (k = 0; k < 4u; k++) {
            d->sharp[k] = rnd_in(r, 0, LV(0.3));
            d->mod[k] = rnd_in(r, LV(0.2), LV(0.5));
        }
        d->lfo_a = rnd_in(r, H16(1.5), H16(6.0));
        d->lfo_sq = 1;
        d->gate_pairs = (uint8_t)(3 + rnd_below(r, 12));
        d->gate_depth = rnd_in(r, LV(0.5), LV(0.9));
        d->burst = rnd_in(r, 128, 512);
        d->touch = rnd_in(r, 200, 500);
        d->hold_lo = 300, d->hold_hi = 1500, d->tap = LV(0.3), d->fingers = 3;
        break;
    default:
        break;
    }
    if (kind >= CS_N0)
        d->lfo_b = mulq16(d->lfo_a, rnd_in(r, R16(0.37), R16(2.71)));
    /* a phrase: two to four plates it comes back to, sometimes on a pulse (always for the new species but WIRE) */
    if ((kind >= CS_N0 && kind != CS_WIRE) || rnd_chance(r, 50)) {
        d->mlen = (uint8_t)rnd_in(r, 2, 4);
        for (k = 0; k < 4u; k++)
            d->motif[k] = (uint8_t)rnd_below(r, 8);
        d->mfollow = (uint8_t)rnd_in(r, 150, 230);
        d->rhythm = (uint8_t)(kind == CS_MOTH || rnd_chance(r, 45));
    }
    if ((d->gate_pairs & 15u) == 15u)                /* a pair the gates never close on: the bed */
        d->gate_pairs = (uint8_t)(15u & ~(1u << rnd_below(r, 4)));
    if (path == PATH_AMBIENT)                         /* sharp and deeply modulated together grates */
        for (k = 0; k < 4u; k++)
            if (d->sharp[k] > LV(0.5) && d->mod[k] > LV(0.5))
                d->mod[k] = LV(0.5);
}

static void cal_design(cal_design_t *d, int32_t kind, uint32_t seed, uint32_t path, uint32_t gen)
{
    rnd_t rr, *r = &rr;
    uint32_t k, set;
    path = path < PATH_N ? path : PATH_OPEN;
    rnd_seed(r, seed ^ 0xCA11u);
    {   /* (the family drawn always, a family given or not: the seed and the family then make the sound again, however
         * the family was come to -- SHIFT + PRESETS, a preset, a session) */
        int32_t drawn = gen == 0u ? (path == PATH_OPEN ? rnd_below(r, CS_N0) : (int32_t)rnd_weighted(r, CS_W[path], CS_N0))
                                  : (int32_t)rnd_weighted(r, CS_W1[path], CS_N);
        if (kind < 0 || kind >= CS_N)
            kind = drawn;
    }
    memset(d, 0, sizeof *d);
    d->seed = seed;
    d->kind = (uint8_t)kind;
    d->path = (uint8_t)path;
    d->gen = (uint8_t)gen;
    /* the tuning: a set of intervals from the root, each pair detuned against itself (it beats) */
    if (gen == 0u)
        set = path == PATH_OPEN ? (uint32_t)rnd_below(r, 6) : rnd_weighted(r, CSET_W[path], 6);
    else if (kind == CS_LITANY)
        set = rnd_chance(r, 50) ? 4u : rnd_chance(r, 50) ? 0u : 7u;   /* unisons, pillars, the minor seventh */
    else if (kind == CS_UNDERTOW)
        set = rnd_chance(r, 50) ? 2u : 8u;                            /* the depths, open fifths */
    else if (kind == CS_WIRE)
        set = rnd_chance(r, 40) ? 1u : rnd_chance(r, 50) ? 5u : 9u;   /* clusters, tritones, quarter tones */
    else
        set = rnd_weighted(r, CSET_W1[path], CSET_N1);
    for (k = 0; k < 8u; k++) {
        int32_t semis = CAL_SETS[set][k], cents;
        if (kind == CS_CARRION || kind == CS_SPORE || kind == CS_UNDERTOW)
            semis -= 12;
        if (kind == CS_WIRE)
            semis += 12;
        cents = (k & 1u) ? rnd_in(r, 3, kind == CS_THORN || kind == CS_RUST ? 40 : 18) : rnd_in(r, -4, 4);
        if (rnd_chance(r, 50))
            cents = -cents;
        d->tune[k] = (int16_t)(semis * 16 + cents * 16 / 100 + (set == 9u && (k & 1u) ? 8 : 0));
    }
    for (k = 0; k < 4u; k++) {
        int slow = rnd_chance(r, kind == CS_HIVE || kind == CS_THORN ? 30 : 75);
        d->att[k] = slow ? rnd_in(r, 200, 3000) : rnd_in(r, 5, 100);
        d->rel[k] = slow ? rnd_in(r, 2000, 9000) : rnd_in(r, 80, 900);
        d->sharp[k] = rnd_in(r, 0, kind == CS_VEIL ? LV(0.25) : kind == CS_THORN || kind == CS_RUST ? Q15 : LV(0.7));
        d->mod[k] = rnd_in(r, kind == CS_VEIL ? 0 : LV(0.1), kind == CS_VEIL ? LV(0.3) : kind == CS_THORN ? Q15 : LV(0.75));
        d->src[k] = (uint8_t)rnd_below(r, 4);
        d->lfo_idx[k] = rnd_in(r, 0, LV(0.9));
    }
    d->hold[0] = rnd_chance(r, 60) ? rnd_in(r, LV(0.15), LV(0.6)) : 0;
    d->hold[1] = rnd_chance(r, 60) ? rnd_in(r, LV(0.15), LV(0.6)) : 0;
    /* the hyper LFO: slow and breathing, or quick and rhythmic (the hive), the two never in step */
    if (kind == CS_HIVE || (kind == CS_RUST && rnd_chance(r, 50))) {
        d->lfo_a = rnd_in(r, H16(2.0), H16(11.0));
        d->lfo_sq = 1;
        d->gate_pairs = (uint8_t)(1 + rnd_below(r, 15));
        d->gate_depth = rnd_in(r, LV(0.6), Q15);
    } else {
        d->lfo_a = rnd_in(r, H16(0.05), H16(1.5));
        d->lfo_sq = (uint8_t)rnd_chance(r, 35);
        d->gate_pairs = (uint8_t)(rnd_chance(r, 30) ? 1 + rnd_below(r, 15) : 0);
        d->gate_depth = rnd_in(r, LV(0.3), LV(0.9));
    }
    d->lfo_b = mulq16(d->lfo_a, rnd_in(r, R16(0.37), R16(2.71)));   /* (never a whole ratio, nearly) */
    d->link = (uint8_t)rnd_below(r, 4);
    d->vib = rnd_in(r, 0, LV(0.04));
    /* the delay: a chorus, a flanger, a warp */
    d->del_t[0] = rnd_in(r, 20, 700);
    d->del_t[1] = rnd_in(r, 20, 700);
    d->del_src = (uint8_t)rnd_chance(r, kind == CS_CARRION || kind == CS_RUST ? 60 : 25);
    d->del_mod = d->del_src ? rnd_in(r, LV(0.05), LV(0.4)) : rnd_in(r, LV(0.02), LV(0.15));
    d->del_fb = rnd_in(r, LV(0.1), kind == CS_CARRION ? LV(0.8) : LV(0.6));
    d->del_mix = rnd_in(r, LV(0.1), LV(0.55));
    d->drive = rnd_in(r, IX(1.0), kind == CS_THORN || kind == CS_RUST || kind == CS_CARRION ? IX(6.0) : IX(2.0));
    d->total_fb = rnd_in(r, LV(0.05), kind == CS_CARRION || kind == CS_THORN ? LV(0.9) : LV(0.5));
    /* its life of its own */
    d->burst = kind == CS_HIVE || kind == CS_THORN ? rnd_in(r, 256, 256 * 5) : rnd_in(r, 0, 256 * 2);
    d->surge = kind == CS_CARRION || kind == CS_RUST ? rnd_in(r, 256 * 2, 256 * 10) : rnd_in(r, 0, 256 * 3);
    d->drift = rnd_in(r, LV(0.1), LV(0.6));
    d->out = IX(1.0);
    /* the hand: how a player of this species would touch the plates */
    switch (kind) {
    case CS_SPORE:
        d->touch = rnd_in(r, 50, 160);            /* 0.2 .. 0.6 a second */
        d->hold_lo = 2000, d->hold_hi = 8000, d->tap = LV(0.05), d->fingers = (uint8_t)rnd_in(r, 2, 4);
        break;
    case CS_THORN:
        d->touch = rnd_in(r, 380, 1000);
        d->hold_lo = 200, d->hold_hi = 1500, d->tap = LV(0.4), d->fingers = (uint8_t)rnd_in(r, 3, 5);
        break;
    case CS_HIVE:
        d->touch = rnd_in(r, 770, 2050);
        d->hold_lo = 50, d->hold_hi = 400, d->tap = LV(0.7), d->fingers = 4;
        break;
    case CS_CARRION:
        d->touch = rnd_in(r, 80, 260);
        d->hold_lo = 1000, d->hold_hi = 6000, d->tap = LV(0.15), d->fingers = 3;
        break;
    case CS_VEIL:
        d->touch = rnd_in(r, 50, 130);
        d->hold_lo = 3000, d->hold_hi = 10000, d->tap = 0, d->fingers = (uint8_t)rnd_in(r, 2, 3);
        break;
    default:   /* RUST */
        d->touch = rnd_in(r, 260, 770);
        d->hold_lo = 300, d->hold_hi = 3000, d->tap = LV(0.3), d->fingers = 4;
        break;
    }
    d->home = 0;
    for (k = rnd_in(r, 1, 3); k; k--)
        d->home |= (uint8_t)(1u << rnd_below(r, 8));
    d->first = (uint8_t)rnd_in(r, 1, 3);
    if (gen >= 1u)
        cal_care1(d, (uint32_t)kind, path, r);
    if (path == PATH_AMBIENT) {
        for (k = 0; k < 4u; k++) {
            if (d->att[k] < 200 && rnd_chance(r, 60)) {   /* most pairs swell */
                d->att[k] = rnd_in(r, 300, 2500);
                d->rel[k] = rnd_in(r, 2500, 9000);
            }
            d->sharp[k] = mulq15(d->sharp[k], LV(0.6));
        }
        d->drive = clampi(d->drive, IX(1.0), IX(2.5));
        d->total_fb = mulq15(d->total_fb, LV(0.75));
        d->burst = mulq15(d->burst, LV(0.4));
        d->surge = mulq15(d->surge, LV(0.4));
        d->touch = mulq15(d->touch, LV(0.7));
        d->hold_lo += d->hold_lo / 2;
        d->hold_hi += d->hold_hi / 2;
        d->tap = mulq15(d->tap, LV(0.5));
    } else if (path == PATH_EXP) {
        for (k = 0; k < 4u; k++)
            d->sharp[k] = clampi(d->sharp[k], LV(0.3), Q15);
        d->drive = clampi(d->drive + IX(1.0), IX(1.0), IX(7.0));
        d->burst += d->burst / 2 + 128;
        d->surge += d->surge / 2;
        d->touch += d->touch / 3;
    }
}

/* the design's level from how loud it usually is (as an_measure): C3 held 2 s (the hand playing, the slow
 * pairs swelling), the 75th percentile of 12 ms windows brought to 0.1 RMS */
#define CAL_PROBE_BLOCKS 2756u
#define cal_probe (*(cal_t *)(void *)&an_probe)   /* (ANIMA's probe: never both at once) */
typedef char cal_probe_fits[sizeof(cal_t) <= sizeof(an_voice_t) && _Alignof(cal_t) <= _Alignof(an_voice_t) ? 1 : -1];
static void cal_measure(cal_design_t *d)
{
    static int32_t win[CAL_PROBE_BLOCKS / 16u + 1u];
    int32_t out[CTL];
    uint32_t b, k, nwin = 0;
    int64_t acc = 0;
    d->out = IX(1.0);
    cal_init(&cal_probe, d->seed ^ 0x51u);
    cal_note_on(&cal_probe, d, 48, 1u);
    for (b = 0; b < CAL_PROBE_BLOCKS; b++) {
        uint32_t i;
        cal_render_fp(&cal_probe, d, out);
        for (i = 0; i < CTL; i++)
            acc += ((int64_t)out[i] * out[i]) >> 6;      /* Q24 */
        if ((b & 15u) == 15u) {
            if (nwin < NELEM(win))
                win[nwin++] = (int32_t)((acc >> 9) > 0x7FFFFFFF ? 0x7FFFFFFF : (acc >> 9));   /* / (16 x 32) */
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
        uint32_t root = nwin ? isqrt32((uint32_t)win[nwin * 3u / 4u]) : 0;   /* Q12 */
        int32_t g = root ? (int32_t)(1677722u / root) : 4096 * 8;            /* to 0.1 RMS, Q12 */
        d->out = clampi(g, 300, 4096 * 8);
    }
}
