/* SPDX-License-Identifier: GPL-3.0-only
 * TONITOR: BRICKLAYER, a step sequencer built a note at a time, one per track (SHIFT + the envelope key, on and
 * off; the key blinks while the chosen track's is on; held 0.6 s while it is on, its page up or down). On, it
 * waits for PLAY: on its page PLAY starts it from its first step and stops it (blinking while it plays); on the
 * other pages PLAY is the looper's, which runs beside it.
 *
 * On its page REC is the entry: pressed, entry on or off (REC lit while on); while on, each white key adds its
 * note to the end of the sequence (and sounds); SHIFT + a white key takes that note out wherever it is; SHIFT +
 * REC takes out the last one; REC held 1 s empties it. Notes are kept as steps of the scale (and the octave they
 * were played at), so they follow the loops' transposition (SHIFT + OCT; SHIFT + the track's button + OCT), the
 * scale and the root, as latched notes do.
 *
 * The notes play in the order they were added, spread over LENGTH steps (KNOB 1, from as many as there are notes
 * up to 32) as evenly as can be -- a Euclidean rhythm. KNOB 2 the rate (0.5 .. 16 steps a second), KNOB 3
 * HARMONY (how often a note becomes a diad -- a third or a fifth above -- or a triad), KNOB 4 VARIATION: each
 * note's level and length spread (none at 0: a steady level, 70 % of the step); the pace wandering (from half to
 * twice), now and then stopping a moment or rushing; and, as likely as it says, stutters (a note struck 2 .. 4
 * times within its step), beat repeats (the last 2 .. 4 steps played again), echoes (1 .. 3, quieter) and ghost
 * notes (an empty step sounding one of the sequence's notes, quietly).
 * On its page the loops' direction and speed keys are its own: REVERSE, PENDULUM, RANDOM (a step drawn at random
 * each time), HALF and DOUBLE SPEED.
 *
 * It runs in the main loop on the milliseconds that have passed, as BALLSY does; its notes go as a key's do, but
 * the looper does not record them (ui.c seq_press). */

#define BL_MAX 32
#define BL_Q 24                  /* notes to come (echoes, stutters) and sounding, waiting for their time */

static struct {
    uint8_t on[NLAYER], n[NLAYER], len[NLAYER];
    uint8_t run[NLAYER];         /* playing (PLAY starts and stops it; on, it waits for PLAY) */
    int8_t w[NLAYER][BL_MAX], o[NLAYER][BL_MAX];   /* each note: its step of the scale, its octave */
    int8_t rate[NLAYER], harm[NLAYER], unst[NLAYER];   /* 0 .. 100 */
    uint8_t rec[NLAYER];         /* entry on (REC lit, on its page) */
    uint8_t pos[NLAYER];         /* the step to play next */
    uint8_t last[NLAYER];        /* the step played last (the screen's) */
    uint8_t dir[NLAYER], spd[NLAYER];   /* LD_* and LS_*, as a looper's (its page: the 4th .. 8th black keys) */
    int8_t pdir[NLAYER];         /* PENDULUM: which way now */
    uint8_t hist[NLAYER][4];     /* the last steps played (a beat repeat plays them again) */
    uint32_t due[NLAYER];        /* .. when (fm1_ms) */
    int8_t rpt[NLAYER];          /* a beat repeat: steps of hist still to play again */
    int32_t pace[NLAYER], paim[NLAYER];   /* INSTABILITY's pace, Q16, and where it heads */
    uint32_t hold_until[NLAYER];   /* a stop: no step before then */
    uint32_t rush_until[NLAYER];
    struct { uint8_t layer, note, form, vel; uint32_t at, len; uint8_t on; } q[BL_Q];   /* on: 1 to strike, 2 sounding */
    rnd_t r;
    int8_t touched;
    uint32_t touch_until, drawn_at, t;
} bl;

static void bl_defaults(uint32_t l)
{
    bl.on[l] = bl.run[l] = bl.n[l] = bl.rec[l] = bl.pos[l] = 0;
    bl.len[l] = 0;
    bl.rate[l] = 50;
    bl.harm[l] = 0;
    bl.unst[l] = 0;
    bl.rpt[l] = 0;
    bl.dir[l] = LD_FWD;
    bl.spd[l] = LS_NORMAL;
    bl.pdir[l] = 1;
    bl.pace[l] = bl.paim[l] = Q16;
}
static uint32_t bl_rnd(void)
{
    if (!bl.r.s)
        rnd_seed(&bl.r, fm1_ms ^ 0xB71Cu);
    return rnd_next(&bl.r);
}
static int bl_odds(int32_t q15) { return (int32_t)(bl_rnd() >> 17) < q15; }   /* a chance, Q15 */
static uint32_t bl_in(uint32_t lo, uint32_t hi) { return lo + (bl_rnd() >> 8) % (hi - lo + 1u); }
/* a step's time: 0.5 x 32^(rate / 100) steps a second, ms */
static uint32_t bl_step_ms(uint32_t l)
{
    uint32_t sps = (uint32_t)(((uint64_t)32768u * exp2_q16(clampi(bl.rate[l], 0, 100) * 3277)) >> 16);   /* steps a second, Q16 */
    return sps ? (uint32_t)(65536000u / sps) : 2000u;
}
/* is step i of track l one of its notes (and which): n of len spread as evenly as can be */
static int bl_note_at(uint32_t l, uint32_t i)
{
    uint32_t n = bl.n[l], len = bl.len[l];
    if (!n || !len || i >= len)
        return -1;
    if (i * n % len >= n)                           /* (the first note on the first step: 3 in 8 at 0, 3, 6) */
        return -1;
    return (int)(i * n / len);
}
static uint32_t bl_pitch(uint32_t l, int32_t k, int32_t extra)
{
    return scale_note(bl.w[l][k] + extra + ui.lx + ui.lxt[l], bl.o[l][k]);
}
/* a note of track l to strike in wait ms (0: now), for len ms, at vel (1 .. 126; 0 full) */
static void bl_play(uint32_t l, uint32_t note, uint32_t vel, uint32_t len, uint32_t wait)
{
    uint32_t i, slot = BL_Q, f1 = ui.chord_on[l] ? ui.chord_form[l] + 1u : 0u;
    for (i = 0; i < BL_Q; i++)
        if (!bl.q[i].on) {
            slot = i;
            break;
        }
    if (slot == BL_Q) {                             /* (all taken: the soonest let go now, its place taken) */
        slot = 0;
        for (i = 1; i < BL_Q; i++)
            if (bl.q[i].on == 2u && (int32_t)(bl.q[i].at - bl.q[slot].at) < 0)
                slot = i;
        if (bl.q[slot].on == 2u)
            seq_press(bl.q[slot].layer, bl.q[slot].note, bl.q[slot].form, 0);
    }
    bl.q[slot].layer = (uint8_t)l;
    bl.q[slot].note = (uint8_t)note;
    bl.q[slot].form = (uint8_t)f1;
    bl.q[slot].vel = (uint8_t)vel;
    bl.q[slot].len = len < 20u ? 20u : len > 1500u ? 1500u : len;
    bl.q[slot].at = fm1_ms + wait;
    bl.q[slot].on = 1;
}
static void bl_release(uint32_t l)                  /* track l's notes (all tracks' if l is NLAYER) let go, none to come */
{
    uint32_t i;
    for (i = 0; i < BL_Q; i++)
        if (bl.q[i].on && (l >= NLAYER || bl.q[i].layer == l)) {
            if (bl.q[i].on == 2u)
                seq_press(bl.q[i].layer, bl.q[i].note, bl.q[i].form, 0);
            bl.q[i].on = 0;
        }
}
/* a step of track l: its note (and what HARMONY and INSTABILITY make of it), or a ghost */
static void bl_step(uint32_t l, uint32_t sm)
{
    int32_t k = bl_note_at(l, bl.last[l]), u = bl.unst[l] * 327, h = bl.harm[l] * 327;
    uint32_t gate = sm * 7u / 10u, vel = 110u, nt;
    if (u) {                                        /* VARIATION: the level (down to -12 dB) and the length (x 1/4 .. 2) */
        vel = 126u - (uint32_t)(((uint64_t)(bl_rnd() >> 16) * (uint32_t)u * 86u) >> 31);
        int32_t e = (int32_t)(((((bl_rnd() >> 16) * 3u) >> 1) - 65536u));   /* -2 .. 1 octaves, Q15 */
        gate = (uint32_t)(((uint64_t)gate * exp2_q16((int32_t)(((int64_t)e * u) >> 15) * 2)) >> 16);
    }
    if (k < 0) {                                    /* an empty step: now and then a ghost */
        if (bl.n[l] && bl_odds(u / 6))
            bl_play(l, bl_pitch(l, (int32_t)bl_in(0, bl.n[l] - 1u), 0), bl_in(25, 50), gate, 0);
        return;
    }
    nt = bl_pitch(l, k, 0);
    if (bl_odds(u / 12)) {                          /* a stutter: 2 .. 4 within the step */
        uint32_t m = bl_in(2, 4), j;
        for (j = 0; j < m; j++)
            bl_play(l, nt, vel - j * 10u, sm / m * 7u / 10u, j * sm / m);
    } else
        bl_play(l, nt, vel, gate, 0);
    if (bl_odds(h)) {                               /* a diad (a third or a fifth above), or a triad */
        int tri = bl_odds(Q15 * 3 / 10);
        bl_play(l, bl_pitch(l, k, tri || bl_odds(Q15 / 2) ? 2 : 4), vel * 4u / 5u, gate, 0);
        if (tri)
            bl_play(l, bl_pitch(l, k, 4), vel * 4u / 5u, gate, 0);
    }
    if (bl_odds(u / 8)) {                           /* echoes, quieter each */
        uint32_t m = bl_in(1, 3), j, d = sm * bl_in(5, 15) / 10u, v = vel;
        for (j = 1; j <= m; j++) {
            v = v * 3u / 5u;
            bl_play(l, nt, v > 10u ? v : 10u, gate, j * d);
        }
    }
}
/* the next step: forwards, backwards, to and fro, or one drawn at random */
static void bl_advance(uint32_t l)
{
    uint32_t len = bl.len[l], p = bl.pos[l];
    if (len <= 1u) {
        bl.pos[l] = 0;
        return;
    }
    if (bl.dir[l] == LD_REV)
        p = p ? p - 1u : len - 1u;
    else if (bl.dir[l] == LD_PEND) {
        if ((bl.pdir[l] > 0 && p + 1u >= len) || (bl.pdir[l] < 0 && p == 0u))
            bl.pdir[l] = (int8_t)-bl.pdir[l];
        p = bl.pdir[l] > 0 ? p + 1u : p - 1u;
    } else if (bl.dir[l] == LD_RAND)
        p = bl_in(0, len - 1u);
    else
        p = p + 1u >= len ? 0u : p + 1u;
    bl.pos[l] = (uint8_t)p;
}
/* the main loop, every pass: each running track's steps as they fall due; the notes' strikes and ends */
static void bl_tick(void)
{
    uint32_t now = fm1_ms, l, i;
    for (l = 0; l < NLAYER; l++) {
        uint32_t sm;
        int32_t u;
        if (!bl.on[l] || !bl.run[l] || !bl.n[l])
            continue;
        if (bl.len[l] < bl.n[l])
            bl.len[l] = bl.n[l];
        if ((int32_t)(now - bl.due[l]) < 0 || (int32_t)(now - bl.hold_until[l]) < 0)
            continue;
        u = bl.unst[l] * 327;
        /* INSTABILITY's pace: towards a new aim now and then (from half to twice); a stop or a rush */
        if (u && bl_odds(Q15 / 4))
            bl.paim[l] = (int32_t)exp2_q16((int32_t)(((int64_t)(int32_t)(bl_rnd() >> 16) - 32768) * u >> 15) * 2);
        if (!u)
            bl.paim[l] = Q16;
        bl.pace[l] += (bl.paim[l] - bl.pace[l]) >> 2;
        sm = bl_step_ms(l);
        sm = (sm << 16) / (uint32_t)(bl.pace[l] > 8192 ? bl.pace[l] : 8192);   /* (sm <= 2000: 32 bits) */
        if ((int32_t)(now - bl.rush_until[l]) < 0)
            sm /= 2u;
        sm = bl.spd[l] == LS_HALF ? sm * 2u : bl.spd[l] == LS_DOUBLE ? sm / 2u : sm;
        if (bl.rpt[l] > 0)                          /* a beat repeat under way: the last steps again, oldest first */
            bl.last[l] = bl.hist[l][(4 - bl.rpt[l]--) & 3];
        else {
            bl.last[l] = bl.pos[l];
            bl.hist[l][0] = bl.hist[l][1];
            bl.hist[l][1] = bl.hist[l][2];
            bl.hist[l][2] = bl.hist[l][3];
            bl.hist[l][3] = bl.pos[l];
            bl_advance(l);
            if (u && bl.len[l] > 2u && bl_odds(u / 16))   /* a beat repeat: the last 2 .. 4 steps again */
                bl.rpt[l] = (int8_t)bl_in(2, bl.len[l] < 4u ? bl.len[l] : 4u);
        }
        bl_step(l, sm);
        if (u && bl_odds(u / 20))                   /* a stop, a moment */
            bl.hold_until[l] = now + sm + bl_in(200, 1200);
        else if (u && bl_odds(u / 20))
            bl.rush_until[l] = now + bl_in(300, 1200);
        bl.due[l] = now + sm;
    }
    for (i = 0; i < BL_Q; i++) {
        if (bl.q[i].on == 1u && (int32_t)(now - bl.q[i].at) >= 0) {
            press_vel = bl.q[i].vel;
            seq_press(bl.q[i].layer, bl.q[i].note, bl.q[i].form, 1);
            press_vel = 0;
            bl.q[i].on = 2;
            bl.q[i].at = now + bl.q[i].len;
        } else if (bl.q[i].on == 2u && (int32_t)(now - bl.q[i].at) >= 0) {
            seq_press(bl.q[i].layer, bl.q[i].note, bl.q[i].form, 0);
            bl.q[i].on = 0;
        }
    }
}
static void bl_set_run(uint32_t l, int run)            /* PLAY: started from its first step, or stopped */
{
    bl.run[l] = (uint8_t)(run && bl.on[l]);
    if (bl.run[l]) {
        bl.pos[l] = bl.last[l] = 0;
        bl.pdir[l] = 1;
        bl.due[l] = fm1_ms;
        bl.rpt[l] = 0;
        bl.pace[l] = bl.paim[l] = Q16;
    } else
        bl_release(l);
}
static void bl_toggle(uint32_t l)                   /* SHIFT + the envelope key: on (waiting for PLAY) or off */
{
    bl.on[l] = (uint8_t)!bl.on[l];
    bl.rec[l] = 0;
    bl_set_run(l, 0);
}
/* entry: a key's note added at the end (white index w at octave o) */
static void bl_add(uint32_t l, int32_t w, int32_t o)
{
    if (bl.n[l] >= BL_MAX)
        return;
    bl.w[l][bl.n[l]] = (int8_t)w;
    bl.o[l][bl.n[l]] = (int8_t)o;
    bl.n[l]++;
    if (bl.len[l] < bl.n[l])
        bl.len[l] = bl.n[l];
}
static void bl_remove_at(uint32_t l, uint32_t k)
{
    for (; k + 1u < bl.n[l]; k++) {
        bl.w[l][k] = bl.w[l][k + 1u];
        bl.o[l][k] = bl.o[l][k + 1u];
    }
    if (bl.n[l])
        bl.n[l]--;
}
static void bl_remove(uint32_t l, int32_t w, int32_t o)   /* SHIFT + a white key: that note, wherever it is */
{
    uint32_t k = bl.n[l];
    while (k--)
        if (bl.w[l][k] == w && bl.o[l][k] == o)
            bl_remove_at(l, k);
}
static void bl_knob(uint32_t l, uint32_t j, int32_t s)
{
    if (j == 0u)
        bl.len[l] = (uint8_t)clampi(bl.len[l] + s, bl.n[l] ? bl.n[l] : 1, BL_MAX);
    else if (j == 1u)
        bl.rate[l] = (int8_t)clampi(bl.rate[l] + 2 * s, 0, 100);
    else if (j == 2u)
        bl.harm[l] = (int8_t)clampi(bl.harm[l] + 2 * s, 0, 100);
    else
        bl.unst[l] = (int8_t)clampi(bl.unst[l] + 2 * s, 0, 100);
    if (bl.pos[l] >= bl.len[l])
        bl.pos[l] = 0;
    if (bl.last[l] >= bl.len[l])
        bl.last[l] = 0;
    bl.touched = (int8_t)j;
    bl.touch_until = fm1_ms + 1500u;
}

/* the page: a line of text over the grid -- what was just turned, else REC and the count while entering, else the
 * notes and the steps; the steps as cells, eight a row (up to four rows), a note's cell filled in the track's
 * colour, an empty one an outline, the step playing white */
static void draw_bl_page(void)
{
    uint32_t l = ui.sel, i, len = bl.len[l] ? bl.len[l] : 1u, rows = (len + 7u) / 8u;
    uint16_t col = LAYER_COL[l], fg = col;
    int32_t t = (int32_t)(bl.touch_until - fm1_ms) > 0 ? bl.touched : -1;
    uint32_t playing = bl.last[l];                  /* (the step just played) */
    char m[24];
    m[0] = 0;
    if (t == 0) {
        str_cpy(m, "LENGTH ", sizeof m);
        fmt_int(m + str_len(m), bl.len[l]);
    } else if (t == 1) {
        uint32_t sps = (uint32_t)(((uint64_t)32768u * exp2_q16(clampi(bl.rate[l], 0, 100) * 3277)) >> 16);
        str_cpy(m, "RATE ", sizeof m);
        fmt_fix(m + str_len(m), (int32_t)((sps * 10u + 32768u) >> 16), 1);
    } else if (t == 2) {
        str_cpy(m, "HARMONY ", sizeof m);
        fmt_int(m + str_len(m), bl.harm[l]);
    } else if (t == 3) {
        str_cpy(m, "VARIATION ", sizeof m);
        fmt_int(m + str_len(m), bl.unst[l]);
    } else if (bl.rec[l]) {
        str_cpy(m, "REC ", sizeof m);
        fmt_int(m + str_len(m), bl.n[l]);
        fg = RGB(255, 60, 60);
    } else {
        fmt_int(m, bl.n[l]);
        str_cpy(m + str_len(m), " IN ", 5);
        fmt_int(m + str_len(m), bl.len[l]);
    }
    cv_begin(240, 30, C_BLACK);
    if (text_w(&FONT_S, m) <= 236 + 2 * ((int32_t)str_len(m) - 1))
        cv_text_fit(2, &FONT_S, m, fg);
    else
        cv_text_c(2 + (FONT_S.h - FONT_XS.h) / 2, &FONT_XS, m, fg);
    cv_blit(0, 0);
    CV_AREA(0, 30, 240, 210, C_BLACK) {
        int32_t ch = rows <= 2u ? 80 : 44, gap = 6;  /* (big cells while there are few rows) */
        for (i = 0; i < len; i++) {
            int32_t x = 4 + (int32_t)(i % 8u) * 29, y = 4 + (int32_t)(i / 8u) * (ch + gap);
            int note = bl_note_at(l, i) >= 0, now = bl.on[l] && i == playing && bl.n[l];
            uint16_t c = now ? C_WHITE : note ? col : RGB(70, 70, 70);
            if (note || now)
                cv_rect(x, y, 26, ch, c);
            else
                cv_frame(x, y, 26, ch, 2, c);
        }
    }
    bl.drawn_at = fm1_ms;
}
