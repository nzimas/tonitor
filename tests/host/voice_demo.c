/* SPDX-License-Identifier: GPL-3.0-only
 * TONITOR's SCREEN READER on the computer: lines of the screen's text, spoken into a WAV file by
 * firmware/src/voice.c, through the same path as on the FM-1 (vo_text, the ring, vo_block).
 *
 *   voice_demo OUT.wav "line" ["line" ...]     each line an utterance, 0.2 s apart
 *   voice_demo -c                              the check (tests/run_tests.sh): every word of the dictionary and
 *                                              some screen lines spoken, none silent, none clipping, the ring never
 *                                              overrun; exits 1 on a failure
 *
 * Build: tests/run_tests.sh (or see the clang line there). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#define POOL
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
#include "voice.c"

static int32_t *buf;
static uint32_t nbuf, cap;
static int32_t blk[64];

/* speak one utterance into the buffer; its length in samples, its peak */
static uint32_t speak(const char *line, int32_t *peak)
{
    uint32_t n0 = nbuf, guard = 0;
    *peak = 0;
    vo_begin(1);
    vo_text(line);
    vo_end();
    do {
        vo_block(blk, 64);
        if (nbuf + 64 > cap) {
            cap = cap ? cap * 2 : 1 << 20;
            buf = realloc(buf, cap * sizeof *buf);
        }
        for (int i = 0; i < 64; i++) {
            buf[nbuf++] = blk[i];
            if (abs(blk[i]) > *peak)
                *peak = abs(blk[i]);
        }
    } while (vo_busy() && ++guard < 44100 * 30 / 64);
    return nbuf - n0;
}
static void wav_write(const char *path)
{
    FILE *f = fopen(path, "wb");
    uint32_t bytes = nbuf * 2u, v;
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
    W16(1);
    W32(44100u);
    W32(88200u);
    W16(2);
    W16(16);
    fwrite("data", 1, 4, f);
    W32(bytes);
    for (uint32_t i = 0; i < nbuf; i++)
        W16(buf[i] > 32767 ? 32767 : buf[i] < -32768 ? -32768 : buf[i]);
    fclose(f);
}

static int check(void)
{
    static const char *const LINES[] = {"GLOBAL SETUP", "SETTINGS", "SCREEN READER ON", "TRACK 1 +2 VAR",
        "-28.5DB", "12MS", "1.2S", "80%", "C#3", "REPEATS 100", "SAVED 12", "ARMED 3", "XQZW",
        "MIXO- LYDIAN", "0.25", "4.5KHZ"};
    int fails = 0;
    int32_t peak, worst = 0;
    uint32_t i, n;
    char w[32];
    for (i = 0; i < VO_NDICT; i++) {              /* every word on its own */
        str_cpy(w, (const char *)VO_DICT + VO_DOFF[i], sizeof w);
        nbuf = 0;
        n = speak(w, &peak);
        if (peak < 1500 || n < 44100 / 10) {
            printf("FAIL voice: \"%s\" silent or cut (peak %d, %u samples)\n", w, peak, n);
            fails++;
        }
        if (peak > 32767) {
            printf("FAIL voice: \"%s\" clips (peak %d)\n", w, peak);
            fails++;
        }
        if (peak > worst)
            worst = peak;
    }
    for (i = 0; i < sizeof LINES / sizeof *LINES; i++) {
        nbuf = 0;
        speak(LINES[i], &peak);
        if (peak < 1500 || peak > 32767) {
            printf("FAIL voice: \"%s\" peak %d\n", LINES[i], peak);
            fails++;
        }
    }
    {   /* a line longer than the ring: cut short, not overrun */
        char lng[600] = "";
        for (i = 0; i < 40; i++)
            strcat(lng, "SATURATION ");
        nbuf = 0;
        n = speak(lng, &peak);
        if (n > 44100u * 25u) {
            printf("FAIL voice: an over-long line ran %u samples\n", n);
            fails++;
        }
    }
    {   /* cut off mid-word: the new one heard, the old one not resumed */
        nbuf = 0;
        vo_begin(1);
        vo_text("SATURATION SATURATION SATURATION");
        vo_end();
        for (i = 0; i < 40; i++)
            vo_block(blk, 64);
        n = speak("ON", &peak);
        if (n > 44100u) {
            printf("FAIL voice: a cut-off utterance went on (%u samples after the cut)\n", n);
            fails++;
        }
    }
    {   /* SPEECH RATE: 1 the slowest, 5 the fastest */
        uint32_t slow, fast;
        vo_rate(0);
        nbuf = 0;
        slow = speak("SCREEN READER", &peak);
        vo_rate(VO_NRATE - 1u);
        nbuf = 0;
        fast = speak("SCREEN READER", &peak);
        vo_rate(1);
        if (fast * 2u > slow || peak < 1500)
            printf("FAIL voice: SPEECH RATE 5 %u samples, 1 %u\n", fast, slow), fails++;
    }
    printf("voice: %u words and %u lines spoken, peak %d of 32767%s\n", VO_NDICT,
           (unsigned)(sizeof LINES / sizeof *LINES), worst, fails ? "" : ", none silent or clipping");
    return fails ? 1 : 0;
}

int main(int argc, char **argv)
{
    int32_t peak;
    vo_init();
    sp.on = 1;
    if (argc == 2 && !strcmp(argv[1], "-c"))
        return check();
    if (argc < 3) {
        fprintf(stderr, "usage: voice_demo OUT.wav \"line\" ... | voice_demo -c\n");
        return 2;
    }
    for (int a = 2; a < argc; a++) {
        speak(argv[a], &peak);
        for (int i = 0; i < 8820; i++) {
            if (nbuf + 1 > cap) {
                cap = cap ? cap * 2 : 1 << 20;
                buf = realloc(buf, cap * sizeof *buf);
            }
            buf[nbuf++] = 0;
        }
    }
    wav_write(argv[1]);
    printf("%s: %.2f s\n", argv[1], nbuf / 44100.0);
    return 0;
}
