/* SPDX-License-Identifier: GPL-3.0-only
 * TONITOR: the SCREEN READER (GLOBAL SETUP, SETTINGS): the screen's words, spoken by the FM-1 itself.
 *
 * THE VOICE. A formant synthesiser after Klatt (1980), the kind of voice DECtalk used: a glottal pulse and a
 * noise source, through a cascade of resonators (the vocal tract) for the voiced and aspirated sounds and a
 * parallel resonator for frication. Speech is phonemes (ARPAbet, a byte each: the phoneme in the low 6 bits,
 * its stress in the top 2). The audio interrupt reads them from a ring as it speaks, makes each one into one to
 * three segments (targets: formants, amplitudes, a duration, a transition) and moves between the targets a frame
 * (64 samples) at a time. Fixed point only: no float, no 64-bit division. It runs at 22050 Hz, doubled to the
 * audio's 44100 Hz; while it speaks the music is 6 dB lower. It goes to the outputs (after the MASTER knob),
 * not to USB audio.
 *
 * THE WORDS. vo_text() turns the screen's text into phonemes: words from the dictionary (tools/voice_words.txt,
 * packed by tools/gen_voice.py into voice_dict.h), numbers as words (-28.5: minus twenty eight point five), units
 * after a number (MS, S, DB, HZ, KHZ), %, # (sharp), +. A word the dictionary lacks is spelt.
 *
 * THE SCREEN. gfx.c hands over each band's text as it is sent to the screen (sp_cap_*): the band's place, its
 * words, and whether they are dark on light (chosen: a row picked in a list, a value being set, a message).
 * What changed is spoken once the screen has settled (150 ms; at the latest 700 ms after the first change, so a
 * knob turning is heard as it goes): a band whose words changed, or one just chosen. A new page (three bands or
 * more at once) with a chosen row: its title and the chosen row only; else every band that changed, top down.
 *
 *   main loop:  vo_begin(now) vo_text(s) .. vo_end()    an utterance (now: cut off what is being said)
 *               sp_tick(on, ms)                         the screen's changes spoken (on: the setting)
 *   audio:      vo_block(v, n)                          n samples of voice at 44100 Hz (Q15), and the duck */
#include <stdint.h>

#define VO_FS 22050u
#define VO_FRAME 64u                /* samples per parameter frame: 2.9 ms */
#define VO_OPEN 39322u              /* the glottis open 60 % of a period (of 65536) */
#define VO_OPEN_INV ((1u << 30) / VO_OPEN)
#define VO_Q 160u                   /* the phoneme ring (main loop -> audio): ~15 s of speech */
#define VO_NSEG 6u                  /* the segments made ahead */
#define VO_NRATE 5u                 /* the SPEECH RATEs (GLOBAL SETUP: 1 .. 5) */
static const uint8_t VO_RATE[VO_NRATE] = {130, 100, 80, 65, 52};   /* durations, % (100: careful speech) */
static volatile uint8_t vo_pct = 100;  /* the one set (vo_rate) */
#define VO_BASE 110                 /* the pitch, Hz */

/* the phonemes (ARPAbet). The order is the table's; tools/gen_voice.py has the same list */
enum {
    P_SIL,
    P_AA, P_AE, P_AH, P_AO, P_AW, P_AY, P_EH, P_ER, P_EY, P_IH, P_IY, P_OW, P_OY, P_UH, P_UW,
    P_B, P_CH, P_D, P_DH, P_F, P_G, P_HH, P_JH, P_K, P_L, P_M, P_N, P_NG, P_P, P_R, P_S, P_SH,
    P_T, P_TH, P_V, P_W, P_Y, P_Z, P_ZH,
    P_N_
};
#define VO_PAUSE 0x3Eu             /* between bands: 60 ms */
#define VO_STOP 0x3Fu              /* the end of an utterance: the pitch falls, 220 ms of silence */
#define VO_NONE 0xFFu              /* (nothing after: the ring's end) */

enum { K_SIL, K_VOW, K_DIPH, K_LIQ, K_NAS, K_FRIC, K_STOP, K_ASP, K_AFF };

/* a phoneme's targets. Formants in Hz; amplitudes 0..64 (frication up to 90); dur in ms (stressed, careful).
 * For a diphthong, g1..g3 are the formants it glides to. For a stop, f1..f3 are its locus and ff its burst. */
typedef struct {
    uint16_t f1, f2, f3;
    uint16_t g1, g2, g3;
    uint16_t ff, fb;               /* frication (or burst) centre and bandwidth, Hz; ff 0: flat noise */
    uint16_t nz;                   /* nasal zero, Hz (0: none) */
    uint8_t av, af, ah;            /* voicing, frication, aspiration */
    uint8_t dur, kind, voiced;
} vo_ph_t;

static const vo_ph_t VO_PH[P_N_] = {
    /*             f1    f2    f3   g1    g2    g3    ff    fb    nz  av  af  ah  dur  kind  v */
    [P_SIL] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 60, K_SIL, 0},
    [P_AA] = {700, 1220, 2600, 0, 0, 0, 0, 0, 0, 64, 0, 0, 150, K_VOW, 1},
    [P_AE] = {660, 1720, 2410, 0, 0, 0, 0, 0, 0, 64, 0, 0, 150, K_VOW, 1},
    [P_AH] = {620, 1220, 2550, 0, 0, 0, 0, 0, 0, 62, 0, 0, 95, K_VOW, 1},
    [P_AO] = {600, 990, 2570, 0, 0, 0, 0, 0, 0, 64, 0, 0, 150, K_VOW, 1},
    [P_AW] = {700, 1220, 2600, 440, 900, 2300, 0, 0, 0, 64, 0, 0, 185, K_DIPH, 1},
    [P_AY] = {700, 1220, 2600, 360, 2000, 2600, 0, 0, 0, 64, 0, 0, 185, K_DIPH, 1},
    [P_EH] = {530, 1680, 2500, 0, 0, 0, 0, 0, 0, 64, 0, 0, 105, K_VOW, 1},
    [P_ER] = {470, 1270, 1540, 0, 0, 0, 0, 0, 0, 62, 0, 0, 140, K_VOW, 1},
    [P_EY] = {480, 1760, 2520, 330, 2200, 2650, 0, 0, 0, 64, 0, 0, 165, K_DIPH, 1},
    [P_IH] = {400, 1800, 2570, 0, 0, 0, 0, 0, 0, 62, 0, 0, 90, K_VOW, 1},
    [P_IY] = {310, 2200, 2960, 0, 0, 0, 0, 0, 0, 62, 0, 0, 130, K_VOW, 1},
    [P_OW] = {540, 1100, 2300, 420, 880, 2300, 0, 0, 0, 64, 0, 0, 165, K_DIPH, 1},
    [P_OY] = {550, 960, 2400, 360, 1820, 2450, 0, 0, 0, 64, 0, 0, 200, K_DIPH, 1},
    [P_UH] = {450, 1100, 2350, 0, 0, 0, 0, 0, 0, 62, 0, 0, 90, K_VOW, 1},
    [P_UW] = {350, 1250, 2200, 320, 1000, 2200, 0, 0, 0, 62, 0, 0, 135, K_DIPH, 1},
    /* consonants: f1..f3 the locus the vowels move from and to */
    [P_B] = {200, 900, 2100, 0, 0, 0, 0, 0, 0, 0, 30, 0, 45, K_STOP, 1},
    [P_CH] = {250, 2000, 2600, 0, 0, 0, 2700, 1200, 0, 0, 60, 0, 50, K_AFF, 0},
    [P_D] = {200, 1700, 2600, 0, 0, 0, 3800, 2000, 0, 0, 44, 0, 40, K_STOP, 1},
    [P_DH] = {300, 1500, 2600, 0, 0, 0, 0, 0, 0, 36, 10, 0, 45, K_FRIC, 1},
    [P_F] = {300, 1000, 2200, 0, 0, 0, 0, 0, 0, 0, 40, 0, 95, K_FRIC, 0},
    [P_G] = {200, 1900, 2400, 0, 0, 0, 1900, 800, 0, 0, 42, 0, 45, K_STOP, 1},
    [P_HH] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 42, 60, K_ASP, 0},
    [P_JH] = {250, 2000, 2600, 0, 0, 0, 2700, 1200, 0, 30, 40, 0, 40, K_AFF, 1},
    [P_K] = {200, 1900, 2400, 0, 0, 0, 2000, 800, 0, 0, 52, 0, 55, K_STOP, 0},
    [P_L] = {330, 1100, 2800, 0, 0, 0, 0, 0, 0, 52, 0, 0, 60, K_LIQ, 1},
    [P_M] = {260, 1000, 2200, 0, 0, 0, 0, 0, 750, 64, 0, 0, 70, K_NAS, 1},
    [P_N] = {260, 1600, 2600, 0, 0, 0, 0, 0, 1450, 64, 0, 0, 65, K_NAS, 1},
    [P_NG] = {260, 2000, 2700, 0, 0, 0, 0, 0, 2300, 64, 0, 0, 75, K_NAS, 1},
    [P_P] = {200, 900, 2100, 0, 0, 0, 0, 0, 0, 0, 38, 0, 60, K_STOP, 0},
    [P_R] = {310, 1060, 1380, 0, 0, 0, 0, 0, 0, 52, 0, 0, 65, K_LIQ, 1},
    [P_S] = {320, 1700, 2600, 0, 0, 0, 5200, 1800, 0, 0, 60, 0, 105, K_FRIC, 0},
    [P_SH] = {300, 1900, 2500, 0, 0, 0, 2700, 1100, 0, 0, 90, 0, 105, K_FRIC, 0},
    [P_T] = {200, 1700, 2600, 0, 0, 0, 4200, 2000, 0, 0, 56, 0, 50, K_STOP, 0},
    [P_TH] = {300, 1500, 2600, 0, 0, 0, 0, 0, 0, 0, 32, 0, 85, K_FRIC, 0},
    [P_V] = {300, 1000, 2200, 0, 0, 0, 0, 0, 0, 38, 14, 0, 60, K_FRIC, 1},
    [P_W] = {290, 610, 2150, 0, 0, 0, 0, 0, 0, 50, 0, 0, 55, K_LIQ, 1},
    [P_Y] = {260, 2070, 3020, 0, 0, 0, 0, 0, 0, 50, 0, 0, 50, K_LIQ, 1},
    [P_Z] = {320, 1700, 2600, 0, 0, 0, 5200, 1800, 0, 34, 36, 0, 80, K_FRIC, 1},
    [P_ZH] = {300, 1900, 2500, 0, 0, 0, 2700, 1100, 0, 34, 40, 0, 75, K_FRIC, 1},
};

/* a segment: what the synthesiser aims at for n frames */
typedef struct {
    uint16_t f[3];
    uint16_t ff, nz;               /* frication centre, nasal zero (0: none), Hz */
    uint8_t b[3], fb;              /* bandwidths, tens of Hz */
    uint8_t av, af, ah, flat;
    uint8_t n;                     /* frames */
    uint8_t tin;                   /* frames over which the formants arrive here */
    uint8_t fwd;                   /* 1: the transition is all after the boundary (a release) */
    int8_t f0;                     /* the pitch here, Hz from VO_BASE */
} vo_seg_t;

/* a two-pole resonator: y = a x + b y1 + c y2, Q28 */
typedef struct { int32_t a, b, c, y1, y2; } vo_res_t;

/* the ring: the main loop writes an utterance and then moves vo_w; a cut moves the reader to vo_cut_pos */
static uint8_t vo_q[VO_Q] POOL;     /* (in the pool: the RAM of .data and .bss is nearly full) */
static volatile uint32_t vo_w, vo_r, vo_cut_pos, vo_cut_seq;
static uint32_t vo_wl, vo_wbase;   /* (the main loop's: where it writes, from where the room counts) */

/* the audio's side */
static struct {
    vo_seg_t sg[VO_NSEG];
    uint32_t sh, sn;               /* the segments: the one spoken (sh), how many */
    uint32_t k, sub;               /* its frame, the sample within the frame */
    uint32_t seen_cut;
    uint16_t pf[3], pnz;           /* the last segment's targets (where the formants come from) */
    uint8_t prevph, speaking;
    uint32_t phase, inc, noise;
    int32_t av, af, ah;            /* amplitudes now (Q8 of 0..64) */
    int32_t f0;                    /* Hz, Q8 */
    uint8_t flat;
    vo_res_t r[5], np, rf;         /* the cascade F1..F5, the nasal pole, the frication */
    int32_t nz_b, nz_c, nz_x1, nz_x2, nz_g;   /* the nasal zero; the pole and zero's gain at 0 Hz made 1, Q12 */
    int32_t nlast, elast, last;    /* the last noise, the last cascade output, the last 22050 Hz sample */
    int32_t dk;                    /* the duck: how far the music is lowered, Q15 (0: not at all) */
} vce;

/* ------------------------------------------------------------ maths --- */
/* cos over a quarter turn, 64 steps (and two more), Q15 */
static const int16_t VO_COS[66] = {
    32767, 32757, 32728, 32678, 32609, 32521, 32412, 32285, 32137, 31971, 31785, 31580, 31356, 31113,
    30852, 30571, 30273, 29956, 29621, 29268, 28898, 28510, 28105, 27683, 27245, 26790, 26319, 25832,
    25329, 24811, 24279, 23731, 23170, 22594, 22005, 21403, 20787, 20159, 19519, 18868, 18204, 17530,
    16846, 16151, 15446, 14732, 14010, 13279, 12539, 11793, 11039, 10278, 9512, 8739, 7962, 7179, 6393,
    5602, 4808, 4011, 3212, 2410, 1608, 804, 0, -804};
static int32_t vo_qcos(uint32_t a)  /* a: 0..16384 (a quarter turn) */
{
    uint32_t i = a >> 8, f = a & 0xFFu;
    return VO_COS[i] + (((VO_COS[i + 1] - VO_COS[i]) * (int32_t)f) >> 8);
}
static int32_t vo_cos(uint32_t hz)  /* cos(2 pi hz / VO_FS), Q15 */
{
    uint32_t a = (uint32_t)(((uint64_t)hz * 194783u) >> 16) & 0xFFFFu;   /* a turn = 65536 */
    uint32_t q = a >> 14, w = a & 0x3FFFu;
    if (q == 0u)
        return vo_qcos(w);
    if (q == 1u)
        return -vo_qcos(16384u - w);
    if (q == 2u)
        return -vo_qcos(w);
    return vo_qcos(16384u - w);
}
/* a pole pair's coefficients for a centre and a bandwidth, Q28 */
static void vo_coef(uint32_t hz, uint32_t bw, int32_t *b, int32_t *c)
{
    int32_t x = (int32_t)(bw * 38246u);                                  /* pi bw / fs, Q28 */
    int32_t x2 = (int32_t)(((int64_t)x * x) >> 28), x3 = (int32_t)(((int64_t)x2 * x) >> 28);
    int32_t r = (1 << 28) - x + x2 / 2 - x3 / 6;                          /* e^-x */
    *b = (int32_t)(((int64_t)r * vo_cos(hz)) >> 14);                      /* 2 r cos */
    *c = -(int32_t)(((int64_t)r * r) >> 28);
}
static void vo_res_set(vo_res_t *r, uint32_t hz, uint32_t bw)
{
    vo_coef(hz, bw, &r->b, &r->c);
    r->a = (1 << 28) - r->b - r->c;                                       /* gain 1 at 0 Hz */
}
static inline int32_t vo_res(vo_res_t *r, int32_t x)
{
    int32_t y = (int32_t)(((int64_t)r->a * x + (int64_t)r->b * r->y1 + (int64_t)r->c * r->y2) >> 28);
    r->y2 = r->y1;
    r->y1 = y;
    return y;
}

/* ----------------------------------------------- phonemes to segments --- */
static int vo_kind(uint32_t ph) { return ph < P_N_ ? VO_PH[ph].kind : K_SIL; }
static int vo_son(uint32_t ph)   /* the voiced sounds a transition can move through */
{
    int k = vo_kind(ph);
    return k == K_VOW || k == K_DIPH || k == K_LIQ || k == K_NAS;
}
/* the SPEECH RATE, 0 .. VO_NRATE - 1 (the sounds from here on) */
static void vo_rate(uint32_t i) { vo_pct = VO_RATE[i < VO_NRATE ? i : 1u]; }
static uint8_t vo_frames(uint32_t ms)
{
    uint32_t f = (ms * vo_pct * 345u / 100u + 500u) / 1000u + 1u;
    return (uint8_t)(f > 255u ? 255u : f);
}
static vo_seg_t *vo_add(void)
{
    vo_seg_t *s = &vce.sg[(vce.sh + vce.sn) % VO_NSEG];
    const uint16_t *pf = vce.sn ? vce.sg[(vce.sh + vce.sn - 1u) % VO_NSEG].f : vce.pf;
    s->f[0] = pf[0];               /* (the formants where they were: a silence, a fricative's start) */
    s->f[1] = pf[1];
    s->f[2] = pf[2];
    s->ff = s->nz = 0;
    s->b[0] = 7;
    s->b[1] = 10;
    s->b[2] = 15;
    s->fb = 0;
    s->av = s->af = s->ah = s->flat = 0;
    s->n = 1;
    s->tin = 0;
    s->fwd = 0;
    s->f0 = 0;
    vce.sn++;
    return s;
}
static void vo_targets(vo_seg_t *s, const vo_ph_t *p, int glide)
{
    s->f[0] = glide ? p->g1 : p->f1;
    s->f[1] = glide ? p->g2 : p->f2;
    s->f[2] = glide ? p->g3 : p->f3;
}
static uint32_t vo_peek(uint32_t k)   /* the k-th byte not yet read (VO_NONE: none) */
{
    return vo_r + k < vo_w ? vo_q[(vo_r + k) % VO_Q] : VO_NONE;
}
/* the next phoneme from the ring, made into one to three segments (room for three: vce.sn <= VO_NSEG - 3) */
static void vo_expand(void)
{
    uint32_t b = vo_peek(0), c = b & 0x3Fu, st = b >> 6, nb = vo_peek(1), next = nb & 0x3Fu, prev = vce.prevph;
    const vo_ph_t *p;
    vo_seg_t *s;
    uint32_t dur;
    int last = nb == VO_NONE || next == VO_STOP || next == VO_PAUSE;
    vo_r++;
    if (nb == VO_NONE || next >= P_N_)
        next = P_SIL;
    if (c == VO_PAUSE || c == VO_STOP) {
        s = vo_add();
        s->n = vo_frames(c == VO_STOP ? 220u : 60u);
        s->tin = 6;
        s->f0 = (int8_t)(c == VO_STOP ? -18 : -4);
        vce.prevph = P_SIL;
        return;
    }
    if (c >= P_N_)
        return;
    p = &VO_PH[c];
    dur = p->dur;
    if (p->kind == K_VOW || p->kind == K_DIPH)        /* stress: 1 primary, 2 secondary, 0 none */
        dur = st == 1u ? dur : st == 2u ? dur * 4u / 5u : dur * 3u / 5u;
    if (last && vo_son(c))
        dur = dur * 5u / 4u;                           /* a phrase's last sound is longer */
    switch (p->kind) {
    case K_SIL:
        s = vo_add();
        s->n = vo_frames(dur);
        break;
    case K_VOW:
    case K_DIPH: {
        int glide = p->kind == K_DIPH;
        uint32_t n1 = glide ? dur * 9u / 20u : dur;
        int8_t f0 = (int8_t)(last && nb != VO_NONE && next == P_SIL ? -14 : st == 1u ? 16 : st == 2u ? 6 : 0);
        s = vo_add();
        vo_targets(s, p, 0);
        s->av = p->av;
        s->n = vo_frames(n1);
        s->tin = (uint8_t)(vo_son(prev) ? 18 : 12);
        s->fwd = (uint8_t)(vo_kind(prev) == K_STOP);
        s->f0 = f0;
        if (glide) {
            s = vo_add();
            vo_targets(s, p, 1);
            s->av = p->av;
            s->n = vo_frames(dur - n1);
            s->tin = s->n;
            s->fwd = 1;
            s->f0 = (int8_t)(f0 - 4);
        }
        break;
    }
    case K_LIQ:
    case K_NAS:
        s = vo_add();
        vo_targets(s, p, 0);
        s->av = p->av;
        s->nz = p->nz;
        if (p->kind == K_NAS) {
            s->b[0] = 10;
            s->b[1] = 20;
            s->b[2] = 30;
        }
        s->n = vo_frames(dur);
        s->tin = (uint8_t)(c == P_W || c == P_Y ? 10 : 8);
        break;
    case K_FRIC:
        s = vo_add();
        vo_targets(s, p, 0);
        s->av = p->av;
        s->af = p->af;
        s->ff = p->ff;
        s->fb = (uint8_t)(p->fb / 10u);
        s->flat = (uint8_t)(p->ff == 0);
        s->n = vo_frames(dur);
        s->tin = 8;
        break;
    case K_ASP:                    /* HH: the next vowel's shape, breathed */
        s = vo_add();
        if (next != P_SIL)
            vo_targets(s, &VO_PH[next], 0);
        s->ah = p->ah;
        s->n = vo_frames(dur);
        s->tin = 2;
        break;
    case K_STOP:
    case K_AFF:
        s = vo_add();              /* the closure (a voiced stop's murmur) */
        vo_targets(s, p, 0);
        s->av = (uint8_t)(p->voiced ? 14 : 0);
        s->f[0] = 200;
        s->n = vo_frames(p->kind == K_AFF ? 45u : dur);
        s->tin = 6;
        s = vo_add();              /* the burst (an affricate's frication) */
        vo_targets(s, p, 0);
        s->af = p->af;
        s->ff = p->ff;
        s->fb = (uint8_t)(p->fb / 10u);
        s->flat = (uint8_t)(p->ff == 0);
        s->n = vo_frames(p->kind == K_AFF ? (p->voiced ? 55u : 75u) : 8u);
        s->av = (uint8_t)(p->kind == K_AFF && p->voiced ? 30 : 0);
        s->fwd = 1;
        if (!p->voiced && p->kind == K_STOP && prev != P_S) {   /* aspiration into what follows (not sp, st, sk) */
            s = vo_add();
            if (vo_son(next))
                vo_targets(s, &VO_PH[next], 0);
            s->ah = 40;
            s->n = vo_frames(vo_son(next) ? 45u : 15u);
            s->tin = s->n;
            s->fwd = 1;
        }
        break;
    }
    vce.prevph = (uint8_t)c;
}

/* ------------------------------------------------------- the frames --- */
static int32_t vo_lerp(int32_t a, int32_t b, int32_t num, int32_t den) { return den <= 0 ? b : a + (b - a) * num / den; }
/* the targets for this frame: each formant moves from the last segment's to this one's over tin frames, centred
 * on the boundary, or after it for a release (fwd) */
static void vo_frame(void)
{
    const vo_seg_t *s = &vce.sg[vce.sh], *nx = vce.sn > 1u ? &vce.sg[(vce.sh + 1u) % VO_NSEG] : s;
    int32_t k = (int32_t)vce.k, n = s->n, f[3], hin, hout, nz, pz, nnz;
    uint32_t i;
    hin = s->fwd ? s->tin : s->tin / 2;
    hout = nx != s && !nx->fwd ? nx->tin / 2 : 0;
    if (hin > n)
        hin = n;
    if (hout > n / 2)
        hout = n / 2;
    for (i = 0; i < 3u; i++) {
        int32_t cur = s->f[i];
        if (k < hin)
            f[i] = s->fwd ? vo_lerp(vce.pf[i], cur, k, hin) : vo_lerp(vce.pf[i], cur, k + hin, 2 * hin);
        else if (hout && k >= n - hout)
            f[i] = vo_lerp(cur, nx->f[i], k - (n - hout), 2 * hout);
        else
            f[i] = cur;
    }
    vo_res_set(&vce.r[0], (uint32_t)f[0], s->b[0] * 10u + (uint32_t)(f[0] > 500 ? (f[0] - 500) / 10 : 0));
    vo_res_set(&vce.r[1], (uint32_t)f[1], s->b[1] * 10u);
    vo_res_set(&vce.r[2], (uint32_t)f[2], s->b[2] * 10u);
    vo_res_set(&vce.r[3], 3300u, 250u);
    vo_res_set(&vce.r[4], 3850u, 300u);
    nz = s->nz ? s->nz : 270;      /* the nasal pole and zero: the same (cancelled) unless nasal */
    pz = vce.pnz ? vce.pnz : 270;
    nnz = nx->nz ? nx->nz : 270;
    if (k < 4 && pz != nz)
        nz = vo_lerp(pz, nz, k + 4, 8);
    else if (k >= n - 4 && nnz != nz)
        nz = vo_lerp(nz, nnz, k - (n - 4), 8);
    vo_coef(270u, 100u, &vce.np.b, &vce.np.c);
    vo_coef((uint32_t)nz, 100u, &vce.nz_b, &vce.nz_c);
    {   /* the pole's gain at 0 Hz is 1 / ap, the zero's az: scaled by ap / az */
        int32_t ap = (1 << 28) - vce.np.b - vce.np.c, az = (1 << 28) - vce.nz_b - vce.nz_c;
        vce.nz_g = (ap >> 4) / ((az >> 16) | 1);
    }
    if (s->af)
        vo_res_set(&vce.rf, s->ff ? s->ff : 3000u, s->fb ? s->fb * 10u : 3000u);
    vce.flat = s->flat;
    vce.av += ((s->av << 8) - vce.av) >> 1;   /* amplitudes: halfway to the target each frame (3 ms) */
    vce.af += ((s->af << 8) - vce.af) >> 1;
    vce.ah += ((s->ah << 8) - vce.ah) >> 1;
    vce.f0 += (((VO_BASE + s->f0) << 8) - vce.f0) >> 4;
    vce.inc = (uint32_t)(((uint64_t)(uint32_t)vce.f0 * 194783u) >> 8);
    if (++vce.k >= (uint32_t)n) {   /* the segment done: its targets are where the next starts from */
        vce.k = 0;
        vce.pf[0] = s->f[0];
        vce.pf[1] = s->f[1];
        vce.pf[2] = s->f[2];
        vce.pnz = s->nz;
        vce.sh = (vce.sh + 1u) % VO_NSEG;
        vce.sn--;
    }
}
static inline int32_t vo_rand(void)   /* white noise, about +-16384 */
{
    vce.noise = vce.noise * 1664525u + 1013904223u;
    return (int32_t)vce.noise >> 17;
}
/* one sample at 22050 Hz, Q15 */
static int32_t vo_sample(void)
{
    int32_t g = 0, x, fr = 0, y;
    uint32_t i, ph;
    if (vce.sub == 0u) {
        if (vce.seen_cut != vo_cut_seq) {          /* cut off: what was being said, gone; the new from its start */
            vce.seen_cut = vo_cut_seq;
            vo_r = vo_cut_pos;
            vce.sn = 0;
            vce.k = 0;
            vce.prevph = P_SIL;
        }
        while (vce.sn <= VO_NSEG - 3u && vce.sn < 3u && vo_r < vo_w)
            vo_expand();
        if (vce.sn)
            vo_frame();
        else {                                    /* nothing to say: fading out */
            vce.av >>= 1;
            vce.af >>= 1;
            vce.ah >>= 1;
        }
        vce.speaking = (uint8_t)(vce.sn || vce.av >= 16 || vce.af >= 16 || vce.ah >= 16);
    }
    vce.sub = (vce.sub + 1u) % VO_FRAME;
    if (!vce.speaking)
        return 0;
    vce.phase += vce.inc;
    ph = vce.phase;
    {   /* the glottal pulse (KLGLOTT88): the flow's derivative 2x - 3x^2 over the open phase, then the sudden
         * closure: the voice's brightness */
        uint32_t pq = ph >> 16;
        if (pq < VO_OPEN) {
            int32_t xq = (int32_t)((pq * VO_OPEN_INV) >> 14);             /* Q16 */
            g = (2 * xq - (int32_t)(((int64_t)3 * xq * xq) >> 16)) >> 1;  /* +10923 .. -32768 */
        }
    }
    x = (g * (vce.av >> 8)) >> 6;
    x += (((vo_rand() * (vce.ah >> 8)) >> 6) * (ph < 0x80000000u ? 2 : 1)) >> 1;   /* aspiration, pulsed */
    y = (int32_t)(x + (((int64_t)vce.np.b * vce.np.y1 + (int64_t)vce.np.c * vce.np.y2) >> 28));   /* nasal pole */
    vce.np.y2 = vce.np.y1;
    vce.np.y1 = y;
    x = (int32_t)(y - (((int64_t)vce.nz_b * vce.nz_x1 + (int64_t)vce.nz_c * vce.nz_x2) >> 28));  /* .. and zero */
    x = (x * vce.nz_g) >> 12;
    vce.nz_x2 = vce.nz_x1;
    vce.nz_x1 = y;
    for (i = 5; i-- > 0;)                         /* F5 .. F1 */
        x = vo_res(&vce.r[i], x);
    {   /* the cascade falls too fast above F5 (a real tract has more formants): lift 6 dB an octave */
        int32_t e = x - ((vce.elast * 28) >> 5);
        vce.elast = x;
        x = e * 2;
    }
    if (vce.af >= 16) {   /* frication, in parallel: rising 6 dB an octave, then a resonance or flat; a voiced
                          * fricative's noise beats with the voice */
        int32_t r = vo_rand(), n2 = ((r - vce.nlast) * (vce.af >> 8)) >> 7;
        vce.nlast = r;
        if (vce.av >= 256 && ph >= 0x80000000u)
            n2 >>= 1;
        fr = vce.flat ? n2 >> 2 : vo_res(&vce.rf, n2) >> 3;
    }
    return (x >> 1) + fr;
}

/* (audio) n samples at 44100 Hz into v, Q15; the duck follows */
static void vo_block(int32_t *v, uint32_t n)
{
    uint32_t i;
    int32_t target;
    for (i = 0; i < n; i += 2u) {
        int32_t s = vo_sample();
        v[i] = (vce.last + s) >> 1;
        if (i + 1u < n)
            v[i + 1u] = s;
        vce.last = s;
    }
    target = vce.speaking ? 16384 : 0;            /* the music 6 dB down while it speaks */
    vce.dk += target > vce.dk ? (target - vce.dk + 7) >> 3 : -((vce.dk - target + 7) >> 3);
}

/* ------------------------------------------------ the main loop's side --- */
static void vo_init(void)
{
    vce.noise = 0x1234567u;
    vce.f0 = VO_BASE << 8;
    vce.pf[0] = 500;
    vce.pf[1] = 1500;
    vce.pf[2] = 2500;
}
static int vo_busy(void) { return vo_r != vo_w || vce.speaking; }
static void vo_put(uint32_t b)
{
    if (vo_wl - vo_wbase < VO_Q - 2u)             /* (full: the rest is dropped; VO_STOP still fits) */
        vo_q[vo_wl++ % VO_Q] = (uint8_t)b;
}
static void vo_begin(int now)
{
    vo_wl = vo_w;
    vo_wbase = vo_r;
    if (now) {
        vo_cut_pos = vo_w;
        vo_wbase = vo_w;
        RING_PUBLISH();
        vo_cut_seq++;
    }
}
static void vo_end(void)
{
    vo_q[vo_wl++ % VO_Q] = VO_STOP;
    RING_PUBLISH();
    vo_w = vo_wl;
}

/* the words: voice_dict.h holds them sorted, each its letters (lower case), a 0, then its phonemes */
#include "voice_dict.h"
static int vo_word(const char *w, uint32_t n)     /* put a word's phonemes; 0 if it isn't there */
{
    int lo = 0, hi = (int)VO_NDICT - 1;
    while (lo <= hi) {
        int m = (lo + hi) / 2, c = 0;
        const uint8_t *e = VO_DICT + VO_DOFF[m];
        uint32_t i;
        for (i = 0; i < n && !c; i++)
            c = (int)(uint8_t)w[i] - (int)e[i];
        if (!c)
            c = -(int)e[n];                       /* (the entry longer: it comes after) */
        if (!c) {
            for (i = n + 1u; VO_DOFF[m] + i < VO_DOFF[m + 1]; i++)
                vo_put(e[i]);
            return 1;
        }
        if (c < 0)
            hi = m - 1;
        else
            lo = m + 1;
    }
    return 0;
}
static void vo_say_word(const char *w) { vo_word(w, str_len(w)); }
static void vo_number(uint32_t v)                 /* 0 .. 999 999, as words */
{
    static const char *const ONES[20] = {"zero", "one", "two", "three", "four", "five", "six", "seven", "eight",
        "nine", "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen",
        "nineteen"};
    static const char *const TENS[10] = {"", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy",
        "eighty", "ninety"};
    if (v >= 1000u) {
        vo_number(v / 1000u);
        vo_say_word("thousand");
        if (!(v %= 1000u))
            return;
    }
    if (v >= 100u) {
        vo_say_word(ONES[v / 100u]);
        vo_say_word("hundred");
        if (!(v %= 100u))
            return;
    }
    if (v >= 20u) {
        vo_say_word(TENS[v / 10u]);
        if (v % 10u)
            vo_say_word(ONES[v % 10u]);
    } else
        vo_say_word(ONES[v]);
}
static int vo_alpha(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
static int vo_digit(char c) { return c >= '0' && c <= '9'; }
/* a line of the screen's text, as phonemes */
static void vo_text(const char *s)
{
    char w[24];
    int after_num = 0;
    while (*s) {
        char c = *s;
        uint32_t n = 0;
        if (vo_digit(c) || ((c == '-' || c == '+') && vo_digit(s[1]))) {
            uint32_t v = 0;
            if (c == '-' || c == '+') {
                vo_say_word(c == '-' ? "minus" : "plus");
                s++;
            }
            while (vo_digit(*s) && n++ < 6u)
                v = v * 10u + (uint32_t)(*s++ - '0');
            while (vo_digit(*s))
                s++;
            vo_number(v);
            if (*s == '.' && vo_digit(s[1])) {    /* the decimals, one by one */
                vo_say_word("point");
                for (s++; vo_digit(*s); s++)
                    vo_number((uint32_t)(*s - '0'));
            }
            after_num = 1;
            continue;
        }
        if (vo_alpha(c)) {
            static const char *const UNIT[5][2] = {{"ms", "milliseconds"}, {"s", "seconds"}, {"db", "decibels"},
                                                   {"hz", "hertz"}, {"khz", "kilohertz"}};
            uint32_t i;
            while (vo_alpha(*s) && n < sizeof w - 1u) {
                char l = *s++;
                w[n++] = (char)(l >= 'A' && l <= 'Z' ? l - 'A' + 'a' : l);
            }
            while (vo_alpha(*s))
                s++;
            w[n] = 0;
            if (after_num)
                for (i = 0; i < 5u; i++)
                    if (str_eq(w, UNIT[i][0])) {
                        vo_say_word(UNIT[i][1]);
                        n = 0;
                        break;
                    }
            if (n && !vo_word(w, n))              /* not a word it knows: spelt */
                for (i = 0; i < n; i++) {
                    char l[2] = {w[i], 0};
                    vo_say_word(l);
                }
            after_num = 0;
            continue;
        }
        if (c == '%')
            vo_say_word("percent");
        else if (c == '#')
            vo_say_word("sharp");
        else if (c == '+')
            vo_say_word("plus");
        else if (c == '/' || c == ':' || c == ',')
            vo_put(VO_PAUSE);
        after_num = 0;
        s++;
    }
}

/* -------------------------------------------------------- the screen --- */
#define SP_NB 12u                   /* the bands remembered */
#define SP_NP 6u                    /* the changes waiting to be spoken */
#define SP_LEN 28u
static struct {
    struct { uint16_t key; uint8_t hl, age; uint32_t h; } b[SP_NB];
    struct { uint16_t key; uint8_t hl; char t[SP_LEN]; } p[SP_NP];
    uint32_t np, t_first, t_last, now;
    char cap[SP_LEN];
    uint32_t cap_n;
    uint8_t cap_hl, on, clock;
    uint8_t intro;                  /* (setup.c) turned on (1) or off (2) with its keys: said so */
} sp;
/* (gfx.c) a band begun; text drawn on it (dark: on a light ground, chosen); the band sent at (x, y) */
static void sp_cap_begin(void)
{
    sp.cap_n = 0;
    sp.cap_hl = 0;
}
static void sp_cap_text(const char *s, uint16_t c)
{
    uint32_t lum = (c >> 11) + ((c >> 6) & 31u) + (c & 31u);   /* 0 .. 93 */
    if (!sp.on || !*s)
        return;
    if (lum < 24u)
        sp.cap_hl = 1;
    if (sp.cap_n && sp.cap_n < SP_LEN - 1u)
        sp.cap[sp.cap_n++] = ' ';
    while (*s && sp.cap_n < SP_LEN - 1u)
        sp.cap[sp.cap_n++] = *s++;
}
static void sp_cap_commit(uint32_t x, uint32_t y)
{
    uint32_t i, key = y * 128u + x / 2u, h = 2166136261u, slot = SP_NB, best = 0;
    int changed, gained;
    if (!sp.on)
        return;
    for (i = 0; i < sp.cap_n; i++)
        h = (h ^ (uint8_t)sp.cap[i]) * 16777619u;
    for (i = 0; i < SP_NB && slot == SP_NB; i++)
        if (sp.b[i].age && sp.b[i].key == key)
            slot = i;
    if (slot == SP_NB) {                          /* a band not seen: a free place, or the one unused longest */
        for (i = 0; i < SP_NB; i++) {
            uint32_t d = sp.b[i].age ? (uint8_t)(sp.clock - sp.b[i].age) : 256u;
            if (d >= best) {
                best = d;
                slot = i;
            }
        }
    }
    changed = sp.b[slot].key != key || !sp.b[slot].age || sp.b[slot].h != h;
    gained = sp.cap_hl && (!sp.b[slot].hl || changed);
    sp.b[slot].key = (uint16_t)key;
    sp.b[slot].h = h;
    sp.b[slot].hl = sp.cap_hl;
    sp.b[slot].age = ++sp.clock ? sp.clock : ++sp.clock;
    for (i = 0; i < sp.np && sp.p[i].key != key; i++)
        ;
    if (!changed && !gained)
        return;
    if (!sp.cap_n) {                              /* a band emptied: nothing to say, and not what it said */
        if (i < sp.np)
            sp.p[i] = sp.p[--sp.np];
        return;
    }
    if (i == sp.np) {
        if (sp.np == SP_NP)
            return;
        sp.np++;
    }
    sp.p[i].key = (uint16_t)key;
    sp.p[i].hl = sp.cap_hl;
    for (h = 0; h < sp.cap_n; h++)
        sp.p[i].t[h] = sp.cap[h];
    sp.p[i].t[sp.cap_n] = 0;
    if (sp.np == 1u && i == 0u)
        sp.t_first = sp.now;
    sp.t_last = sp.now;
}
#ifdef HOST
static char sp_said[4096];          /* (the host's tests: what was said, a line an utterance, "; " between bands) */
static uint32_t sp_said_n;
static void sp_log(const char *t)
{
    while (*t && sp_said_n < sizeof sp_said - 1u)
        sp_said[sp_said_n++] = *t++;
    sp_said[sp_said_n] = 0;
}
#else
#define sp_log(t) ((void)0)
#endif
/* (main loop, after the screen is drawn) */
static void sp_tick(int on, uint32_t ms)
{
    uint32_t i, j, hl = 0, top = 0;
    sp.now = ms;
    if (!on) {
        if (sp.on) {
            sp.on = 0;
            sp.np = 0;
            vo_begin(1);                          /* (silence at once) */
            vo_end();
        }
        return;
    }
    if (!sp.on) {
        for (i = 0; i < SP_NB; i++)
            sp.b[i].age = 0;
        sp.np = 0;
        sp.on = 1;
        return;
    }
    if (!sp.np || ((ms - sp.t_last) < 150u && (ms - sp.t_first) < 700u))
        return;
    for (i = 1; i < sp.np; i++)                   /* top down (by the band's place) */
        for (j = i; j > 0 && sp.p[j - 1u].key > sp.p[j].key; j--) {
            uint16_t k = sp.p[j].key;
            uint8_t l = sp.p[j].hl;
            char t[SP_LEN];
            str_cpy(t, sp.p[j].t, SP_LEN);
            sp.p[j].key = sp.p[j - 1u].key;
            sp.p[j].hl = sp.p[j - 1u].hl;
            str_cpy(sp.p[j].t, sp.p[j - 1u].t, SP_LEN);
            sp.p[j - 1u].key = k;
            sp.p[j - 1u].hl = l;
            str_cpy(sp.p[j - 1u].t, t, SP_LEN);
        }
    for (i = 0; i < sp.np; i++)
        hl += sp.p[i].hl;
    top = sp.np >= 3u && hl;                      /* a new page with a row chosen: its title and that row */
    vo_begin(1);
    if (sp.intro == 1u) {                         /* (turned on with its keys: said so first) */
        sp.intro = 0;
        vo_text("SCREEN READER ON");
        vo_put(VO_PAUSE);
        sp_log("SCREEN READER ON; ");
    }
    for (i = 0; i < sp.np; i++)
        if ((!top || i == 0u || sp.p[i].hl) && !(i && str_eq(sp.p[i].t, sp.p[i - 1u].t))) {   /* (text across two
                                                                                                * bands: once) */
            vo_text(sp.p[i].t);
            vo_put(VO_PAUSE);
            sp_log(sp.p[i].t);
            sp_log("; ");
        }
    vo_end();
    sp_log("\n");
    sp.np = 0;
}
