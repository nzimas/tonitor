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
#include <math.h>
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
#include "russolo.c"
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
        fm_generate(&pt, which, seed, host_path, GEN_NOW);
        fm_measure(&pt);
        fm_note_on(&fv, &pt, note, 1);
    } else {
        an_design(&d, which, seed, host_path, GEN_NOW);
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
        alive = engine == ENG_FM ? fm_render(&fv, &pt, out, 0) : an_render(&av, &d, out, CTL);
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
        if (vox[k].gate && vox_owner(&vox[k]) == l)
            return 1;
    return 0;
}
static uint32_t nsounding(uint32_t l)
{
    uint32_t k, n = 0;
    for (k = 0; k < NVOICE; k++)
        n += vox[k].sounding && vox[k].layer == l;
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
    fm_generate(&fm_buf[3][fm_use[3] ^ 1u], FF_DRONE, 4242u, PATH_OPEN, GEN_NOW);
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
            printf("FAIL flows: layer %u still sounding after every key is up (gate %d, sounding %u, engine %d, meter %d)\n", l + 1, lgate(l), nsounding(l), lay[l].engine, lay[l].meter);
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
        fm_generate(&f2, -1, fp[l].seed, fp[l].path, fp[l].gen);
        fm_measure(&f2);
        an_design(&a2, -1, ad[l].seed, ad[l].path, ad[l].gen);
        an_measure(&a2);
        if (memcmp(&f2, &fp[l], sizeof f2) || memcmp(&a2, &ad[l], sizeof a2)) {
            printf("FAIL sessions: layer %u's sounds are not made again from their seeds\n", l + 1);
            fail++;
        }
    }
    /* an empty slot chosen in the list (slot 9): a clean slate, as at start; then SAVE held: saved there, at once */
    ui.latch[1] = 1u;
    ui.scale = 3;
    ui.lx = 2;
    ui.rvp[0] = 12;
    ui.edit = 1;
    save_btn(0);
    host_enc[PANEL_ENC[EN_PRESET]] = 8;
    ui_input();
    ui_draw();
    snprintf(path, sizeof path, "%s/screen-16-empty-slot.ppm", dir);
    ppm(path);
    {
        uint32_t fseed = fm_buf[0][fm_pub[0]].seed, bad = 0;
        save_btn(0);
        run_blocks(200, 0);
        if (ses.current != 9u || ses_has(9))
            printf("FAIL sessions: the empty slot 9 was not chosen (current %u)\n", ses.current), fail++;
        for (l = 0; l < NLAYER; l++)
            bad += loop[l].ntrk || loop[l].play || ui.latch[l] || ui.engine[l] != (l & 1u ? ENG_ANIMA : ENG_FM)
                   || lay[l].engine != ui.engine[l] || ui.level[l] != LEVEL_UNITY || ui.pan[l] != PAN_DEF[l]
                   || ui.send[l] != SEND_DEF || ui.oct[l] || ui.flt[l] || ui.gr[l] || ui.cr[l] || ui.path[l] != PATH_OPEN
                   || layer_sounding(l);
        if (bad || ui.scale || ui.root || ui.lx || ui.rvp[0] != RV_DEF[0] || ui.edit || ui.sel || fm_buf[0][fm_pub[0]].seed == fseed)
            printf("FAIL sessions: an empty slot chosen does not start from a clean slate (%u layers)\n", bad), fail++;
    }
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
    {   /* (session 1 as saved, copied from the flash; its sequence number newer) */
        static uint8_t rec[SES_BUF];
        int i1 = ses_find(1);
        ses_head_t *rh = (ses_head_t *)rec;
        uint32_t len, ns;
        int at = -1;
        ses_flash_read(SES_LO + ses.sect[i1] * SES_SECT, rec, sizeof(ses_head_t));
        len = rh->len;
        ses_flash_read(SES_LO + ses.sect[i1] * SES_SECT, rec, len);
        rh->seq = ++ses.seq;
        rh->head_crc = ses_crc(rec, 30u, 0);
        ns = (len + 4095u) / 4096u;
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
        ses_flash_prog(SES_LO + (uint32_t)at * 4096u, rec, len);
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
    {   /* the largest session there can be -- six full loopers, ten loops each, every event used -- written a sector
         * at a time and read back the same, event for event */
        static looper_t keep[NLAYER];
        uint32_t l2, k2;
        for (l2 = 0; l2 < NLAYER; l2++)
            ev_post(EV_LOOP_CLEAR, l2, 0, 0);
        run_blocks(5, 0);
        memset(host_flash, 0xFF, sizeof host_flash);
        ses_scan(1);
        run_blocks(10, 0);
        for (l2 = 0; l2 < NLAYER; l2++) {
            looper_t *lp = &loop[l2];
            uint32_t per = LOOP_N / LOOP_TRK;
            for (k2 = 0; k2 < LOOP_TRK; k2++) {
                lp->trk[k2].first = (uint16_t)(k2 * per);
                lp->trk[k2].n = (uint16_t)per;
                lp->trk[k2].len = 5000u + k2 * 77u + l2;
            }
            for (k2 = 0; k2 < LOOP_N; k2++)
                lp->ev[k2] = LE((k2 % per) * 20u, 40u + (k2 * 7u + l2) % 40u, (k2 & 1u) == 0u);
            lp->nev = per * LOOP_TRK;
            lp->ntrk = LOOP_TRK;
            lp->mute = (uint16_t)(0x155u >> (l2 & 1u));
        }
        memcpy(keep, loop, sizeof keep);
        if (ses_save(7) != SES_OK)
            printf("FAIL sessions: the largest session does not save\n"), fail++;
        printf("sessions: the largest one, %u sectors\n", ses.nsect[ses_find(7)]);
        for (l2 = 0; l2 < NLAYER; l2++)
            ev_post(EV_LOOP_CLEAR, l2, 0, 0);
        run_blocks(5, 0);
        if (ses_load_slot(7) != SES_OK)
            printf("FAIL sessions: the largest session does not load\n"), fail++;
        run_blocks(5, 0);
        for (l2 = 0; l2 < NLAYER; l2++) {
            const looper_t *a2 = &keep[l2], *b2 = &loop[l2];
            if (a2->ntrk != b2->ntrk || a2->mute != b2->mute || memcmp(a2->ev, b2->ev, sizeof a2->ev))
                printf("FAIL sessions: layer %u's loops come back otherwise\n", l2 + 1), fail++;
            for (k2 = 0; k2 < LOOP_TRK; k2++)
                if (a2->trk[k2].len != b2->trk[k2].len || a2->trk[k2].n != b2->trk[k2].n ||
                    a2->trk[k2].first != b2->trk[k2].first) {
                    printf("FAIL sessions: layer %u loop %u comes back otherwise\n", l2 + 1, k2 + 1), fail++;
                    break;
                }
        }
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
            uint32_t nv = 0;
            for (l = 0; l < NLAYER; l++)
                nv += lay[l].engine == ENG_ANIMA ? nsounding(l) : 0;
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
            cal_design(&cal_buf[0][cal_use[0] ^ 1u], (int32_t)sp, 1000u + n * 7919u + sp * 31u, PATH_OPEN, GEN_NOW);
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
            cal_design(&cal_buf[0][0], (int32_t)sp, 77u + sp, PATH_OPEN, GEN_NOW);
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
            cal_design(&cal_buf[l][cal_use[l]], CS_HIVE, 50u + l, PATH_OPEN, GEN_NOW);
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
                if (vox[k2].gate && vox_owner(&vox[k2]) == 1u && (!vox[k2].sounding || vox[k2].fade || vox[k2].layer != 1u))
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
    memset(host_flash, 0xFF, sizeof host_flash);    /* a session keeps them (and each layer's grain character) */
    ses_scan(1);
    {
        uint32_t keep = ui.latch[0], gs = gran[2].seed;
        ses_save(2);
        press_phys(KEY_SHIFT, 1);                     /* let go of them, the grains drawn anew, then the session */
        press_key(1, 1);
        press_key(1, 0);
        press_key(3, 1);
        press_key(3, 0);
        press_phys(KEY_SHIFT, 0);
        gran_profile(2, 12345u);
        run_blocks(100, 0);
        if (ui.latch[0] || lgate(0))
            printf("FAIL latch: SHIFT and the lit keys do not let both go\n"), fail++;
        ses_load_slot(2);
        run_blocks(100, 0);
        ui_leds();
        if (ui.latch[0] != keep || nsounding(0) < 2u || !lgate(0) || !host_led[14u + 2u])
            printf("FAIL latch: a session does not bring its latched notes back, sounding and lit\n"), fail++;
        if (gran[2].seed != gs)
            printf("FAIL latch: a session does not keep the granulator's character\n"), fail++;
    }
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
            fm_generate(&fp, -1, 77u + i * 7919u, pa, GEN_NOW);
            an_design(&ad, -1, 77u + i * 7919u, pa, GEN_NOW);
            cal_design(&cd, -1, 77u + i * 7919u, pa, GEN_NOW);
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
                    cal_design(&cd, -1, seed, pa, GEN_NOW);
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

/* presets: SHIFT and SAVE opens the layer's engine's; SAVE held saves into the slot chosen (asked over a full one,
 * NO first), SAVE loads; each engine its own 64; they outlive a restart and leave the sessions be; the sound loaded is
 * the sound saved, bit for bit */
static void shift_save(void)
{
    press_phys(KEY_SHIFT, 1);
    host_press |= 1u << PANEL_BTN[B_SAVE];
    fm1_in.buttons |= 1u << PANEL_BTN[B_SAVE];
    ui_input();
    fm1_in.buttons &= ~(1u << PANEL_BTN[B_SAVE]);
    ui_input();
    press_phys(KEY_SHIFT, 0);
}
static int cmd_presets(const char *dir)
{
    uint32_t fail = 0;
    char path[512];
    fm_patch_t keep;
    cal_design_t keep_c;
    grain_reset();
    memset(host_flash, 0xFF, sizeof host_flash);
    ses_scan(1);
    run_blocks(40, 0);
    press_btn(B_FX);
    press_btn(B_HOME);
    run_blocks(40, 0);
    keep = fm_buf[0][fm_pub[0]];
    shift_save();
    if (ui.pre_page != SP_LIST || ui.pre_eng != ENG_FM || ui.pre_ix != 1u)
        printf("FAIL presets: SHIFT and SAVE do not open FM's presets on P1\n"), fail++;
    save_btn(900);                                /* P1 empty: saved at once */
    if (pre_kind(ENG_FM, 1) != keep.family || ui.pre_page != SP_LIST)
        printf("FAIL presets: SAVE held did not save into P1 (%d)\n", pre_kind(ENG_FM, 1)), fail++;
    host_enc[PANEL_ENC[EN_PRESET]] = 1;           /* P2, beside the full P1 */
    ui_input();
    ui.force = 1;
    ui_draw();
    snprintf(path, sizeof path, "%s/screen-31-presets-full.ppm", dir);
    ppm(path);
    host_enc[PANEL_ENC[EN_PRESET]] = 4;           /* P6: page 2 */
    ui_input();
    ui.force = 1;
    ui_draw();
    snprintf(path, sizeof path, "%s/screen-29-presets.ppm", dir);
    ppm(path);
    host_enc[PANEL_ENC[EN_PRESET]] = -9;          /* back to P1 */
    ui_input();
    shift_save();                                 /* closed; a new sound; opened again on P1 */
    if (ui.pre_page)
        printf("FAIL presets: SHIFT and SAVE do not close the page\n"), fail++;
    press_btn(B_HOME);
    run_blocks(40, 0);
    shift_save();
    save_btn(900);                                /* P1 full: asked, NO */
    if (ui.pre_page != SP_ASK || ui.pre_yes)
        printf("FAIL presets: no OVERWRITE question (NO first) over a full slot\n"), fail++;
    ui.force = 1;
    ui_draw();
    snprintf(path, sizeof path, "%s/screen-30-preset-overwrite.ppm", dir);
    ppm(path);
    save_btn(0);
    {
        static fm_patch_t now;
        now = fm_buf[0][fm_pub[0]];
        save_btn(0);                              /* P1 loaded: the first sound back, bit for bit */
        run_blocks(100, 0);
        if (ui.pre_page || memcmp(fm_buf[0][fm_pub[0]].p, keep.p, sizeof keep.p) || fm_buf[0][fm_pub[0]].gain != keep.gain ||
            fm_buf[0][fm_pub[0]].seed != keep.seed || lay[0].engine != ENG_FM)
            printf("FAIL presets: P1 does not load the sound saved into it\n"), fail++;
        if (!memcmp(now.p, keep.p, sizeof keep.p))
            printf("FAIL presets: (the test's second sound is the first)\n"), fail++;
    }
    /* CALIGULA's own presets: layer 2, P5; loaded into layer 4 */
    press_btn(B_SEL);
    while (ui.engine[1] != ENG_CAL) {
        host_enc[PANEL_ENC[EN_PRESET]] = 1;
        ui_input();
    }
    run_blocks(40, 0);
    press_btn(B_HOME);
    run_blocks(40, 0);
    keep_c = cal_buf[1][cal_pub[1]];
    shift_save();
    host_enc[PANEL_ENC[EN_PRESET]] = 4;
    ui_input();
    save_btn(900);
    if (ui.pre_eng != ENG_CAL || pre_kind(ENG_CAL, 5) != keep_c.kind || pre_kind(ENG_CAL, 1) >= 0 || pre_kind(ENG_ANIMA, 5) >= 0)
        printf("FAIL presets: CALIGULA's P5 not saved, or not its own\n"), fail++;
    shift_save();
    ses_scan(1);                                  /* a restart */
    if (pre_kind(ENG_CAL, 5) != keep_c.kind || pre_kind(ENG_FM, 1) != keep.family)
        printf("FAIL presets: they do not outlive a restart\n"), fail++;
    press_btn(B_LFO);
    while (ui.engine[3] != ENG_CAL) {
        host_enc[PANEL_ENC[EN_PRESET]] = 1;
        ui_input();
    }
    run_blocks(40, 0);
    shift_save();
    if (ui.pre_ix != 5u)
        printf("FAIL presets: the page does not come back on the slot last chosen\n"), fail++;
    host_enc[PANEL_ENC[EN_PRESET]] = -64;         /* to P1, then P5 */
    ui_input();
    host_enc[PANEL_ENC[EN_PRESET]] = 4;
    ui_input();
    save_btn(0);
    run_blocks(100, 0);
    if (memcmp(&cal_buf[3][cal_pub[3]], &keep_c, sizeof keep_c))
        printf("FAIL presets: CALIGULA's P5 does not load into layer 4 as saved (seed %u/%u kind %u/%u out %d/%d path %u/%u gen %u/%u)\n",
               cal_buf[3][cal_pub[3]].seed, keep_c.seed, cal_buf[3][cal_pub[3]].kind, keep_c.kind, cal_buf[3][cal_pub[3]].out,
               keep_c.out, cal_buf[3][cal_pub[3]].path, keep_c.path, cal_buf[3][cal_pub[3]].gen, keep_c.gen), fail++;
    /* the sessions: untouched, still saved and loaded */
    if (ses_next_number() != 1u || ses_has(1))
        printf("FAIL presets: the presets show as a session\n"), fail++;
    if (ses_save(1) != SES_OK || ses_load_slot(1) != SES_OK || pre_kind(ENG_FM, 1) != keep.family)
        printf("FAIL presets: sessions and presets get in each other's way\n"), fail++;
    printf("presets: %s\n", fail ? "FAILED" : "ok");
    return fail ? 1 : 0;
}

/* EDIT's macros: none is the sound as drawn, bit for bit (FM); each macro of each engine moves the sound, the
 * brightening ones up and down as they turn, never to silence nor past the ceiling; the EDIT key, its light, the
 * dial's names; HOME clears them; each engine keeps its own; sessions and presets keep them (and 0.21's presets,
 * without them, still load) */
typedef struct { double rms, bright, peak, am; } mfeel_t;
static mfeel_t mfeel(const int32_t *x, uint32_t n)
{
    mfeel_t f;
    double e = 0, d = 0, w[256];
    uint32_t i, nw = 0;
    memset(&f, 0, sizeof f);
    for (i = 1; i < n; i++) {
        double a = x[i], b = x[i] - x[i - 1];
        e += a * a, d += b * b;
        if (fabs(a) > f.peak)
            f.peak = fabs(a);
    }
    for (i = 0; i + 1024u <= n && nw < 256u; i += 1024u, nw++) {
        uint32_t k;
        double a = 0;
        for (k = 0; k < 1024u; k++)
            a += (double)x[i + k] * x[i + k];
        w[nw] = sqrt(a / 1024);
    }
    {   /* the level's movement: the mean step between 23 ms windows over the mean level */
        double m = 0, st = 0;
        for (i = 0; i < nw; i++)
            m += w[i] / nw;
        for (i = 1; i < nw; i++)
            st += fabs(w[i] - w[i - 1]) / (nw - 1);
        f.am = m > 0 ? st / m : 0;
    }
    f.rms = sqrt(e / n);
    f.bright = e > 0 ? 7019.0 * sqrt(d / e) : 0;
    return f;
}
#define MAC_N (44100u * 3u)
static int32_t mac_buf[MAC_N];
/* the spectrogram (dB, 2048-point frames every 4096 samples, 50 Hz .. 12 kHz) of mac_buf, and the distance between
 * two, frame by frame: the mean |dB| over the bins either sounds in -- what changes over time (beating, a tremolo)
 * counts as well as the spectrum */
#define MSP_F 32u
#define MSP_B 560u
#define MSP_N (MSP_F * MSP_B)
static void mspec(double *sp)
{
    static double re[2048], im[2048];
    uint32_t f, k, lo = 2048u * 50u / 44100u;
    for (f = 0; f < MSP_F; f++) {
        uint32_t at = f * 4096u;
        for (k = 0; k < 2048u; k++) {
            re[k] = at + k < MAC_N ? mac_buf[at + k] * (0.5 - 0.5 * cos(6.283185307179586 * k / 2048)) : 0;
            im[k] = 0;
        }
        fft(re, im, 2048u);
        for (k = 0; k < MSP_B; k++)
            sp[f * MSP_B + k] = 10 * log10(re[lo + k] * re[lo + k] + im[lo + k] * im[lo + k] + 1e5);
    }
}
static double mspec_dist(const double *a, const double *b)
{
    uint32_t k, n = 0;
    double d = 0;
    for (k = 0; k < MSP_N; k++)
        if ((a[k] > b[k] ? a[k] : b[k]) > 70.0) {      /* (bins with sound in them: 20 dB over the floor) */
            d += fabs(a[k] - b[k]);
            n++;
        }
    return n ? d / n : 0;
}
/* engine e's sound (seed, kind) held, its first 3 s, with the macros m: into mac_buf */
static void mac_render(uint32_t e, int32_t kind, uint32_t seed, const int8_t *m)
{
    static fm_patch_t fp;
    static an_design_t ad;
    static an_voice_t av;
    static cal_design_t cd;
    static cal_t c;
    static rus_design_t rd;
    static rus_t rc;
    fm_voice_t fv;
    int32_t out[CTL];
    uint32_t b, i, n = 0;
    if (e == ENG_FM) {
        fm_generate(&fp, kind, seed, PATH_OPEN, GEN_NOW);
        fm_measure(&fp);
        memset(&fv, 0, sizeof fv);
        fm_note_on(&fv, &fp, 57, 1);
    } else if (e == ENG_ANIMA) {
        an_design(&ad, kind, seed, PATH_OPEN, GEN_NOW);
        an_measure(&ad);
        an_init(&av, seed);
        memcpy(av.mac, m, NMAC);
        an_note_on(&av, &ad, pitch_inc(57 * 16), 1);
    } else if (e == ENG_CAL) {
        cal_design(&cd, kind, seed, PATH_OPEN, GEN_NOW);
        cal_measure(&cd);
        cal_init(&c, seed);
        memcpy(c.mac, m, NMAC);
        cal_note_on(&c, &cd, 57, 1u);
    } else {
        rus_design(&rd, kind, seed, PATH_OPEN, GEN_NOW);
        rus_measure(&rd);
        rus_init(&rc, seed);
        memcpy(rc.mac, m, NMAC);
        rus_note_on(&rc, &rd, 57);
    }
    for (b = 0; b < 1378u * 7u / 2u; b++) {
        int alive = e == ENG_FM ? fm_render(&fv, &fp, out, m) : e == ENG_ANIMA ? an_render(&av, &ad, out, CTL)
                    : e == ENG_CAL ? cal_render(&c, &cd, out) : rus_render(&rc, &rd, out);
        if (!alive)
            memset(out, 0, sizeof out);
        for (i = 0; i < CTL && n < MAC_N; i++)     /* (the attack too: a macro is heard as notes are played) */
            mac_buf[n++] = out[i];
    }
}
/* how much each ANIMA macro changes the sound: the difference between the render with the macro at an end and
 * without it, against the sound (the hand moves alike in both: what differs is the macro's), dB; FM beside it */
static int cmd_macdiff(void)
{
    static int32_t base[MAC_N], alt[MAC_N];
    uint32_t e, j, kind, end;
    for (e = 0; e < 2u; e++)
        for (j = 0; j < NMAC; j++)
            for (end = 0; end < 2u; end++) {
                double v[64];
                uint32_t nv = 0, x, y;
                for (kind = 0; kind < (e ? (uint32_t)AK_N : (uint32_t)FF_N) && nv < 64u; kind++) {
                    int8_t m[NMAC] = {0, 0, 0, 0};
                    double d2 = 0, b2 = 0;
                    uint32_t i;
                    mac_render(e, (int32_t)kind, 4000u + kind * 31u, m);
                    memcpy(base, mac_buf, sizeof base);
                    m[j] = end ? MAC_MAX : -MAC_MAX;
                    mac_render(e, (int32_t)kind, 4000u + kind * 31u, m);
                    memcpy(alt, mac_buf, sizeof alt);
                    for (i = 0; i < MAC_N; i++) {
                        double dd = (double)alt[i] - base[i];
                        d2 += dd * dd;
                        b2 += (double)base[i] * base[i];
                    }
                    if (b2 > 1e3)
                        v[nv++] = 10 * log10(d2 / b2 + 1e-9);
                }
                for (x = 1; x < nv; x++)
                    for (y = x; y > 0 && v[y - 1] > v[y]; y--) {
                        double t = v[y];
                        v[y] = v[y - 1];
                        v[y - 1] = t;
                    }
                printf("macdiff %-6s %u %s: the change %+5.1f dB against the sound (median; the least %+5.1f)\n", e ? "ANIMA" : "FM",
                       j + 1, end ? "up  " : "down", nv ? v[nv / 2u] : 0.0, nv ? v[0] : 0.0);
            }
    return 0;
}

static int cmd_macros(const char *dir)
{
    static const uint32_t NK[4] = {FF_N, AK_N, CS_N, RK_N};
    static const char *const EN[4] = {"FM", "ANIMA", "CALIGULA", "RUSSOLO"};
    uint32_t fail = 0, e, j, kind, s;
    char path[512];
    /* none: bit for bit */
    for (kind = 0; kind < FF_N; kind++) {
        static fm_patch_t fp;
        fm_voice_t a, b2;
        int32_t oa[CTL], ob[CTL];
        static const int8_t Z[NMAC] = {0, 0, 0, 0};
        uint32_t blk;
        fm_generate(&fp, (int32_t)kind, 300u + kind, PATH_OPEN, GEN_NOW);
        fm_measure(&fp);
        memset(&a, 0, sizeof a);
        memset(&b2, 0, sizeof b2);
        fm_note_on(&a, &fp, 57, 1);
        fm_note_on(&b2, &fp, 57, 1);
        for (blk = 0; blk < 2000u; blk++) {
            int ra = fm_render(&a, &fp, oa, 0), rb = fm_render(&b2, &fp, ob, Z);
            if (ra != rb || (ra && memcmp(oa, ob, sizeof oa))) {
                printf("FAIL macros: FM %s with its macros at 0 is not as drawn\n", FF_NAME[kind]), fail++;
                break;
            }
        }
    }
    /* each macro moves each engine's sounds; the brightening ones brighten; none silences or blows up */
    for (e = 0; e < 4u; e++)
        for (j = 0; j < NMAC; j++) {
            uint32_t moved = 0, tried = 0, wrong = 0, bad = 0, nch = 0;
            static double chs[256];
            for (kind = 0; kind < NK[e]; kind++)
                for (s = 0; s < 2u; s++) {
                    int8_t m[NMAC] = {0, 0, 0, 0};
                    mfeel_t f0, fl, fh;
                    double ch;
                    static double s0[MSP_N], sl[MSP_N], sh[MSP_N];
                    mac_render(e, (int32_t)kind, 4000u + kind * 31u + s * 7919u, m);
                    f0 = mfeel(mac_buf, MAC_N);
                    mspec(s0);
                    if (f0.rms < 30)
                        continue;                         /* (a sound that is not sounding yet: nothing to move) */
                    m[j] = -MAC_MAX;
                    mac_render(e, (int32_t)kind, 4000u + kind * 31u + s * 7919u, m);
                    fl = mfeel(mac_buf, MAC_N);
                    mspec(sl);
                    m[j] = MAC_MAX;
                    mac_render(e, (int32_t)kind, 4000u + kind * 31u + s * 7919u, m);
                    fh = mfeel(mac_buf, MAC_N);
                    mspec(sh);
                    tried++;
                    ch = mspec_dist(sl, s0) + mspec_dist(sh, s0);   /* dB: how far each end takes the spectrum */
                    if (nch < 256u)
                        chs[nch++] = ch;
                    if (ch > 3.0 || fabs(fh.am - fl.am) > 0.05)
                        moved++;
                    else if (getenv("MACDBG"))
                        printf("  still: %s %s seed %u: %.1f dB, am %.3f/%.3f\n", EN[e],
                               e == ENG_FM ? FF_NAME[kind] : e == ENG_ANIMA ? AK_NAME[kind] : e == ENG_CAL ? CS_NAME[kind] : RK_NAME[kind],
                               4000u + kind * 31u + s * 7919u, ch, fl.am, fh.am);
                    if (j == 0u && fh.bright <= fl.bright && getenv("MACDBG"))
                        printf("  darker: %s %s: %.0f / %.0f / %.0f Hz\n", EN[e],
                               e == ENG_FM ? FF_NAME[kind] : e == ENG_ANIMA ? AK_NAME[kind] : e == ENG_CAL ? CS_NAME[kind] : RK_NAME[kind],
                               fl.bright, f0.bright, fh.bright);
                    if (j == 0u && e != ENG_RUS && fh.bright <= fl.bright)   /* BRIGHT, TONE, EDGE: brighter turned up */
                        wrong++;
                    if (fl.peak > 8.0 * 32768 || fh.peak > 8.0 * 32768 || fl.rms < f0.rms * 0.02 || fh.rms < f0.rms * 0.02) {
                        bad++;
                        if (getenv("MACDBG"))
                            printf("  bad: %s macro %u %s: rms %.0f / %.0f / %.0f, peak %.0f / %.0f\n", EN[e],
                                   j + 1, e == ENG_FM ? FF_NAME[kind] : e == ENG_ANIMA ? AK_NAME[kind] : e == ENG_CAL ? CS_NAME[kind] : RK_NAME[kind],
                                   fl.rms, f0.rms, fh.rms, fl.peak, fh.peak);
                    }
                }
            {   /* (how far, typically: the median of the two ends' distances from the sound as drawn) */
                uint32_t x, y;
                for (x = 1; x < nch; x++)
                    for (y = x; y > 0 && chs[y - 1] > chs[y]; y--) {
                        double t = chs[y];
                        chs[y] = chs[y - 1];
                        chs[y - 1] = t;
                    }
                printf("macros %-8s %u: moves %u of %u sounds, typically by %.1f dB%s%s\n", EN[e], j + 1, moved, tried,
                       nch ? chs[nch / 2u] : 0.0, wrong ? " (darker turned up on some)" : "", bad ? " (silent or wild at an end on some)" : "");
            }
            if (moved * 10u < tried * 8u)
                printf("FAIL macros: %s macro %u moves too few sounds\n", EN[e], j + 1), fail++;
            if (wrong * 10u > tried)
                printf("FAIL macros: %s macro 1 does not brighten\n", EN[e]), fail++;
            if (bad)
                printf("FAIL macros: %s macro %u silences or blows up %u sounds\n", EN[e], j + 1, bad), fail++;
        }
    /* the panel */
    grain_reset();
    memset(host_flash, 0xFF, sizeof host_flash);
    ses_scan(1);
    run_blocks(40, 0);
    press_btn(B_FX);
    press_phys(KEY_EDIT, 1);
    press_phys(KEY_EDIT, 0);
    ui_leds();
    if (!ui.edit || !host_led[14u + KEY_EDIT])
        printf("FAIL macros: the EDIT key does not turn EDIT on, lit\n"), fail++;
    host_enc[PANEL_ENC[EN_K1]] = 10;
    host_enc[PANEL_ENC[EN_K3]] = -5;
    ui_input();
    if (ui.mac[0][ui.engine[0]][0] != 20 || ui.mac[0][ui.engine[0]][2] != -10 || lay_mac[0][0] != 20 || ui.flt[0] || ui.cr[0])
        printf("FAIL macros: KNOB 1 and 3 do not turn the macros under EDIT\n"), fail++;
    ui.force = 1;
    ui_draw();
    snprintf(path, sizeof path, "%s/screen-32-macro.ppm", dir);
    ppm(path);
    while (ui.engine[0] != ENG_CAL) {                 /* another engine: its own macros, none yet */
        host_enc[PANEL_ENC[EN_PRESET]] = 1;
        ui_input();
    }
    run_blocks(40, 0);
    if (lay_mac[0][0] || lay_mac[0][2])
        printf("FAIL macros: another engine's sound takes the macros of the first\n"), fail++;
    host_enc[PANEL_ENC[EN_K4]] = 7;
    ui_input();
    {
        int8_t keep[NLAYER][NENG][NMAC];
        memcpy(keep, ui.mac, sizeof keep);
        ses_save(1);                                  /* a session keeps them */
        memset(ui.mac, 0, sizeof ui.mac);
        ses_load_slot(1);
        if (memcmp(keep, ui.mac, sizeof keep) || lay_mac[0][3] != 14)
            printf("FAIL macros: a session does not keep them\n"), fail++;
        pre_save(0, ENG_CAL, 3);                      /* a preset keeps them */
        memset(ui.mac[0][ENG_CAL], 0, NMAC);
        pre_load(0, ENG_CAL, 3);
        run_blocks(40, 0);
        if (ui.mac[0][ENG_CAL][3] != 14 || lay_mac[0][3] != 14)
            printf("FAIL macros: a preset does not keep them\n"), fail++;
    }
    press_btn(B_HOME);                                /* a new sound: as drawn */
    run_blocks(40, 0);
    if (ui.mac[0][ENG_CAL][3] || lay_mac[0][3] || ui.mac[0][ENG_FM][0] != 20)
        printf("FAIL macros: HOME does not clear the new sound's macros (only)\n"), fail++;
    press_phys(KEY_EDIT, 1);
    press_phys(KEY_EDIT, 0);
    host_enc[PANEL_ENC[EN_K1]] = 3;
    ui_input();
    if (ui.edit || ui.flt[0] != 6)
        printf("FAIL macros: EDIT off does not give KNOB 1 back to the filter\n"), fail++;
    {   /* 0.21's presets (12 bytes a slot, no macros) still load, and are widened when one is saved */
        ses_head_t *h = (ses_head_t *)ses_chunk;
        uint32_t len = 32u + 3u * PRE_SLOTS * 12u;      /* (0.21's: three engines) */
        pre_rec_t old;
        memset(ses_chunk, 0, len);
        memset(&old, 0, sizeof old);
        old.seed = cal_buf[0][cal_pub[0]].seed;
        old.word = ses_c2_word(cal_buf[0][cal_pub[0]].out, GEN_NOW, PATH_OPEN);
        old.used = 1;
        old.kind = cal_buf[0][cal_pub[0]].kind;
        memcpy(ses_chunk + 32u + (ENG_CAL * PRE_SLOTS + 6u) * 12u, &old, 12u);
        h->magic = SES_MAGIC, h->num = PRE_NUM, h->format = 1, h->len = len, h->seq = ++ses.seq;
        h->body_crc = ses_crc(ses_chunk + 32u, len - 32u, 0);
        h->head_crc = ses_crc(ses_chunk, 30u, 0);
        ses_write_chunk(PRE_NUM, len);
        if (pre_kind(ENG_CAL, 7) != old.kind || pre_load(0, ENG_CAL, 7) != SES_OK || ui.mac[0][ENG_CAL][0])
            printf("FAIL macros: 0.21's presets do not load\n"), fail++;
        ui.mac[0][ENG_CAL][1] = 30;
        pre_save(0, ENG_CAL, 9);
        if (pre_kind(ENG_CAL, 7) != old.kind || pre_kind(ENG_CAL, 9) < 0)
            printf("FAIL macros: a save over 0.21's presets loses them\n"), fail++;
    }
    printf("macros: %s\n", fail ? "FAILED" : "ok");
    return fail ? 1 : 0;
}

/* PLAY held: the chosen layer's loops on the white keys (lit: playing); a key takes its loop out (its notes let go,
 * the others on) or brings it back in from its start; stopped, the keys choose what PLAY then starts. PLAY let go
 * soon still plays and stops. A session keeps which are out */
static void play_btn(uint32_t held_ms, int key_w)
{
    host_press |= 1u << PANEL_BTN[B_PLAY];
    fm1_in.buttons |= 1u << PANEL_BTN[B_PLAY];
    ui_input();
    fm1_ms += held_ms;
    ui_input();
    if (key_w >= 0) {
        press_key((uint32_t)key_w, 1);
        run_blocks(5, 0);
        press_key((uint32_t)key_w, 0);
    }
    fm1_in.buttons &= ~(1u << PANEL_BTN[B_PLAY]);
    ui_input();
    run_blocks(5, 0);
}
static int loop_note_on(uint32_t l, uint32_t note)   /* a voice of layer l holds note for the loop */
{
    uint32_t k;
    for (k = 0; k < NVOICE; k++)
        if (vox[k].gate && vox_owner(&vox[k]) == l && vox[k].src == SRC_LOOP && vox[k].note == note)
            return 1;
    return 0;
}
/* the notes the loops sound on layer l now (gated voices of the pool, from a loop): how many, and the lowest two */
static uint32_t loop_sounding(uint32_t l, uint32_t *lo, uint32_t *hi)
{
    uint32_t k, n = 0;
    *lo = *hi = 999u;
    for (k = 0; k < NVOICE; k++)
        if (vox[k].gate && vox_owner(&vox[k]) == l && vox[k].src == SRC_LOOP) {
            uint32_t nt = vox[k].note;
            n++;
            if (nt < *lo) {
                *hi = *lo;
                *lo = nt;
            } else if (nt < *hi)
                *hi = nt;
        }
    return n;
}
static void loop_to(uint32_t l, uint32_t pos)   /* run until loop 1 of layer l is at pos */
{
    uint32_t guard = 0;
    while (loop[l].trk[0].pos != pos && guard++ < 100000u)
        run_blocks(1, 0);
}
static void shift_oct_press(int up)
{
    press_phys(KEY_SHIFT, 1);
    ui_input();
    press_btn(up ? B_OCTUP : B_OCTDN);
    press_phys(KEY_SHIFT, 0);
    ui_input();
}
static void shift_oct(int up)
{
    shift_oct_press(up);
    run_blocks(2, 0);
}
/* SHIFT and OCT: every loop moved by steps of the scale, the notes sounding moving at once, nothing left hanging; an
 * overdub recorded while moved plays as played; the scale changed walks the new one; a session keeps it */
static int cmd_transpose(void)
{
    uint32_t fail = 0, n, lo, hi;
    uint32_t c3 = scale_note(0, 0);
    grain_reset();
    memset(host_flash, 0xFF, sizeof host_flash);
    ses_scan(1);
    ui.scale = 0;
    ui.root = 0;
    ui.lx = 0;
    lx_post();
    run_blocks(4, 0);
    if (lx_map(60, 1, 0xAB5u) != 62u || lx_map(61, 1, 0xAB5u) != 63u || lx_map(60, -1, 0xAB5u) != 59u
        || lx_map(64, 2, 0xAB5u) != 67u || lx_map(60, 7, 0xAB5u) != 72u || lx_map(60, -14, 0xAB5u) != 36u)
        printf("FAIL transpose: the steps of C major are not walked right\n"), fail++;
    press_btn(B_FX);
    press_btn(B_REC);                                 /* a loop: C3 held through it */
    press_key(0, 1);
    run_blocks(400, 0);
    press_btn(B_REC);
    press_key(0, 0);
    loop_to(0, 100);
    n = loop_sounding(0, &lo, &hi);
    if (n != 1u || lo != c3)
        printf("FAIL transpose: the loop does not sound its C3 (%u notes, %u)\n", n, lo), fail++;
    shift_oct(1);
    n = loop_sounding(0, &lo, &hi);
    if (n != 1u || lo != c3 + 2u || ui.oct[0] != 0)
        printf("FAIL transpose: SHIFT OCT+ does not move it to D at once (%u notes, %u; octave %d)\n", n, lo, ui.oct[0]), fail++;
    shift_oct(1);
    shift_oct(1);
    n = loop_sounding(0, &lo, &hi);
    if (n != 1u || lo != c3 + 5u)
        printf("FAIL transpose: three steps up is not F (%u notes, %u)\n", n, lo), fail++;
    shift_oct(0);
    shift_oct(0);
    shift_oct(0);
    shift_oct(0);
    n = loop_sounding(0, &lo, &hi);
    if (n != 1u || lo != c3 - 1u || ui.lx != -1)
        printf("FAIL transpose: back down past the start is not B2 (%u notes, %u, lx %d)\n", n, lo, ui.lx), fail++;
    loop_to(0, 50);                                   /* and round the loop: the next C3 struck as B2 too */
    n = loop_sounding(0, &lo, &hi);
    if (n != 1u || lo != c3 - 1u)
        printf("FAIL transpose: round the loop it is not B2 (%u notes, %u)\n", n, lo), fail++;
    ui.scale = 5;                                     /* minor pentatonic, C: one step down from C3 is A#2 */
    lx_post();
    run_blocks(2, 0);
    n = loop_sounding(0, &lo, &hi);
    if (n != 1u || lo != c3 - 2u)
        printf("FAIL transpose: a new scale is not walked (%u notes, %u)\n", n, lo), fail++;
    press_btn(B_REC);                                 /* an overdub while moved: G3 (key 3 of the pentatonic) */
    loop_to(0, 0);
    press_key(3, 1);
    run_blocks(200, 0);
    press_btn(B_REC);
    press_key(3, 0);
    loop_to(0, 100);
    n = loop_sounding(0, &lo, &hi);
    if (loop[0].ntrk != 2u || n != 2u || lo != c3 - 2u || hi != scale_note(3, 0))
        printf("FAIL transpose: an overdub made while moved does not play as played (%u loops, %u notes, %u %u)\n",
               loop[0].ntrk, n, lo, hi), fail++;
    {   /* a session keeps it all */
        int32_t xb = loop[0].trk[1].xb;
        if (ses_save(1) != SES_OK)
            printf("FAIL transpose: the session was not saved\n"), fail++;
        ui.lx = 0;
        ui.scale = 0;
        lx_post();
        run_blocks(2, 0);
        ses_load_slot(1);
        loop_to(0, 100);
        n = loop_sounding(0, &lo, &hi);
        if (ui.lx != -1 || ui.scale != 5u || loop[0].trk[1].xb != xb || xb != -1 || n != 2u || lo != c3 - 2u || hi != scale_note(3, 0))
            printf("FAIL transpose: a session does not keep it (lx %d, xb %d/%d, %u notes, %u %u)\n", ui.lx, loop[0].trk[1].xb, xb,
                   n, lo, hi), fail++;
    }
    press_btn(B_PLAY);                                /* stopped: nothing left hanging */
    run_blocks(3, 0);
    n = loop_sounding(0, &lo, &hi);
    if (n)
        printf("FAIL transpose: %u notes hang after the loops stop\n", n), fail++;
    {   /* moved, no loop note sounding stays on: every step up and down while two loops play */
        uint32_t i, worst = 0;
        press_btn(B_PLAY);
        for (i = 0; i < 40u; i++) {
            shift_oct(i % 7u < 4u);
            run_blocks(37, 0);
            n = loop_sounding(0, &lo, &hi);
            if (n > worst)
                worst = n;
        }
        if (worst > 2u)
            printf("FAIL transpose: %u loop notes at once from two loops of one note each\n", worst), fail++;
    }
    printf("transpose: %s\n", fail ? "FAILED" : "ok");
    return fail ? 1 : 0;
}

/* clicks: loops recorded, overdubbed, wrapped, stopped, started, taken out and in, transposed, on each engine --
 * the output searched for discontinuities (a jump in the second difference far above its neighbourhood), each
 * told with what happened just before it; and the blocks' cost at those moments against the others' */
#define CK_MAX (44100u * 40u)
static float ck_x[CK_MAX];
static uint32_t ck_n;
static double ck_ns[CK_MAX / CTL];
static struct { uint32_t blk; const char *what; } ck_ev[256];
static uint32_t ck_nev;
static void ck_mark(const char *what)
{
    if (ck_nev < 256u) {
        ck_ev[ck_nev].blk = ck_n / CTL;
        ck_ev[ck_nev++].what = what;
    }
}
static void ck_run(uint32_t nb)
{
    while (nb--) {
        struct timespec a, b;
        uint32_t i, w0 = loop[0].ntrk ? loop[0].trk[0].pos : 1u;
        clock_gettime(CLOCK_MONOTONIC, &a);
        host_pump();
        clock_gettime(CLOCK_MONOTONIC, &b);
        if (ck_n / CTL < CK_MAX / CTL)
            ck_ns[ck_n / CTL] = (b.tv_sec - a.tv_sec) * 1e9 + (b.tv_nsec - a.tv_nsec);
        if (loop[0].play && loop[0].ntrk && w0 != 1u && loop[0].trk[0].pos == 1u)
            ck_mark("loop 1 wraps");
        for (i = 0; i < CTL && ck_n < CK_MAX; i++)
            ck_x[ck_n++] = (float)mix_out[2u * i];
    }
}
static void ck_keys(const uint32_t *k, uint32_t n, int down)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        press_key(k[i], down);
}
/* the sounds every track starts with, drawn on generation gen (as sounds_init, which draws on GEN_NOW): a test of
 * the loops' mechanics keeps the sounds it was made with */
static void sounds_gen(uint32_t gen)
{
    uint32_t l;
    for (l = 0; l < NLAYER; l++) {
        uint32_t s = next_seed();
        fm_generate(&fm_buf[l][0], -1, s, PATH_OPEN, gen);
        fm_measure(&fm_buf[l][0]);
        an_design(&an_buf[l][0], -1, s * 7u + 3u, PATH_OPEN, gen);
        an_measure(&an_buf[l][0]);
        cal_design(&cal_buf[l][0], -1, s * 13u + 5u, PATH_OPEN, gen);
        cal_measure(&cal_buf[l][0]);
        rus_design(&rus_buf[l][0], -1, s * 17u + 9u, PATH_OPEN, gen);
        rus_measure(&rus_buf[l][0]);
        fm_pub[l] = fm_use[l] = an_pub[l] = an_use[l] = 0;
        cal_pub[l] = cal_use[l] = rus_pub[l] = rus_use[l] = 0;
        gran_profile(l, next_seed());
    }
}
static int cmd_clicks(void)
{
    static const char *const EN[4] = {"FM", "ANIMA", "CALIGULA", "RUSSOLO"};
    static const uint32_t CH1[3] = {0, 2, 4}, CH2[2] = {1, 5};
    uint32_t e, total = 0;
    for (e = 0; e < 4u; e++) {
        uint32_t i, j, nclk = 0, guard = 0;
        double base = 0, nb = 0;
        grain_reset();
        sounds_gen(2u);                       /* (after it: grain_reset draws them on GEN_NOW) */
        ck_n = ck_nev = 0;
        press_btn(B_FX);
        while (ui.engine[0] != e && guard++ < 20u) {
            host_enc[PANEL_ENC[EN_PRESET]] = 1;
            ui_input();
        }
        ui.send[0] = 0;
        level_pan_apply(0);
        ck_run(60);
        ck_mark("REC, a chord");
        press_btn(B_REC);
        ck_keys(CH1, 3, 1);
        ck_run(300);
        ck_mark("chord let go");
        ck_keys(CH1, 3, 0);
        ck_run(20);
        ck_mark("REC: loop 1 closed, plays");
        press_btn(B_REC);
        ck_run(700);
        ck_mark("REC armed, an overdub");
        press_btn(B_REC);
        ck_run(40);
        ck_keys(CH2, 2, 1);
        ck_run(150);
        ck_mark("overdub keys let go");
        ck_keys(CH2, 2, 0);
        ck_run(30);
        ck_mark("REC: overdub closed");
        press_btn(B_REC);
        ck_run(700);
        ck_mark("PLAY: stop");
        press_btn(B_PLAY);
        ck_run(250);
        ck_mark("PLAY: start");
        press_btn(B_PLAY);
        ck_run(500);
        ck_mark("overdub out");
        ev_post(EV_LOOP_TRK, 0, 1, 0);
        ck_run(300);
        ck_mark("overdub in");
        ev_post(EV_LOOP_TRK, 0, 1, 1);
        ck_run(300);
        ck_mark("SHIFT OCT+");
        shift_oct_press(1);
        ck_run(300);
        ck_mark("SHIFT OCT-");
        shift_oct_press(0);
        ck_run(300);
        ck_mark("SHIFT PLAY: all stop");
        press_phys(KEY_SHIFT, 1);
        press_btn(B_PLAY);
        press_phys(KEY_SHIFT, 0);
        ck_run(300);
        /* the clicks */
        for (i = 600; i + 2 < ck_n; i++) {
            double d2 = fabs(ck_x[i] - 2.0 * ck_x[i - 1] + ck_x[i - 2]), r = 0;
            uint32_t m;
            if (d2 < 200.0)
                continue;
            for (m = i - 520; m < i - 8; m++) {
                double d = ck_x[m] - 2.0 * ck_x[m - 1] + ck_x[m - 2];
                r += d * d;
            }
            r = sqrt(r / 512.0);
            if (d2 > 8.0 * r + 200.0) {
                const char *what = "(nothing)";
                uint32_t blk = i / CTL, since = 0;
                for (j = 0; j < ck_nev; j++)
                    if (ck_ev[j].blk <= blk) {
                        what = ck_ev[j].what;
                        since = blk - ck_ev[j].blk;
                    }
                if (nclk < 12u && e != ENG_RUS)
                    printf("clicks %-8s at %6.2f s: jump %5.0f against %4.0f, %3u blocks after: %s\n", EN[e], i / 44100.0, d2, r,
                           since, what);
                if (getenv("CKDUMP") && nclk < 3u) {
                    uint32_t q;
                    for (q = i - 6; q < i + 10; q++)
                        printf(" %d", (int)ck_x[q]);
                    printf("   (sample %u, block offset %u)\n", i, i % CTL);
                }
                nclk++;
                i += 400;
            }
        }
        for (i = 0; i < ck_n / CTL; i++) {
            base += ck_ns[i];
            nb++;
        }
        base /= nb;
        printf("clicks %-8s %u found; a block costs %.0f ns on average here", EN[e], nclk, base);
        for (j = 0; j < ck_nev; j++) {
            double mx = 0;
            for (i = ck_ev[j].blk; i < ck_ev[j].blk + 3u && i < ck_n / CTL; i++)
                if (ck_ns[i] > mx)
                    mx = ck_ns[i];
            if (mx > 3 * base)
                printf("; %s: %.0f", ck_ev[j].what, mx);
        }
        printf("\n");
        if (e <= ENG_ANIMA)                   /* (CALIGULA's squares and RUSSOLO's clicks are its sound: told, not judged) */
            total += nclk;
    }
    printf("clicks: %s\n", total ? "FAILED" : "ok");
    return total ? 1 : 0;
}

/* what one note on and one note off cost, on each engine, against a block of six sounding layers */
static double ns_now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e9 + t.tv_nsec;
}
static int cmd_evcost(void)
{
    static const char *const EN[4] = {"FM", "ANIMA", "CALIGULA", "RUSSOLO"};
    uint32_t e, r, k;
    for (e = 0; e < 4u; e++) {
        double on = 0, off = 0, blk = 0, worst_on = 0, worst_off = 0;
        layers_init();
        ui_init();
        sounds_init();
        lay[0].engine = lay[0].want_engine = (uint8_t)e;
        run_blocks(10, 0);
        for (r = 0; r < 200u; r++) {
            double t0 = ns_now(), t1;
            for (k = 0; k < 4u; k++)
                note_on(0, 48u + 4u * k, SRC_LOOP);
            t1 = ns_now();
            on += t1 - t0;
            if (t1 - t0 > worst_on)
                worst_on = t1 - t0;
            t0 = ns_now();
            run_blocks(1, 0);
            blk += ns_now() - t0;
            run_blocks(20, 0);
            t0 = ns_now();
            loop_notes_off(0);
            t1 = ns_now();
            off += t1 - t0;
            if (t1 - t0 > worst_off)
                worst_off = t1 - t0;
            run_blocks(30, 0);
        }
        printf("evcost %-8s four notes on: %6.0f ns (worst %6.0f); all off: %6.0f ns (worst %6.0f); the block after: %6.0f ns\n",
               EN[e], on / 200, worst_on, off / 200, worst_off, blk / 200);
    }
    return 0;
}

/* a note held when REC closes the loop is tied over its end: going round it is neither let go nor struck again
 * (the same voice, never re-attacked; CALIGULA's hand stays down); taken out just as it goes round, it stops */
static int cmd_ties(void)
{
    uint32_t fail = 0, e;
    for (e = 0; e < 3u; e++) {               /* FM, ANIMA, CALIGULA */
        uint32_t guard = 0, k, v0 = NVOICE, born = 0, i, broke = 0, n, lo, hi;
        layers_init();
        ui_init();
        sounds_init();
        grain_reset();
        press_btn(B_FX);
        while (ui.engine[0] != e && guard++ < 20u) {
            host_enc[PANEL_ENC[EN_PRESET]] = 1;
            ui_input();
        }
        run_blocks(40, 0);
        press_btn(B_REC);
        press_key(0, 1);                     /* a drone: held through REC */
        run_blocks(200, 0);
        press_key(3, 1);                     /* and a note let go before the end */
        run_blocks(50, 0);
        press_key(3, 0);
        run_blocks(50, 0);
        press_btn(B_REC);
        run_blocks(5, 0);
        press_key(0, 0);
        loop_to(0, 10);
        for (k = 0; k < NVOICE; k++)
            if (vox[k].gate && vox_owner(&vox[k]) == 0u && vox[k].src == SRC_LOOP && vox[k].note == scale_note(0, 0)) {
                v0 = k;
                born = vox[k].born;
            }
        if (e != ENG_CAL && v0 == NVOICE)
            printf("FAIL ties: %u: the drone does not sound from the loop\n", e), fail++;
        for (i = 0; i < 3u * 300u + 40u; i++) {   /* three times round */
            run_blocks(1, 0);
            if (e == ENG_CAL) {
                if (!cal_rt[0].nheld)
                    broke++;
            } else if (!vox[v0].gate || vox[v0].born != born || vox[v0].note != scale_note(0, 0))
                broke++;
        }
        if (broke)
            printf("FAIL ties: %u: the drone is let go or struck again going round (%u blocks)\n", e, broke), fail++;
        /* the other note does go and come back each time round */
        loop_to(0, 230);
        n = loop_sounding(0, &lo, &hi);
        if (e != ENG_CAL && (n != 2u || hi != scale_note(3, 0)))
            printf("FAIL ties: %u: the note let go before the end does not come back (%u notes)\n", e, n), fail++;
        /* out just as it goes round: nothing left hanging */
        loop_to(0, loop[0].trk[0].len - 1u);
        run_blocks(1, 0);                     /* (it has gone round: wrap set, its first events not yet played) */
        if (!loop[0].trk[0].wrap)
            printf("FAIL ties: %u: the test missed the turn\n", e), fail++;
        loop_trk_set(0, 0, 0);
        run_blocks(20, 0);
        if (loop_sounding(0, &lo, &hi) || (e == ENG_CAL && cal_rt[0].nheld))
            printf("FAIL ties: %u: taken out as it goes round, the drone hangs\n", e), fail++;
    }
    printf("ties: %s\n", fail ? "FAILED" : "ok");
    return fail ? 1 : 0;
}

/* the burst heard as a loop first plays (two FM loops going, a third layer's loop closed): the layer's loudness
 * (12 ms RMS) as REC closes it and in its first round, against a later round and a start after a stop */
static double bu_rms[200000];
static uint32_t bu_n;
static void bu_run(uint32_t nb)
{
    while (nb--) {
        uint32_t i;
        double a = 0;
        host_pump();
        for (i = 0; i < CTL; i++)
            a += (double)mix_out[2u * i] * mix_out[2u * i];
        if (bu_n < 200000u)
            bu_rms[bu_n++] = a / CTL;
    }
}
static double bu_peak(uint32_t from, uint32_t n)   /* the loudest 16-block window, RMS */
{
    uint32_t i, j;
    double best = 0;
    for (i = from; i + 16u <= from + n && i + 16u <= bu_n; i++) {
        double a = 0;
        for (j = 0; j < 16u; j++)
            a += bu_rms[i + j];
        a = sqrt(a / 16);
        if (a > best)
            best = a;
    }
    return best;
}
static int cmd_burst(void)
{
    static const char *const EN[4] = {"FM", "ANIMA", "CALIGULA", "RUSSOLO"};
    uint32_t e, held, fail = 0, seed;
    for (seed = 0; seed < 6u; seed++)
    for (e = 1; e < 3u; e++)
        for (held = 0; held < 2u; held++) {
            uint32_t guard = 0, l, close, len, restart;
            double first, later, again;
            layers_init();
            ui_init();
            ui.seed = 0x1234u + seed * 7919u;
            sounds_init();
            grain_reset();
            bu_n = 0;
            for (l = 0; l < 2u; l++) {             /* two FM loops, playing (unheard: level 0) */
                press_btn(l ? B_SEL : B_FX);
                while (ui.engine[l] != ENG_FM && guard++ < 20u) {
                    host_enc[PANEL_ENC[EN_PRESET]] = 1;
                    ui_input();
                }
                ui.level[l] = 0;
                level_pan_apply(l);
                press_btn(B_REC);
                press_key(l * 2u, 1);
                press_key(l * 2u + 4u, 1);
                bu_run(300 + l * 77u);
                press_key(l * 2u, 0);
                press_key(l * 2u + 4u, 0);
                bu_run(40);
                press_btn(B_REC);
                bu_run(50);
            }
            press_btn(B_ENV);                      /* the third layer, CALIGULA or ANIMA: played, then looped */
            guard = 0;
            while (ui.engine[2] != e && guard++ < 20u) {
                host_enc[PANEL_ENC[EN_PRESET]] = 1;
                ui_input();
            }
            ui.send[2] = 0;
            level_pan_apply(2);
            bu_run(100);
            press_btn(B_REC);
            press_key(1, 1);
            bu_run(250);
            press_key(1, 0);
            press_key(3, 1);
            bu_run(250);
            if (!held) {
                press_key(3, 0);
                bu_run(60);
            }
            close = bu_n;
            press_btn(B_REC);
            bu_run(20);
            if (held)
                press_key(3, 0);
            len = loop[2].trk[0].len;
            bu_run(len * 3u);
            later = bu_peak(close + 2u * len, 200);
            first = bu_peak(close, 200);
            press_btn(B_PLAY);                     /* stopped, a while, started again */
            bu_run(len);
            restart = bu_n;
            press_btn(B_PLAY);
            bu_run(len * 2u);
            again = bu_peak(restart, 200);
            printf("burst %-8s %s seed %u: first round %6.0f, a later one %6.0f, after a restart %6.0f%s\n", EN[e],
                   held ? "key held at REC" : "keys let go   ", seed, first, later, again,
                   first > 1.6 * later && first > 1.6 * again ? "   <-- louder at first" : "");
            if (first > 1.6 * later && first > 1.6 * again)
                fail++;
        }
    printf("burst: %s\n", fail ? "FAILED" : "ok");
    return fail ? 1 : 0;
}

/* the budget: with a cost model (a block's time 1000: an FM voice 100, ANIMA 150, CALIGULA 300), release tails
 * piled up and held notes, then CALIGULA woken by a loop: no block over its time, release tails let go first,
 * never a held note */
static int cmd_budget(void)
{
    uint32_t fail = 0, b, k, n, worst = 0, tails0 = 0, tails1 = 0, held_lost = 0;
    host_cost_model[ENG_FM] = 100;
    host_cost_model[ENG_ANIMA] = 150;
    host_cost_model[ENG_CAL] = 300;
    host_cost_model[ENG_RUS] = 50;
    layers_init();
    ui_init();
    sounds_init();
    grain_reset();
    for (k = 0; k < NLAYER; k++)                  /* long releases on the ANIMA layers */
        an_buf[k][an_use[k]].rel = 8000;
    lay[2].engine = lay[2].want_engine = ui.engine[2] = ENG_CAL;
    ev_post(EV_ENGINE, 2, 0, ENG_CAL);
    run_blocks(20, 0);
    for (n = 0; n < 7u; n++) {                   /* ANIMA (layer 2): seven notes struck and let go */
        note_on(1, 50u + 3u * n, SRC_KEY);
        run_blocks(3, 0);
        note_off(1, 50u + 3u * n, SRC_KEY);
    }
    note_on(0, 60, SRC_KEY);                     /* FM (layer 1): two held */
    note_on(0, 64, SRC_KEY);
    run_blocks(2, 0);
    for (k = 0; k < NVOICE; k++)
        tails0 += vox[k].sounding && !vox[k].gate;
    for (b = 0; b < 400u; b++) {
        if (b == 100u)
            note_on(2, 48, SRC_LOOP);            /* CALIGULA wakes (a loop's note) */
        if (b == 200u)
            note_on(1, 70, SRC_KEY);             /* and a new ANIMA note on top */
        run_blocks(1, 0);
        if (b > 2u && blk_last > worst)
            worst = blk_last;
        for (k = 0; k < NVOICE; k++)
            if (vox[k].gate && vox[k].fade && !vox[k].pend)
                held_lost++;
    }
    for (k = 0; k < NVOICE; k++)
        tails1 += vox[k].sounding && !vox[k].gate;
    printf("budget: %u release tails before, %u after; the costliest block %u of 1000 (budget %u)\n", tails0, tails1, worst,
           BUDGET);
    if (worst > 1000u)
        printf("FAIL budget: a block took more than its time (%u)\n", worst), fail++;
    if (held_lost)
        printf("FAIL budget: a held note was let go (%u blocks)\n", held_lost), fail++;
    if (!cal_rt[2].live || !lgate(0))
        printf("FAIL budget: CALIGULA or the held FM notes do not sound\n"), fail++;
    if (tails0 < 3u)
        printf("FAIL budget: the test did not pile up tails (%u)\n", tails0), fail++;
    memset(host_cost_model, 0, sizeof host_cost_model);
    layers_init();
    printf("budget: %s\n", fail ? "FAILED" : "ok");
    return fail ? 1 : 0;
}

/* a track's button held 0.8 s: copied (it blinks); another's held: pasted there (the tone and the loops), the
 * clipboard emptied; the source held again: copied again */
static void hold_btn(uint32_t label, uint32_t ms)
{
    uint32_t t;
    host_press |= 1u << PANEL_BTN[label];
    fm1_in.buttons |= 1u << PANEL_BTN[label];
    ui_input();
    for (t = 0; t < ms; t += 50u) {
        fm1_ms += 50u;
        ui_input();
        run_blocks(2, 0);
    }
    fm1_in.buttons &= ~(1u << PANEL_BTN[label]);
    ui_input();
}
static int cmd_clip(const char *dir)
{
    uint32_t fail = 0, k, i;
    static fm_patch_t f0;
    static an_design_t a0;
    char path[512];
    layers_init();
    ui_init();
    sounds_init();
    grain_reset();
    memset(host_flash, 0xFF, sizeof host_flash);
    ses_scan(1);
    press_btn(B_FX);                                  /* track 1: ANIMA, macros, settings, three loops, one out */
    while (ui.engine[0] != ENG_ANIMA) {
        host_enc[PANEL_ENC[EN_PRESET]] = 1;
        ui_input();
    }
    ui.mac[0][ENG_ANIMA][1] = 30;
    mac_apply(0);
    ui.flt[0] = 7;
    ui.oct[0] = -1;
    ui.send[0] = 77;
    ui.level[0] = 11;
    ui.path[0] = PATH_EXP;
    for (i = 0; i < 3u; i++) {
        press_btn(B_REC);
        press_key(i * 2u, 1);
        run_blocks(300 + 50 * i, 0);
        press_key(i * 2u, 0);
        run_blocks(20, 0);
        press_btn(B_REC);
        run_blocks(30, 0);
    }
    loop_trk_set(0, 1, 0);
    run_blocks(10, 0);
    f0 = fm_buf[0][fm_pub[0]];
    a0 = an_buf[0][an_pub[0]];
    hold_btn(B_FX, 900);                              /* copied */
    ui_leds();
    if (clip_from() != 0 || !ui.flash_on || strcmp(ui.flash_l1, "COPIED"))
        printf("FAIL clip: holding track 1 does not copy it\n"), fail++;
    {
        uint32_t lit = 0, unlit = 0;
        for (k = 0; k < 8u; k++) {
            fm1_ms += 125u;
            ui_leds();
            if (host_led[PANEL_BTN[B_FX]])
                lit++;
            else
                unlit++;
        }
        if (!lit || !unlit)
            printf("FAIL clip: track 1 does not blink while it is on the clipboard\n"), fail++;
    }
    press_btn(B_ENV);                                 /* track 3: chosen, then held: pasted */
    ui.level[2] = 15;
    ui.latch[2] = 1u;
    ui_draw();
    hold_btn(B_ENV, 900);
    ui.force = 1;
    ui_draw();
    snprintf(path, sizeof path, "%s/screen-35-pasted.ppm", dir);
    ppm(path);
    run_blocks(200, 0);
    if (clip_from() >= 0 || strcmp(ui.flash_l1, "PASTED"))
        printf("FAIL clip: holding track 3 does not paste (or the clipboard is not emptied)\n"), fail++;
    if (ui.engine[2] != ENG_ANIMA || lay[2].engine != ENG_ANIMA || memcmp(&an_buf[2][an_pub[2]], &a0, sizeof a0)
        || memcmp(&fm_buf[2][fm_pub[2]], &f0, sizeof f0) || ui.mac[2][ENG_ANIMA][1] != 30 || ui.flt[2] != 7
        || ui.oct[2] != -1 || ui.send[2] != 77 || ui.path[2] != PATH_EXP)
        printf("FAIL clip: track 3 does not have track 1's tone\n"), fail++;
    if (ui.level[2] != 15 || ui.latch[2])
        printf("FAIL clip: track 3's level was taken, or its latch kept\n"), fail++;
    if (loop[2].ntrk != 3u || loop[2].mute != loop[0].mute || loop[2].nev != loop[0].nev || !loop[2].play
        || memcmp(loop[2].ev, loop[0].ev, loop[0].nev * 4u))
        printf("FAIL clip: track 3 does not have track 1's loops, playing (%u loops)\n", loop[2].ntrk), fail++;
    for (k = 0; k < 3u; k++)
        if (loop[2].trk[k].len != loop[0].trk[k].len)
            printf("FAIL clip: loop %u's length differs\n", k + 1), fail++;
    {
        uint32_t n = 0;
        for (k = 0; k < NVOICE; k++)
            n += vox[k].gate && vox_owner(&vox[k]) == 2u && vox[k].src == SRC_LOOP;
        run_blocks(800, 0);
        for (k = 0; k < NVOICE; k++)
            n += vox[k].gate && vox_owner(&vox[k]) == 2u && vox[k].src == SRC_LOOP;
        if (!n)
            printf("FAIL clip: track 3's pasted loops do not play\n"), fail++;
    }
    if (loop[0].ntrk != 3u || !loop[0].play || ui.engine[0] != ENG_ANIMA)
        printf("FAIL clip: track 1 was changed by the copy\n"), fail++;
    hold_btn(B_ENV, 900);                             /* the next hold: a copy again (of track 3) */
    if (clip_from() != 2)
        printf("FAIL clip: after a paste, a hold does not copy again\n"), fail++;
    hold_btn(B_ENV, 900);                             /* the source held again: copied again, nothing pasted */
    if (clip_from() != 2 || strcmp(ui.flash_l1, "COPIED"))
        printf("FAIL clip: the source held again does not copy it again\n"), fail++;
    hold_btn(B_LFO, 300);                             /* a short press only chooses */
    if (clip_from() != 2 || ui.sel != 3u)
        printf("FAIL clip: a short press did something else than choose the track\n"), fail++;
    printf("clip: %s\n", fail ? "FAILED" : "ok");
    return fail ? 1 : 0;
}

/* ANIMA's output, fingerprinted: every kind on every path, several seeds and notes, held then let go (a check that
 * a faster render is the same render, bit for bit) */
static int cmd_anref(void)
{
    static an_design_t d;
    static an_voice_t v;
    int32_t out[CTL], k;
    uint32_t pa, s, b, i, h = 2166136261u, clk = 0, cached = getenv("ANCACHE") != 0;
    for (pa = 0; pa < PATH_N; pa++)
        for (k = 0; k < AK_N; k++)
            for (s = 0; s < 4u; s++) {
                uint32_t hk = 2166136261u;
                an_design(&d, k, 77u + s * 7919u + (uint32_t)k * 31u, pa, GEN_NOW);
                an_measure(&d);
                an_init(&v, 5u + s);
                an_note_on(&v, &d, pitch_inc((int32_t)(36u + 9u * s) * 16), 1);
                for (b = 0; b < 2600u; b++) {
                    if (b == 1800u)
                        an_note_off(&v);
                    if (b == 900u)
                        an_note_on(&v, &d, pitch_inc((int32_t)(43u + 9u * s) * 16), 0);
                    if (cached) {                     /* (as the ISR: the RAM copies, one copied a block) */
                        an_cache_now = ++clk;
                        an_cache_filled = 0;
                    }
                    if (!an_render(&v, &d, out, CTL))
                        memset(out, 0, sizeof out);
                    an_cache_now = 0;
                    for (i = 0; i < CTL; i++)
                        hk = (hk ^ (uint32_t)out[i]) * 16777619u;
                }
                printf("anref %u %d %u %08x\n", pa, k, s, hk);
                h = (h ^ hk) * 16777619u;
            }
    printf("anref all %08x\n", h);
    return 0;
}

/* ANIMA's kinds: their loudness, their pitch (crossings at C3 and C4) and their peak, to set the half rate beside
 * the full one */
static int cmd_anstat(void)
{
    static an_design_t d;
    static an_voice_t v;
    int32_t out[CTL], k;
    uint32_t s, b, i, note;
    for (k = 0; k < AK_N; k++)
        for (s = 0; s < 3u; s++) {
            double rms[2] = {0, 0}, pk = 0;
            uint32_t cr[2] = {0, 0};
            an_design(&d, k, 400u + s * 7919u + (uint32_t)k * 31u, PATH_OPEN, GEN_NOW);
            an_measure(&d);
            for (note = 0; note < 2u; note++) {
                int32_t last = 0;
                an_init(&v, 5u + s);
                an_note_on(&v, &d, pitch_inc((int32_t)(48u + 12u * note) * 16), 1);
                for (b = 0; b < 2000u; b++) {
                    if (!an_render(&v, &d, out, CTL))
                        memset(out, 0, sizeof out);
                    if (b < 600u)
                        continue;
                    for (i = 0; i < CTL; i++) {
                        rms[note] += (double)out[i] * out[i];
                        cr[note] += (out[i] >= 0) != (last >= 0);
                        last = out[i];
                        if (fabs((double)out[i]) > pk)
                            pk = fabs((double)out[i]);
                    }
                }
            }
            printf("anstat %-8s %u: rms %6.0f, crossings C3 %6u C4 %6u, peak %6.0f\n", AK_NAME[k], s,
                   sqrt(rms[0] / (1400.0 * CTL)), cr[0], cr[1], pk);
        }
    return 0;
}

static int cmd_overdubs(const char *dir)
{
    uint32_t fail = 0, i, heard;
    uint32_t n0 = scale_note(0, 0), n2 = scale_note(4, 0);
    char path[512];
    grain_reset();
    memset(host_flash, 0xFF, sizeof host_flash);
    ses_scan(1);
    run_blocks(40, 0);
    press_btn(B_FX);
    for (i = 0; i < 3u; i++) {                       /* three loops: keys 0, 2, 4 held through each */
        press_btn(B_REC);
        press_key(i * 2u, 1);
        run_blocks(400, 0);
        press_btn(B_REC);
        press_key(i * 2u, 0);
        run_blocks(30, 0);
    }
    if (loop[0].ntrk != 3u || !loop[0].play || loop[0].mute)
        printf("FAIL overdubs: three loops not recorded, playing, all in (%u)\n", loop[0].ntrk), fail++;
    /* held, a moment: the view, the three lit; let go: still playing (no PLAY) */
    host_press |= 1u << PANEL_BTN[B_PLAY];
    fm1_in.buttons |= 1u << PANEL_BTN[B_PLAY];
    ui_input();
    fm1_ms += 500;
    ui_input();
    ui_leds();
    if (!ui.ovd || !host_led[14u + 0] || !host_led[14u + 2] || !host_led[14u + 4] || host_led[14u + 5])
        printf("FAIL overdubs: PLAY held does not show the three loops lit on the white keys\n"), fail++;
    press_key(2, 1);                                  /* loop 3 out (white key 2, the third) */
    run_blocks(5, 0);
    press_key(2, 0);
    ui.force = 1;
    ui_draw();
    snprintf(path, sizeof path, "%s/screen-33-overdubs.ppm", dir);
    ppm(path);
    fm1_in.buttons &= ~(1u << PANEL_BTN[B_PLAY]);
    ui_input();
    run_blocks(5, 0);
    if (ui.ovd || !loop[0].play || loop[0].mute != 4u)
        printf("FAIL overdubs: loop 3 not out, or PLAY held stopped the loops (mute %x play %u)\n", loop[0].mute, loop[0].play), fail++;
    heard = 0;
    for (i = 0; i < 900u; i++) {
        host_pump();
        heard |= (uint32_t)loop_note_on(0, n2);
    }
    if (heard)
        printf("FAIL overdubs: loop 3, out, still sounds\n"), fail++;
    play_btn(500, 2);                                 /* back in: from its start, at once */
    if (loop[0].mute || loop[0].trk[2].pos > 10u)
        printf("FAIL overdubs: loop 3 does not come back in from its start (pos %u)\n", loop[0].trk[2].pos), fail++;
    play_btn(50, -1);                                 /* PLAY let go soon: stopped */
    if (loop[0].play)
        printf("FAIL overdubs: a short PLAY does not stop the loops\n"), fail++;
    play_btn(500, 0);                                 /* stopped: only loop 2 chosen */
    play_btn(500, 2);
    if (loop[0].play || loop[0].mute != 5u)
        printf("FAIL overdubs: stopped, the keys do not choose (mute %x play %u)\n", loop[0].mute, loop[0].play), fail++;
    ses_save(1);
    play_btn(50, -1);                                 /* PLAY: only loop 2 */
    heard = 0;
    for (i = 0; i < 900u; i++) {
        host_pump();
        heard |= (uint32_t)loop_note_on(0, n0) << 1 | (uint32_t)loop_note_on(0, scale_note(2, 0));
    }
    if (!loop[0].play || heard != 1u)
        printf("FAIL overdubs: PLAY does not start only the loop chosen (heard %x)\n", heard), fail++;
    ses_load_slot(1);
    run_blocks(20, 0);
    if (loop[0].mute != 5u || loop[0].ntrk != 3u)
        printf("FAIL overdubs: a session does not keep the loops held out (%x)\n", loop[0].mute), fail++;
    printf("overdubs: %s\n", fail ? "FAILED" : "ok");
    return fail ? 1 : 0;
}

/* the pool: NVOICE voices for all the layers. A chord on one layer takes as many as it needs; the pool full, a note
 * takes the oldest tail, else its layer's oldest held note, else the oldest held of all; a voice taken from another
 * layer sounds with its new layer's engine, and the old layer's key lifted does not end it; a layer changing its
 * engine leaves the other layers' voices be (one handed over in its fade too); nothing stuck at the end */
static uint32_t owned(uint32_t l)               /* voices with a note of layer l held, or ringing on it */
{
    uint32_t k, n = 0;
    for (k = 0; k < NVOICE; k++)
        n += (vox[k].gate && vox_owner(&vox[k]) == l) || (vox[k].sounding && vox[k].layer == l);
    return n;
}
static int cmd_voices(void)
{
    uint32_t fail = 0, i, k, n;
    grain_reset();
    run_blocks(40, 0);
    press_btn(B_FX);                                 /* layer 1: FM; a six-note chord */
    for (i = 0; i < 6u; i++)
        press_key(i, 1);
    run_blocks(50, 0);
    if (nsounding(0) != 6u)
        printf("FAIL voices: a six-note chord sounds on %u voices\n", nsounding(0)), fail++;
    for (i = 6; i < 12u; i++)                        /* twelve held: the pool full */
        press_key(i, 1);
    run_blocks(50, 0);
    if (nsounding(0) != 12u)
        printf("FAIL voices: twelve notes on one layer sound on %u voices\n", nsounding(0)), fail++;
    press_key(12, 1);                                /* a thirteenth: the layer's oldest held note (key 0) goes */
    run_blocks(50, 0);
    for (k = 0, n = 0; k < NVOICE; k++)
        n += vox[k].gate && vox[k].note == scale_note(0, 0);
    if (n || nsounding(0) != 12u)
        printf("FAIL voices: the thirteenth note did not take the oldest held (%u)\n", n), fail++;
    press_btn(B_SEL);                                /* layer 2: ANIMA; a note: layer 1's oldest held goes */
    press_key(15, 1);
    run_blocks(60, 0);
    if (nsounding(1) != 1u || nsounding(0) != 11u)
        printf("FAIL voices: layer 2's note did not take a voice from layer 1 (%u, %u)\n", nsounding(1), nsounding(0)), fail++;
    for (k = 0; k < NVOICE; k++)                     /* .. and sounds as ANIMA */
        if (vox[k].layer == 1u && vox[k].sounding && !an_vox[k].live)
            printf("FAIL voices: the voice layer 2 took does not sound as ANIMA\n"), fail++;
    press_btn(B_FX);                                 /* layer 1's keys up: their tails ring; layer 2 keeps its note */
    for (i = 0; i <= 12u; i++)
        press_key(i, 0);
    run_blocks(20, 0);
    if (nsounding(1) != 1u || !lgate(1))
        printf("FAIL voices: lifting layer 1's keys touched layer 2's note\n"), fail++;
    press_btn(B_SEL);                                /* layer 2: four more notes take layer 1's tails, not each other */
    for (i = 0; i < 4u; i++)
        press_key(i, 1);
    run_blocks(60, 0);
    if (nsounding(1) != 5u)
        printf("FAIL voices: layer 2's chord over layer 1's tails sounds on %u voices\n", nsounding(1)), fail++;
    /* layer 1 changes its engine while one of its tails fades to be layer 2's: layer 2 keeps all it plays */
    press_btn(B_FX);
    press_key(14, 1);
    run_blocks(3, 0);
    press_key(14, 0);
    press_btn(B_SEL);
    press_key(5, 1);                                 /* (takes a tail of layer 1: it fades) */
    press_btn(B_FX);
    host_enc[PANEL_ENC[EN_PRESET]] = 1;              /* layer 1 to ANIMA, now */
    ui_input();
    run_blocks(60, 0);
    if (owned(1) != 6u || nsounding(1) != 6u)
        printf("FAIL voices: layer 1's engine change cost layer 2 its notes (%u held, %u sounding)\n", owned(1), nsounding(1)), fail++;
    press_btn(B_SEL);                                /* every key up: nothing stuck */
    for (i = 0; i < 16u; i++)
        press_key(i, 0);
    run_blocks(1378u * 12u, 0);
    for (k = 0, n = 0; k < NVOICE; k++)
        n += vox[k].sounding || vox[k].gate || vox[k].pend;
    if (n)
        printf("FAIL voices: %u voices still busy once every key is up\n", n), fail++;
    printf("voices: %s\n", fail ? "FAILED" : "ok");
    return fail ? 1 : 0;
}

/* what an older Tonitor saved loads here: a session of 0.25 (format 8, three engines) and its presets (format 2) --
 * every layer's engine, level, sounds (by their seeds), macros, the reverb, the loop, the granulator; the presets
 * found and loaded; then a preset saved over them, the old ones kept */
#include "../compat/expected_025.h"
static uint32_t file_into_flash(const char *path, uint32_t sect)
{
    FILE *f = fopen(path, "rb");
    uint32_t n;
    if (!f)
        return 0;
    n = (uint32_t)fread(host_flash + SES_LO + sect * SES_SECT, 1, 65536, f);
    fclose(f);
    return n;
}
static int cmd_compat(void)
{
    uint32_t fail = 0, l;
    grain_reset();
    memset(host_flash, 0xFF, sizeof host_flash);
    if (!file_into_flash("tests/compat/ses_025_f8.bin", 3) || !file_into_flash("tests/compat/pre_025_f2.bin", 20)) {
        printf("FAIL compat: the 0.25 files are missing\n");
        return 1;
    }
    ses_scan(1);
    run_blocks(20, 0);
    if (ses_load_slot(5) != SES_OK)
        printf("FAIL compat: 0.25's session does not load\n"), fail++;
    run_blocks(100, 0);
    for (l = 0; l < NLAYER; l++) {
        if (ui.engine[l] != C25_ENGINE[l] || ui.level[l] != C25_LEVEL[l] || ui.mac[l][ui.engine[l]][1] != C25_MAC1[l])
            printf("FAIL compat: layer %u's engine, level or macro\n", l + 1), fail++;
        if (fm_buf[l][fm_pub[l]].seed != C25_FM[l] || an_buf[l][an_pub[l]].seed != C25_AN[l] ||
            cal_buf[l][cal_pub[l]].seed != C25_CAL[l])
            printf("FAIL compat: layer %u's sounds\n", l + 1), fail++;
        if (ui.mac[l][ENG_RUS][0] || ui.mac[l][ENG_RUS][3])
            printf("FAIL compat: layer %u: RUSSOLO's macros not in the middle\n", l + 1), fail++;
    }
    for (l = 0; l < 4u; l++)
        if (ui.rvp[l] != C25_RVP[l])
            printf("FAIL compat: the reverb\n"), fail++, l = 4u;
    if (loop[0].ntrk != C25_LOOP0 || gran[0].seed != C25_GRAN0)
        printf("FAIL compat: the loop or the granulator\n"), fail++;
    if (pre_kind(ENG_FM, 3) != C25_PRE_FM3_KIND || pre_kind(ENG_CAL, 10) != C25_PRE_CAL10_KIND || pre_kind(ENG_RUS, 1) >= 0)
        printf("FAIL compat: 0.25's presets are not found as they were\n"), fail++;
    if (pre_load(1, ENG_CAL, 10) != SES_OK || cal_buf[1][cal_pub[1]].seed != C25_PRE_CAL10_SEED)
        printf("FAIL compat: 0.25's CALIGULA P10 does not load\n"), fail++;
    if (pre_save(0, ENG_RUS, 2) != SES_OK || pre_kind(ENG_FM, 3) != C25_PRE_FM3_KIND ||
        pre_kind(ENG_CAL, 10) != C25_PRE_CAL10_KIND || pre_kind(ENG_RUS, 2) < 0)
        printf("FAIL compat: a preset saved over 0.25's loses them\n"), fail++;
    printf("compat: %s\n", fail ? "FAILED" : "ok");
    return fail ? 1 : 0;
}

/* RUSSOLO: every kind sounds while a key is held and only then, falls silent after, never clips; the pitched kinds
 * follow the key (an octave up: twice the crossings); a WAV of each; six layers' cost */
static double rus_crossings(uint32_t kind, uint32_t note, uint32_t seed)
{
    static rus_design_t d;
    static rus_t c;
    int32_t out[CTL], prev = 0;
    uint32_t b, i, n = 0;
    rus_design(&d, (int32_t)kind, seed, PATH_OPEN, GEN_NOW);
    d.bits = 10, d.hold = 1, d.p2 = kind == RK_HOLD ? Q15 : 0, d.chaos = kind == RK_HOLD ? Q15 : 0;   /* (the pitch alone) */
    rus_measure(&d);
    rus_init(&c, seed);
    rus_note_on(&c, &d, note);
    for (b = 0; b < 1378u; b++) {
        rus_render(&c, &d, out);
        if (b >= 400u)
            for (i = 0; i < CTL; i++) {
                if ((prev < 0) != (out[i] < 0))
                    n++;
                prev = out[i];
            }
    }
    return n;
}
static int cmd_russolo(const char *dir)
{
    static rus_design_t d;
    static rus_t c;
    uint32_t fail = 0, k, b, i, s;
    char path[512];
    wav_ch = 1;
    for (k = 0; k < RK_N; k++)
        for (s = 0; s < 3u; s++) {
            int32_t out[CTL];
            double held = 0, peak = 0, after = 0, before = 0;
            rus_design(&d, (int32_t)k, 900u + s * 7919u + k * 31u, s == 1u ? PATH_AMBIENT : s == 2u ? PATH_EXP : PATH_OPEN, GEN_NOW);
            rus_measure(&d);
            rus_init(&c, 77u + k);
            wav_n = 0;
            for (b = 0; b < 1378u * 14u; b++) {
                if (b == 1378u)
                    rus_note_on(&c, &d, s == 2u ? 60 : 48);
                if (b == 1378u * 5u)
                    rus_note_off(&c, s == 2u ? 60 : 48);
                if (!rus_render(&c, &d, out))
                    memset(out, 0, sizeof out);
                for (i = 0; i < CTL; i++) {
                    double a = fabs((double)mulq12(out[i], LAYER_GAIN));
                    wav_put(mulq12(out[i], LAYER_GAIN));
                    if (fabs((double)out[i]) >= 32767.0)   /* (the engine's own output: a layer's limiter follows) */
                        peak = 32767.0;
                    if (a > peak && peak < 32767.0)
                        peak = a < 32766.0 ? a : 32766.0;
                    if (b < 1378u)
                        before += a;
                    else if (b < 1378u * 5u)
                        held += a * a;
                    else if (b >= 1378u * 13u)       /* (8 s after: the longest release, 5 s, and its tail) */
                        after += a;
                }
            }
            if (s == 0u) {
                snprintf(path, sizeof path, "%s/russolo-%s.wav", dir, RK_NAME[k]);
                wav_write(path);
            }
            held = 10 * log10(held / (1378.0 * 4 * CTL) / 1073741824.0 + 1e-12);
            printf("russolo %-8s %u: %.1f dB held, peak %.1f dB\n", RK_NAME[k], s + 1, held, dbp(peak));
            if (before > 0)
                printf("FAIL russolo: %s sounds with no key\n", RK_NAME[k]), fail++;
            if (held < -45.0)
                printf("FAIL russolo: %s %u is silent\n", RK_NAME[k], s + 1), fail++;
            if (after > 0 || c.live)
                printf("FAIL russolo: %s %u still sounds 8 s after the key\n", RK_NAME[k], s + 1), fail++;
            if (peak >= 32767.0)
                printf("FAIL russolo: %s %u clips\n", RK_NAME[k], s + 1), fail++;
        }
    {   /* the key's pitch, where the kind has one */
        static const uint8_t PITCHED[] = {RK_NOR, RK_XOR, RK_RING, RK_CASCADE, RK_CHORD, RK_HOLD};
        for (k = 0; k < sizeof PITCHED; k++) {
            double lo = rus_crossings(PITCHED[k], 48, 31u + k), hi = rus_crossings(PITCHED[k], 60, 31u + k);
            printf("russolo %-8s crossings at C3 %.0f, at C4 %.0f (x%.2f)\n", RK_NAME[PITCHED[k]], lo, hi, hi / (lo + 1));
            if (hi < lo * 1.6 || hi > lo * 2.5)
                printf("FAIL russolo: %s does not follow the key\n", RK_NAME[PITCHED[k]]), fail++;
        }
    }
    {   /* six layers of it: the cost */
        clock_t t0;
        static rus_t six[NLAYER];
        static rus_design_t ds[NLAYER];
        int32_t out[CTL];
        for (k = 0; k < NLAYER; k++) {
            rus_design(&ds[k], (int32_t)(RK_CHUNKS + (k % 5u)), 5u + k, PATH_EXP, GEN_NOW);
            rus_init(&six[k], k);
            rus_note_on(&six[k], &ds[k], 48 + k);
        }
        t0 = clock();
        for (b = 0; b < 1378u * 3u; b++)
            for (k = 0; k < NLAYER; k++)
                rus_render(&six[k], &ds[k], out);
        printf("russolo: six layers: %.2f us a block on the host\n", (double)(clock() - t0) / CLOCKS_PER_SEC * 1e6 / (1378 * 3));
    }
    {   /* the flow: layer 4 to it on PRESETS, a key, a loop of it, a session, back */
        grain_reset();
        memset(host_flash, 0xFF, sizeof host_flash);
        ses_scan(1);
        press_btn(B_LFO);
        while (ui.engine[3] != ENG_RUS) {
            host_enc[PANEL_ENC[EN_PRESET]] = 1;
            ui_input();
        }
        run_blocks(40, 0);
        if (lay[3].engine != ENG_RUS)
            printf("FAIL russolo: PRESETS does not reach it\n"), fail++;
        ui.force = 1;
        ui_draw();
        ppm("build/screens/screen-34-russolo.ppm");
        press_btn(B_REC);
        press_key(2, 1);
        run_blocks(300, 0);
        if (!rus_rt[3].nheld || !rus_rt[3].live)
            printf("FAIL russolo: a key does not play it\n"), fail++;
        press_key(2, 0);
        run_blocks(200, 0);
        press_btn(B_REC);
        run_blocks(1000, 0);
        if (loop[3].ntrk != 1u)
            printf("FAIL russolo: its loop was not kept\n"), fail++;
        {
            rus_design_t keep = rus_buf[3][rus_pub[3]];
            ses_save(1);
            new_rus(3, 999u, PATH_OPEN);
            run_blocks(40, 0);
            ses_load_slot(1);
            if (memcmp(&keep, &rus_buf[3][rus_pub[3]], sizeof keep) || ui.engine[3] != ENG_RUS)
                printf("FAIL russolo: a session does not keep its sound\n"), fail++;
        }
    }
    printf("russolo: %s\n", fail ? "FAILED" : "ok");
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
    if (argc >= 3 && !strcmp(argv[1], "russolo"))
        return cmd_russolo(argv[2]);
    if (argc >= 2 && !strcmp(argv[1], "compat"))
        return cmd_compat();
    if (argc >= 2 && !strcmp(argv[1], "voices"))
        return cmd_voices();
    if (argc >= 2 && !strcmp(argv[1], "evcost"))
        return cmd_evcost();
    if (argc >= 3 && !strcmp(argv[1], "clip"))
        return cmd_clip(argv[2]);
    if (argc >= 2 && !strcmp(argv[1], "macdiff"))
        return cmd_macdiff();
    if (argc >= 2 && !strcmp(argv[1], "anstat"))
        return cmd_anstat();
    if (argc >= 2 && !strcmp(argv[1], "anref"))
        return cmd_anref();
    if (argc >= 2 && !strcmp(argv[1], "budget"))
        return cmd_budget();
    if (argc >= 2 && !strcmp(argv[1], "burst"))
        return cmd_burst();
    if (argc >= 2 && !strcmp(argv[1], "ties"))
        return cmd_ties();
    if (argc >= 2 && !strcmp(argv[1], "clicks"))
        return cmd_clicks();
    if (argc >= 2 && !strcmp(argv[1], "transpose"))
        return cmd_transpose();
    if (argc >= 3 && !strcmp(argv[1], "overdubs"))
        return cmd_overdubs(argv[2]);
    if (argc >= 3 && !strcmp(argv[1], "macros"))
        return cmd_macros(argv[2]);
    if (argc >= 3 && !strcmp(argv[1], "presets"))
        return cmd_presets(argv[2]);
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
        an_design(&d, atoi(argv[2]), seed, PATH_OPEN, GEN_NOW);
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
