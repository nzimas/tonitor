/* SPDX-License-Identifier: GPL-3.0-only
 * TONITOR: drawing. No full framebuffer: a band of the screen is drawn into a canvas in RAM and sent to
 * the panel in one DMA transfer (lcd.c). Pixels are stored byte-swapped (the panel takes RGB565 big-endian).
 * Text is alpha-blended onto the canvas's background. */
typedef struct {
    uint8_t h, n;
    const char *chars;           /* the characters it has, in order */
    const uint8_t *adv, *bw;     /* advance, bitmap width (from FONT_PAD left of the pen) */
    const uint32_t *off;
    const uint8_t *data;         /* 4-bit alpha, two pixels a byte */
} font_t;
#include "tonitor_font.h"

#define RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
#define C_BLACK 0x0000u
#define C_WHITE 0xFFFFu
#define C_GREY RGB(110, 110, 110)

#define CV_MAX (240u * 112u)
static uint16_t cv_px[CV_MAX] POOL;
static uint32_t cv_w, cv_h;
static uint16_t cv_bg;

static inline uint16_t swap16(uint32_t c) { return (uint16_t)(((c >> 8) & 0xFFu) | ((c & 0xFFu) << 8)); }

static void cv_begin(uint32_t w, uint32_t h, uint16_t bg)
{
    uint32_t i, n;
    if (w * h > CV_MAX)
        h = CV_MAX / w;
    lcd_sync();                       /* the last transfer may still read the canvas */
    cv_w = w;
    cv_h = h;
    cv_bg = bg;
    n = w * h;
    for (i = 0; i < n; i++)
        cv_px[i] = swap16(bg);
}
/* the colour text blends onto, where it is not the canvas's own (a box drawn on it) */
static void cv_bg_set(uint16_t bg) { cv_bg = bg; }
static void cv_blit(uint32_t x, uint32_t y) { lcd_blit(x, y, cv_w, cv_h, cv_px); }

static void cv_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    int32_t x1 = x + w, y1 = y + h, i;
    uint16_t sc = swap16(c);
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (x1 > (int32_t)cv_w)
        x1 = (int32_t)cv_w;
    if (y1 > (int32_t)cv_h)
        y1 = (int32_t)cv_h;
    for (; y < y1; y++)
        for (i = x; i < x1; i++)
            cv_px[(uint32_t)y * cv_w + (uint32_t)i] = sc;
}
/* an outline t pixels thick */
static void cv_frame(int32_t x, int32_t y, int32_t w, int32_t h, int32_t t, uint16_t c)
{
    cv_rect(x, y, w, t, c);
    cv_rect(x, y + h - t, w, t, c);
    cv_rect(x, y, t, h, c);
    cv_rect(x + w - t, y, t, h, c);
}

static int32_t glyph(const font_t *f, uint32_t ch)
{
    int32_t i;
    if (ch >= 'a' && ch <= 'z')
        ch -= 32u;                    /* capitals only */
    for (i = 0; i < f->n; i++)
        if ((uint8_t)f->chars[i] == ch)
            return i;
    for (i = 0; i < f->n; i++)
        if (f->chars[i] == '?')
            return i;
    return 0;
}

static int32_t text_w(const font_t *f, const char *s)
{
    int32_t w = 0;
    for (; *s; s++)
        w += f->adv[glyph(f, (uint8_t)*s)];
    return w;
}

/* text at (x, y) (top left), colour c over the canvas background; returns the end x */
static int32_t cv_text(int32_t x, int32_t y, const font_t *f, const char *s, uint16_t c)
{
    uint16_t ramp[16];
    int32_t r = c >> 11, g = (c >> 5) & 63, b = c & 31, a;
    int32_t br = cv_bg >> 11, bgc = (cv_bg >> 5) & 63, bb = cv_bg & 31;
    for (a = 0; a < 16; a++)
        ramp[a] = swap16((uint32_t)(((br + (r - br) * a / 15) << 11) | ((bgc + (g - bgc) * a / 15) << 5) |
                                    (bb + (b - bb) * a / 15)));
    for (; *s; s++) {
        int32_t gi = glyph(f, (uint8_t)*s), gx, gy, w = f->bw[gi], bpr = (w + 1) / 2;
        const uint8_t *gd = f->data + f->off[gi];
        for (gy = 0; gy < f->h; gy++) {
            int32_t py = y + gy;
            if ((uint32_t)py >= cv_h)
                continue;
            for (gx = 0; gx < w; gx++) {
                int32_t px = x - FONT_PAD + gx;
                uint32_t v = gd[gy * bpr + gx / 2];
                v = (gx & 1) ? (v & 15u) : (v >> 4);
                if (v && (uint32_t)px < cv_w)
                    cv_px[(uint32_t)py * cv_w + (uint32_t)px] = ramp[v];
            }
        }
        x += f->adv[gi];
    }
    return x;
}
/* centred, and squeezed to the canvas's width (a few pixels less between the letters) if it is wider */
static void cv_text_fit(int32_t y, const font_t *f, const char *s, uint16_t c)
{
    int32_t w = text_w(f, s), n = (int32_t)str_len(s), room = (int32_t)cv_w - 4, squeeze = 0, x;
    if (w > room && n > 1)
        squeeze = (w - room + n - 2) / (n - 1);
    if (squeeze > 6)
        squeeze = 6;
    x = ((int32_t)cv_w - (w - squeeze * (n - 1))) / 2;
    for (; *s; s++) {
        char one[2] = {*s, 0};
        x = cv_text(x, y, f, one, c) - squeeze;
    }
}
/* centred in the canvas's width */
static void cv_text_c(int32_t y, const font_t *f, const char *s, uint16_t c)
{
    cv_text(((int32_t)cv_w - text_w(f, s)) / 2, y, f, s, c);
}

/* one-shot: text in a band, cleared to black, sent (boot, crash, update, rescue) */
static void draw_text_box(uint32_t x, uint32_t y, uint32_t w, const font_t *f, const char *s, uint16_t c, int align)
{
    int32_t tw = text_w(f, s), tx = 0;
    cv_begin(w, f->h, C_BLACK);
    if (align == 1)
        tx = ((int32_t)w - tw) / 2;
    else if (align == 2)
        tx = (int32_t)w - tw;
    cv_text(tx, 0, f, s, c);
    cv_blit(x, y);
    lcd_sync();
}
