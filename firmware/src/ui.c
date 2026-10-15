/* SPDX-License-Identifier: GPL-3.0-only
 * TONITOR: the panel and the screen.
 *
 *   FX SEL ENV LFO EDIT GLO   the top row: layers 1 to 6 (its light: the layer chosen)
 *   PRESETS (knob)            the layer's engine: FM, ANIMA
 *   HOME                      a new sound for the layer, on its engine
 *   OCT- / OCT+               the layer's octave, -3 .. +3 (their lights: below or above the middle)
 *   the white keys            play the layer chosen, in the scale chosen: the first key is the root, every
 *                             white key the next note of the scale, up across the keyboard. Two voices: a note
 *                             never cuts the release of the one before. A key lifted ends its note on the layer
 *                             it was played on, so a held note goes on sounding while another is played.
 *                             (The black keys play no notes: kept for other things.)
 *   ARP                       the SCALE page (its light: on it): SELECT the scale, ALGORITHM the root
 *   REC                       the layer's looper: arm a new loop (the first note starts it), end it (it
 *                             plays, over the layer's other loops, each in its own length): up to 10 loops
 *   REC held                  empty the layer's looper (all its loops)
 *   PLAY                      stop the layer's loops, or play them again
 *   SELECT (knob)             the layer's level, OFF and -28.5 .. +4.5 dB in steps of 1.5 dB
 *   ALGORITHM (knob)          the layer's pan, left to right in 41 steps
 *   KNOB 1                    the layer's DJ filter: left a resonant low-pass, right a high-pass, the middle
 *                             off; while it turns (and a second after) the whole screen is its dial
 *   KNOB 2                    the layer's granulator: the middle off; left dark, long, smeared grains; right
 *                             short, fast, scattered, jittered ones (gran.c); its dial over the screen
 *   KNOB 3                    the layer's crush: the middle off; left fewer bits, right a lower sample rate;
 *                             its dial over the screen
 *   KNOB 4                    the layer's send to the master reverb, 0 .. 100; its dial over the screen
 *   SEQ                       the REVERB page (its light: on it); there KNOB 1..4 are SIZE, DECAY, DAMP and
 *                             MIX. SEQ again, or a layer's button, goes back
 *   SAVE                      the SESSIONS page: PRESETS chooses a session, SAVE loads it and goes back
 *   SAVE held                 save: over the session loaded (or last saved) at once; else a new session,
 *                             numbered after the last, asked first (PRESETS: NO / YES, SAVE to answer)
 *   USB MIDI notes            as the keys
 *   MASTER (knob)             the level
 *
 * The screen is for low vision: a few words, very large, high contrast, each layer its own colour --
 * the layer's number, its engine, its sound; a short message in black on white when something changes;
 * the looper's state over the sound, red: ARMED flashing, REC steady; a looper emptied flashes the whole
 * screen. Down the left, the layer's level meter: what it sounds, in its colour, and a white bar across it
 * at the level the SELECT knob sets. Along the bottom, its pan: a block in its colour on a track from L to
 * R, a tick at the middle. KNOB 1 to 4 (filter, grain, crush, reverb send) show, while they turn and a second
 * after, a dial over the whole screen: the same for the three. Turning the other knobs says the value in the
 * message band. The REVERB page shows its four values in rows,
 * each with a bar; the one just turned in black on white. Saving, loading, emptying a looper flash the
 * whole screen. The SESSIONS page shows the session chosen as a very large number. */

/* the panel: which matrix button carries which printed label (as SLOOP's panel.c, default table) */
enum { B_FX, B_SEL, B_ENV, B_LFO, B_EDIT, B_GLO, B_HOME, B_SAVE, B_ARP, B_SEQ, B_PLAY, B_REC, B_OCTDN, B_OCTUP, NB };
enum { EN_SELECT, EN_ALGO, EN_PRESET, EN_K1, EN_K2, EN_K3, EN_K4, NE };
static const uint8_t PANEL_BTN[NB] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0, 1};
static const uint8_t PANEL_ENC[NE] = {0, 1, 6, 2, 3, 4, 5};
static const uint8_t LAYER_BTN[NLAYER] = {B_FX, B_SEL, B_ENV, B_LFO, B_EDIT, B_GLO};
#define NKEY 27u
#define KEY_SHIFT 25u            /* the rightmost black key: pressed, the keys held latch; held, a latched key lets go */
#define KEY_NONE 0xFFu
#define KEY_AMB 1u               /* the leftmost black key: the AMBIENT path (again: back to the open path) */
#define KEY_EXP 3u               /* the second: the EXPERIMENTAL path */
#define KEY_EDIT 22u             /* the black key before SHIFT: EDIT, on and off (KNOB 1 .. 4: the sound's macros) */
#define KEY_REV 8u               /* the 4th black key: the chosen track's loops backwards (again: forwards) */
#define KEY_PEND 10u             /* the 5th: PENDULUM, forwards and backwards in turn */
#define KEY_RAND 13u             /* the 6th: RANDOM, forwards from a note drawn at random each time the next is due */
#define KEY_HALF 15u             /* the 7th: half speed (again: normal) */
#define KEY_DOUBLE 17u           /* the 8th: double speed */
#define KEY_NOTE0 53             /* the lowest key, F3 */

static const uint16_t LAYER_COL[NLAYER] = {RGB(255, 214, 0), RGB(0, 214, 255), RGB(255, 92, 210),
                                           RGB(96, 255, 96), RGB(255, 140, 0), RGB(190, 160, 255)};

static struct {
    uint8_t sel;
    uint8_t engine[NLAYER];
    int8_t oct[NLAYER];
    uint8_t key_layer[NKEY], key_note[NKEY];   /* what a key down holds (KEY_NONE: nothing, it latched or let go) */
    uint8_t path[NLAYER];        /* each layer's generative path (PATH_*): HOME draws on it */
    uint8_t edit;                /* EDIT on: KNOB 1 .. 4 turn the chosen layer's sound's macros */
    uint8_t play_down, ovd;      /* PLAY down (since play_t0); held: the overdubs on the white keys */
    uint32_t play_t0;
    int32_t shown_ovd;
    int8_t mac[NLAYER][NENG][NMAC];   /* the macros on each layer's sound of each engine (0: as drawn) */
    uint32_t latch[NLAYER];      /* the keys latched on each layer (lit while it is chosen) */
    uint8_t latch_note[NLAYER][NKEY];
    uint8_t midi_layer[128];
    uint32_t keys;               /* the keys down last time */
    char msg[16];
    uint32_t msg_until;
    uint32_t home_until;         /* HOME's light after a new sound */
    uint32_t seed;
    uint32_t rec_t0;             /* REC pressed at; held 1 s: the looper emptied */
    uint8_t rec_down, rec_done;
    uint32_t flash_t0;           /* the screen flashes from then (a looper emptied, a session saved / loaded) */
    uint8_t flash_on, flash_phase;
    uint16_t flash_col;
    char flash_l1[12], flash_l2[12];
    uint8_t sc_page;             /* the SCALE page is up */
    uint8_t scale, root;         /* the keyboard's scale (SCALES) and root (0 = C .. 11 = B) */
    int8_t lx;                   /* the loops' transposition, in steps of the scale (SHIFT + OCT) */
    int8_t lxt[NLAYER];          /* each track's own, on top (SHIFT + its button held + OCT) */
    int8_t sc_touched;           /* 0 the root just turned, 1 the scale, -1 none (until sc_touch_until) */
    uint32_t sc_touch_until;
    int8_t shown_sc_touch;
    uint8_t shown_scale, shown_root;
    uint8_t ses_page;            /* 0 none, SP_LIST the sessions, SP_ASK the question before a save */
    uint8_t ses_ix;              /* the slot shown on the SESSIONS page, 1 .. 64 */
    uint8_t ses_yes;             /* the question's answer */
    uint16_t ses_new;            /* the number it asks about */
    uint8_t ses_over;            /* .. and it holds a session: the question is OVERWRITE */
    uint8_t pre_page;            /* 0 none, SP_LIST the presets of pre_eng, SP_ASK the question before saving over one */
    uint8_t pre_eng;             /* the engine whose presets they are (the layer's, as SHIFT and SAVE opened them) */
    uint8_t pre_ix;              /* the slot chosen, 1 .. 64 */
    uint8_t pre_yes;
    int16_t shown_pre;           /* (what the page shows: slot, question and answer) */
    uint32_t save_t0;            /* SAVE pressed at: held 0.8 s is a save, shorter a press */
    uint8_t save_down, save_long;
    uint8_t trk_down, trk_done;  /* a track's button held (its number + 1) since trk_t0: copy, or paste */
    uint32_t trk_t0;
    int8_t shown_ses;            /* (the page as last drawn) */
    int16_t shown_ses_ix;
    /* the screen: what each band shows, as last drawn */
    int32_t shown_sel, shown_eng, shown_msg;
    char shown_text[16];
    int8_t level[NLAYER];        /* 0 = off, 1..LEVEL_MAX: 1.5 dB steps, LEVEL_UNITY = 0 dB */
    int8_t pan[NLAYER];          /* -PAN_MAX (left) .. PAN_MAX (right) */
    int8_t send[NLAYER];         /* 0 .. 100: to the reverb */
    int8_t flt[NLAYER];          /* the DJ filter, -FLT_MAX .. FLT_MAX, 0 off */
    int8_t gr[NLAYER];           /* the granulator, -FLT_MAX .. FLT_MAX, 0 off */
    int8_t cr[NLAYER];           /* the crush, -FLT_MAX .. FLT_MAX, 0 off */
    uint8_t dial;                /* the dial over the screen: DIAL_FLT, DIAL_GR, DIAL_SEND */
    uint32_t dial_until, dial_drawn_at;   /* it shows until then; when it was last drawn */
    int16_t shown_dial;          /* (the value last drawn, + the kind x 256) */
    int8_t rvp[4];               /* the reverb: SIZE DECAY DAMP MIX, 0 .. 100 */
    int8_t dlp[4];               /* the delay: TIME FEEDBACK DAMP MIX, 0 .. 100 */
    int8_t dsend[NLAYER];        /* each track's send to the delay, 0 .. 100 (SHIFT + KNOB 4) */
    uint8_t rv_page;             /* the REVERB page is up -- or the DELAY page, fx 1 (SEQ; SHIFT + SEQ) */
    uint8_t fx;
    int8_t rv_touched;           /* the row just turned (-1 none), until rv_touch_until */
    uint32_t rv_touch_until;
    int8_t shown_rvp[4], shown_touch;
    uint8_t shown_page;
    int32_t shown_meter, shown_level, shown_pan, shown_msel;
    uint32_t meter_at;
    uint8_t force;
} ui;

/* ------------------------------------------------------------ notes --- */
static void note_press(uint32_t l, uint32_t note) { ev_post(EV_ON, l, note, 0); }
static void note_release(uint32_t l, uint32_t note) { ev_post(EV_OFF, l, note, 0); }

static void ui_draw(void);
static void say(const char *s, uint32_t ms)
{
    str_cpy(ui.msg, s, sizeof ui.msg);
    ui.msg_until = fm1_ms + ms;
}

#define LEVEL_MAX 23
#define LEVEL_UNITY 20
#define PAN_MAX 20
static const int8_t PAN_DEF[NLAYER] = {-10, 10, -5, 5, -2, 2};
#define SEND_DEF 40
enum { DIAL_FLT, DIAL_GR, DIAL_SEND, DIAL_CR, DIAL_DSEND, DIAL_MAC };   /* the knobs whose dial covers the screen (DIAL_MAC + j: EDIT's j) */

/* the scales: the white keys play their degrees, from the root, up */
#define NSCALE 9
static const char *const SCALE_L1[NSCALE] = {"MAJOR", "MINOR", "MINOR", "MINOR", "DORIAN", "MINOR", "WHOLE", "ENIGMATIC", "JAPANESE"};
static const char *const SCALE_L2[NSCALE] = {"", "NATURAL", "HARMONIC", "MELODIC", "", "PENTA", "TONE", "", "IN"};
static const uint8_t SCALE_N[NSCALE] = {7, 7, 7, 7, 7, 5, 6, 7, 5};
static const uint8_t SCALE_STEP[NSCALE][7] = {
    {0, 2, 4, 5, 7, 9, 11},      /* major */
    {0, 2, 3, 5, 7, 8, 10},      /* natural minor */
    {0, 2, 3, 5, 7, 8, 11},      /* harmonic minor */
    {0, 2, 3, 5, 7, 9, 11},      /* melodic minor (rising) */
    {0, 2, 3, 5, 7, 9, 10},      /* dorian */
    {0, 3, 5, 7, 10},            /* minor pentatonic */
    {0, 2, 4, 6, 8, 10},         /* whole tone */
    {0, 1, 4, 6, 8, 10, 11},     /* enigmatic (Verdi's scala enigmatica) */
    {0, 1, 5, 7, 8},             /* in (miyako-bushi): Japan's sorrowful scale, koto and shakuhachi */
};
static const char *const NOTE_NAME[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
#define SCALE_BASE 48            /* the root's octave: C3 .. B3 on the first key (the layer's octave moves it) */

/* white key k's place among the white keys (0 .. 15), -1 for a black key (keys from F3, chromatic) */
static int32_t white_ix(uint32_t k)
{
    static const int8_t W[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};   /* C .. B: whites in an octave */
    uint32_t n = 53u + k, o = n / 12u;
    int32_t w = W[n % 12u];
    if (w < 0)
        return -1;
    return (int32_t)o * 7 + w - (4 * 7 + 3);                                  /* F3 = 0 */
}
/* the note white key w plays, in the scale, on a layer at octave oct */
static uint32_t scale_note(int32_t w, int32_t oct)
{
    int32_t n = SCALE_N[ui.scale], q = w >= 0 ? w / n : -((n - 1 - w) / n);   /* (below the root too: a step moved) */
    return (uint32_t)clampi(SCALE_BASE + ui.root + 12 * oct + 12 * q + SCALE_STEP[ui.scale][w - q * n], 12, 120);
}
/* the loops' transposition and the scale it walks, to the ISR (which moves the notes sounding) */
static void lx_post(void)
{
    uint32_t k, m = 0;
    for (k = 0; k < SCALE_N[ui.scale]; k++)
        m |= 1u << ((ui.root + SCALE_STEP[ui.scale][k]) % 12u);
    lx_want_mask = (uint16_t)m;
    lx_want_steps = ui.lx;
    for (k = 0; k < NLAYER; k++)
        lx_want_tr[k] = ui.lxt[k];
    RING_PUBLISH();
    ev_post(EV_LOOP_XP, 0, 0, 0);
}
static const int8_t RV_DEF[4] = {70, 65, 40, 50};      /* lush, present, not overdone */
static const char *const RV_NAME[4] = {"SIZE", "DECAY", "DAMP", "MIX"};
static const int8_t DL_DEF[4] = {65, 40, 40, 50};      /* 400 ms, a few repeats, a little darker each */
static const char *const DL_NAME[4] = {"TIME", "FEEDBACK", "DAMP", "MIX"};

/* the layer's level and pan, to the ISR */
static void level_pan_apply(uint32_t l)
{
    int32_t lv = ui.level[l], db16, ph;
    /* 1.5 dB a step: log2 of the gain = (lv - unity) x 1.5 / 6.0206 */
    db16 = (lv - LEVEL_UNITY) * 16328;                     /* x 0.24914, Q16 */
    lay_gain[l] = lv <= 0 ? 0 : (int32_t)(exp2_q16(db16) >> 2);   /* Q14 */
    ph = (int32_t)((uint32_t)(ui.pan[l] + PAN_MAX) * (0x40000000u / (2u * PAN_MAX)));   /* 0 .. a quarter turn */
    lay_pl[l] = sin_ph((uint32_t)ph + 0x40000000u);
    lay_pr[l] = sin_ph((uint32_t)ph);
    lay_send[l] = ui.send[l] * 32767 / 100;
    lay_dsend[l] = ui.dsend[l] * 32767 / 100;
    lay_flt[l] = ui.flt[l];
    lay_gr[l] = ui.gr[l];
    lay_cr[l] = ui.cr[l];
}

enum { SP_LIST = 1, SP_ASK = 2 };
#define C_SES RGB(120, 255, 200)
#define C_FULL RGB(255, 50, 50)          /* a slot with a session in it */
#define C_OK RGB(60, 220, 90)
#define C_BAD RGB(255, 40, 40)

/* the whole screen flashes: l1 over l2 (a number in very large type), in colour c and black by turns */
static void flash(const char *l1, const char *l2, uint16_t c)
{
    str_cpy(ui.flash_l1, l1, sizeof ui.flash_l1);
    str_cpy(ui.flash_l2, l2, sizeof ui.flash_l2);
    ui.flash_col = c;
    ui.flash_t0 = fm1_ms;
    ui.flash_on = 1;
    ui.flash_phase = 0xFF;
}

/* sessions.c (after this file) */
static int ses_save(uint32_t num);
static int ses_load(uint32_t i);
static uint32_t ses_next_number(void);
static int ses_has(uint32_t num);
static int ses_current(void);
static void ses_choose(uint32_t num);
static void ses_fresh(uint32_t num);
static void clip_copy(uint32_t l);
static void clip_paste(uint32_t l);
static int clip_from(void);                  /* the track on the clipboard, -1 if it is empty */
static int ses_load_slot(uint32_t num);
static void mac_apply(uint32_t l);
static int pre_kind(uint32_t e, uint32_t slot);   /* the kind of sound in engine e's slot, -1 if none */
static int pre_save(uint32_t l, uint32_t e, uint32_t slot);
static int pre_load(uint32_t l, uint32_t e, uint32_t slot);
#define PRE_SLOTS 64u
#define SES_SLOTS 64u
enum { SES_OK, SES_NOFLASH, SES_FULL, SES_FAIL };

static void ses_done(int rc, const char *what, uint32_t num)
{
    char n[8];
    fmt_int(n, (int32_t)num);
    if (rc == SES_OK)
        flash(what, n, C_OK);
    else
        flash(rc == SES_FULL ? "FULL" : rc == SES_NOFLASH ? "NO FLASH" : "FAILED", "", C_BAD);
    ui.force = 1;
}
/* the presets page: SAVE held saves the layer's sound into the slot chosen (asked first over a full one) */
static void pre_done(int rc, const char *what)
{
    ses_done(rc, what, ui.pre_ix);               /* the whole screen: SAVED / LOADED and the slot, very large */
}
static void pre_long(void)
{
    if (ui.pre_page == SP_ASK)
        return;
    if (pre_kind(ui.pre_eng, ui.pre_ix) >= 0) {
        ui.pre_page = SP_ASK;
        ui.pre_yes = 0;                          /* (over a sound: NO until turned to YES) */
        ui.force = 1;
        return;
    }
    pre_done(pre_save(ui.sel, ui.pre_eng, ui.pre_ix), "SAVED");
}
/* .. SAVE pressed: the slot chosen loaded into the layer (and back to playing), or the question answered */
static void pre_short(void)
{
    if (ui.pre_page == SP_ASK) {
        ui.pre_page = SP_LIST;
        ui.force = 1;
        if (ui.pre_yes)
            pre_done(pre_save(ui.sel, ui.pre_eng, ui.pre_ix), "SAVED");
        else
            say("NOT SAVED", 1200);
        return;
    }
    if (pre_kind(ui.pre_eng, ui.pre_ix) < 0) {
        say("EMPTY", 900);
        return;
    }
    if (ui.engine[ui.sel] != ui.pre_eng) {       /* (the layer's engine moved on: to the presets' again) */
        ui.engine[ui.sel] = ui.pre_eng;
        ev_post(EV_ENGINE, ui.sel, 0, ui.pre_eng);
        mac_apply(ui.sel);
    }
    ui.pre_page = 0;
    pre_done(pre_load(ui.sel, ui.pre_eng, ui.pre_ix) == SES_OK ? SES_OK : SES_FAIL, "LOADED");
}

/* SAVE held */
static void save_long(void)
{
    if (ui.pre_page) {
        pre_long();
        return;
    }
    if (ui.ses_page == SP_ASK)
        return;
    ui.rv_page = 0;
    if (ui.ses_page == SP_LIST && ui.ses_ix != ses_current()) {   /* in the list: into the slot shown, asked */
        ui.ses_new = ui.ses_ix;
        ui.ses_over = (uint8_t)ses_has(ui.ses_ix);
        ui.ses_page = SP_ASK;
        ui.ses_yes = (uint8_t)!ui.ses_over;      /* (over a session: NO until turned to YES) */
        ui.force = 1;
        return;
    }
    if (ses_current()) {                         /* a session loaded: saved over it, at once */
        ui.ses_page = 0;
        flash("SAVING", "", C_OK);
        ui_draw();
        ses_done(ses_save((uint32_t)ses_current()), "SAVED", (uint32_t)ses_current());
        return;
    }
    ui.ses_new = (uint16_t)ses_next_number();   /* none chosen: the first empty slot, asked first */
    if (!ui.ses_new) {
        ses_done(SES_FULL, "", 0);
        return;
    }
    ui.ses_over = 0;
    ui.ses_page = SP_ASK;
    ui.ses_yes = 1;
    ui.force = 1;
}
/* SAVE pressed (let go before it was held) */
static void save_short(void)
{
    if (ui.pre_page) {
        pre_short();
        return;
    }
    if (ui.ses_page == SP_ASK) {
        uint32_t num = ui.ses_new;
        ui.ses_page = 0;
        ui.force = 1;
        if (!ui.ses_yes) {
            say("NOT SAVED", 1500);
            return;
        }
        flash("SAVING", "", C_OK);
        ui_draw();
        ses_done(ses_save(num), "SAVED", num);
        return;
    }
    if (ui.ses_page == SP_LIST) {                /* the slot shown: loaded (or, empty, chosen), and back */
        uint32_t num = ui.ses_ix;
        ui.ses_page = 0;
        ui.force = 1;
        if (ses_has(num)) {
            flash("LOADING", "", C_SES);
            ui_draw();
            ses_done(ses_load_slot(num), "LOADED", num);
        } else {
            char n[8];
            flash("NEW", "", C_SES);
            ui_draw();
            ses_fresh(num);                      /* a clean slate, as at start; SAVE held writes it there */
            fmt_int(n, (int32_t)num);
            flash("NEW", n, C_WHITE);
        }
        return;
    }
    ui.rv_page = 0;                              /* the SESSIONS page, on the session chosen if any */
    ui.sc_page = 0;
    ui.ses_page = SP_LIST;
    ui.ses_ix = (uint8_t)(ses_current() ? ses_current() : 1);
    ui.force = 1;
}

static uint32_t next_seed(void)
{
#if TELEMETRY
    ui.seed = ui.seed * 1664525u + 1013904223u;  /* (a diagnostic build: the same sounds at every start) */
#else
    ui.seed = ui.seed * 1664525u + 1013904223u + fm1_ticks();
#endif
    return ui.seed;
}

/* the macros of layer l's sound on the engine it plays, to the ISR */
static void mac_apply(uint32_t l)
{
    uint32_t j;
    for (j = 0; j < NMAC; j++)
        lay_mac[l][j] = ui.mac[l][ui.engine[l]][j];
}
/* the name of EDIT's macro j on layer l's sound */
static const char *mac_name(uint32_t l, uint32_t j)
{
    uint32_t e = ui.engine[l];
    return e == ENG_FM ? fm_mac_name(&fm_buf[l][fm_pub[l]], j) : e == ENG_ANIMA ? an_mac_name(&an_buf[l][an_pub[l]], j)
           : e == ENG_CAL ? cal_mac_name(&cal_buf[l][cal_pub[l]], j) : rus_mac_name(&rus_buf[l][rus_pub[l]], j);
}

/* HOME: a new sound for the layer chosen */
static void new_sound(void)
{
    uint32_t l = ui.sel;
    int ok;
    say("NEW", 900);
    ui.home_until = fm1_ms + 400u;
    ui_draw();                                   /* the message first: an FM patch takes ~30 ms to measure */
    ok = ui.engine[l] == ENG_FM      ? new_fm(l, next_seed(), ui.path[l])
         : ui.engine[l] == ENG_ANIMA ? new_anima(l, next_seed(), ui.path[l])
         : ui.engine[l] == ENG_CAL   ? new_cal(l, next_seed(), ui.path[l])
                                     : new_rus(l, next_seed(), ui.path[l]);
    if (!ok)
        say("BUSY", 900);
    else {                                       /* a new sound: as drawn, no macros */
        memset(ui.mac[l][ui.engine[l]], 0, NMAC);
        mac_apply(l);
    }
}

/* --------------------------------------------------------------- input --- */
static int32_t enc_take(uint32_t role) { return fm1_enc_take(PANEL_ENC[role]); }
static int btn_bit(uint32_t mask, uint32_t label) { return (int)((mask >> PANEL_BTN[label]) & 1u); }

static void midi_take(void)                      /* USB MIDI notes (usb.c's ring) */
{
    while (mi_r != mi_w) {
        uint32_t pkt = midi_in_q[mi_r % MQ], cin = pkt & 15u, note = (pkt >> 16) & 0x7Fu, vel = pkt >> 24 & 0x7Fu;
#if TELEMETRY
        if (((pkt >> 8) & 15u) < NLAYER && cin == 9u && vel)
            ui.sel = (uint8_t)((pkt >> 8) & 15u);   /* (a diagnostic build: the MIDI channel chooses the track) */
#endif
        RING_PUBLISH();
        mi_r++;
        if (cin == 9u && vel) {
            if (ui.midi_layer[note] != 0xFFu)
                note_release(ui.midi_layer[note], note);
            ui.midi_layer[note] = ui.sel;
            note_press(ui.sel, note);
        } else if (cin == 8u || cin == 9u) {
            if (ui.midi_layer[note] != 0xFFu) {
                note_release(ui.midi_layer[note], note);
                ui.midi_layer[note] = 0xFFu;
            }
        }
    }
}

static void ui_input(void)
{
    uint32_t rel, press = fm1_input_edges(&rel), keys, ch, k, l;
    int32_t s;
    (void)rel;
    for (l = 0; l < NLAYER; l++)
        if (btn_bit(press, LAYER_BTN[l])) {
            ui.trk_down = (uint8_t)(l + 1u);
            ui.trk_t0 = fm1_ms;
            ui.trk_done = 0;
            ui.sel = (uint8_t)l;
            if (ui.rv_page || ui.ses_page || ui.sc_page || ui.pre_page) {   /* a layer chosen: back to it */
                ui.rv_page = 0;
                ui.ses_page = 0;
                ui.sc_page = 0;
                ui.pre_page = 0;
                ui.force = 1;
            }
        }
    if (ui.trk_down) {                           /* a track's button held: it is copied, or the copy pasted onto it */
        l = ui.trk_down - 1u;
        if (!btn_bit(fm1_in.buttons, LAYER_BTN[l]))
            ui.trk_down = 0;
        else if ((fm1_in.notes >> KEY_SHIFT) & 1u)
            ui.trk_done = 1;                     /* (with SHIFT: the track's transposition, never a copy) */
        else if (!ui.trk_done && (uint32_t)(fm1_ms - ui.trk_t0) >= 800u) {
            char n[12] = "TRACK ";
            ui.trk_done = 1;
            n[6] = (char)('1' + l);
            n[7] = 0;
            if (clip_from() >= 0 && clip_from() != (int)l) {
                flash("PASTING", n, LAYER_COL[l]);
                ui_draw();
                clip_paste(l);
                flash("PASTED", n, LAYER_COL[l]);
            } else {
                clip_copy(l);
                flash("COPIED", n, LAYER_COL[l]);
            }
        }
    }
    if (btn_bit(press, B_SEQ)) {                 /* the REVERB page; with SHIFT, the DELAY page (again: back) */
        uint8_t fx = (uint8_t)((fm1_in.notes >> KEY_SHIFT) & 1u);
        ui.rv_page = (uint8_t)!(ui.rv_page && ui.fx == fx);
        ui.fx = fx;
        ui.ses_page = 0;
        ui.pre_page = 0;
        ui.sc_page = 0;
        ui.force = 1;
    }
    if (btn_bit(press, B_ARP)) {
        ui.sc_page = (uint8_t)!ui.sc_page;
        ui.rv_page = 0;
        ui.ses_page = 0;
        ui.pre_page = 0;
        ui.force = 1;
    }
    if (btn_bit(press, B_SAVE) && (fm1_in.notes >> KEY_SHIFT) & 1u) {   /* SHIFT and SAVE: the presets, or back */
        if (ui.pre_page)
            ui.pre_page = 0;
        else {
            ui.pre_page = SP_LIST;
            ui.pre_eng = ui.engine[ui.sel];
            if (!ui.pre_ix)
                ui.pre_ix = 1;
            ui.rv_page = ui.ses_page = ui.sc_page = 0;
        }
        ui.force = 1;
    } else if (btn_bit(press, B_SAVE)) {
        ui.save_t0 = fm1_ms;
        ui.save_down = 1;
        ui.save_long = 0;
    }
    if (ui.save_down) {
        if (!btn_bit(fm1_in.buttons, B_SAVE)) {
            ui.save_down = 0;
            if (!ui.save_long)
                save_short();
        } else if (!ui.save_long && (uint32_t)(fm1_ms - ui.save_t0) >= 800u) {
            ui.save_long = 1;
            save_long();
        }
    }
    if ((fm1_in.notes >> KEY_SHIFT) & 1u && (s = enc_take(EN_K4)) != 0) {   /* SHIFT + KNOB 4: the track's delay send */
        ui.dsend[ui.sel] = (int8_t)clampi(ui.dsend[ui.sel] + 2 * s, 0, 100);
        level_pan_apply(ui.sel);
        ui.dial = DIAL_DSEND;
        ui.dial_until = fm1_ms + 1000u;
    }
    if (ui.rv_page) {                            /* KNOB 1..4: the reverb, or the delay */
        static const uint8_t K[4] = {EN_K1, EN_K2, EN_K3, EN_K4};
        uint32_t j;
        int8_t *pv = ui.fx ? ui.dlp : ui.rvp;
        for (j = 0; j < 4u; j++) {
            s = enc_take(K[j]);
            if (s) {
                pv[j] = (int8_t)clampi(pv[j] + 2 * s, 0, 100);
                if (ui.fx)
                    dl_param(ui.dlp[0], ui.dlp[1], ui.dlp[2], ui.dlp[3]);
                else
                    rv_param(ui.rvp[0], ui.rvp[1], ui.rvp[2], ui.rvp[3]);
                ui.rv_touched = (int8_t)j;
                ui.rv_touch_until = fm1_ms + 1500u;
            }
        }
    } else if (ui.edit) {                        /* EDIT: KNOB 1 .. 4, the sound's macros; each its dial */
        static const uint8_t K[4] = {EN_K1, EN_K2, EN_K3, EN_K4};
        uint32_t j;
        for (j = 0; j < NMAC; j++) {
            s = enc_take(K[j]);
            if (s) {
                int8_t *m = &ui.mac[ui.sel][ui.engine[ui.sel]][j];
                *m = (int8_t)clampi(*m + 2 * s, -MAC_MAX, MAC_MAX);
                mac_apply(ui.sel);
                ui.dial = (uint8_t)(DIAL_MAC + j);
                ui.dial_until = fm1_ms + 1000u;
            }
        }
    } else {
        s = enc_take(EN_K1);                     /* KNOB 1: the layer's filter; its dial over the screen */
        if (s) {
            ui.flt[ui.sel] = (int8_t)clampi(ui.flt[ui.sel] + 2 * s, -FLT_MAX, FLT_MAX);
            level_pan_apply(ui.sel);
            ui.dial = DIAL_FLT;
            ui.dial_until = fm1_ms + 1000u;
        }
        s = enc_take(EN_K2);                     /* KNOB 2: the layer's granulator */
        if (s) {
            ui.gr[ui.sel] = (int8_t)clampi(ui.gr[ui.sel] + 2 * s, -FLT_MAX, FLT_MAX);
            level_pan_apply(ui.sel);
            ui.dial = DIAL_GR;
            ui.dial_until = fm1_ms + 1000u;
        }
        s = enc_take(EN_K3);                     /* KNOB 3: the layer's crush */
        if (s) {
            ui.cr[ui.sel] = (int8_t)clampi(ui.cr[ui.sel] + 2 * s, -FLT_MAX, FLT_MAX);
            level_pan_apply(ui.sel);
            ui.dial = DIAL_CR;
            ui.dial_until = fm1_ms + 1000u;
        }
        s = enc_take(EN_K4);                     /* KNOB 4: the layer's send */
        if (s) {
            ui.send[ui.sel] = (int8_t)clampi(ui.send[ui.sel] + 2 * s, 0, 100);
            level_pan_apply(ui.sel);
            ui.dial = DIAL_SEND;
            ui.dial_until = fm1_ms + 1000u;
        }
    }
    if (btn_bit(press, B_HOME))
        new_sound();
    if (btn_bit(press, B_REC)) {                 /* the looper: at once (the loop ends exactly here) */
        const looper_t *p = &loop[ui.sel];
        char m[12] = "LOOP ";
        ev_post(EV_LOOP_REC, ui.sel, 0, 0);
        ui.rec_t0 = fm1_ms;
        ui.rec_down = 1;
        ui.rec_done = 0;
        if (p->rec == LR_REC) {                  /* ended: LOOP n */
            fmt_int(m + 5, p->ntrk + 1);
            say(m, 1200);
        } else if (p->rec == LR_IDLE && p->ntrk >= LOOP_TRK)
            say("10 LOOPS", 1500);               /* full: not armed */
    }
    if (ui.rec_down && !btn_bit(fm1_in.buttons, B_REC))
        ui.rec_down = 0;
    if (ui.rec_down && !ui.rec_done && (uint32_t)(fm1_ms - ui.rec_t0) >= 1000u) {   /* held: emptied */
        ev_post(EV_LOOP_CLEAR, ui.sel, 0, 0);
        ui.lxt[ui.sel] = 0;                      /* (emptied: its own transposition gone too) */
        lx_post();
        char d[8] = "LOOP  ";
        ui.rec_done = 1;
        d[5] = (char)('1' + ui.sel);
        flash(d, "EMPTY", LAYER_COL[ui.sel]);
    }
    if (btn_bit(press, B_PLAY)) {
        if ((fm1_in.notes >> KEY_SHIFT) & 1u) {  /* SHIFT and PLAY, any layer: every loop stops if one plays, */
            uint32_t l, playing = 0, kept = 0;   /* else every loop there is starts, together */
            for (l = 0; l < NLAYER; l++) {
                playing |= loop[l].play;
                kept |= loop[l].ntrk != 0u;
            }
            if (playing || kept)
                ev_post(EV_LOOPS, 0, 0, (uint32_t)!playing);
            say(playing ? "LOOPS OFF" : kept ? "LOOPS ON" : "NO LOOPS", 1200);
        } else {                                 /* PLAY: let go soon, the loops; held, the overdubs */
            ui.play_down = 1;
            ui.play_t0 = fm1_ms;
        }
    }
    if (ui.play_down) {
        if (!btn_bit(fm1_in.buttons, B_PLAY)) {
            ui.play_down = 0;
            if (ui.ovd) {
                ui.ovd = 0;
                ui.force = 1;
            } else
                ev_post(EV_LOOP_PLAY, ui.sel, 0, 0);
        } else if (!ui.ovd && (uint32_t)(fm1_ms - ui.play_t0) >= 450u) {
            ui.ovd = 1;                          /* held: the chosen layer's loops on the white keys */
            ui.force = 1;
        }
    }
    if (ui.sc_page) {                            /* the SCALE page: SELECT the root, ALGORITHM the scale */
        s = enc_take(EN_ALGO);
        if (s) {
            ui.scale = (uint8_t)clampi((int32_t)ui.scale + s, 0, NSCALE - 1);
            lx_post();                           /* (the loops moved walk the new scale) */
            ui.sc_touched = 1;
            ui.sc_touch_until = fm1_ms + 1500u;
        }
        s = enc_take(EN_SELECT);
        if (s) {
            ui.root = (uint8_t)(((int32_t)ui.root + s % 12 + 12) % 12);   /* round the octave */
            lx_post();
            ui.sc_touched = 0;
            ui.sc_touch_until = fm1_ms + 1500u;
        }
    }
    s = enc_take(EN_SELECT);
    if (s) {                                     /* the level */
        char m[12] = "VOL ";                     /* as the knob's share of its turn: 0 .. 100 % */
        int32_t lv = clampi(ui.level[ui.sel] + s, 0, LEVEL_MAX);
        ui.level[ui.sel] = (int8_t)lv;
        level_pan_apply(ui.sel);
        if (lv == 0)
            str_cpy(m + 4, "OFF", 8);
        else {
            fmt_int(m + 4, (lv * 100 + LEVEL_MAX / 2) / LEVEL_MAX);
            str_cpy(m + str_len(m), "%", 2);
        }
        say(m, 1200);
    }
    s = enc_take(EN_ALGO);
    if (s) {                                     /* the pan */
        char m[12] = "PAN ";
        int32_t pn = clampi(ui.pan[ui.sel] + s, -PAN_MAX, PAN_MAX), pc = (pn < 0 ? -pn : pn) * 100 / PAN_MAX;
        ui.pan[ui.sel] = (int8_t)pn;
        level_pan_apply(ui.sel);
        if (!pn)
            str_cpy(m + 4, "C", 8);
        else {
            m[4] = pn < 0 ? 'L' : 'R';
            m[5] = ' ';
            fmt_int(m + 6, pc);
        }
        say(m, 1200);
    }
    if ((btn_bit(press, B_OCTDN) || btn_bit(press, B_OCTUP)) && (fm1_in.notes >> KEY_SHIFT) & 1u) {
        char m[16] = "TRANSP ";                  /* SHIFT and OCT: every loop a step of the scale down or up; */
        int32_t lim = 2 * SCALE_N[ui.scale], t = -1, x;   /* a track's button held too: that track's loops only */
        for (l = 0; l < NLAYER; l++)
            if (btn_bit(fm1_in.buttons, LAYER_BTN[l]))
                t = (int32_t)l;
        if (t >= 0) {
            x = clampi(ui.lxt[t] + (btn_bit(press, B_OCTUP) ? 1 : -1), -lim, lim);
            ui.lxt[t] = (int8_t)x;
            ui.trk_done = 1;                     /* (its button held for this: no copy) */
            m[7] = 'T';
            m[8] = (char)('1' + t);
            m[9] = ' ';
            m[10] = 0;
        } else {
            x = clampi(ui.lx + (btn_bit(press, B_OCTUP) ? 1 : -1), -lim, lim);
            ui.lx = (int8_t)x;
        }
        lx_post();
        if (x > 0)
            str_cpy(m + str_len(m), "+", 2);
        fmt_int(m + str_len(m), x);
        say(m, 1200);
    } else if (btn_bit(press, B_OCTDN) || btn_bit(press, B_OCTUP)) {
        char m[8] = "OCT  ";
        int32_t o = clampi(ui.oct[ui.sel] + (btn_bit(press, B_OCTUP) ? 1 : -1), -3, 3);
        ui.oct[ui.sel] = (int8_t)o;
        m[4] = o < 0 ? '-' : o > 0 ? '+' : ' ';
        m[5] = (char)('0' + (o < 0 ? -o : o));
        m[6] = 0;
        say(m, 1200);
    }
    s = enc_take(EN_PRESET);
    if (s && ui.pre_page == SP_LIST) {           /* PRESETS: through the presets */
        ui.pre_ix = (uint8_t)clampi((int32_t)ui.pre_ix + s, 1, PRE_SLOTS);
        s = 0;
    } else if (s && ui.pre_page == SP_ASK) {
        ui.pre_yes = (uint8_t)(s > 0);
        s = 0;
    } else if (s && ui.ses_page == SP_LIST) {    /* PRESETS: through the sessions */
        ui.ses_ix = (uint8_t)clampi((int32_t)ui.ses_ix + s, 1, SES_SLOTS);   /* every detent a slot */
        s = 0;
    } else if (s && ui.ses_page == SP_ASK) {     /* .. NO (left) or YES (right) */
        ui.ses_yes = (uint8_t)(s > 0);
        s = 0;
    }
    if (s) {
        int32_t e = clampi((int32_t)ui.engine[ui.sel] + (s > 0 ? 1 : -1), 0, NENG - 1);
        if (e != ui.engine[ui.sel]) {
            ui.engine[ui.sel] = (uint8_t)e;
            ev_post(EV_ENGINE, ui.sel, 0, (uint32_t)e);
            mac_apply(ui.sel);
        }
    }
    keys = fm1_in.notes;
    ch = keys ^ ui.keys;
    {   /* the chosen track's loops: backwards or PENDULUM (or forwards again); half or double speed (or normal) */
        const looper_t *p = &loop[ui.sel];
        if ((ch & keys) >> KEY_REV & 1u || (ch & keys) >> KEY_PEND & 1u || (ch & keys) >> KEY_RAND & 1u) {
            uint32_t want = (ch & keys) >> KEY_REV & 1u ? LD_REV : (ch & keys) >> KEY_PEND & 1u ? LD_PEND : LD_RAND;
            want = p->dmode == want ? LD_FWD : want;
            ev_post(EV_LOOP_MODE, ui.sel, 0, want);
            say(want == LD_REV ? "REVERSE" : want == LD_PEND ? "PENDULUM" : want == LD_RAND ? "RANDOM" : "FORWARD", 1200);
        }
        if ((ch & keys) >> KEY_HALF & 1u || (ch & keys) >> KEY_DOUBLE & 1u) {
            uint32_t want = (ch & keys) >> KEY_HALF & 1u ? LS_HALF : LS_DOUBLE;
            want = p->rate == want ? LS_NORMAL : want;
            ev_post(EV_LOOP_MODE, ui.sel, 1, want);
            say(want == LS_HALF ? "HALF SPEED" : want == LS_DOUBLE ? "DOUBLE SPEED" : "NORMAL SPEED", 1200);
        }
    }
    if ((ch & keys) >> KEY_EDIT & 1u) {          /* EDIT on or off */
        ui.edit = (uint8_t)!ui.edit;
        say(ui.edit ? "EDIT" : "EDIT OFF", 900);
    }
    for (k = KEY_AMB; k <= KEY_EXP; k += KEY_EXP - KEY_AMB)
        if ((ch & keys) >> k & 1u) {              /* a path for the layer chosen: its next sounds lean to it */
            uint32_t pa = k == KEY_AMB ? PATH_AMBIENT : PATH_EXP;
            ui.path[ui.sel] = (uint8_t)(ui.path[ui.sel] == pa ? PATH_OPEN : pa);
            say(ui.path[ui.sel] == PATH_AMBIENT ? "AMBIENT" : ui.path[ui.sel] == PATH_EXP ? "EXPERIMENTAL" : "OPEN", 1200);
        }
    if ((ch & keys) >> KEY_SHIFT & 1u) {         /* SHIFT pressed: the keys held latch, on the layer they play */
        for (k = 0; k < NKEY; k++)
            if (((ui.keys & keys) >> k & 1u) && white_ix(k) >= 0 && ui.key_layer[k] != KEY_NONE) {
                uint32_t l = ui.key_layer[k];
                ui.latch[l] |= 1u << k;
                ui.latch_note[l][k] = ui.key_note[k];
                ui.key_layer[k] = KEY_NONE;     /* (letting go of the key no longer ends it) */
            }
    }
    for (k = 0; ch && k < NKEY; k++) {
        if (!((ch >> k) & 1u))
            continue;
        if (white_ix(k) < 0)
            continue;                            /* the black keys: no notes */
        if (((keys >> k) & 1u) && ui.ovd) {      /* PLAY held: white key w, loop w in or out */
            uint32_t w = (uint32_t)white_ix(k);
            const looper_t *p = &loop[ui.sel];
            ui.key_layer[k] = KEY_NONE;
            if (w < p->ntrk)
                ev_post(EV_LOOP_TRK, ui.sel, w, (p->mute >> w) & 1u);
            ui.force = 1;
        } else if ((keys >> k) & 1u) {
            /* (moved as the track's loops are -- all of them, and its own -- so what is added fits them) */
            uint32_t note = scale_note(white_ix(k) + ui.lx + ui.lxt[ui.sel], ui.oct[ui.sel]), bit = 1u << k;
            ui.key_layer[k] = KEY_NONE;
            if (ui.latch[ui.sel] & bit) {        /* a latched key: SHIFT held lets go of it; alone, nothing */
                if ((keys >> KEY_SHIFT) & 1u) {
                    ui.latch[ui.sel] &= ~bit;
                    note_release(ui.sel, ui.latch_note[ui.sel][k]);
                }
            } else {
                ui.key_layer[k] = ui.sel;
                ui.key_note[k] = (uint8_t)note;
                note_press(ui.sel, note);
            }
        } else if (ui.key_layer[k] != KEY_NONE)
            note_release(ui.key_layer[k], ui.key_note[k]);
    }
    ui.keys = keys;
    midi_take();
}

/* ---------------------------------------------------------------- LEDs --- */
static void led_dim_key(uint32_t id, int on)
{
    uint32_t p, r;
    for (p = 0; p < FM1_NCOL; p++)
        for (r = 1; r < 5u; r++)
            if (FM1_KEYMAP[r][p] == (int8_t)id) {
                if (on)
                    fm1_led_dim[p] |= (uint8_t)(1u << r);
                else
                    fm1_led_dim[p] &= (uint8_t)~(1u << r);
            }
}
static void ui_leds(void)
{
    uint32_t l, k;
    for (l = 0; l < NLAYER; l++) {
        fm1_led_key(PANEL_BTN[LAYER_BTN[l]], clip_from() == (int)l ? (fm1_ms / 250u) & 1u : l == ui.sel);   /* (on the
                                                                                                         * clipboard: it blinks) */
        led_dim_key(PANEL_BTN[LAYER_BTN[l]], layer_sounding(l));   /* the others glow while they sound */
    }
    {
        const looper_t *p = &loop[ui.sel];
        fm1_led_key(PANEL_BTN[B_REC], p->rec == LR_REC || (p->rec == LR_ARMED && (fm1_ms / 250u) & 1u));
        fm1_led_key(PANEL_BTN[B_PLAY], p->play && p->ntrk);
    }
    fm1_led_key(PANEL_BTN[B_HOME], (int32_t)(ui.home_until - fm1_ms) > 0);
    fm1_led_key(PANEL_BTN[B_SEQ], ui.rv_page);
    fm1_led_key(PANEL_BTN[B_ARP], ui.sc_page);
    fm1_led_key(PANEL_BTN[B_OCTDN], ui.oct[ui.sel] < 0);
    fm1_led_key(PANEL_BTN[B_OCTUP], ui.oct[ui.sel] > 0);
    for (k = 0; k < NKEY; k++) {
        int32_t w = white_ix(k);
        if (ui.ovd)                              /* PLAY held: the loops that play, lit */
            fm1_led_key(14u + k, w >= 0 && (uint32_t)w < loop[ui.sel].ntrk && !((loop[ui.sel].mute >> w) & 1u));
        else
            fm1_led_key(14u + k, w >= 0 && ((((ui.keys >> k) & 1u) && ui.key_layer[k] == ui.sel) ||
                                            ((ui.latch[ui.sel] >> k) & 1u)));
    }
    fm1_led_key(14u + KEY_SHIFT, (ui.keys >> KEY_SHIFT) & 1u);
    fm1_led_key(14u + KEY_AMB, ui.path[ui.sel] == PATH_AMBIENT);
    fm1_led_key(14u + KEY_EXP, ui.path[ui.sel] == PATH_EXP);
    fm1_led_key(14u + KEY_EDIT, ui.edit);
    fm1_led_key(14u + KEY_REV, loop[ui.sel].dmode == LD_REV);
    fm1_led_key(14u + KEY_PEND, loop[ui.sel].dmode == LD_PEND);
    fm1_led_key(14u + KEY_RAND, loop[ui.sel].dmode == LD_RAND);
    fm1_led_key(14u + KEY_HALF, loop[ui.sel].rate == LS_HALF);
    fm1_led_key(14u + KEY_DOUBLE, loop[ui.sel].rate == LS_DOUBLE);
}

/* -------------------------------------------------------------- screen --- */
#define BAND_A 0                 /* the layer's number */
#define BAND_B 98                /* its engine */
#define BAND_C 146               /* its sound, or a message */
#define BAND_M 192               /* its level: the meter, across */
#define BAND_D 216               /* its pan, across, at the very bottom */
#define C_REC RGB(255, 30, 30)
#define C_SEND RGB(140, 190, 255)

static void draw_number(void)
{
    char d[2] = {(char)('1' + ui.sel), 0};
    CV_AREA(0, BAND_A, 240, BAND_B - BAND_A, C_BLACK)
        cv_text_c(3, &FONT_XL, d, LAYER_COL[ui.sel]);
}
static void draw_engine(void)
{
    const char *e = ENG_NAME[ui.engine[ui.sel]];
    cv_begin(240, 48, C_BLACK);
    if (text_w(&FONT_L, e) <= (int32_t)cv_w - 4)
        cv_text_c(2, &FONT_L, e, C_WHITE);
    else
        cv_text_fit((48 - FONT_M.h) / 2, &FONT_M, e, C_WHITE);   /* (CALIGULA: too long for the large size) */
    cv_blit(0, BAND_B);
}
/* the message band's size for s: M, or S if even squeezed M will not fit the width */
static const font_t *band_font(const char *s)
{
    return text_w(&FONT_M, s) - 6 * ((int32_t)str_len(s) - 1) <= 236 ? &FONT_M : &FONT_S;
}
/* message: 0 the sound (layer colour on black), 1 a message (black on white), 2 the looper (white on red),
 * 3 the looper's other flash (red on black) */
static void draw_sound(const char *name, int message)
{
    static const uint16_t BG[4] = {C_BLACK, C_WHITE, RGB(220, 0, 0), C_BLACK};
    uint16_t fg = message == 0 ? LAYER_COL[ui.sel] : message == 1 ? C_BLACK : message == 2 ? C_WHITE : RGB(255, 40, 40);
    const font_t *f = band_font(name);
    cv_begin(240, 44, BG[message & 3]);
    cv_text_fit((44 - f->h) / 2, f, name, fg);
    cv_blit(0, BAND_C);
}

/* the meter, across: 228 px for -48 .. +6 dBFS (of a layer's own output, after its level) */
#define METER_X 6
#define METER_LEN 228
static int32_t meter_px_db10(int32_t db10)   /* tenths of a dB -> pixels from the left */
{
    return clampi((db10 + 480) * METER_LEN / 540, 0, METER_LEN);
}
static int32_t meter_px(int32_t peak)
{
    int32_t db10;
    if (peak < 130)
        return 0;                                /* below -48 dB */
    db10 = (int32_t)(((int64_t)(log2_q16((uint32_t)peak) - (15 << 16)) * 602) >> 16) / 10;   /* 20 log10 = 6.02 log2 */
    return meter_px_db10(db10);
}
static void draw_meter(int32_t px)
{
    int32_t lv = ui.level[ui.sel], mark;
    cv_begin(240, BAND_D - BAND_M - 2, C_BLACK);
    cv_frame(METER_X - 2, 3, METER_LEN + 4, 16, 2, C_GREY);
    if (px)
        cv_rect(METER_X, 5, px, 12, LAYER_COL[ui.sel]);
    if (lv) {                                    /* the level the knob sets: a white bar right across */
        mark = METER_X + meter_px_db10((lv - LEVEL_UNITY) * 15 - 150);   /* where -15 dBFS of one layer lands */
        cv_rect(mark - 2, 0, 5, 22, C_WHITE);
    } else
        cv_rect(METER_X - 2, 0, 5, 22, C_REC);   /* off: red at the left */
    cv_blit(0, BAND_M);
    ui.shown_meter = px;
    ui.shown_level = lv;
}
static void draw_pan(void)
{
    int32_t pn = ui.pan[ui.sel], x = 120 + pn * 104 / PAN_MAX;   /* the track: x 16 .. 224 */
    cv_begin(240, 240 - BAND_D, C_BLACK);
    cv_rect(6, 10, 228, 4, C_GREY);
    cv_rect(6, 4, 3, 16, C_GREY);                /* the ends */
    cv_rect(231, 4, 3, 16, C_GREY);
    cv_rect(119, 2, 3, 20, C_WHITE);             /* the middle */
    cv_rect(x - 9, 0, 18, 24, C_WHITE);          /* the layer's place: its colour, a white edge */
    cv_rect(x - 6, 3, 12, 18, LAYER_COL[ui.sel]);
    cv_blit(0, BAND_D);
    ui.shown_pan = pn;
}

/* the REVERB page: a title, then a row a parameter: its name, its value, a bar under them */
#define C_DELAY RGB(255, 170, 90)
static void draw_rv_title(void)
{
    cv_begin(240, 52, C_BLACK);
    cv_text_c(3, &FONT_L, ui.fx ? "DELAY" : "REVERB", ui.fx ? C_DELAY : C_SEND);
    cv_blit(0, 0);
}
static void draw_rv_row(uint32_t j, int touched)
{
    char v[12];
    const int8_t *pv = ui.fx ? ui.dlp : ui.rvp;
    uint16_t bg = touched ? C_WHITE : C_BLACK, fg = touched ? C_BLACK : C_WHITE;
    cv_begin(240, 47, bg);
    {
        const char *name = ui.fx ? DL_NAME[j] : RV_NAME[j];
        const font_t *f = &FONT_M;
        int32_t y = 1;
        if (ui.fx && j == 0u) {                  /* (the delay's TIME in ms) */
            fmt_int(v, dl_ms(pv[0]));
            str_cpy(v + str_len(v), " MS", 4);
        } else
            fmt_int(v, pv[j]);
        if (text_w(f, name) + text_w(f, v) > 220) {   /* (FEEDBACK 60, TIME 400 MS: a size smaller) */
            f = &FONT_S;
            y = 1 + (FONT_M.h - FONT_S.h) / 2;
        }
        cv_text(6, y, f, name, fg);
        cv_text(234 - text_w(f, v), y, f, v, fg);
    }
    cv_rect(6, 38, 228, 6, touched ? C_GREY : RGB(60, 60, 60));
    cv_rect(6, 38, pv[j] * 228 / 100, 6, touched ? C_BLACK : ui.fx ? C_DELAY : C_SEND);
    cv_blit(0, 52 + 47 * j);
    ui.shown_rvp[j] = pv[j];
}
static void draw_rv_page(void)
{
    uint32_t j;
    int32_t t = (int32_t)(ui.rv_touch_until - fm1_ms) > 0 ? ui.rv_touched : -1;
    if (ui.force || !ui.shown_page) {
        draw_rv_title();
        for (j = 0; j < 4u; j++)
            draw_rv_row(j, (int32_t)j == t);
        ui.shown_page = (uint8_t)(1 + ui.fx * 10);
        ui.shown_touch = (int8_t)t;
        ui.force = 0;
        return;
    }
    for (j = 0; j < 4u; j++)
        if (ui.shown_rvp[j] != (ui.fx ? ui.dlp : ui.rvp)[j] || (((int32_t)j == t) != ((int32_t)j == ui.shown_touch)))
            draw_rv_row(j, (int32_t)j == t);
    ui.shown_touch = (int8_t)t;
}

/* the SCALE page: the title; the root, very large; the scale's name, one or two lines. What was just turned
 * is drawn black on white for a moment */
#define C_SCALE RGB(255, 196, 90)
static void draw_sc_page(void)
{
    int32_t t = (int32_t)(ui.sc_touch_until - fm1_ms) > 0 ? ui.sc_touched : -1;
    uint16_t bg, fg;
    if (!ui.force && ui.shown_scale == ui.scale && ui.shown_root == ui.root && ui.shown_sc_touch == t)
        return;
    cv_begin(240, 50, C_BLACK);
    cv_text_c(6, &FONT_M, "SCALE", C_SCALE);
    cv_blit(0, 0);
    bg = t == 0 ? C_WHITE : C_BLACK;
    fg = t == 0 ? C_BLACK : C_WHITE;
    CV_AREA(0, 50, 240, 100, bg)
        cv_text_c(4, &FONT_XL, NOTE_NAME[ui.root], fg);
    bg = t == 1 ? C_WHITE : C_BLACK;
    fg = t == 1 ? C_BLACK : C_SCALE;
    CV_AREA(0, 150, 240, 90, bg) {
        if (SCALE_L2[ui.scale][0]) {
            cv_text_fit(4, &FONT_M, SCALE_L1[ui.scale], fg);
            cv_text_fit(46, &FONT_M, SCALE_L2[ui.scale], fg);
        } else
            cv_text_fit(26, &FONT_M, SCALE_L1[ui.scale], fg);
    }
    ui.shown_scale = ui.scale;
    ui.shown_root = ui.root;
    ui.shown_sc_touch = (int8_t)t;
}

/* a layer's knob as a dial over the whole screen, drawn band by band (the canvas holds 112 rows): its name and
 * the layer's number; a dial from 7 o'clock to 5, the way it is turned in the layer's colour, a white pointer;
 * under it what it does. The filter and the granulator are off at 12 o'clock and go either way; the send
 * starts at 7 o'clock */
#define DIAL_CX 120
#define DIAL_CY 124
#define DIAL_R 76
static void dial_dot(int32_t oy, int32_t x, int32_t y, int32_t r, uint16_t c) { cv_rect(x - r, y - r - oy, 2 * r + 1, 2 * r + 1, c); }
/* the point at angle a (a turn = 2^32, 0 at the top, clockwise) and radius r */
static void dial_at(uint32_t a, int32_t r, int32_t *x, int32_t *y)
{
    *x = DIAL_CX + mulq15(r, sin_ph(a));
    *y = DIAL_CY - mulq15(r, sin_ph(a + 0x40000000u));
}
static void flt_label(char *m, int32_t v)
{
    int32_t hz;
    if (!v) {
        str_cpy(m, "OFF", 12);
        return;
    }
    hz = (int32_t)exp2_q16(flt_oct(v) - (16 << 16));
    str_cpy(m, v < 0 ? "LP " : "HP ", 12);
    if (hz < 1000)
        fmt_int(m + 3, hz);
    else
        fmt_fix(m + 3, hz / 100, 1);
    str_cpy(m + str_len(m), hz < 1000 ? " HZ" : " KHZ", 6);
}
static int32_t dial_value(void)
{
    if (ui.dial >= DIAL_MAC)
        return ui.mac[ui.sel][ui.engine[ui.sel]][(ui.dial - DIAL_MAC) & 3u];
    return ui.dial == DIAL_FLT ? ui.flt[ui.sel] : ui.dial == DIAL_GR ? ui.gr[ui.sel] : ui.dial == DIAL_CR ? ui.cr[ui.sel]
           : ui.dial == DIAL_DSEND ? ui.dsend[ui.sel] : ui.send[ui.sel];
}
static void draw_dial(void)
{
    static const int32_t BAND[3][2] = {{0, 112}, {112, 112}, {224, 16}};
    static const char *const NAME[5] = {"FILTER  ", "GRAIN  ", "REVERB  ", "CRUSH  ", "DELAY  "};
    int32_t v = dial_value(), b, k, from, to;    /* the arc lit, in steps of 3 degrees from 12 o'clock */
    uint32_t av;
    char t[16], m[16];
    uint16_t col = LAYER_COL[ui.sel];
    const font_t *tf;
    if (ui.dial >= DIAL_MAC) {                   /* EDIT's macro: its name for this sound, and the layer */
        str_cpy(t, mac_name(ui.sel, (ui.dial - DIAL_MAC) & 3u), sizeof t - 2);
        str_cpy(t + str_len(t), "  ", 3);
    } else
        str_cpy(t, NAME[ui.dial], sizeof t);
    t[str_len(t) - 1] = (char)('1' + ui.sel);
    tf = text_w(&FONT_M, t) <= 236 ? &FONT_M : &FONT_S;   /* (a long name, SATURATION 1: a size smaller) */
    if (ui.dial == DIAL_SEND || ui.dial == DIAL_DSEND) {   /* 0 .. 100: from 7 o'clock round */
        from = -45;
        to = -45 + v * 90 / 100;
        av = 0xA0000000u + (0xC0000000u / 100u) * (uint32_t)v;   /* -135 degrees, + 270 x v / 100 */
        str_cpy(m, "SEND ", sizeof m);
        fmt_int(m + 5, v);
    } else {                                     /* -64 .. 64: either way from 12 o'clock */
        from = 0;
        to = v * 45 / FLT_MAX;
        av = (uint32_t)((int32_t)(0x60000000u / FLT_MAX) * v);
        if (ui.dial == DIAL_FLT)
            flt_label(m, v);
        else if (ui.dial >= DIAL_MAC) {          /* +40 %, -25 %, or the sound as it was drawn */
            if (!v)
                str_cpy(m, "AS DRAWN", sizeof m);
            else {
                m[0] = v > 0 ? '+' : '-';
                fmt_int(m + 1, (v < 0 ? -v : v) * 100 / MAC_MAX);
                str_cpy(m + str_len(m), "%", 2);
            }
        } else if (!v)
            str_cpy(m, "OFF", sizeof m);
        else if (ui.dial == DIAL_CR) {           /* BITS 6.5, RATE 2.4K, RATE 300 */
            if (v < 0) {
                str_cpy(m, "BITS ", sizeof m);
                fmt_fix(m + 5, cr_bits10(v), 1);
            } else {
                int32_t hz = cr_hz(v);
                str_cpy(m, "RATE ", sizeof m);
                if (hz < 1000)
                    fmt_int(m + 5, hz);
                else
                    fmt_fix(m + 5, hz / 100, 1);
                if (hz >= 1000)
                    str_cpy(m + str_len(m), "K", 2);   /* (short: RATE 2.6K reads at this size) */
            }
        }
        else {
            str_cpy(m, v < 0 ? "DARK " : "SCATTER ", sizeof m);
            fmt_int(m + str_len(m), (v < 0 ? -v : v) * 100 / FLT_MAX);
        }
    }
    if (to < from) {
        int32_t x_ = to;
        to = from;
        from = x_;
    }
    for (b = 0; b < 3; b++) {
        int32_t oy = BAND[b][0], x, y;
        CV_AREA(0, (uint32_t)oy, 240, (uint32_t)BAND[b][1], C_BLACK) {
        cv_text(((int32_t)cv_w - text_w(tf, t)) / 2, 2 + ((int32_t)FONT_M.h - (int32_t)tf->h) / 2 - oy, tf, t, col);
        for (k = -45; k <= 45; k++) {            /* the track, every 3 degrees */
            dial_at((uint32_t)(k * 35791394), DIAL_R, &x, &y);
            dial_dot(oy, x, y, 4, C_GREY);
        }
        if (v)                                   /* the way it is turned */
            for (k = from; k <= to; k++) {
                dial_at((uint32_t)(k * 35791394), DIAL_R, &x, &y);
                dial_dot(oy, x, y, 7, col);
            }
        for (k = 0; k <= DIAL_R - 16; k += 4) {  /* the pointer */
            dial_at(av, k, &x, &y);
            dial_dot(oy, x, y, 4, C_WHITE);
        }
        dial_dot(oy, DIAL_CX, DIAL_CY, 10, C_WHITE);
        if (b >= 1)
            cv_text_fit(200 - oy, &FONT_M, m, v ? C_WHITE : C_GREY);
        }
    }
    ui.shown_dial = (int16_t)(v + ui.dial * 256);
    ui.dial_drawn_at = fm1_ms;
}

/* the whole screen flashing (flash()) for 1.5 s, the colour and black taking turns */
static int all_digits(const char *t)
{
    if (!*t)
        return 0;
    for (; *t; t++)
        if (*t < '0' || *t > '9')
            return 0;
    return 1;
}
static int draw_flash(void)
{
    uint32_t t = fm1_ms - ui.flash_t0, ph = t / 250u, on;
    uint16_t bg, fg;
    if (!ui.flash_on)
        return 0;
    if (t >= 1500u) {
        ui.flash_on = 0;
        ui.force = 1;                              /* back to the screen as it was */
        return 0;
    }
    if (ph == ui.flash_phase)
        return 1;
    ui.flash_phase = (uint8_t)ph;
    on = !(ph & 1u);
    bg = on ? ui.flash_col : C_BLACK;
    fg = on ? C_BLACK : ui.flash_col;
    CV_AREA(0, 0, 240, 100, bg)
        cv_text_c(ui.flash_l2[0] ? 40 : 52, &FONT_L, ui.flash_l1, fg);
    CV_AREA(0, 100, 240, 112, bg) {
        if (all_digits(ui.flash_l2))
            cv_text_c(4, &FONT_XL, ui.flash_l2, fg);
        else
            cv_text_fit(10, &FONT_L, ui.flash_l2, fg);   /* (squeezed a little rather than cut at the edge) */
    }
    cv_begin(240, 28, bg);
    cv_blit(0, 212);
    return 1;
}

/* the SESSIONS page: the title, the session chosen (very large), where it is in the list; the question */
/* the question's answer, in a canvas 76 high: NO | YES, the one chosen in white */
static void draw_noyes(int yes)
{
    cv_frame(6, 10, 108, 60, 3, yes ? C_GREY : C_WHITE);
    cv_frame(126, 10, 108, 60, 3, yes ? C_WHITE : C_GREY);
    if (!yes)
        cv_rect(9, 13, 102, 54, C_WHITE);
    else
        cv_rect(129, 13, 102, 54, C_WHITE);
    cv_bg_set(yes ? C_BLACK : C_WHITE);
    cv_text(60 - text_w(&FONT_M, "NO") / 2, 22, &FONT_M, "NO", yes ? C_GREY : C_BLACK);
    cv_bg_set(yes ? C_WHITE : C_BLACK);
    cv_text(180 - text_w(&FONT_M, "YES") / 2, 22, &FONT_M, "YES", yes ? C_BLACK : C_GREY);
    cv_bg_set(C_BLACK);
}
/* text left-aligned at x (x < 0: centred in the canvas), a size smaller or squeezed a little to fit w */
static void cv_text_in(int32_t x, int32_t w, int32_t band_h, const char *t, uint16_t c)
{
    const font_t *f = &FONT_M;
    int32_t n = (int32_t)str_len(t), sq = 0;
    if (text_w(f, t) - 2 * (n - 1) > w)          /* (a size smaller rather than letters crushed together) */
        f = &FONT_S;
    if (text_w(f, t) > w && n > 1)
        sq = clampi((text_w(f, t) - w + n - 2) / (n - 1), 0, 4);
    if (x < 0)
        x = ((int32_t)cv_w - (text_w(f, t) - sq * (n - 1))) / 2;
    for (; *t; t++) {
        char one[2] = {*t, 0};
        x = cv_text(x, (band_h - f->h) / 2, f, one, c) - sq;
    }
}
static const char *kind_name(uint32_t e, int32_t k)
{
    return e == ENG_FM ? FF_NAME[k % FF_N] : e == ENG_ANIMA ? AK_NAME[k % AK_N] : e == ENG_CAL ? CS_NAME[k % CS_N]
                                                                                                : RK_NAME[k % RK_N];
}
/* the PRESETS page: the engine's name over four slots (the page of the one chosen), the chosen one in white; a
 * full slot's number in red with its sound's name, an empty one grey. Or, asked: OVERWRITE?, the slot, NO | YES */
static void draw_pre_page(void)
{
    char t[20], n[8];
    uint32_t j, first = (ui.pre_ix - 1u) / 4u * 4u + 1u;
    int16_t key = (int16_t)(ui.pre_ix * 4 + (ui.pre_page == SP_ASK ? 2 + ui.pre_yes : 0));
    if (!ui.force && ui.shown_pre == key)
        return;
    ui.shown_pre = key;
    if (ui.pre_page == SP_ASK) {
        const font_t *f = band_font("OVERWRITE?");
        cv_begin(240, 52, C_BLACK);
        cv_text_fit((52 - f->h) / 2, f, "OVERWRITE?", C_FULL);
        cv_blit(0, 0);
        n[0] = 'P';
        fmt_int(n + 1, ui.pre_ix);
        CV_AREA(0, 52, 240, 112, C_BLACK)
            cv_text_c(8, &FONT_XL, n + 1, C_FULL);
        CV_AREA(0, 164, 240, 76, C_BLACK)
            draw_noyes(ui.pre_yes);
        return;
    }
    str_cpy(t, ENG_NAME[ui.pre_eng], sizeof t);
    str_cpy(t + str_len(t), " PRESETS", 9);
    cv_begin(240, 52, C_BLACK);
    cv_text_in(-1, 228, 52, t, C_SES);
    cv_blit(0, 0);
    for (j = 0; j < 4u; j++) {
        uint32_t slot = first + j;
        int sel = slot == ui.pre_ix;
        int32_t k = pre_kind(ui.pre_eng, slot);
        uint16_t bg = sel ? C_WHITE : C_BLACK;
        cv_begin(240, 47, bg);                   /* (47: the rows meet, nothing of the screen before between) */
        n[0] = 'P';
        fmt_int(n + 1, (int32_t)slot);
        cv_text_in(6, 84, 47, n, k >= 0 ? C_FULL : sel ? C_BLACK : C_GREY);
        cv_text_in(92, 142, 47, k >= 0 ? kind_name(ui.pre_eng, k) : "EMPTY", sel ? C_BLACK : k >= 0 ? C_WHITE : C_GREY);
        cv_blit(0, 52 + 47 * j);
    }
}

/* PLAY held: the chosen layer's loops, 1 .. 10 as the white keys have them -- in, the layer's colour; out, a grey
 * frame; none yet, nothing */
static void draw_ovd_page(void)
{
    const looper_t *p = &loop[ui.sel];
    int32_t key = p->ntrk | p->mute << 4 | (int32_t)ui.sel << 16;
    uint32_t r, c;
    char t[12] = "LOOPS ";
    if (!ui.force && ui.shown_ovd == key)
        return;
    ui.shown_ovd = key;
    t[6] = (char)('1' + ui.sel);
    t[7] = 0;
    cv_begin(240, 52, C_BLACK);
    cv_text_c((52 - FONT_M.h) / 2, &FONT_M, p->ntrk ? t : "NO LOOPS", LAYER_COL[ui.sel]);
    cv_blit(0, 0);
    for (r = 0; r < 2u; r++)
        CV_AREA(0, 52 + 94 * r, 240, 94, C_BLACK)
        for (c = 0; c < 5u; c++) {
            uint32_t k = r * 5u + c;
            int32_t x = 4 + (int32_t)c * 47;
            char n[4];
            const font_t *f = k < 9u ? &FONT_M : &FONT_S;
            if (k >= p->ntrk)
                continue;
            fmt_int(n, (int32_t)k + 1);
            if (!((p->mute >> k) & 1u)) {
                cv_rect(x, 6, 43, 82, LAYER_COL[ui.sel]);
                cv_bg_set(LAYER_COL[ui.sel]);
                cv_text(x + (43 - text_w(f, n)) / 2, 6 + (82 - f->h) / 2, f, n, C_BLACK);
                cv_bg_set(C_BLACK);
            } else {
                cv_frame(x, 6, 43, 82, 3, C_GREY);
                cv_text(x + (43 - text_w(f, n)) / 2, 6 + (82 - f->h) / 2, f, n, C_GREY);
            }
        }
}

static void draw_ses_page(void)
{
    char n[8];
    int has = ses_has(ui.ses_ix);
    if (!ui.force && ui.shown_ses == (int8_t)ui.ses_page && ui.shown_ses_ix == (int16_t)(ui.ses_page == SP_ASK ? ui.ses_yes : ui.ses_ix))
        return;
    {
        const char *t = ui.ses_page != SP_ASK ? "SESSIONS" : ui.ses_over ? "OVERWRITE?" : "SAVE NEW";
        const font_t *f = band_font(t);
        cv_begin(240, 52, C_BLACK);
        cv_text_fit((52 - f->h) / 2, f, t, ui.ses_page == SP_ASK && ui.ses_over ? C_FULL : C_SES);
        cv_blit(0, 0);
    }
    fmt_int(n, ui.ses_page == SP_ASK ? ui.ses_new : ui.ses_ix);
    CV_AREA(0, 52, 240, 112, C_BLACK)            /* a slot with a session in it: its number in red; empty, grey */
        cv_text_c(8, &FONT_XL, n, ui.ses_page == SP_ASK ? (ui.ses_over ? C_FULL : C_WHITE) : has ? C_FULL : C_GREY);
    CV_AREA(0, 164, 240, 76, C_BLACK) {
        if (ui.ses_page == SP_ASK)
            draw_noyes(ui.ses_yes);
        else if (ui.ses_ix == ses_current())
            cv_text_c(20, &FONT_M, has ? "LOADED" : "CHOSEN", C_OK);
        else if (!has)
            cv_text_c(20, &FONT_M, "EMPTY", C_GREY); /* (a full one: its red number says it) */
    }
    ui.shown_ses = (int8_t)ui.ses_page;
    ui.shown_ses_ix = (int16_t)(ui.ses_page == SP_ASK ? ui.ses_yes : ui.ses_ix);
}

static void ui_draw(void)
{
    const char *name = sound_name(ui.sel, ui.engine[ui.sel]);
    int msg = (int32_t)(ui.msg_until - fm1_ms) > 0;
    const char *c = msg ? ui.msg : name;
    uint32_t st = loop[ui.sel].rec;
    char lp[12];
    int32_t px;
    if (draw_flash())
        return;
    if ((int32_t)(ui.dial_until - fm1_ms) > 0 && !ui.ses_page && !ui.sc_page && !ui.rv_page && !ui.pre_page && !ui.ovd) {
        if (ui.shown_page != 4 ||
            (ui.shown_dial != (int16_t)(dial_value() + ui.dial * 256) && (uint32_t)(fm1_ms - ui.dial_drawn_at) >= 40u))
            draw_dial();
        ui.shown_page = 4;
        ui.shown_ses = 0;
        return;
    }
    if (ui.ovd) {
        if (ui.shown_page != 6)
            ui.force = 1;
        draw_ovd_page();
        ui.shown_page = 6;
        ui.force = 0;
        return;
    }
    if (ui.pre_page) {
        if (ui.shown_page != 5)
            ui.force = 1;
        draw_pre_page();
        ui.shown_page = 5;
        ui.force = 0;
        return;
    }
    if (ui.ses_page) {
        draw_ses_page();
        ui.shown_page = 2;
        ui.force = 0;
        return;
    }
    ui.shown_ses = 0;
    if (ui.sc_page) {
        if (ui.shown_page != 3)
            ui.force = 1;
        draw_sc_page();
        ui.shown_page = 3;
        ui.force = 0;
        return;
    }
    if (ui.rv_page) {
        if (ui.shown_page != 1 + ui.fx * 10)
            ui.force = 1;                        /* from another page or the dial: all of it */
        draw_rv_page();
        return;
    }
    if (ui.shown_page) {                         /* back from a page: everything again */
        ui.shown_page = 0;
        ui.force = 1;
    }
    if (!msg && st != LR_IDLE) {                 /* the looper over the sound: red, unmissable; REC 3 */
        str_cpy(lp, st == LR_REC ? "REC " : "ARMED ", sizeof lp);
        fmt_int(lp + str_len(lp), loop[ui.sel].ntrk + 1);
        c = lp;
        msg = st == LR_REC ? 2 : (fm1_ms / 250u) & 1u ? 2 : 3;
    }
    if (ui.force || ui.shown_sel != ui.sel) {
        draw_number();
        ui.shown_sel = ui.sel;
        ui.shown_eng = ui.shown_msg = ui.shown_pan = ui.shown_level = -100;   /* the rest follows the layer */
    }
    if (ui.shown_eng != ui.engine[ui.sel]) {
        draw_engine();
        ui.shown_eng = ui.engine[ui.sel];
        ui.shown_msg = -1;
    }
    if (ui.shown_msg != msg || !str_eq(c, ui.shown_text)) {
        draw_sound(c, msg);
        str_cpy(ui.shown_text, c, sizeof ui.shown_text);
        ui.shown_msg = msg;
    }
    px = meter_px(lay[ui.sel].meter);
    if (ui.shown_level != ui.level[ui.sel] || ((uint32_t)(fm1_ms - ui.meter_at) >= 30u && px != ui.shown_meter)) {
        draw_meter(px);
        ui.meter_at = fm1_ms;
    }
    if (ui.shown_pan != ui.pan[ui.sel])
        draw_pan();
    ui.force = 0;
}

static void ui_init(void)
{
    uint32_t l;
    memset(&ui, 0, sizeof ui);
    memset(ui.midi_layer, 0xFF, sizeof ui.midi_layer);
    for (l = 0; l < NLAYER; l++) {
        ui.engine[l] = lay[l].engine;
        ui.level[l] = LEVEL_UNITY;
        ui.pan[l] = PAN_DEF[l];
        ui.send[l] = SEND_DEF;
        level_pan_apply(l);
    }
    for (l = 0; l < 4u; l++)
        ui.rvp[l] = RV_DEF[l];
    rv_param(ui.rvp[0], ui.rvp[1], ui.rvp[2], ui.rvp[3]);
    for (l = 0; l < 4u; l++)
        ui.dlp[l] = DL_DEF[l];
    dl_param(ui.dlp[0], ui.dlp[1], ui.dlp[2], ui.dlp[3]);
    ui.rv_touched = -1;
    ui.shown_sel = ui.shown_eng = ui.shown_msg = -1;
#if TELEMETRY
    ui.seed = 0x5EEDu;
#else
    ui.seed = fm1_ticks() ^ 0x5EEDu;
#endif
    ui.force = 1;
}

/* the sounds every layer starts with: one of each engine */
static void sounds_init(void)
{
    uint32_t l;
    for (l = 0; l < NLAYER; l++) {
        uint32_t s = next_seed();
        fm_generate(&fm_buf[l][0], -1, s, PATH_OPEN, GEN_NOW);
        fm_measure(&fm_buf[l][0]);
        an_design(&an_buf[l][0], -1, s * 7u + 3u, PATH_OPEN, GEN_NOW);
        an_measure(&an_buf[l][0]);
        cal_design(&cal_buf[l][0], -1, s * 13u + 5u, PATH_OPEN, GEN_NOW);
        cal_measure(&cal_buf[l][0]);
        rus_design(&rus_buf[l][0], -1, s * 17u + 9u, PATH_OPEN, GEN_NOW);
        rus_measure(&rus_buf[l][0]);
        fm_pub[l] = fm_use[l] = an_pub[l] = an_use[l] = 0;
        cal_pub[l] = cal_use[l] = rus_pub[l] = rus_use[l] = 0;
        gran_profile(l, next_seed());            /* the granulator's character, drawn */
    }
}
