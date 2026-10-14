/* SPDX-License-Identifier: GPL-3.0-only
 * TONITOR host harness: the firmware's own DSP, sounds, layers and screen, compiled for the computer.
 *
 *   host render DIR [SEEDS]   every FM family and ANIMA kind, SEEDS sounds each (default 3): a note held
 *                             8 s and released, a WAV each, and a line of figures (levels, clipping, cost)
 *   host mix DIR              the six layers entering one after another through the real mixer (mix.wav)
 *   host screens DIR          the screen in several states, as PPM images (tools/ppm2png.py makes PNGs)
 *   host flows                the panel's flows: held notes through changes, two layers at once, no stuck note
 *   host check                every sound of 40 seeds per family / kind: none silent, none clipping hard,
 *                             none running away; exits 1 on a failure (tests/run_tests.sh)
 *
 * Build: tests/run_tests.sh (or see the clang line there). */
#define HOST 1
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "tonitor_tables.h"

#define NELEM(a) (sizeof(a) / sizeof((a)[0]))
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
static volatile uint32_t fm1_ms;
static uint32_t host_ticks;
static uint32_t fm1_ticks(void) { return host_ticks += 24000u; }
static void host_pump(void);

static int str_eq(const char *a, const char *b) { return strcmp(a, b) == 0; }
static unsigned str_len(const char *s) { return (unsigned)strlen(s); }
static void str_cpy(char *d, const char *s, unsigned max)
{
    unsigned i = 0;
    while (s[i] && i + 1 < max) {
        d[i] = s[i];
        i++;
    }
    d[i] = 0;
}

static void fmt_fix(char *b, int32_t v, int decimals)   /* (libc.c's) */
{
    char t[12];
    int n = 0, neg = v < 0, i;
    uint32_t u = (uint32_t)(neg ? -v : v);
    do {
        t[n++] = (char)('0' + u % 10u);
        u /= 10u;
        if (n == decimals)
            t[n++] = '.';
    } while (u || n <= decimals);
    if (neg)
        t[n++] = '-';
    for (i = 0; i < n; i++)
        b[i] = t[n - 1 - i];
    b[n] = 0;
}
static void fmt_int(char *b, int32_t v) { fmt_fix(b, v, 0); }

/* the screen: a framebuffer instead of the panel */
static uint16_t fb[240 * 240];
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    uint32_t i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (x + i < 240u && y + j < 240u) {
                uint16_t c = px[j * w + i];
                fb[(y + j) * 240u + x + i] = (uint16_t)((c >> 8) | (c << 8));
            }
}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (x + i < 240u && y + j < 240u)
                fb[(y + j) * 240u + x + i] = c;
}

#include "dsp.h"
#include "gfx.c"
#include "fm.c"
#include "anima.c"
#include "cal.c"
#include "reverb.c"
#define MQ 64u
static uint32_t midi_in_q[MQ];
static volatile uint32_t mi_w, mi_r;
#include "layers.c"

/* the panel */
#define FM1_NCOL 11u
static const int8_t FM1_KEYMAP[6][FM1_NCOL] = {
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, {5, 11, 4, 10, 3, 9, 2, 8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13, 7, 6, 12}, {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    {0, 1, 15, 14, 17, 16, 19, 18, 20, 21, 22}, {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}};
static struct { uint32_t notes, buttons; } fm1_in;
static uint32_t host_press;
static int32_t host_enc[7];
static uint8_t fm1_led_dim[FM1_NCOL];
static uint32_t fm1_input_edges(uint32_t *rel)
{
    uint32_t p = host_press;
    host_press = 0;
    if (rel)
        *rel = 0;
    return p;
}
static int32_t fm1_enc_take(uint32_t e)
{
    int32_t s = host_enc[e];
    host_enc[e] = 0;
    return s;
}
static uint8_t host_led[64];
static void fm1_led_key(uint32_t id, int on) { if (id < 64u) host_led[id] = (uint8_t)!!on; }
#include "ui.c"
#include "sessions.c"

/* the flash, emulated: NOR (erased to 0xFF, programming only clears bits); erases counted */
static uint8_t host_flash[0x100000];
static uint32_t host_erases;
static int ses_flash_read(uint32_t off, void *p, uint32_t n)
{
    memcpy(p, host_flash + off, n);
    return 0;
}
static int ses_flash_erase(uint32_t off)
{
    if (off < SES_LO || off + 4096u > SES_HI || (off & 4095u))
        return -8;
    memset(host_flash + off, 0xFF, 4096u);
    host_erases++;
    return 0;
}
static int ses_flash_prog(uint32_t off, const void *p, uint32_t n)
{
    uint32_t i;
    if (off < SES_LO || off + n > SES_HI)
        return -8;
    for (i = 0; i < n; i++)
        host_flash[off + i] &= ((const uint8_t *)p)[i];
    return 0;
}

/* ------------------------------------------------------------- audio --- */
static int16_t *wav_buf;
static uint32_t wav_n, wav_cap, wav_ch;
static void wav_put(int32_t s)
{
    if (wav_n == wav_cap) {
        wav_cap = wav_cap ? wav_cap * 2 : 1 << 20;
        wav_buf = realloc(wav_buf, wav_cap * sizeof *wav_buf);
    }
    wav_buf[wav_n++] = (int16_t)clampi(s, -32768, 32767);
}
static void wav_write(const char *path)
{
    FILE *f = fopen(path, "wb");
    uint32_t bytes = wav_n * 2u, v;
    if (!f) {
        perror(path);
        exit(2);
    }
#define W32(x) (v = (x), fwrite(&v, 4, 1, f))
#define W16(x) do { uint16_t s_ = (uint16_t)(x); fwrite(&s_, 2, 1, f); } while (0)
    fwrite("RIFF", 1, 4, f);
    W32(36u + bytes);
    fwrite("WAVEfmt ", 1, 8, f);
    W32(16u);
    W16(1);
    W16(wav_ch);
    W32(44100u);
    W32(44100u * 2u * wav_ch);
    W16(2 * wav_ch);
    W16(16);
    fwrite("data", 1, 4, f);
    W32(bytes);
    fwrite(wav_buf, 2, wav_n, f);
    fclose(f);
    wav_n = 0;
}

typedef struct {
    double peak, rms_max, rms_all, sum2;
    uint32_t clip, n;
    double ns_block;
} stats_t;

static uint32_t host_path;       /* the path render_sound draws on */
/* one sound held for hold_s, then released for tail_s; out to the WAV buffer (mono, x LAYER_GAIN as a layer) */
static stats_t render_sound(int engine, int which, uint32_t seed, int note, double hold_s, double tail_s, int to_wav)
{
    static fm_patch_t pt;
    static an_design_t d;
    static an_voice_t av;
    fm_voice_t fv;
    stats_t st;
    int32_t out[CTL];
    uint32_t b, i, nb = (uint32_t)((hold_s + tail_s) * 44100.0 / CTL), off = (uint32_t)(hold_s * 44100.0 / CTL);
    double win = 0;
    uint32_t wn = 0;
    int32_t lim_env = 0, lim_g = Q15;
    clock_t t0;
    memset(&st, 0, sizeof st);
    memset(&fv, 0, sizeof fv);
    if (engine == ENG_FM) {
        fm_generate(&pt, which, seed, host_path);
        fm_measure(&pt);
        fm_note_on(&fv, &pt, note, 1);
    } else {
        an_design(&d, which, seed, host_path);
        an_measure(&d);
        an_init(&av, seed);
        an_note_on(&av, &d, pitch_inc(note * 16), 1);
    }
    t0 = clock();
    for (b = 0; b < nb; b++) {
        int alive;
        if (b == off) {
            if (engine == ENG_FM)
                fm_note_off(&fv, &pt);
            else
                an_note_off(&av);
        }
        alive = engine == ENG_FM ? fm_render(&fv, &pt, out) : an_render(&av, &d, out, CTL);
        if (!alive)
            memset(out, 0, sizeof out);
        for (i = 0; i < CTL; i++)
            out[i] = mulq12(out[i], LAYER_GAIN);
        lim_block(&lim_env, &lim_g, out, CTL);        /* as a layer does */
        for (i = 0; i < CTL; i++) {
            int32_t x = out[i];
            double a = x < 0 ? -x : x;
            if (a > st.peak)
                st.peak = a;
            if (a >= 32767)
                st.clip++;
            st.sum2 += (double)x * x;
            win += (double)x * x;
            if (++wn == 2048) {
                double r = win / wn;
                if (r > st.rms_max)
                    st.rms_max = r;
                win = 0;
                wn = 0;
            }
            if (to_wav)
                wav_put(x);
        }
        st.n += CTL;
    }
    st.ns_block = (double)(clock() - t0) / CLOCKS_PER_SEC * 1e9 / nb;
    st.rms_all = st.sum2 / st.n;
    return st;
}

static double db(double ms) { return ms > 0 ? 10.0 * __builtin_log10(ms / (32768.0 * 32768.0)) : -999; }
static double dbp(double p) { return p > 0 ? 20.0 * __builtin_log10(p / 32768.0) : -999; }

static void cmd_render(const char *dir, int seeds)
{
    int e, w, s;
    char path[512];
    wav_ch = 1;
    printf("%-8s %-9s %4s  %7s %7s %7s %6s %8s\n", "engine", "sound", "seed", "peak", "rmsmax", "rms", "clip", "us/block");
    for (e = 0; e < NENG; e++) {
        int nw = e == ENG_FM ? FF_N : AK_N;
        for (w = 0; w < nw; w++)
            for (s = 0; s < seeds; s++) {
                uint32_t seed = 1000u + (uint32_t)s * 7919u + (uint32_t)w * 31u;
                const char *nm = e == ENG_FM ? FF_NAME[w] : AK_NAME[w];
                stats_t st = render_sound(e, w, seed, s == 1 ? 43 : s == 2 ? 72 : 57, 8.0, 5.0, 1);
                snprintf(path, sizeof path, "%s/%s-%s-%d.wav", dir, ENG_NAME[e], nm, s + 1);
                wav_write(path);
                printf("%-8s %-9s %4d  %6.1f  %6.1f  %6.1f %6u %8.2f\n", ENG_NAME[e], nm, s + 1, dbp(st.peak),
                       db(st.rms_max), db(st.rms_all), st.clip, st.ns_block / 1000.0);
            }
    }
}

/* the ISR, on the computer: mix_block on demand (sound_slot_free waits on it) */
static int32_t mix_out[2 * CTL];
static uint32_t ms_frac;
static void host_pump(void)
{
    mix_block(mix_out);
    ms_frac += CTL * 1000u;
    fm1_ms += ms_frac / 44100u;
    ms_frac %= 44100u;
}
static void run_blocks(uint32_t nb, int to_wav)
{
    uint32_t i;
    while (nb--) {
        host_pump();
        if (to_wav)
            for (i = 0; i < 2 * CTL; i++)
                wav_put(mix_out[i]);
    }
}
/* a key by its place on the keyboard (0 = F3 .. 26 = G5) */
static void press_phys(uint32_t k, int down)
{
    if (down)
        fm1_in.notes |= 1u << k;
    else
        fm1_in.notes &= ~(1u << k);
    ui_input();
}
/* white key k (0 .. 15, wrapping): the keys that play */
static void press_key(uint32_t k, int down)
{
    uint32_t p;
    for (p = 0; p < NKEY; p++)
        if (white_ix(p) == (int32_t)(k % 16u))
            break;
    press_phys(p, down);
}
static void press_btn(uint32_t label)
{
    host_press |= 1u << PANEL_BTN[label];
    ui_input();
}

static void cmd_mix(const char *dir)
{
    char path[512];
    static const uint8_t KEY[NLAYER] = {0, 7, 12, 4, 19, 26};
    uint32_t l;
    wav_ch = 2;
    layers_init();
    ui_init();
    sounds_init();
    master_q12 = 4096;
    for (l = 0; l < NLAYER; l++) {           /* every 5 s another layer comes in, its key held */
        press_btn(LAYER_BTN[l]);
        press_key(KEY[l], 1);
        run_blocks(44100u * 5u / CTL, 1);
    }
    run_blocks(44100u * 5u / CTL, 1);
    for (l = 0; l < NLAYER; l++) {           /* and they let go, one by one */
        press_key(KEY[l], 0);
        run_blocks(44100u * 2u / CTL, 1);
    }
    run_blocks(44100u * 6u / CTL, 1);
    snprintf(path, sizeof path, "%s/mix.wav", dir);
    wav_write(path);
    printf("mix: %s\n", path);
}

static void ppm(const char *path)
{
    FILE *f = fopen(path, "wb");
    uint32_t i;
    fprintf(f, "P6\n240 240\n255\n");
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = fb[i];
        uint8_t rgb[3] = {(uint8_t)((c >> 11) << 3), (uint8_t)(((c >> 5) & 63u) << 2), (uint8_t)((c & 31u) << 3)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}
static void cmd_screens(const char *dir)
{
    char path[512];
    layers_init();
    ui_init();
    sounds_init();
    lcd_fill(0, 0, 240, 240, 0);
    ui.force = 1;
    ui_draw();
    snprintf(path, sizeof path, "%s/screen-1-layer1.ppm", dir);
    ppm(path);
    press_btn(B_ENV);                        /* layer 3, a key held: its meter rises */
    press_key(10, 1);
    run_blocks(400, 0);
    fm1_ms += 1000;
    ui_draw();
    snprintf(path, sizeof path, "%s/screen-2-layer3-playing.ppm", dir);
    ppm(path);
    press_btn(B_SEL);                        /* layer 2, turned to the other engine */
    host_enc[PANEL_ENC[EN_PRESET]] = ui.engine[1] == ENG_FM ? 1 : -1;
    ui_input();
    run_blocks(40, 0);
    ui_draw();
    snprintf(path, sizeof path, "%s/screen-3-layer2-engine.ppm", dir);
    ppm(path);
    press_btn(B_HOME);                       /* a new sound: the message */
    ui_draw();
    snprintf(path, sizeof path, "%s/screen-4-new.ppm", dir);
    ppm(path);
    fm1_ms += 2000;
    press_btn(B_OCTUP);
    ui_draw();
    snprintf(path, sizeof path, "%s/screen-5-octave.ppm", dir);
    ppm(path);
    fm1_ms += 2000;
    ui_draw();
    snprintf(path, sizeof path, "%s/screen-6-after.ppm", dir);
    ppm(path);
    press_btn(B_LFO);                        /* layer 4 recording, layer 2 looping */
    press_btn(B_REC);
    press_key(4, 1);
    run_blocks(300, 0);
    fm1_ms += 50;
    ui_draw();
    snprintf(path, sizeof path, "%s/screen-8-recording.ppm", dir);
    ppm(path);
    press_key(4, 0);
    press_btn(B_ENV);                        /* layer 3: the level down 4 steps, the pan right */
    host_enc[PANEL_ENC[EN_SELECT]] = -4;
    host_enc[PANEL_ENC[EN_ALGO]] = 17;
    ui_input();
    press_key(10, 1);
    run_blocks(300, 0);
    fm1_ms += 50;
    ui_draw();
    snprintf(path, sizeof path, "%s/screen-9-level-pan.ppm", dir);
    ppm(path);
    fm1_ms += 2000;
    run_blocks(100, 0);
    ui_draw();
    snprintf(path, sizeof path, "%s/screen-10-meter.ppm", dir);
    ppm(path);
    host_enc[PANEL_ENC[EN_K4]] = 10;         /* the layer's send up to 60 */
    ui_input();
    fm1_ms += 2000;
    ui_draw();
    snprintf(path, sizeof path, "%s/screen-11-send.ppm", dir);
    ppm(path);
    press_btn(B_SEQ);                        /* the REVERB page, DECAY turned */
    host_enc[PANEL_ENC[EN_K2]] = 3;
    ui_input();
    ui_draw();
    snprintf(path, sizeof path, "%s/screen-12-reverb.ppm", dir);
    ppm(path);
    press_btn(B_SEQ);                        /* and back */
    fm1_ms += 2000;
    ui_draw();
    snprintf(path, sizeof path, "%s/screen-13-back.ppm", dir);
    ppm(path);
    printf("screens: %s\n", dir);
}

static int cmd_check(void)
{
    int e, w, s, fail = 0;
    double worst_us[NENG] = {0, 0};
    for (e = 0; e < NENG; e++) {
        int nw = e == ENG_FM ? FF_N : AK_N;
        for (w = 0; w < nw; w++)
            for (s = 0; s < 40; s++) {
                uint32_t seed = 77u + (uint32_t)s * 104729u + (uint32_t)w * 13u;
                const char *nm = e == ENG_FM ? FF_NAME[w] : AK_NAME[w];
                int note = 36 + (s * 7) % 48;
                stats_t st = render_sound(e, w, seed, note, 6.0, 4.0, 0);
                double rmax = db(st.rms_max);
                if (getenv("HOST_VERBOSE"))
                    printf("%s %s %.1f %.1f seed %u note %d\n", ENG_NAME[e], nm, rmax, dbp(st.peak), seed, note);
                if (st.ns_block > worst_us[e])
                    worst_us[e] = st.ns_block;
                if (rmax < -60.0 || st.clip > st.n / 200u || st.peak != st.peak) {
                    printf("FAIL %-6s %-9s seed %u note %d: rms max %.1f dB, peak %.1f dB, clipped %u of %u\n",
                           ENG_NAME[e], nm, seed, note, rmax, dbp(st.peak), st.clip, st.n);
                    fail++;
                }
            }
    }
    printf("check: %s (%d failures); worst host cost a block: FM %.1f us, ANIMA %.1f us\n", fail ? "FAILED" : "ok",
           fail, worst_us[0] / 1000.0, worst_us[1] / 1000.0);
    return fail ? 1 : 0;
}

static int lsound(uint32_t l) { return layer_sounding(l); }
static int lgate(uint32_t l)
{
    uint32_t k;
    for (k = 0; k < NVOICE; k++)
        if (lay[l].v[k].gate)
            return 1;
    return 0;
}
static uint32_t nsounding(uint32_t l)
{
    uint32_t k, n = 0;
    for (k = 0; k < NVOICE; k++)
        n += lay[l].v[k].sounding;
    return n;
}

/* the panel's flows: a held note through an engine change and a new sound; held notes on two layers at once;
 * a note's release not cut by the next; the looper; every layer silent once every key is up (no stuck note) */
static int cmd_flows(void)
{
    uint32_t l, fail = 0, i, n;
    layers_init();
    ui_init();
    sounds_init();
    master_q12 = 4096;
    press_btn(B_FX);                          /* layer 1, a key held */
    press_key(5, 1);
    run_blocks(1000, 0);
    if (!lsound(0) || !lgate(0)) {
        printf("FAIL flows: layer 1 does not sound with its key held\n");
        fail++;
    }
    host_enc[PANEL_ENC[EN_PRESET]] = ui.engine[0] == ENG_FM ? 1 : -1;   /* the other engine, the key still held */
    ui_input();
    run_blocks(200, 0);
    if (lay[0].engine != ui.engine[0] || !lsound(0)) {
        printf("FAIL flows: the engine change did not carry the held note over\n");
        fail++;
    }
    press_btn(B_HOME);                        /* a new sound, the key still held */
    run_blocks(200, 0);
    if (!lsound(0) || (ui.engine[0] == ENG_FM ? fm_pub[0] != fm_use[0] : an_pub[0] != an_use[0])) {
        printf("FAIL flows: the new sound was not taken under the held note\n");
        fail++;
    }
    press_btn(B_ENV);                         /* layer 3 played while layer 1 is held */
    press_key(12, 1);
    run_blocks(500, 0);
    if (!lsound(0) || !lsound(2)) {
        printf("FAIL flows: layers 1 and 3 do not sound together\n");
        fail++;
    }
    press_key(5, 0);                          /* layer 1's key up while layer 3 is chosen: layer 1 lets go */
    run_blocks(10, 0);
    if (lgate(0)) {
        printf("FAIL flows: layer 1's note did not end when its key was lifted\n");
        fail++;
    }
    press_key(12, 0);
    run_blocks(44100u * 40u / CTL, 0);
    /* a release not cut: a DRONE (long release) on layer 4, a note, let go, the next note 100 ms later */
    fm_generate(&fm_buf[3][fm_use[3] ^ 1u], FF_DRONE, 4242u, PATH_OPEN);
    fm_measure(&fm_buf[3][fm_use[3] ^ 1u]);
    fm_pub[3] = fm_use[3] ^ 1u;
    press_btn(B_LFO);
    if (ui.engine[3] != ENG_FM) {
        host_enc[PANEL_ENC[EN_PRESET]] = -1;
        ui_input();
    }
    run_blocks(100, 0);
    press_key(3, 1);
    run_blocks(1500, 0);
    press_key(3, 0);
    run_blocks(140, 0);
    press_key(8, 1);
    run_blocks(200, 0);
    if (nsounding(3) != 2u) {
        printf("FAIL flows: the second note cut the first one's release (%u voices sounding)\n", nsounding(3));
        fail++;
    }
    press_key(8, 0);
    run_blocks(44100u * 40u / CTL, 0);
    /* the looper: layer 5 armed, three notes at their own times, REC again, then it plays round */
    press_btn(B_EDIT);
    press_btn(B_REC);
    run_blocks(300, 0);                       /* armed: nothing yet */
    if (loop[4].rec != LR_ARMED) {
        printf("FAIL flows: REC did not arm the looper\n");
        fail++;
    }
    press_key(0, 1);                          /* the first note: it starts */
    run_blocks(400, 0);
    press_key(0, 0);
    run_blocks(300, 0);
    press_key(9, 1);
    run_blocks(100, 0);
    press_key(17, 1);
    run_blocks(200, 0);
    press_key(9, 0);                          /* key 17 still held when the loop ends */
    run_blocks(100, 0);
    press_btn(B_REC);
    run_blocks(2, 0);
    press_key(17, 0);
    host_press = 0;
    if (!loop[4].play || loop[4].ntrk != 1u || loop[4].trk[0].len < 1095u || loop[4].trk[0].len > 1105u ||
        loop[4].trk[0].n != 6u) {
        printf("FAIL flows: the loop is not as played (play %d, %u loops, len %u blocks, %u events)\n", loop[4].play,
               loop[4].ntrk, loop[4].trk[0].len, loop[4].trk[0].n);
        fail++;
    }
    for (i = n = 0; i < 3u * 1100u; i++) {     /* three times round: its notes come back */
        run_blocks(1, 0);
        n += lgate(4);
    }
    if (n < 3u * 600u || n > 3u * 1000u) {
        printf("FAIL flows: the loop does not play its notes (%u blocks of %u with a note)\n", n, 3u * 1100u);
        fail++;
    }
    /* an overdub: REC while it plays (armed, the loop goes on), a note 300 blocks on, REC 700 blocks later:
     * a second loop, 700 blocks long, both going round, each in its own length */
    press_btn(B_REC);
    run_blocks(300, 0);
    if (loop[4].rec != LR_ARMED || !loop[4].play || loop[4].trk[0].pos == 0)
        printf("FAIL flows: REC over a playing loop did not arm (or stopped it)\n"), fail++;
    press_key(12, 1);
    run_blocks(200, 0);
    press_key(12, 0);
    run_blocks(500, 0);
    {
        uint32_t p0 = loop[4].trk[0].pos;
        press_btn(B_REC);
        run_blocks(1, 0);
        if (loop[4].ntrk != 2u || loop[4].trk[1].len < 695u || loop[4].trk[1].len > 705u || loop[4].trk[1].n != 2u ||
            loop[4].trk[1].first != 6u || loop[4].rec != LR_IDLE)
            printf("FAIL flows: the overdub is not a second loop as played (%u loops, len %u, %u events)\n",
                   loop[4].ntrk, loop[4].trk[1].len, loop[4].trk[1].n), fail++;
        if (loop[4].trk[0].pos != (p0 + 1u) % loop[4].trk[0].len && loop[4].trk[0].pos != (p0 + 2u) % loop[4].trk[0].len)
            printf("FAIL flows: the first loop jumped when the overdub ended\n"), fail++;
    }
    for (i = 0; i < 8u; i++) {               /* up to ten loops; the eleventh is refused */
        press_btn(B_REC);
        press_key(i, 1);
        run_blocks(20, 0);
        press_key(i, 0);
        run_blocks(20, 0);
        press_btn(B_REC);
        run_blocks(5, 0);
    }
    press_btn(B_REC);
    run_blocks(2, 0);
    if (loop[4].ntrk != 10u || loop[4].rec != LR_IDLE || !str_eq(ui.msg, "10 LOOPS"))
        printf("FAIL flows: not ten loops and no more (%u, rec %d, %s)\n", loop[4].ntrk, loop[4].rec, ui.msg), fail++;
    run_blocks(3000, 0);
    press_btn(B_PLAY);                        /* stopped */
    run_blocks(10, 0);
    if (loop[4].play || lgate(4)) {
        printf("FAIL flows: PLAY did not stop the loop\n");
        fail++;
    }
    press_btn(B_PLAY);
    run_blocks(10, 0);
    if (!loop[4].play || loop[4].trk[0].pos > 20u || loop[4].trk[9].pos > 20u) {
        printf("FAIL flows: PLAY did not play the loops again from their starts\n");
        fail++;
    }
    fm1_in.buttons |= 1u << PANEL_BTN[B_REC];  /* REC held: emptied */
    press_btn(B_REC);
    fm1_ms += 1100;
    ui_input();
    fm1_in.buttons &= ~(1u << PANEL_BTN[B_REC]);
    run_blocks(10, 0);
    if (loop[4].ntrk || loop[4].play || !ui.flash_on) {
        printf("FAIL flows: REC held did not empty the looper (%u loops)\n", loop[4].ntrk);
        fail++;
    }
    {
        char path[] = "build/screens/screen-7-cleared.ppm";
        ui_draw();
        ppm(path);
    }
    for (l = 0; l < NLAYER; l++) {            /* a key on every layer, then all up */
        press_btn(LAYER_BTN[l]);
        press_key(l * 3u, 1);
        press_key(l * 3u + 1u, 1);
        press_key(l * 3u, 0);
    }
    run_blocks(2000, 0);
    for (l = 0; l < NLAYER; l++)
        press_key(l * 3u + 1u, 0);
    run_blocks(44100u * 120u / CTL, 0);       /* 2 min: a DRONE's release is long */
    for (l = 0; l < NLAYER; l++)
        if (lsound(l) || lgate(l)) {
            printf("FAIL flows: layer %u still sounding after every key is up (gate %d %d, sounding %d %d, engine %d, meter %d)\n", l + 1, lay[l].v[0].gate, lay[l].v[1].gate, lay[l].v[0].sounding, lay[l].v[1].sounding, lay[l].engine, lay[l].meter);
            fail++;
        }
    if (ev_w - ev_r) {
        printf("FAIL flows: events left in the ring\n");
        fail++;
    }
    /* the scales: ARP, the root A, the scale minor pentatonic; the first white key the root, each next white
     * key the next degree; a black key no note */
    {
        static const uint8_t WANT[16] = {57, 60, 62, 64, 67, 69, 72, 74, 76, 79, 81, 84, 86, 88, 91, 93};
        uint32_t k, w = 0;
        press_btn(B_FX);
        while (ui.oct[0] != 0)
            press_btn(ui.oct[0] > 0 ? B_OCTDN : B_OCTUP);
        press_btn(B_ARP);
        host_enc[PANEL_ENC[EN_SELECT]] = 9;      /* C -> A */
        host_enc[PANEL_ENC[EN_ALGO]] = 5;        /* major -> minor pentatonic */
        ui_input();
        if (ui.root != 9u || ui.scale != 5u)
            printf("FAIL flows: the SCALE page set root %u scale %u\n", ui.root, ui.scale), fail++;
        for (k = 0; k < NKEY; k++) {
            if (white_ix(k) < 0) {
                uint32_t before = ev_w;
                press_phys(k, 1);
                press_phys(k, 0);
                if (ev_w != before)
                    printf("FAIL flows: black key %u played a note\n", k), fail++;
                continue;
            }
            if (scale_note(white_ix(k), 0) != WANT[w])
                printf("FAIL flows: white key %u plays %u, not %u\n", w, scale_note(white_ix(k), 0), WANT[w]), fail++;
            w++;
        }
        if (w != 16u)
            printf("FAIL flows: %u white keys, not 16\n", w), fail++;
        ui_draw();
        ppm("build/screens/screen-17-scale.ppm");
        press_btn(B_ARP);
        run_blocks(44100u * 20u / CTL, 0);
    }
    printf("flows: %s\n", fail ? "FAILED" : "ok");
    return fail ? 1 : 0;
}

/* the reverb alone: its tail at the defaults (RT60), its level against the dry of one layer at the default
 * send, stability at its extremes, a WAV of a burst through it; exits 1 on a failure */
static double rv_run(uint32_t blocks, int noise, double *ms_l, double *peak)
{
    static rnd_t r;
    int32_t in[CTL], l[CTL], rr[CTL];
    uint32_t b, i;
    double sum = 0;
    *peak = 0;
    for (b = 0; b < blocks; b++) {
        for (i = 0; i < CTL; i++) {
            in[i] = noise ? rnd_bi(&r) >> 2 : 0;
            l[i] = rr[i] = 0;
        }
        rv_block(in, l, rr, CTL);
        for (i = 0; i < CTL; i++) {
            double a = abs(l[i]) > abs(rr[i]) ? abs(l[i]) : abs(rr[i]);
            sum += (double)l[i] * l[i];
            if (a > *peak)
                *peak = a;
            if (wav_ch == 2) {
                wav_put(l[i]);
                wav_put(rr[i]);
            }
        }
    }
    *ms_l = sum / (blocks * CTL);
    return *ms_l;
}
static int cmd_reverb(const char *dir)
{
    double ms, pk, ms_in = (32768.0 / 4) * (32768.0 / 4) / 3.0, base, t;
    uint32_t k, fail = 0;
    char path[512];
    rv_init();
    rv_param(70, 65, 40, 50);
    wav_ch = 2;
    rv_run(1378 * 2, 1, &ms, &pk);                    /* 2 s of noise, then the tail */
    base = ms;
    printf("reverb defaults: wet %.1f dB against the dry going in (noise), peak %.1f dBFS\n", 10 * __builtin_log10(ms / ms_in), dbp(pk));
    for (k = 0, t = 0; k < 200; k++) {               /* the tail, 50 ms at a time: where it is 60 dB down */
        rv_run(69, 0, &ms, &pk);
        if (10 * __builtin_log10(ms / base + 1e-30) < -60) {
            t = (k + 1) * 0.05;
            break;
        }
    }
    printf("reverb defaults: RT60 %.2f s\n", t);
    if (t < 1.2 || t > 8.0) {
        printf("FAIL reverb: the default tail is %.2f s\n", t);
        fail++;
    }
    snprintf(path, sizeof path, "%s/reverb-defaults.wav", dir);
    wav_write(path);
    wav_ch = 1;
    rv_init();
    rv_param(100, 100, 0, 100);                      /* the extremes: a minute of loud noise */
    rv_run(1378 * 60, 1, &ms, &pk);
    printf("reverb extremes: peak %.1f dBFS after a minute of noise\n", dbp(pk));
    rv_run(1378 * 20, 0, &ms, &pk);
    printf("reverb extremes: 20 s after, %.1f dB\n", 10 * __builtin_log10(ms / (32768.0 * 32768.0) + 1e-30));
    if (pk != pk || ms > 1e9) {
        printf("FAIL reverb: runs away at its extremes\n");
        fail++;
    }
    {
        clock_t c0 = clock();
        rv_run(1378 * 10, 1, &ms, &pk);
        printf("reverb: %.2f us a block on the host\n", (double)(clock() - c0) / CLOCKS_PER_SEC * 1e6 / (1378 * 10));
    }
    printf("reverb: %s\n", fail ? "FAILED" : "ok");
    return fail ? 1 : 0;
}

/* SAVE: pressed and let go (held_ms 0), or held */
static void save_btn(uint32_t held_ms)
{
    host_press |= 1u << PANEL_BTN[B_SAVE];
    fm1_in.buttons |= 1u << PANEL_BTN[B_SAVE];
    ui_input();
    fm1_ms += held_ms;
    ui_input();
    fm1_in.buttons &= ~(1u << PANEL_BTN[B_SAVE]);
    ui_input();
    run_blocks(200, 0);
    fm1_ms += 2000;                           /* past any flash */
    ui_draw();
}
static int same_machine(const char *what, const uint8_t *eng, const int8_t *lv, const int8_t *pn, const int8_t *sd,
                        const int8_t *oc, const fm_patch_t *fp, const an_design_t *ad, const int8_t *rvp,
                        const looper_t *lp)
{
    uint32_t l, bad = 0;
    for (l = 0; l < NLAYER; l++) {
        bad += ui.engine[l] != eng[l] || ui.level[l] != lv[l] || ui.pan[l] != pn[l] || ui.send[l] != sd[l] ||
               ui.oct[l] != oc[l];
        bad += memcmp(&fm_buf[l][fm_pub[l]], &fp[l], sizeof(fm_patch_t)) != 0;
        bad += memcmp(&an_buf[l][an_pub[l]], &ad[l], sizeof(an_design_t)) != 0;
        {
            uint32_t k;
            bad += loop[l].ntrk != lp[l].ntrk || loop[l].nev != lp[l].nev || loop[l].play != lp[l].play ||
                   memcmp(loop[l].ev, lp[l].ev, lp[l].nev * 4u) != 0;
            for (k = 0; k < lp[l].ntrk; k++)
                bad += loop[l].trk[k].len != lp[l].trk[k].len || loop[l].trk[k].n != lp[l].trk[k].n ||
                       loop[l].trk[k].first != lp[l].trk[k].first;
        }
    }
    bad += memcmp(ui.rvp, rvp, 4) != 0;
    if (bad)
        printf("FAIL sessions: %s: %u differences\n", what, bad);
    return bad != 0;
}
static int cmd_sessions(const char *dir)
{
    static uint8_t eng[NLAYER];
    static int8_t lv[NLAYER], pn[NLAYER], sd[NLAYER], oc[NLAYER], rvp[4];
    static fm_patch_t fp[NLAYER];
    static an_design_t ad[NLAYER];
    static looper_t lp[NLAYER];
    uint32_t l, fail = 0, i, e0;
    char path[512];
    rnd_t r;
    rnd_seed(&r, 99);
    memset(host_flash, 0xFF, sizeof host_flash);
    for (i = SES_LO; i < SES_LO + 20u * 4096u; i++)   /* SLOOP's projects there: garbage to the sessions */
        host_flash[i] = (uint8_t)rnd_next(&r);
    layers_init();
    ui_init();
    sounds_init();
    ses_scan(1);
    master_q12 = 4096;
    if (ses.n) {
        printf("FAIL sessions: found %u in SLOOP's data\n", ses.n);
        fail++;
    }
    /* a machine: layer 2 ANIMA an octave up, quieter, panned, more reverb; the reverb larger; a loop on 3 */
    press_btn(B_SEL);
    host_enc[PANEL_ENC[EN_SELECT]] = -3;
    host_enc[PANEL_ENC[EN_ALGO]] = 7;
    host_enc[PANEL_ENC[EN_K4]] = 5;
    press_btn(B_OCTUP);
    press_btn(B_SEQ);
    host_enc[PANEL_ENC[EN_K1]] = 8;
    ui_input();
    press_btn(B_SEQ);
    press_btn(B_ENV);
    press_btn(B_REC);
    press_key(2, 1);
    run_blocks(300, 0);
    press_key(2, 0);
    press_key(7, 1);
    run_blocks(200, 0);
    press_btn(B_REC);
    press_key(7, 0);
    run_blocks(50, 0);
    press_btn(B_REC);                         /* and an overdub: a second loop on layer 3 */
    press_key(4, 1);
    run_blocks(120, 0);
    press_key(4, 0);
    run_blocks(90, 0);
    press_btn(B_REC);
    run_blocks(50, 0);
    if (loop[2].ntrk != 2u)
        printf("FAIL sessions: no overdub to save (%u loops)\n", loop[2].ntrk), fail++;
    /* SAVE held: asked about session 1 (YES), SAVE: saved */
    save_btn(900);
    if (ui.ses_page != SP_ASK || ui.ses_new != 1u) {
        printf("FAIL sessions: SAVE held did not ask about session 1 (page %d, %u)\n", ui.ses_page, ui.ses_new);
        fail++;
    }
    snprintf(path, sizeof path, "%s/screen-14-save-ask.ppm", dir);
    ppm(path);
    save_btn(0);
    if (ses.n != 1u || ses.current != 1u) {
        printf("FAIL sessions: session 1 not saved (%u, current %u)\n", ses.n, ses.current);
        fail++;
    }
    /* changed, and SAVE held again: over session 1, at once */
    press_btn(B_FX);
    host_enc[PANEL_ENC[EN_ALGO]] = -6;
    ui_input();
    new_sound();
    run_blocks(100, 0);
    e0 = host_erases;
    save_btn(900);
    if (ses.n != 1u || ui.ses_page) {
        printf("FAIL sessions: SAVE held did not save over session 1 (%u sessions, page %d)\n", ses.n, ui.ses_page);
        fail++;
    }
    if (host_erases != e0) {
        printf("FAIL sessions: saving erased %u sectors with blank ones free\n", host_erases - e0);
        fail++;
    }
    for (l = 0; l < NLAYER; l++) {            /* what session 1 holds now */
        eng[l] = ui.engine[l];
        lv[l] = ui.level[l];
        pn[l] = ui.pan[l];
        sd[l] = ui.send[l];
        oc[l] = ui.oct[l];
        fp[l] = fm_buf[l][fm_pub[l]];
        ad[l] = an_buf[l][an_pub[l]];
        lp[l] = loop[l];
    }
    memcpy(rvp, ui.rvp, 4);
    /* a second one: as if nothing were loaded */
    press_btn(B_LFO);
    host_enc[PANEL_ENC[EN_SELECT]] = -10;
    ui_input();
    ses.current = 0;
    save_btn(900);
    if (ui.ses_new != 2u)
        printf("FAIL sessions: the next number is %u, not 2\n", ui.ses_new), fail++;
    save_btn(0);
    /* a reboot: the list from the flash */
    ses_scan(1);
    if (ses.n != 2u || ses.num[0] != 1u || ses.num[1] != 2u) {
        printf("FAIL sessions: after a reboot %u sessions\n", ses.n);
        fail++;
    }
    /* another machine, then SAVE: the list; PRESETS to session 1; SAVE: loaded */
    layers_init();
    ui_init();
    sounds_init();
    run_blocks(100, 0);
    save_btn(0);
    if (ui.ses_page != SP_LIST)
        printf("FAIL sessions: SAVE did not open the list\n"), fail++;
    host_enc[PANEL_ENC[EN_PRESET]] = 1;
    ui_input();
    ui_draw();
    snprintf(path, sizeof path, "%s/screen-15-sessions.ppm", dir);
    ppm(path);
    host_enc[PANEL_ENC[EN_PRESET]] = -1;
    ui_input();
    save_btn(0);
    if (ui.ses_page || ses.current != 1u)
        printf("FAIL sessions: SAVE in the list did not load session 1\n"), fail++;
    fail += (uint32_t)same_machine("session 1 loaded", eng, lv, pn, sd, oc, fp, ad, rvp, lp);
    run_blocks(1500, 0);                      /* its loop plays */
    if (!loop[2].play || !loop[2].trk[0].pos)
        printf("FAIL sessions: the loaded loop does not play\n"), fail++;
    /* the seeds make the same sounds again (a later firmware's sessions) */
    for (l = 0; l < NLAYER; l++) {
        static fm_patch_t f2;
        static an_design_t a2;
        fm_generate(&f2, -1, fp[l].seed, fp[l].path);
        fm_measure(&f2);
        an_design(&a2, -1, ad[l].seed, ad[l].path);
        an_measure(&a2);
        if (memcmp(&f2, &fp[l], sizeof f2) || memcmp(&a2, &ad[l], sizeof a2)) {
            printf("FAIL sessions: layer %u's sounds are not made again from their seeds\n", l + 1);
            fail++;
        }
    }
    /* an empty slot chosen in the list (slot 9), then SAVE held: saved there, at once */
    save_btn(0);
    host_enc[PANEL_ENC[EN_PRESET]] = 8;
    ui_input();
    ui_draw();
    snprintf(path, sizeof path, "%s/screen-16-empty-slot.ppm", dir);
    ppm(path);
    save_btn(0);
    if (ses.current != 9u || ses_has(9))
        printf("FAIL sessions: the empty slot 9 was not chosen (current %u)\n", ses.current), fail++;
    save_btn(900);
    if (!ses_has(9) || ui.ses_page)
        printf("FAIL sessions: SAVE held did not save into slot 9\n"), fail++;
    /* none loaded, the list, a full slot (1): SAVE held asks OVERWRITE (NO first); NO keeps it, YES writes over it */
    ses.current = 0;
    save_btn(0);
    if (ui.ses_page != SP_LIST || ui.ses_ix != 1u || !ses_has(1))
        printf("FAIL sessions: the list does not open on the full slot 1\n"), fail++;
    ui.level[0] = 5;
    save_btn(900);
    if (ui.ses_page != SP_ASK || ui.ses_new != 1u || !ui.ses_over || ui.ses_yes)
        printf("FAIL sessions: SAVE held on a full slot does not ask to overwrite it, NO first\n"), fail++;
    ui_draw();
    snprintf(path, sizeof path, "%s/screen-28-overwrite.ppm", dir);
    ppm(path);
    save_btn(0);
    ses_load_slot(1);
    if (ui.level[0] == 5 || ses.current != 1u)
        printf("FAIL sessions: NO wrote over session 1\n"), fail++;
    ses.current = 0;
    ui.level[0] = 5;
    save_btn(0);
    save_btn(900);
    host_enc[PANEL_ENC[EN_PRESET]] = 1;           /* YES */
    ui_input();
    save_btn(0);
    if (ses.current != 1u || ui.ses_page)
        printf("FAIL sessions: YES did not save over session 1 (current %u)\n", ses.current), fail++;
    ui.level[0] = 9;
    ses_load_slot(1);
    if (ui.level[0] != 5)
        printf("FAIL sessions: session 1 was not overwritten (level %d)\n", ui.level[0]), fail++;
    /* full: sessions until there is no room, then the message, the others intact */
    ses.current = 0;
    for (i = 0; i < 100u; i++) {
        ses.current = 0;                      /* each a new one */
        save_btn(900);
        save_btn(0);
        if (!str_eq(ui.flash_l1, "SAVED"))
            break;
    }
    printf("sessions: %u saved before %s\n", ses.n, ui.flash_l1);
    if (ses.n != SES_SLOTS)
        printf("FAIL sessions: %u sessions when the slots were full\n", ses.n), fail++;
    if (!str_eq(ui.flash_l1, "FULL"))
        printf("FAIL sessions: no FULL when the flash is full\n"), fail++;
    /* a save cut short: two copies of session 1; the newer one wins at the next boot */
    {
        int i1 = ses_find(1);
        uint32_t len = ses_pack(1), ns = (len + 4095u) / 4096u;
        int at = -1;
        for (i = 0; i + ns <= SES_NSECT && at < 0; i++) {
            uint32_t k;
            for (k = 0; k < ns && !ses.used[i + k]; k++)
                ;
            if (k == ns)
                at = (int)i;
        }
        if (at < 0) {                          /* make room: drop the last one */
            ses_kill(ses.sect[ses.n - 1u], ses.nsect[ses.n - 1u]);
            ses.n--;
            at = ses_alloc(ns);
        }
        for (i = 0; i < ns; i++)
            ses_flash_erase(SES_LO + ((uint32_t)at + i) * 4096u);
        ses_flash_prog(SES_LO + (uint32_t)at * 4096u, ses_buf, len);
        ses_scan(1);
        if (ses_find(1) < 0 || ses.sect[ses_find(1)] != (uint8_t)at)
            printf("FAIL sessions: after a save cut short, not the newer copy (%d)\n", i1), fail++;
    }
    /* a session of 0.5 .. 0.7 (format 1, one loop a layer): still loads, its loop as the layer's only one */
    {
        static uint8_t b[8192];
        ses_head_t *h = (ses_head_t *)b;
        uint8_t *q = b + sizeof *h;
        uint32_t ev[2] = {LE(10, 60, 1), LE(300, 60, 0)};
        int slot;
        memset(b, 0, sizeof b);
        q += 8;                                   /* globals: layer 1, reverb 0, C major */
        for (l = 0; l < NLAYER; l++) {
            ses_layer_t y;
            memset(&y, 0, sizeof y);
            y.level = 20;
            y.loop_state = l == 1 ? 3 : 0;        /* LP_PLAY of format 1 */
            y.loop_len = l == 1 ? 500 : 0;
            y.loop_n = l == 1 ? 2 : 0;
            memcpy(q, &y, sizeof y);
            q += sizeof y;
            memcpy(q, &fm_buf[l][fm_pub[l]], sizeof(fm_patch_t));
            q += sizeof(fm_patch_t);
            memcpy(q, &an_buf[l][an_pub[l]], sizeof(an_design_t));
            q += sizeof(an_design_t);
            if (l == 1) {
                memcpy(q, ev, sizeof ev);
                q += sizeof ev;
            }
        }
        h->magic = SES_MAGIC;
        h->num = 50;
        h->format = 1;
        h->len = (uint32_t)(q - b);
        h->seq = 1;
        h->fm_size = sizeof(fm_patch_t);
        h->an_size = sizeof(an_design_t);
        h->body_crc = ses_crc(b + sizeof *h, h->len - sizeof *h, 0);
        h->head_crc = ses_crc(b, 30u, 0);
        slot = ses_find(50);
        if (slot >= 0) {
            ses_kill(ses.sect[slot], ses.nsect[slot]);
            ses_scan(1);
        }
        slot = ses_alloc(2);
        ses_flash_erase(SES_LO + (uint32_t)slot * 4096u);
        ses_flash_erase(SES_LO + ((uint32_t)slot + 1u) * 4096u);
        ses_flash_prog(SES_LO + (uint32_t)slot * 4096u, b, h->len);
        ses_scan(1);
        if (ses_load_slot(50) != SES_OK || loop[1].ntrk != 1u || loop[1].trk[0].len != 500u || loop[1].trk[0].n != 2u ||
            !loop[1].play || loop[1].ev[0] != ev[0])
            printf("FAIL sessions: a format-1 session does not load (%u loops)\n", loop[1].ntrk), fail++;
    }
    printf("sessions: %s\n", fail ? "FAILED" : "ok");
    return fail ? 1 : 0;
}

/* the DJ filter as layer_block runs it, on a sine of hz (0: white noise): its RMS; the largest step */
static double flt_run(uint32_t l, uint32_t hz, double *jump)
{
    static rnd_t r;
    layer_t *y = &lay[l];
    uint32_t b, i, ph = 0, inc = (uint32_t)(hz * 97391.55);
    double sa = 0, prev = 0;
    *jump = 0;
    for (b = 0; b < 1378u; b++) {
        int32_t buf[CTL], vv = lay_flt[l], a = vv < 0 ? -vv : vv;
        int32_t amt1 = a >= 4 ? Q15 : a * (Q15 / 4);
        int32_t q1 = 2896 + (a >= 24 ? 13488 : a * 13488 / 24);
        for (i = 0; i < CTL; i++) {
            ph += inc;
            buf[i] = hz ? sin_ph(ph) >> 2 : rnd_bi(&r) >> 3;
        }
        if (vv)
            y->f_hp = (uint8_t)(vv > 0);
        if (amt1 || y->f_amt) {
            svf_c c;
            int32_t a0 = y->f_amt, da = (amt1 - a0) / CTL;
            if (!y->f_amt) {
                y->f_oct = flt_oct(vv ? vv : (y->f_hp ? 1 : -1));
                y->f_q = q1;
                y->flt.ic1 = y->flt.ic2 = 0;
            }
            if (vv)
                y->f_oct += (flt_oct(vv) - y->f_oct) >> 2;
            y->f_q += (q1 - y->f_q) >> 2;
            svf_coef(&c, (int32_t)exp2_q16(y->f_oct - (16 << 16)), y->f_q);
            for (i = 0; i < CTL; i++) {
                int32_t f = svf_run(&y->flt, &c, buf[i], y->f_hp ? 3 : 1);
                a0 += da;
                buf[i] += mulq15(f - buf[i], a0);
            }
            y->f_amt = amt1;
        }
        for (i = 0; i < CTL; i++) {
            double x = buf[i];
            if (b > 300)
                sa += x * x;
            if (b > 0 && (x - prev > *jump || prev - x > *jump))
                *jump = x > prev ? x - prev : prev - x;
            prev = x;
        }
    }
    return sa;
}
static double flt_db(int32_t v, uint32_t hz)
{
    double j, ref, out;
    lay_flt[0] = 0;
    ref = flt_run(0, hz, &j);
    lay_flt[0] = (int8_t)v;
    out = flt_run(0, hz, &j);
    lay_flt[0] = 0;
    flt_run(0, hz, &j);
    return 10 * __builtin_log10(out / ref);
}
static int cmd_filter(const char *dir)
{
    double lp_lo, lp_hi, hp_lo, hp_hi, body, j, a, worst = 0;
    uint32_t fail = 0;
    int32_t v;
    char path[512];
    layers_init();
    ui_init();
    lp_lo = flt_db(-48, 110);                    /* the low-pass at ~1/4 of the way: ~150 Hz */
    lp_hi = flt_db(-48, 5000);
    hp_lo = flt_db(48, 110);                     /* the high-pass at ~2.4 kHz */
    hp_hi = flt_db(48, 8000);
    body = flt_db(-24, 110);                     /* full resonance, the cutoff far above: the body kept */
    printf("filter LP -48 (%d Hz): 110 Hz %.1f dB, 5 kHz %.1f dB\n", (int)exp2_q16(flt_oct(-48) - (16 << 16)), lp_lo, lp_hi);
    printf("filter HP +48 (%d Hz): 110 Hz %.1f dB, 8 kHz %.1f dB\n", (int)exp2_q16(flt_oct(48) - (16 << 16)), hp_lo, hp_hi);
    printf("filter LP -24 (Q 4, %d Hz) on 110 Hz: %.2f dB\n", (int)exp2_q16(flt_oct(-24) - (16 << 16)), body);
    if (lp_lo < -3.0 || lp_hi > -30.0)
        printf("FAIL filter: the low-pass does not keep the lows and cut the highs\n"), fail++;
    if (hp_lo > -30.0 || hp_hi < -3.0 || hp_hi > 3.0)
        printf("FAIL filter: the high-pass does not cut the lows and keep the highs\n"), fail++;
    if (body < -0.5)
        printf("FAIL filter: the resonance thins the body\n"), fail++;
    for (v = -64; v <= 64; v += 2) {             /* a sweep end to end: no blow-up */
        lay_flt[0] = (int8_t)v;
        a = flt_run(0, 0, &j);
        if (j > worst)
            worst = j;
        if (a != a || a > 1e15)
            printf("FAIL filter: runs away at %d\n", v), fail++;
    }
    printf("filter sweep on noise: largest step between samples %.0f (of 32768)\n", worst);
    lay_flt[0] = 0;
    /* the dial on the screen */
    sounds_init();
    ui.force = 1;
    ui_draw();
    host_enc[PANEL_ENC[EN_K1]] = -14;
    ui_input();
    ui_draw();
    snprintf(path, sizeof path, "%s/screen-18-filter-lp.ppm", dir);
    ppm(path);
    host_enc[PANEL_ENC[EN_K1]] = 25;
    ui_input();
    fm1_ms += 50;
    ui_draw();
    snprintf(path, sizeof path, "%s/screen-19-filter-hp.ppm", dir);
    ppm(path);
    fm1_ms += 1100;
    ui_draw();
    if (ui.shown_page != 0)
        printf("FAIL filter: the dial does not go away\n"), fail++;
    printf("filter: %s\n", fail ? "FAILED" : "ok");
    return fail ? 1 : 0;
}

/* the granulator: off is the layer untouched; each side heard, stable, not clipping; a tail after the
 * voices; the worst cost (six layers at the far right); a WAV of each side; the dials */
static void grain_reset(void)                   /* the same sounds every time */
{
    host_ticks = 0;
    fm1_ms = 0;
    memset(lay, 0, sizeof lay);                 /* (on the FM-1 they start zeroed, once) */
    memset(loop, 0, sizeof loop);
    lay_age = blk_clock = 0;
    ev_r = ev_w = 0;
    fm1_in.notes = fm1_in.buttons = 0;
    layers_init();
    ui_init();
    sounds_init();
    master_q12 = 4096;
}
static int cmd_grain(const char *dir)
{
    static const int8_t V[5] = {-64, -32, 0, 32, 64};
    static const char *const NM[5] = {"dark-full", "dark-half", "off", "scatter-half", "scatter-full"};
    uint32_t fail = 0, i, b, l;
    char path[512];
    double ms[5], pk[5];
    for (i = 0; i < 5u; i++) {                  /* layer 1 (FM) held 5 s, let go, 3 s */
        double sum = 0, peak = 0, tail = 0;
        grain_reset();
        wav_ch = 2;
        press_btn(B_FX);
        host_enc[PANEL_ENC[EN_K2]] = V[i] / 2;
        ui_input();
        press_key(3, 1);
        for (b = 0; b < 1378u * 8u; b++) {
            uint32_t k;
            if (b == 1378u * 5u)
                press_key(3, 0);
            host_pump();
            for (k = 0; k < 2 * CTL; k++) {
                double a = abs(mix_out[k]);
                wav_put(mix_out[k]);
                if (a > peak)
                    peak = a;
                if (b < 1378u * 5u)
                    sum += a * a;
                else if (b > 1378u * 5u + 700u)
                    tail += a * a;
            }
        }
        snprintf(path, sizeof path, "%s/grain-%s.wav", dir, NM[i]);
        wav_write(path);
        ms[i] = sum / (1378.0 * 5 * 2 * CTL);
        pk[i] = peak;
        printf("grain %-13s held %.1f dB  peak %.1f dB  after release %.1f dB\n", NM[i],
               10 * __builtin_log10(ms[i] / 1073741824.0 + 1e-12), dbp(peak),
               10 * __builtin_log10(tail / (1378.0 * 2.3 * 2 * CTL) / 1073741824.0 + 1e-12));
        if (peak != peak || peak >= 32767)
            printf("FAIL grain: %s clips or runs away\n", NM[i]), fail++;
    }
    for (i = 0; i < 5u; i++)
        if (i != 2 && (10 * __builtin_log10(ms[i] / ms[2]) < -12.0 || 10 * __builtin_log10(ms[i] / ms[2]) > 6.0))
            printf("FAIL grain: %s is %.1f dB from the dry layer\n", NM[i], 10 * __builtin_log10(ms[i] / ms[2])), fail++;
    {   /* off: bit for bit the layer without a granulator */
        static int32_t a[1378 * 2 * 32];
        uint32_t n = 0, diff = 0;
        for (i = 0; i < 2u; i++) {
            grain_reset();
            press_btn(B_FX);
            if (i) {                             /* turned and back to the middle */
                host_enc[PANEL_ENC[EN_K2]] = 10;
                ui_input();
                host_enc[PANEL_ENC[EN_K2]] = -10;
                ui_input();
            }
            press_key(3, 1);
            for (b = 0; b < 1378u; b++) {
                uint32_t k;
                host_pump();
                for (k = 0; k < 2 * CTL; k++) {
                    if (!i)
                        a[n++] = mix_out[k];
                    else
                        diff += a[b * 2 * CTL + k] != mix_out[k];
                }
            }
        }
        if (diff)
            printf("FAIL grain: off is not the layer untouched (%u samples differ)\n", diff), fail++;
    }
    {   /* the worst: six layers held, all at the far right, the engines all ANIMA; and without the grains */
        clock_t c0;
        double us[2];
        uint32_t pass;
        for (pass = 0; pass < 2u; pass++) {
        grain_reset();
        for (l = 0; l < NLAYER; l++) {
            press_btn(LAYER_BTN[l]);
            if (ui.engine[l] != ENG_ANIMA) {
                host_enc[PANEL_ENC[EN_PRESET]] = 1;
                ui_input();
            }
            host_enc[PANEL_ENC[EN_K2]] = pass ? 32 : 0;
            ui_input();
            press_key(l * 2u, 1);
            press_key(l * 2u + 1u, 1);
        }
        run_blocks(200, 0);
        {
            uint32_t nv = 0, k;
            for (l = 0; l < NLAYER; l++)
                for (k = 0; k < NVOICE; k++)
                    nv += lay[l].v[k].sounding && lay[l].engine == ENG_ANIMA;
            if (nv != 12u)
                printf("grain: (only %u ANIMA voices sounding)\n", nv);
        }
        c0 = clock();
        run_blocks(1378 * 3, 0);
        us[pass] = (double)(clock() - c0) / CLOCKS_PER_SEC * 1e6 / (1378 * 3);
        }
        printf("grain: six layers of two ANIMA voices: %.1f us a block on the host, %.1f with all of them scattering\n",
               us[0], us[1]);
    }
    layers_init();                              /* the dials */
    ui_init();
    sounds_init();
    ui.force = 1;
    ui_draw();
    host_enc[PANEL_ENC[EN_K2]] = -20;
    ui_input();
    ui_draw();
    snprintf(path, sizeof path, "%s/screen-20-grain.ppm", dir);
    ppm(path);
    fm1_ms += 1100;
    ui_draw();
    host_enc[PANEL_ENC[EN_K4]] = 10;
    ui_input();
    ui_draw();
    snprintf(path, sizeof path, "%s/screen-21-send.ppm", dir);
    ppm(path);
    fm1_ms += 1100;
    ui_draw();
    snprintf(path, sizeof path, "%s/screen-22-main.ppm", dir);
    ppm(path);
    printf("grain: %s\n", fail ? "FAILED" : "ok");
    return fail ? 1 : 0;
}

/* the crush: off is the layer untouched; left, at the far end, only a few levels; right, at the far end,
 * each sample held ~147 times (300 Hz); stable; its dial; saved in a session */
static int cmd_crush(const char *dir)
{
    static const int8_t V[3] = {0, -64, 64};
    uint32_t fail = 0, i, b, k;
    char path[512];
    static int32_t ref[1378 * 2 * CTL];
    for (i = 0; i < 3u; i++) {
        uint32_t diff = 0, runs = 0, changes = 0, n = 0;
        int32_t vals[64], nv = 0, prev = 0x7FFFFFFF;
        grain_reset();
        rv.mix = 0;                               /* (the layer alone: no reverb tail in the mix) */
        press_btn(B_FX);
        host_enc[PANEL_ENC[EN_K3]] = V[i] / 2;
        ui_input();
        press_key(3, 1);
        for (b = 0; b < 1378u; b++) {
            host_pump();
            for (k = 0; k < 2 * CTL; k += 2) {   /* the left side */
                int32_t x = lay[0].cr_hold, s_ = mix_out[k];
                (void)x;
                if (!i)
                    ref[n] = s_;
                else if (ref[n] != s_)
                    diff++;
                n++;
                if (s_ != prev)
                    changes++;
                prev = s_;
            }
        }
        if (i == 1) {                              /* the bits: the layer's own samples, before the mix */
            int32_t buf[CTL], j;
            for (j = 0; j < CTL; j++)
                buf[j] = (j - 16) * 1000;
            crush_block(&lay[0], buf, CTL, -64);
            for (j = 0; j < CTL; j++) {
                int32_t m, f = 0;
                for (m = 0; m < nv; m++)
                    f |= vals[m] == buf[j];
                if (!f && nv < 64)
                    vals[nv++] = buf[j];
            }
            printf("crush left, far end: %d levels across +-16000\n", nv);
            if (nv > 3)
                printf("FAIL crush: the far left keeps %d levels\n", nv), fail++;
        }
        if (i == 2) {
            runs = changes ? n / changes : 0;
            printf("crush right, far end: the output changes every %u samples (rate %d Hz)\n", runs, cr_hz(64));
            if (runs < 100u || runs > 200u)
                printf("FAIL crush: not ~300 Hz at the far right\n"), fail++;
        }
        if (i == 0 && 0)
            (void)diff;
    }
    {   /* off after a turn is the layer untouched */
        uint32_t diff = 0, n = 0;
        grain_reset();
        rv.mix = 0;
        press_btn(B_FX);
        host_enc[PANEL_ENC[EN_K3]] = 10;
        ui_input();
        host_enc[PANEL_ENC[EN_K3]] = -10;
        ui_input();
        press_key(3, 1);
        for (b = 0; b < 1378u; b++) {
            host_pump();
            for (k = 0; k < 2 * CTL; k += 2)
                diff += ref[n++] != mix_out[k];
        }
        if (diff)
            printf("FAIL crush: off is not the layer untouched (%u)\n", diff), fail++;
    }
    {   /* a session keeps it */
        grain_reset();
        memset(host_flash, 0xFF, sizeof host_flash);
        ses_scan(1);
        press_btn(B_SEL);
        host_enc[PANEL_ENC[EN_K3]] = 13;
        ui_input();
        ses_save(3);
        ui.cr[1] = 0;
        ses_load_slot(3);
        if (ui.cr[1] != 26 || lay_cr[1] != 26)
            printf("FAIL crush: a session does not keep it (%d)\n", ui.cr[1]), fail++;
    }
    grain_reset();                               /* the dial */
    ui.force = 1;
    ui_draw();
    host_enc[PANEL_ENC[EN_K3]] = -12;
    ui_input();
    ui_draw();
    snprintf(path, sizeof path, "%s/screen-23-crush-bits.ppm", dir);
    ppm(path);
    fm1_ms += 1100;
    ui_draw();
    host_enc[PANEL_ENC[EN_K3]] = 30;
    ui_input();
    ui_draw();
    snprintf(path, sizeof path, "%s/screen-24-crush-rate.ppm", dir);
    ppm(path);
    printf("crush: %s\n", fail ? "FAILED" : "ok");
    return fail ? 1 : 0;
}

/* CALIGULA, played as the other engines: each species, several organisms (a key held, a second key on top,
 * both let go): heard while held, silent after its tail (16 s: the slowest release), never a sound of its own with no key; not running
 * away over a long life (bursts, surges); its cost, six layers of it; the flow (a loop, a session); a WAV of
 * each species */
static int cmd_caligula(const char *dir)
{
    uint32_t fail = 0, sp, n, b, k, l;
    char path[512];
    for (sp = 0; sp < CS_N; sp++)
        for (n = 0; n < 3u; n++) {
            double sum = 0, peak = 0, after = 0;
            uint32_t mind = 8;
            grain_reset();
            rv.mix = 0;
            press_btn(B_FX);
            while (ui.engine[0] != ENG_CAL) {
                host_enc[PANEL_ENC[EN_PRESET]] = 1;
                ui_input();
            }
            run_blocks(40, 0);
            cal_design(&cal_buf[0][cal_use[0] ^ 1u], (int32_t)sp, 1000u + n * 7919u + sp * 31u, PATH_OPEN);
            cal_measure(&cal_buf[0][cal_use[0] ^ 1u]);
            cal_pub[0] = cal_use[0] ^ 1u;
            run_blocks(40, 0);
            if (cal_rt[0].live)
                printf("FAIL caligula: %s sounds with no key\n", CS_NAME[sp]), fail++;
            wav_ch = 2;
            for (b = 0; b < 1378u * 28u; b++) {
                uint32_t t = b / 1378u;
                if (b % 1378u == 0u) {
                    if (t == 0u)
                        press_key(0, 1);            /* the root */
                    if (t == 5u)
                        press_key(4, 1);            /* a fifth up, on top */
                    if (t == 8u)
                        press_key(4, 0);            /* back to the root */
                    if (t == 10u)
                        press_key(0, 0);            /* let go */
                }
                host_pump();
                if (t >= 1u && t < 10u) {
                    uint32_t d_ = (uint32_t)cal_fingers_down(&cal_rt[0]);
                    if (d_ < mind)
                        mind = d_;
                }
                for (k = 0; k < 2 * CTL; k++) {
                    double a = abs(mix_out[k]);
                    if (n == 0)
                        wav_put(mix_out[k]);
                    if (a > peak)
                        peak = a;
                    if (t >= 1u && t < 10u)
                        sum += a * a;
                    if (t >= 26u)
                        after += a * a;
                }
            }
            if (n == 0) {
                snprintf(path, sizeof path, "%s/caligula-%s.wav", dir, CS_NAME[sp]);
                wav_write(path);
            } else
                wav_n = 0;
            sum = 10 * __builtin_log10(sum / (1378.0 * 9 * 2 * CTL) / 1073741824.0 + 1e-12);
            printf("caligula %-8s %u: %.1f dB held, peak %.1f dB, fingers at least %u\n", CS_NAME[sp], n + 1, sum,
                   dbp(peak), mind);
            if (sum < -45.0 || peak != peak)
                printf("FAIL caligula: %s %u is silent or broken\n", CS_NAME[sp], n + 1), fail++;
            if (mind < 1u)
                printf("FAIL caligula: %s %u: the hand let go of everything while the key was held\n", CS_NAME[sp], n + 1), fail++;
            if (after > 0 || cal_rt[0].live)
                printf("FAIL caligula: %s %u still sounds 16 s after the key (live %d)\n", CS_NAME[sp], n + 1, cal_rt[0].live), fail++;
        }
    {   /* a long life: every species held two minutes, surging and bursting: no run-away */
        for (sp = 0; sp < CS_N; sp++) {
            int32_t out[CTL];
            int32_t worst = 0;
            cal_design(&cal_buf[0][0], (int32_t)sp, 77u + sp, PATH_OPEN);
            cal_buf[0][0].surge = 256 * 20;
            cal_buf[0][0].burst = 256 * 6;
            cal_init(&cal_rt[0], 5);
            cal_note_on(&cal_rt[0], &cal_buf[0][0], 48, 1u);
            for (b = 0; b < 1378u * 120u; b++) {
                uint32_t i;
                cal_render(&cal_rt[0], &cal_buf[0][0], out);
                for (i = 0; i < CTL; i++)
                    if (absi(out[i]) > worst)
                        worst = absi(out[i]);
            }
            if (worst > 8 * Q15)
                printf("FAIL caligula: %s runs away (%d)\n", CS_NAME[sp], worst), fail++;
        }
    }
    {   /* the cost: six layers of it, two keys held each, the hands as busy as they get (HIVE) */
        clock_t c0;
        grain_reset();
        for (l = 0; l < NLAYER; l++) {
            press_btn(LAYER_BTN[l]);
            while (ui.engine[l] != ENG_CAL) {
                host_enc[PANEL_ENC[EN_PRESET]] = 1;
                ui_input();
            }
        }
        run_blocks(40, 0);
        for (l = 0; l < NLAYER; l++) {
            cal_design(&cal_buf[l][cal_use[l]], CS_HIVE, 50u + l, PATH_OPEN);
            cal_buf[l][cal_use[l]].fingers = 8;
            cal_note_on(&cal_rt[l], &cal_buf[l][cal_use[l]], 48, 1u);
            cal_note_on(&cal_rt[l], &cal_buf[l][cal_use[l]], 55, 1u);
            for (k = 0; k < 8u; k++)
                cal_land(&cal_rt[l], &cal_buf[l][cal_use[l]], k, 1);
        }
        run_blocks(200, 0);
        c0 = clock();
        run_blocks(1378 * 3, 0);
        printf("caligula: six layers, all eight plates down: %.1f us a block on the host\n",
               (double)(clock() - c0) / CLOCKS_PER_SEC * 1e6 / (1378 * 3));
    }
    {   /* the flow: layer 4 to it, a loop of it, a session, back */
        grain_reset();
        memset(host_flash, 0xFF, sizeof host_flash);
        ses_scan(1);
        press_btn(B_LFO);
        while (ui.engine[3] != ENG_CAL) {
            host_enc[PANEL_ENC[EN_PRESET]] = 1;
            ui_input();
        }
        run_blocks(40, 0);
        if (lay[3].engine != ENG_CAL)
            printf("FAIL caligula: PRESETS does not reach it\n"), fail++;
        ui.force = 1;
        ui_draw();
        ppm("build/screens/screen-25-caligula.ppm");
        press_btn(B_REC);
        press_key(2, 1);
        run_blocks(300, 0);
        if (!cal_rt[3].nheld || !cal_rt[3].live || cal_rt[3].note[0] != scale_note(2, 0))
            printf("FAIL caligula: a key does not play it at its note\n"), fail++;
        press_key(2, 0);
        run_blocks(200, 0);
        press_btn(B_REC);
        run_blocks(1000, 0);
        if (loop[3].ntrk != 1u)
            printf("FAIL caligula: its loop was not kept\n"), fail++;
        {
            cal_design_t keep = cal_buf[3][cal_pub[3]];
            ses_save(1);
            new_cal(3, 999u, PATH_OPEN);
            run_blocks(40, 0);
            ses_load_slot(1);
            if (memcmp(&keep, &cal_buf[3][cal_pub[3]], sizeof keep) || ui.engine[3] != ENG_CAL)
                printf("FAIL caligula: a session does not keep the organism\n"), fail++;
        }
    }
    {   /* overload with CALIGULA sounding: a note just played on ANIMA is never what goes; a released tail is,
         * then CALIGULA's own tail */
        uint32_t k2, held_ok = 1;
        grain_reset();
        press_btn(B_FX);
        while (ui.engine[0] != ENG_CAL) {
            host_enc[PANEL_ENC[EN_PRESET]] = 1;
            ui_input();
        }
        press_btn(B_SEL);
        while (ui.engine[1] != ENG_ANIMA) {
            host_enc[PANEL_ENC[EN_PRESET]] = 1;
            ui_input();
        }
        run_blocks(40, 0);
        press_btn(B_FX);
        press_key(0, 1);
        press_btn(B_SEL);
        run_blocks(300, 0);
        for (n = 0; n < 4u; n++) {                   /* hit after hit, an overload at each */
            press_key(4, 1);
            run_blocks(30, 0);
            shed_req = 1;
            run_blocks(30, 0);
            for (k2 = 0; k2 < NVOICE; k2++)
                if (lay[1].v[k2].gate && (!lay[1].v[k2].sounding || lay[1].v[k2].fade))
                    held_ok = 0;
            if (!lgate(1))
                held_ok = 0;
            press_key(4, 0);
            run_blocks(150, 0);
        }
        if (!held_ok)
            printf("FAIL caligula: an overload took the note just played\n"), fail++;
        press_btn(B_FX);
        press_key(0, 0);
        run_blocks(5, 0);
        for (n = 0; n < 6u; n++) {                   /* the ANIMA tails first, then CALIGULA's */
            shed_req = 1;
            run_blocks(20, 0);
        }
        run_blocks(60, 0);
        if (cal_rt[0].live || nsounding(1))
            printf("FAIL caligula: overload does not let the tails go (cal %d, anima %u)\n", cal_rt[0].live, nsounding(1)), fail++;
    }
    printf("caligula: %s\n", fail ? "FAILED" : "ok");
    return fail ? 1 : 0;
}

/* the latch: keys held, SHIFT (the rightmost black key) pressed: they latch and light; let go, they still sound;
 * SHIFT held and a lit key: it lets go; SHIFT held and an unlit key plays as usual; on another layer the same
 * key plays as usual; a session load lets go */
static int cmd_latch(void)
{
    uint32_t fail = 0, k0;
    for (k0 = 0; k0 < NKEY && white_ix(k0) != 0; k0++)
        ;
    grain_reset();
    run_blocks(40, 0);
    press_key(0, 1);
    run_blocks(50, 0);
    press_phys(KEY_SHIFT, 1);
    ui_leds();
    if (!host_led[14u + KEY_SHIFT])
        printf("FAIL latch: SHIFT does not light\n"), fail++;
    press_phys(KEY_SHIFT, 0);
    press_key(0, 0);
    run_blocks(200, 0);
    ui_leds();
    if (!lgate(0) || !host_led[14u + k0] || host_led[14u + KEY_SHIFT])
        printf("FAIL latch: the key is not held and lit after it is let go (gate %d, lit %d)\n", lgate(0), host_led[14u + k0]), fail++;
    press_key(0, 1);                             /* pressed again, no SHIFT: still latched */
    press_key(0, 0);
    run_blocks(200, 0);
    if (!lgate(0))
        printf("FAIL latch: a plain press of a latched key let go of it\n"), fail++;
    press_phys(KEY_SHIFT, 1);                    /* SHIFT, then an unlit key: it plays, and ends, as usual */
    press_key(2, 1);
    run_blocks(50, 0);
    press_key(2, 0);
    press_phys(KEY_SHIFT, 0);
    run_blocks(50, 0);
    if (ui.latch[0] != 1u << k0 || nsounding(0) < 1u)
        printf("FAIL latch: SHIFT then a key latched it (%x)\n", ui.latch[0]), fail++;
    press_btn(B_SEL);                            /* layer 2: dark, and the key plays there as usual */
    run_blocks(10, 0);
    ui_leds();
    if (host_led[14u + k0])
        printf("FAIL latch: layer 1's latch is lit on layer 2\n"), fail++;
    press_key(0, 1);
    run_blocks(100, 0);
    if (!lgate(1))
        printf("FAIL latch: the key does not play on layer 2\n"), fail++;
    press_key(0, 0);
    run_blocks(100, 0);
    if (lgate(1) || !lgate(0))
        printf("FAIL latch: layer 2's note hangs, or layer 1's latch went\n"), fail++;
    press_btn(B_FX);                             /* back, SHIFT and the key: let go */
    run_blocks(10, 0);
    press_phys(KEY_SHIFT, 1);
    press_key(0, 1);
    press_key(0, 0);
    press_phys(KEY_SHIFT, 0);
    run_blocks(100, 0);
    ui_leds();
    if (lgate(0) || host_led[14u + k0] || ui.latch[0])
        printf("FAIL latch: SHIFT and the lit key do not let it go\n"), fail++;
    press_key(1, 1);                             /* two held, SHIFT: both latch; a session load lets go of both */
    press_key(3, 1);
    press_phys(KEY_SHIFT, 1);
    press_phys(KEY_SHIFT, 0);
    press_key(1, 0);
    press_key(3, 0);
    run_blocks(100, 0);
    if (!ui.latch[0] || nsounding(0) < 2u || !lgate(0))
        printf("FAIL latch: two keys do not latch\n"), fail++;
    memset(host_flash, 0xFF, sizeof host_flash);
    ses_scan(1);
    ses_save(2);
    ses_load_slot(2);
    if (ui.latch[0])
        printf("FAIL latch: a session load keeps the latches lit\n"), fail++;
    {   /* SHIFT and PLAY, from a layer with no loop: every loop playing stops (kept); again, they all start together;
         * PLAY alone starts one */
        uint32_t l2;
        static const uint32_t LB[2] = {B_FX, B_ENV};
        grain_reset();
        run_blocks(40, 0);
        for (l2 = 0; l2 < 2u; l2++) {
            press_btn(LB[l2]);
            press_btn(B_REC);
            press_key(2, 1);
            run_blocks(100, 0);
            press_key(2, 0);
            run_blocks(100, 0);
            press_btn(B_REC);
            run_blocks(50, 0);
        }
        if (!loop[0].play || !loop[2].play)
            printf("FAIL latch: the loops for SHIFT and PLAY do not play\n"), fail++;
        press_btn(B_EDIT);
        press_phys(KEY_SHIFT, 1);
        press_btn(B_PLAY);
        press_phys(KEY_SHIFT, 0);
        run_blocks(20, 0);
        if (loop[0].play || loop[2].play || loop[4].play || loop[0].ntrk != 1u || loop[2].ntrk != 1u || lgate(0) || lgate(2))
            printf("FAIL latch: SHIFT and PLAY do not stop every loop (%d %d)\n", loop[0].play, loop[2].play), fail++;
        press_phys(KEY_SHIFT, 1);                /* again, nothing playing: every loop starts, together */
        press_btn(B_PLAY);
        press_phys(KEY_SHIFT, 0);
        run_blocks(20, 0);
        if (!loop[0].play || !loop[2].play || loop[4].play || loop[0].trk[0].pos != loop[2].trk[0].pos)
            printf("FAIL latch: SHIFT and PLAY with none playing do not start them all together (%d %d, at %u %u)\n",
                   loop[0].play, loop[2].play, loop[0].trk[0].pos, loop[2].trk[0].pos), fail++;
        press_phys(KEY_SHIFT, 1);
        press_btn(B_PLAY);
        press_phys(KEY_SHIFT, 0);
        run_blocks(20, 0);
        press_btn(B_FX);
        press_btn(B_PLAY);
        run_blocks(20, 0);
        if (!loop[0].play || loop[2].play)
            printf("FAIL latch: PLAY alone does not start its own layer's loop only\n"), fail++;
    }
    printf("latch: %s\n", fail ? "FAILED" : "ok");
    return fail ? 1 : 0;
}

/* the paths: what each engine draws on each (families, kinds, species); how the sounds come out on each,
 * measured (attack, brightness, tail); the keys (lit, toggled, per layer); HOME on a path; a session keeps it.
 * A few of each into dir to listen to */
static const char *const PATH_NAME[PATH_N] = {"OPEN", "AMBIENT", "EXPERIMENTAL"};
typedef struct { double att_ms, bright_hz, tail_ms, peak_db, flat, flux, us; } feel_t;
/* an in-place radix-2 FFT of n (a power of 2) complex points */
static void fft(double *re, double *im, uint32_t n)
{
    uint32_t i, j = 0, len, k;
    for (i = 1; i < n; i++) {
        uint32_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            double t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }
    for (len = 2; len <= n; len <<= 1) {
        double a = -6.283185307179586 / len, wr = __builtin_cos(a), wi = __builtin_sin(a);
        for (i = 0; i < n; i += len) {
            double cr = 1, ci = 0;
            for (k = 0; k < len / 2; k++) {
                double ur = re[i + k], ui = im[i + k];
                double vr = re[i + k + len / 2] * cr - im[i + k + len / 2] * ci;
                double vi = re[i + k + len / 2] * ci + im[i + k + len / 2] * cr;
                double t;
                re[i + k] = ur + vr, im[i + k] = ui + vi;
                re[i + k + len / 2] = ur - vr, im[i + k + len / 2] = ui - vi;
                t = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = t;
            }
        }
    }
}
static feel_t feel(uint32_t from, uint32_t hold, uint32_t n)
{
    feel_t f;
    double mx = 0, e = 0, d = 0, w[512];
    uint32_t nw = n / 1024u, i, k, att = 0, tail = nw;
    memset(&f, 0, sizeof f);
    for (k = 0; k < nw && k < 512u; k++) {
        double a = 0;
        for (i = 0; i < 1024u; i++) {
            double x = wav_buf[from + k * 1024u + i];
            a += x * x;
        }
        w[k] = a / 1024;
        if (k * 1024u < hold && w[k] > mx)
            mx = w[k];
    }
    for (k = 0; k < nw && w[k] < mx * 0.25; k++)
        ;
    att = k;
    for (k = hold / 1024u; k < nw; k++)
        if (w[k] < mx * 1e-4) {
            tail = k;
            break;
        }
    for (i = from + 1; i < from + hold; i++) {
        double x = wav_buf[i], y = wav_buf[i - 1];
        e += x * x;
        d += (x - y) * (x - y);
    }
    {   /* the texture, while held (after 0.5 s), 1024-point frames, 100 Hz .. 12 kHz: the spectral flatness (0 a
         * pure tone .. 1 white noise, the geometric over the arithmetic mean of the power) and the flux (how far the
         * normalised spectrum moves from one frame to the next, 0 .. 2) */
        static double re[1024], im[1024], prev[512];
        double sf = 0, sx = 0;
        uint32_t m = 0, lo = 1024u * 100u / 44100u + 1u, hi = 1024u * 12000u / 44100u;
        for (i = from + 22050u; i + 1024u <= from + hold; i += 1024u) {
            double lg = 0, ar = 0, tot = 0, fl = 0;
            for (k = 0; k < 1024u; k++) {
                re[k] = wav_buf[i + k] * (0.5 - 0.5 * __builtin_cos(6.283185307179586 * k / 1024));
                im[k] = 0;
            }
            fft(re, im, 1024u);
            for (k = lo; k < hi; k++) {
                double pw = re[k] * re[k] + im[k] * im[k] + 1e-3;
                lg += __builtin_log(pw), ar += pw;
                re[k] = __builtin_sqrt(pw);
                tot += re[k];
            }
            if (ar < 1e3 * (hi - lo))
                continue;                                     /* (silent: no texture to speak of) */
            sf += __builtin_exp(lg / (hi - lo)) / (ar / (hi - lo));
            for (k = lo; k < hi; k++) {
                double v = re[k] / tot;
                fl += __builtin_fabs(v - prev[k]);
                prev[k] = v;
            }
            if (m)
                sx += fl;
            m++;
        }
        f.flat = m ? sf / m : 0;
        f.flux = m > 1 ? sx / (m - 1) : 0;
    }
    f.att_ms = att * 1024.0 / 44.1;
    f.bright_hz = e > 0 ? 44100.0 / 6.2832 * __builtin_sqrt(d / e) : 0;
    f.tail_ms = (tail - hold / 1024.0) * 1024.0 / 44.1;
    f.peak_db = db(mx);
    return f;
}
static int cmd_paths(const char *dir)
{
    uint32_t fail = 0, pa, i, e, l;
    char path[512];
    static fm_patch_t fp;
    static an_design_t ad;
    static cal_design_t cd;
    feel_t avg[3][PATH_N];
    /* what each path draws */
    for (pa = 0; pa < PATH_N; pa++) {
        uint32_t nf[FF_N] = {0}, na[AK_N] = {0}, nc[CS_N] = {0};
        for (i = 0; i < 900u; i++) {
            fm_generate(&fp, -1, 77u + i * 7919u, pa);
            an_design(&ad, -1, 77u + i * 7919u, pa);
            cal_design(&cd, -1, 77u + i * 7919u, pa);
            nf[fp.family]++, na[ad.kind]++, nc[cd.kind]++;
            if (fp.path != pa || ad.path != pa || cd.path != pa)
                printf("FAIL paths: a sound does not keep its path\n"), fail++, i = 900u;
        }
        printf("paths %-12s FM", PATH_NAME[pa]);
        for (i = 0; i < FF_N; i++)
            printf(" %s %u", FF_NAME[i], nf[i]);
        printf("\n%18s ANIMA", "");
        for (i = 0; i < AK_N; i++)
            printf(" %s %u", AK_NAME[i], na[i]);
        printf("\n%18s CALIGULA", "");
        for (i = 0; i < CS_N; i++)
            printf(" %s %u", CS_NAME[i], nc[i]);
        printf("\n");
        if (pa == PATH_AMBIENT && (nf[FF_BRUIT] || nf[FF_FRICTION] || nf[FF_IMPACT] || na[AK_GRIT] || nc[CS_THORN] || nc[CS_HIVE]))
            printf("FAIL paths: AMBIENT draws the harshest\n"), fail++;
        if (pa == PATH_EXP && (nf[FF_SPECTRE] || nc[CS_VEIL]))
            printf("FAIL paths: EXPERIMENTAL draws the calmest\n"), fail++;
    }
    /* how they come out: 4 s held on C3, 4 s let go; the averages of 40 a path and an engine */
    wav_ch = 1;
    for (e = 0; e < 3u; e++)
        for (pa = 0; pa < PATH_N; pa++) {
            feel_t sum;
            uint32_t silent = 0;
            memset(&sum, 0, sizeof sum);
            for (i = 0; i < 40u; i++) {
                uint32_t seed = 5000u + i * 104729u;
                feel_t f;
                clock_t t0 = clock();
                wav_n = 0;
                if (e < 2u) {
                    host_path = pa;
                    render_sound((int)e, -1, seed, 48, 4.0, 4.0, 1);
                } else {
                    static cal_t c;
                    int32_t out[CTL];
                    uint32_t b, k;
                    cal_design(&cd, -1, seed, pa);
                    cal_measure(&cd);
                    cal_init(&c, seed);
                    cal_note_on(&c, &cd, 48, 1u);
                    for (b = 0; b < 1378u * 8u; b++) {
                        if (b == 1378u * 4u)
                            cal_note_off(&c, 48, 1u);
                        if (!cal_render(&c, &cd, out))
                            memset(out, 0, sizeof out);
                        for (k = 0; k < CTL; k++)
                            wav_put(mulq12(out[k], LAYER_GAIN));
                    }
                }
                f = feel(0, 44100u * 4u, wav_n);
                f.us = (double)(clock() - t0) / CLOCKS_PER_SEC * 1e6 / (1378.0 * 8);
                if (f.peak_db < -50)
                    silent++;
                sum.att_ms += f.att_ms / 40, sum.bright_hz += f.bright_hz / 40, sum.tail_ms += f.tail_ms / 40;
                sum.peak_db += f.peak_db / 40, sum.flat += f.flat / 40, sum.flux += f.flux / 40, sum.us += f.us / 40;
                if (i < 4u && pa) {
                    snprintf(path, sizeof path, "%s/path-%s-%s-%u.wav", dir, PATH_NAME[pa], ENG_NAME[e], i + 1);
                    wav_write(path);
                }
            }
            avg[e][pa] = sum;
            printf("paths %-8s %-12s attack %5.0f ms  brightness %5.0f Hz  flatness %.3f  flux %.2f  tail %5.0f ms  "
                   "loudest %5.1f dB  silent %u  %.2f us/block\n", ENG_NAME[e], PATH_NAME[pa], sum.att_ms, sum.bright_hz,
                   sum.flat, sum.flux, sum.tail_ms, sum.peak_db, silent, sum.us);
            if (pa && silent)
                printf("FAIL paths: %s %s draws silent sounds\n", ENG_NAME[e], PATH_NAME[pa]), fail++;
        }
    for (e = 0; e < 3u; e++)
        if (avg[e][PATH_AMBIENT].bright_hz >= avg[e][PATH_EXP].bright_hz)
            printf("FAIL paths: %s AMBIENT is not darker than EXPERIMENTAL\n", ENG_NAME[e]), fail++;
    if (avg[ENG_ANIMA][PATH_EXP].flat < 3 * avg[ENG_ANIMA][PATH_AMBIENT].flat ||
        avg[ENG_ANIMA][PATH_EXP].flux < 1.6 * avg[ENG_ANIMA][PATH_AMBIENT].flux ||
        avg[ENG_ANIMA][PATH_EXP].flat < 1.3 * avg[ENG_ANIMA][PATH_OPEN].flat ||
        avg[ENG_ANIMA][PATH_EXP].flux < 1.2 * avg[ENG_ANIMA][PATH_OPEN].flux)
        printf("FAIL paths: ANIMA EXPERIMENTAL is not far more textured than AMBIENT\n"), fail++;
    for (e = 0; e < 2u; e++)
        if (avg[e][PATH_AMBIENT].tail_ms <= avg[e][PATH_EXP].tail_ms)
            printf("FAIL paths: %s AMBIENT's tails are not longer\n", ENG_NAME[e]), fail++;
    /* the keys: lit, toggled, a layer's own; HOME draws on it; a session keeps it */
    grain_reset();
    memset(host_flash, 0xFF, sizeof host_flash);
    ses_scan(1);
    run_blocks(40, 0);
    press_phys(KEY_AMB, 1);
    press_phys(KEY_AMB, 0);
    ui_leds();
    if (ui.path[0] != PATH_AMBIENT || !host_led[14u + KEY_AMB] || host_led[14u + KEY_EXP])
        printf("FAIL paths: the AMBIENT key does not choose it, lit\n"), fail++;
    for (i = 0; i < 6u; i++) {
        press_btn(B_HOME);
        run_blocks(20, 0);
        if (fm_buf[0][fm_pub[0]].path != PATH_AMBIENT || FF_W[PATH_AMBIENT][fm_buf[0][fm_pub[0]].family] == 0)
            printf("FAIL paths: HOME does not draw on the path\n"), fail++, i = 6u;
    }
    press_phys(KEY_EXP, 1);
    press_phys(KEY_EXP, 0);
    ui.force = 1;
    ui_draw();
    ppm("build/screens/screen-26-experimental.ppm");
    host_enc[PANEL_ENC[EN_SELECT]] = -3;         /* the volume, as a share of the knob */
    ui_input();
    ui_draw();
    ppm("build/screens/screen-27-volume.ppm");
    press_btn(B_SEL);
    run_blocks(10, 0);
    ui_leds();
    if (ui.path[0] != PATH_EXP || ui.path[1] != PATH_OPEN || host_led[14u + KEY_AMB] || host_led[14u + KEY_EXP])
        printf("FAIL paths: a path is not the layer's own\n"), fail++;
    press_btn(B_FX);
    press_btn(B_HOME);
    run_blocks(20, 0);
    press_phys(KEY_EXP, 1);                      /* the lit one again: open */
    press_phys(KEY_EXP, 0);
    ui_leds();
    if (ui.path[0] != PATH_OPEN || host_led[14u + KEY_EXP])
        printf("FAIL paths: the lit key again does not go back to the open path\n"), fail++;
    press_phys(KEY_AMB, 1);
    press_phys(KEY_AMB, 0);
    {
        fm_patch_t keep = fm_buf[0][fm_pub[0]];
        an_design_t keep_a = an_buf[0][an_pub[0]];
        ses_save(3);
        for (l = 0; l < NLAYER; l++)
            ui.path[l] = PATH_OPEN;
        new_fm(0, 31337u, PATH_OPEN);
        run_blocks(20, 0);
        ses_load_slot(3);
        if (ui.path[0] != PATH_AMBIENT || memcmp(&keep, &fm_buf[0][fm_pub[0]], sizeof keep) ||
            an_buf[0][an_pub[0]].path != keep_a.path || an_buf[0][an_pub[0]].seed != keep_a.seed)
            printf("FAIL paths: a session does not keep the path (%u) or its sounds\n", ui.path[0]), fail++;
    }
    printf("paths: %s\n", fail ? "FAILED" : "ok");
    return fail ? 1 : 0;
}

int main(int argc, char **argv)
{
    if (argc >= 3 && !strcmp(argv[1], "caligula"))
        return cmd_caligula(argv[2]);
    if (argc >= 3 && !strcmp(argv[1], "crush"))
        return cmd_crush(argv[2]);
    if (argc >= 3 && !strcmp(argv[1], "grain"))
        return cmd_grain(argv[2]);
    if (argc >= 3 && !strcmp(argv[1], "filter"))
        return cmd_filter(argv[2]);
    if (argc >= 3 && !strcmp(argv[1], "sessions"))
        return cmd_sessions(argv[2]);
    if (argc >= 3 && !strcmp(argv[1], "reverb"))
        return cmd_reverb(argv[2]);
    if (argc >= 3 && !strcmp(argv[1], "paths"))
        return cmd_paths(argv[2]);
    if (argc >= 2 && !strcmp(argv[1], "latch"))
        return cmd_latch();
    if (argc >= 2 && !strcmp(argv[1], "flows"))
        return cmd_flows();
    if (argc >= 5 && !strcmp(argv[1], "dbg")) {   /* dbg KIND SEED NOTE: a design's controllers, block by block */
        static an_design_t d;
        static an_voice_t av;
        int32_t out[CTL];
        uint32_t b, i, seed = (uint32_t)atoi(argv[3]);
        double mx = 0;
        an_design(&d, atoi(argv[2]), seed, PATH_OPEN);
        an_measure(&d);
        printf("out %d filter %d cut %d..%d src %d q %d..%d amp %d..%d src %d env att %d dec %d sus %d\n", d.out, d.filter,
               d.cutoff.lo, d.cutoff.hi, d.cutoff.src, d.q.lo, d.q.hi, d.amp.lo, d.amp.hi, d.amp.src, d.att, d.dec, d.sus);
        an_init(&av, seed);
        an_note_on(&av, &d, pitch_inc(atoi(argv[4]) * 16), 1);
        for (b = 0; b < 8000; b++) {
            an_render(&av, &d, out, CTL);
            for (i = 0; i < CTL; i++)
                if (abs(out[i]) > mx)
                    mx = abs(out[i]);
            if (b % 500 == 0) {
                printf("b %5u env %.3f amp %.3f lA %.2f lFM %.2f idx %.2f cut %5d q %.1f x %.2f y %.2f size %.2f level %.2f pk %.0f\n", b,
                       av.ev / 16777216.0, av.amp / 32768.0, av.lA / 32768.0, av.lFM / 32768.0, av.index / 4096.0,
                       an_at(&d.cutoff, (int32_t[]){Q15, av.f.x, Q15 - av.f.y, av.f.size, av.f.speed, 0, 0, 0, av.f.x, Q15 - av.f.y, 0, 0, 0}),
                       0.0, av.f.x / 32768.0, av.f.y / 32768.0, av.f.size / 32768.0, av.level / 268435456.0, mx);
                mx = 0;
            }
        }
        return 0;
    }
    if (argc >= 3 && !strcmp(argv[1], "render")) {
        cmd_render(argv[2], argc > 3 ? atoi(argv[3]) : 3);
        return 0;
    }
    if (argc >= 3 && !strcmp(argv[1], "mix")) {
        cmd_mix(argv[2]);
        return 0;
    }
    if (argc >= 3 && !strcmp(argv[1], "screens")) {
        cmd_screens(argv[2]);
        return 0;
    }
    if (argc >= 2 && !strcmp(argv[1], "check"))
        return cmd_check();
    fprintf(stderr, "host render DIR [SEEDS] | mix DIR | screens DIR | check\n");
    return 2;
}
