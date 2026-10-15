/* SPDX-License-Identifier: GPL-3.0-only
 * TONITOR: the master delay. Mono, before the reverb: the tracks' delay sends (SHIFT + KNOB 4) summed, its echoes
 * to both sides of the master and half of them into the reverb. Its line: 2 s at FS / 2 (22.05 kHz), 8-bit mu-law
 * as the granulator's -- 44 100 bytes, part in the pool, part in the RAM beside the code (app.ld: .dly), so an
 * index reads one of the two. Each echo through a one-pole low-pass (DAMP) and the feedback (at most 0.95).
 * TIME 20 ms .. 2 s on a log scale; a change of it glides (~0.1 s), so the echoes bend in pitch, as a tape's would,
 * rather than click. Parameters 0 .. 100 from the main loop (DELAY page: SHIFT + SEQ) */

#define DL_N 44100u                     /* 2 s at FS / 2 */
#define DL_NA 27200u                    /* the part in the pool */
static uint8_t dl_a[DL_NA] POOL;
static uint8_t dl_b[DL_N - DL_NA] DLYMEM;
static struct {
    uint32_t w;                         /* where the next sample is written */
    int32_t t;                          /* the delay now, Q8 samples at FS / 2 (it glides to dl_t_want) */
    int32_t lp;                         /* the damping's state */
    int32_t prev;                       /* the last echo, for the line drawn to the next */
} dl;
static volatile int32_t dl_t_want = 400 * 22 * 256, dl_fb, dl_damp = Q15, dl_mix;

static inline int32_t dl_get(uint32_t i) { return gr_dec(i < DL_NA ? dl_a[i] : dl_b[i - DL_NA]); }
static inline void dl_put(uint32_t i, int32_t x)
{
    uint8_t c = x > -4 && x < 4 ? 0u : gr_enc(x);   /* (silence as silence: the code's first step is 6, not 0) */
    if (i < DL_NA)
        dl_a[i] = c;
    else
        dl_b[i - DL_NA] = c;
}

/* TIME as ms: 20 x 100^(v / 100) */
static int32_t dl_ms(int32_t v) { return (int32_t)((20u * exp2_q16(clampi(v, 0, 100) * 4354 + (clampi(v, 0, 100) * 12) / 100)) >> 16); }

static void dl_param(int32_t time, int32_t fb, int32_t damp, int32_t mix)   /* each 0..100 (main loop) */
{
    int32_t ms = dl_ms(time);
    dl_t_want = clampi(ms * 22 * 256 + ms * 13, 2 * 256, (int32_t)(DL_N - 4u) * 256);   /* x 22.05 */
    dl_fb = clampi(fb, 0, 100) * (LV(0.95) / 100);
    dl_damp = Q15 - clampi(damp, 0, 100) * (LV(0.88) / 100);   /* (the low-pass's coefficient: 1 none .. 0.12) */
    dl_mix = clampi(mix, 0, 100) * (Q15 / 100);
}

static void dl_init(void)
{
    memset(dl_a, 0x80, sizeof dl_a);    /* (mu-law's zero is not 0x00: 0x80 is -0, silence either way) */
    memset(dl_b, 0x80, sizeof dl_b);
    memset(&dl, 0, sizeof dl);
    dl.t = dl_t_want;
}

/* a block: in (mono, CTL) into the line; the echoes added to l and r, half of them to rvin */
static void dl_block(const int32_t *in, int32_t *l, int32_t *r, int32_t *rvin)
{
    uint32_t i;
    int32_t fb = dl_fb, damp = dl_damp, mix = dl_mix, tw = dl_t_want;
    for (i = 0; i + 1u < CTL; i += 2u) {
        int32_t x = (in[i] + in[i + 1u]) >> 1, y, o0, wv;
        uint32_t p, i0, f;
        dl.t += (tw - dl.t) >> 11;                       /* (the glide) */
        p = (dl.w << 8) + (DL_N << 8) - (uint32_t)dl.t;
        i0 = (p >> 8) % DL_N;
        f = p & 255u;
        y = dl_get(i0);
        y += ((dl_get(i0 + 1u >= DL_N ? 0u : i0 + 1u) - y) * (int32_t)f) >> 8;
        dl.lp += mulq15(y - dl.lp, damp);
        wv = clampi(x + mulq15(dl.lp, fb), -32767, 32767);
        dl_put(dl.w, wv);
        if (++dl.w >= DL_N)
            dl.w = 0;
        y = mulq15(dl.lp, mix);
        o0 = (dl.prev + y) >> 1;                         /* drawn out to the full rate: half-way, then the echo */
        dl.prev = y;
        l[i] += o0;
        r[i] += o0;
        l[i + 1u] += y;
        r[i + 1u] += y;
        rvin[i] += o0 >> 1;
        rvin[i + 1u] += y >> 1;
    }
}
