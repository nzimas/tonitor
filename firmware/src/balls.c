/* SPDX-License-Identifier: GPL-3.0-only
 * TONITOR: BALLS, a sequencer of bouncing balls, one per track (SHIFT + EDIT, on and off; EDIT blinks while the
 * chosen track's is on). One to eight balls move in a box; each time one hits a wall it plays a note on the
 * track. Each wall holds the scale's notes, one segment a step, an octave apart: from the bottom (the lowest)
 * clockwise -- left, top, right -- the pitch rising all the way round (the bottom from right to left, the left
 * upwards, the top from left to right, the right downwards). By default the bottom is the octave below the
 * keyboard's, the left the keyboard's own, the top one above, the right two above. Each bounce bends a ball's
 * way by up to 12 degrees at random, kept 15 .. 75 degrees off the walls: its path wanders, and never settles
 * between two walls. Each hit's note has a length (50 ms .. 1.5 s) and a level (-10 dB .. full) drawn at random.
 *
 * INSTABILITY (KNOB 4): each ball has a wind of its own, two slowly wandering random values (a new aim drawn
 * twice a second on average, followed over ~0.3 s): one sets its speed (from an eighth to eight times KNOB 2's,
 * as far as the knob goes -- near a halt, then rushing on), the other turns it (up to half a turn a second); and
 * now and then a gust throws it off its way (up to 60 degrees). At 0: none of it, a steady pace and straight
 * lines.
 *
 * Its page (shown as it starts; afterwards EDIT held 0.6 s, up or down -- the balls play on either way, the other
 * controls and pages as usual meanwhile): KNOB 1 how many (1 .. 8, each sent off in a direction drawn at
 * random), KNOB 2 their speed, KNOB 3 the four octaves (an octave a click, over the whole range), KNOB 4 their
 * instability (0 .. 100).
 *
 * It runs in the main loop on the milliseconds that have passed (in steps of at most 4 ms), so it keeps time
 * whatever the screen costs; its notes go as a key's do (the scale, the loops' transposition, chords, the
 * loopers recording them). */

#define BB_MAX 8
#define BB_ONE (1 << 24)         /* the box's side, Q24 */
#define BB_R 360000              /* a ball's radius, Q24 (~5 px) */
#define BB_OFFS 48
#define BB_LO (-3)               /* the window's lowest place: the bottom wall's octave, from the keyboard's */
#define BB_HI 2

typedef struct {
    int32_t x, y;                /* Q24 (y down) */
    uint32_t th;                 /* its way, 2^32 a turn (0: right, a quarter: down) */
    int32_t ws, wa, ts, ta;      /* its wind: speed and turn, Q15 -1 .. 1, and where each is heading */
} bb_ball_t;
static struct {
    uint8_t on[NLAYER];
    int8_t n[NLAYER], speed[NLAYER], base[NLAYER], unst[NLAYER];   /* the knobs: 1 .. 8, 0 .. 100, BB_LO .. BB_HI, 0 .. 100 */
    bb_ball_t b[NLAYER][BB_MAX];
    uint32_t lit[NLAYER][4][7];  /* a segment hit: lit until then */
    struct { uint8_t layer, note, form; uint32_t at; } off[BB_OFFS];   /* the notes playing, let go at .at */
    uint32_t t;                  /* fm1_ms of the last step */
    uint32_t hits[4];            /* (the walls' hits, counted: the test reads them) */
    rnd_t r;
    int8_t touched;              /* the knob just turned (-1 none), until touch_until */
    uint32_t touch_until, drawn_at;
} bb;

static void bb_defaults(uint32_t l)
{
    bb.on[l] = 0;
    bb.n[l] = 1;
    bb.speed[l] = 50;
    bb.base[l] = -1;
    bb.unst[l] = 0;
}
/* |v| for a speed: 0.08 x 30^(s / 100) boxes a second (0.08 .. 2.4), Q24 */
static int32_t bb_vel(int32_t s) { return (int32_t)(((int64_t)(BB_ONE / 100) * 8 * exp2_q16(clampi(s, 0, 100) * 3216)) >> 16); }
#define BB_A0 178956971u         /* 15 degrees */
#define BB_A1 894784853u         /* 75 */
#define BB_BEND 143165577        /* 12 */
#define BB_GUST 715827883        /* 60 */
#define BB_VMAX (3 * BB_ONE)     /* (the fastest a wind may take it: 3 boxes a second) */

/* ball k of track l from near the middle, in a direction drawn away from the walls' axes */
static void bb_launch(uint32_t l, uint32_t k)
{
    bb_ball_t *b = &bb.b[l][k];
    (void)l;
    b->th = (uint32_t)rnd_in(&bb.r, 0x0E38E38E, 0x31C71C71) + (rnd_next(&bb.r) & 0xC0000000u);   /* 20 .. 70, a quadrant */
    b->x = BB_ONE / 2 + rnd_in(&bb.r, -BB_ONE / 8, BB_ONE / 8);
    b->y = BB_ONE / 2 + rnd_in(&bb.r, -BB_ONE / 8, BB_ONE / 8);
    b->ws = b->wa = b->ts = b->ta = 0;
}
/* a bounce bends the way a little, at random, and keeps it 15 .. 75 degrees off the walls (in its quadrant) */
static void bb_bend(bb_ball_t *b)
{
    uint32_t q = b->th & 0xC0000000u;
    int32_t a = (int32_t)(b->th & 0x3FFFFFFFu) + rnd_in(&bb.r, -BB_BEND, BB_BEND);
    b->th = q | (uint32_t)clampi(a, (int32_t)BB_A0, (int32_t)BB_A1);
}
static void bb_start(uint32_t l)
{
    uint32_t k;
    for (k = 0; k < BB_MAX; k++)
        bb_launch(l, k);
    memset(bb.lit[l], 0, sizeof bb.lit[l]);
}
/* every note of track l playing, let go (all tracks if l is NLAYER) */
static void bb_release(uint32_t l)
{
    uint32_t i;
    for (i = 0; i < BB_OFFS; i++)
        if (bb.off[i].at && (l >= NLAYER || bb.off[i].layer == l)) {
            chord_press(bb.off[i].layer, bb.off[i].note, bb.off[i].form, 0);
            bb.off[i].at = 0;
        }
}
/* the note of segment s of wall j (0 bottom, 1 left, 2 top, 3 right) on track l */
static uint32_t bb_note(uint32_t l, uint32_t j, uint32_t s)
{
    return scale_note((int32_t)s + ui.lx + ui.lxt[l], bb.base[l] + (int32_t)j);
}
/* a hit: segment s of wall j sounds on track l (as a key: chords too), and lights */
static void bb_hit(uint32_t l, uint32_t j, uint32_t s)
{
    uint32_t i, slot = 0, f1 = ui.chord_on[l] ? ui.chord_form[l] + 1u : 0u, note = bb_note(l, j, s);
    uint32_t len = (50u * exp2_q16(rnd_in(&bb.r, 0, 322000))) >> 16;   /* 50 ms .. 1.5 s (x 2^4.9) */
    for (i = 0; i < BB_OFFS; i++) {                 /* the same note sounding: let go first (struck again) */
        if (bb.off[i].at && bb.off[i].layer == l && bb.off[i].note == note) {
            chord_press(l, note, bb.off[i].form, 0);
            bb.off[i].at = 0;
        }
    }
    for (i = 0; i < BB_OFFS; i++) {                 /* a free slot, else the one ending soonest */
        if (!bb.off[i].at) {
            slot = i;
            break;
        }
        if ((int32_t)(bb.off[i].at - bb.off[slot].at) < 0)
            slot = i;
    }
    if (bb.off[slot].at)
        chord_press(bb.off[slot].layer, bb.off[slot].note, bb.off[slot].form, 0);
    press_vel = (uint8_t)rnd_in(&bb.r, 40, 126);    /* (-10 dB .. full) */
    chord_press(l, note, f1, 1);
    press_vel = 0;
    bb.off[slot].layer = (uint8_t)l;
    bb.off[slot].note = (uint8_t)note;
    bb.off[slot].form = (uint8_t)f1;
    bb.off[slot].at = (fm1_ms + len) | 1u;
    bb.lit[l][j][s % 7u] = (fm1_ms + 150u) | 1u;
    bb.hits[j]++;
}
/* one step of dt ms for track l's balls */
static void bb_step(uint32_t l, int32_t dt)
{
    uint32_t k, n = SCALE_N[ui.scale];
    int32_t v0 = bb_vel(bb.speed[l]), kq = bb.unst[l] * 327;   /* (instability, Q15) */
    for (k = 0; k < (uint32_t)bb.n[l]; k++) {
        bb_ball_t *b = &bb.b[l][k];
        int32_t lo = BB_R, hi = BB_ONE - BB_R, v = v0, vx, vy;
        if (kq) {                                   /* its wind */
            if (rnd_next(&bb.r) < (uint32_t)dt * 8589934u) {   /* (twice a second on average) a new aim */
                b->ts = rnd_bi(&bb.r);
                b->ta = rnd_bi(&bb.r);
            }
            b->ws += (b->ts - b->ws) * dt / 300;
            b->wa += (b->ta - b->wa) * dt / 300;
            v = (int32_t)(((int64_t)v0 * exp2_q16(((b->ws * kq) >> 15) * 6)) >> 16);   /* x 2^(+-3) */
            v = clampi(v, BB_ONE / 200, BB_VMAX);
            b->th += (uint32_t)(((b->wa * kq) >> 15) * dt * 65);   /* (up to half a turn a second) */
            if (rnd_next(&bb.r) < (uint32_t)(((uint64_t)(uint32_t)kq * dt * 2621u) >> 15) * 1000u)   /* a gust */
                b->th += (uint32_t)rnd_in(&bb.r, -BB_GUST, BB_GUST);
        }
        vx = (int32_t)(((int64_t)v * sin_ph(b->th + 0x40000000u)) >> 15);   /* (dt <= 4: 32 bits below) */
        vy = (int32_t)(((int64_t)v * sin_ph(b->th)) >> 15);
        b->x += vx * dt / 1000;
        b->y += vy * dt / 1000;
        if (b->y > hi) {                            /* the bottom (y down), right to left */
            b->y = 2 * hi - b->y;
            if (vy > 0)
                b->th = 0u - b->th;
            bb_bend(b);
            bb_hit(l, 0, (uint32_t)(((int64_t)(BB_ONE - b->x) * n) >> 24) % n);
        } else if (b->y < lo) {                     /* the top, left to right */
            b->y = 2 * lo - b->y;
            if (vy < 0)
                b->th = 0u - b->th;
            bb_bend(b);
            bb_hit(l, 2, (uint32_t)(((int64_t)b->x * n) >> 24) % n);
        }
        if (b->x < lo) {                            /* the left, upwards */
            b->x = 2 * lo - b->x;
            if (vx < 0)
                b->th = 0x80000000u - b->th;
            bb_bend(b);
            bb_hit(l, 1, (uint32_t)(((int64_t)(BB_ONE - b->y) * n) >> 24) % n);
        } else if (b->x > hi) {                     /* the right, downwards */
            b->x = 2 * hi - b->x;
            if (vx > 0)
                b->th = 0x80000000u - b->th;
            bb_bend(b);
            bb_hit(l, 3, (uint32_t)(((int64_t)b->y * n) >> 24) % n);
        }
        b->x = clampi(b->x, lo, hi);
        b->y = clampi(b->y, lo, hi);
    }
}
/* the main loop, every pass: the time since the last, in steps; the notes ending let go */
static void bb_tick(void)
{
    uint32_t now = fm1_ms, l, i;
    int32_t dt = (int32_t)(now - bb.t);
    bb.t = now;
    dt = dt > 200 ? 200 : dt;                       /* (a long stall: not a leap through the walls' notes) */
    while (dt > 0) {
        int32_t s = dt > 4 ? 4 : dt;
        for (l = 0; l < NLAYER; l++)
            if (bb.on[l])
                bb_step(l, s);
        dt -= s;
    }
    for (i = 0; i < BB_OFFS; i++)
        if (bb.off[i].at && (int32_t)(now - bb.off[i].at) >= 0) {
            chord_press(bb.off[i].layer, bb.off[i].note, bb.off[i].form, 0);
            bb.off[i].at = 0;
        }
}
/* SHIFT + EDIT: track l's balls on or off */
static void bb_toggle(uint32_t l)
{
    bb.on[l] = (uint8_t)!bb.on[l];
    if (bb.on[l]) {
        if (!bb.r.s)
            rnd_seed(&bb.r, fm1_ms ^ 0xB0B5u);
        bb_start(l);
        bb.t = fm1_ms;
    } else
        bb_release(l);
}
/* KNOB 1 .. 4 while the chosen track's balls are up */
static void bb_knob(uint32_t l, uint32_t j, int32_t s)
{
    if (j == 0u) {
        int32_t n = clampi(bb.n[l] + s, 1, BB_MAX), k;
        for (k = bb.n[l]; k < n; k++)
            bb_launch(l, (uint32_t)k);              /* (the new ones from the middle) */
        bb.n[l] = (int8_t)n;
    } else if (j == 1u)
        bb.speed[l] = (int8_t)clampi(bb.speed[l] + 2 * s, 0, 100);
    else if (j == 2u)
        bb.base[l] = (int8_t)clampi(bb.base[l] + (s > 0 ? 1 : -1), BB_LO, BB_HI);
    else
        bb.unst[l] = (int8_t)clampi(bb.unst[l] + 2 * s, 0, 100);
    bb.touched = (int8_t)j;
    bb.touch_until = fm1_ms + 1500u;
}

/* the page: a line of text (what was just turned, else how many balls) over the box; the walls'
 * segments dim in the track's colour, lit as they are hit; the balls white */
#define BB_X0 6
#define BB_Y0 30                 /* (the text above it, 0 .. 25; the top wall from 26) */
#define BB_W 228
#define BB_H 204
static uint16_t bb_dim(uint16_t c) { return (uint16_t)(((c >> 1) & 0x7BEFu) - ((c >> 3) & 0x18E3u)); }   /* (3/8 of each channel) */
static void bb_fmt_oct(char *m, int32_t o)
{
    if (o > 0)
        str_cpy(m + str_len(m), "+", 2);
    fmt_int(m + str_len(m), o);
}
static void draw_bb_page(void)
{
    uint32_t l = ui.sel, n = SCALE_N[ui.scale], j, s, k;
    uint16_t col = LAYER_COL[l];
    char m[24];
    int32_t t = (int32_t)(bb.touch_until - fm1_ms) > 0 ? bb.touched : -1;
    m[0] = 0;
    if (t <= 0) {                                   /* (as it stands: how many) */
        fmt_int(m, bb.n[l]);
        str_cpy(m + str_len(m), bb.n[l] == 1 ? " BALL" : " BALLS", 8);
    } else if (t == 1) {
        str_cpy(m, "SPEED ", sizeof m);
        fmt_int(m + str_len(m), bb.speed[l]);
    } else if (t == 2) {
        str_cpy(m, "OCT ", sizeof m);
        bb_fmt_oct(m, bb.base[l]);
        str_cpy(m + str_len(m), " TO ", 5);
        bb_fmt_oct(m, bb.base[l] + 3);
    } else {
        str_cpy(m, "INSTABILITY ", sizeof m);
        fmt_int(m + str_len(m), bb.unst[l]);
    }
    cv_begin(240, BB_Y0 - 4, C_BLACK);
    if (text_w(&FONT_S, m) <= 236 + 2 * ((int32_t)str_len(m) - 1))   /* (squeezed a little at most; else smaller) */
        cv_text_fit(0, &FONT_S, m, col);
    else
        cv_text_c((FONT_S.h - FONT_XS.h) / 2, &FONT_XS, m, col);
    cv_blit(0, 0);
    CV_AREA(0, BB_Y0 - 4, 240, 244 - BB_Y0, C_BLACK) {
        int32_t oy = BB_Y0 - 4;                     /* (the area's top on the screen: its drawing is placed from there) */
        for (j = 0; j < 4u; j++)                    /* the walls, a segment a step (pitch rising clockwise) */
            for (s = 0; s < n; s++) {
                int lit = bb.lit[l][j][s] && (int32_t)(bb.lit[l][j][s] - fm1_ms) > 0;
                uint16_t c = lit ? C_WHITE : bb_dim(col);
                int32_t a0 = (int32_t)s * (j & 1u ? BB_H : BB_W) / (int32_t)n + 2;
                int32_t a1 = (int32_t)(s + 1u) * (j & 1u ? BB_H : BB_W) / (int32_t)n - 2;
                if (j == 0u)                        /* bottom: right to left */
                    cv_rect(BB_X0 + BB_W - a1, BB_Y0 + BB_H - oy, a1 - a0, 4, c);
                else if (j == 2u)                   /* top: left to right */
                    cv_rect(BB_X0 + a0, BB_Y0 - 4 - oy, a1 - a0, 4, c);
                else if (j == 1u)                   /* left: upwards */
                    cv_rect(BB_X0 - 4, BB_Y0 + BB_H - a1 - oy, 4, a1 - a0, c);
                else                                /* right: downwards */
                    cv_rect(BB_X0 + BB_W, BB_Y0 + a0 - oy, 4, a1 - a0, c);
            }
        for (k = 0; k < (uint32_t)bb.n[l]; k++) {   /* the balls: discs of radius 5 */
            const bb_ball_t *b = &bb.b[l][k];
            int32_t cx = BB_X0 + (int32_t)(((int64_t)b->x * BB_W) >> 24);
            int32_t cy = BB_Y0 + (int32_t)(((int64_t)b->y * BB_H) >> 24) - oy, dy;
            static const uint8_t HW[11] = {2, 3, 4, 5, 5, 5, 5, 5, 4, 3, 2};
            for (dy = -5; dy <= 5; dy++)
                cv_rect(cx - HW[dy + 5], cy + dy, 2 * HW[dy + 5] + 1, 1, bb.on[l] ? C_WHITE : C_GREY);
        }
    }
    bb.drawn_at = fm1_ms;
}
