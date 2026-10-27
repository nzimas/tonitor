/* SPDX-License-Identifier: GPL-3.0-only
 * TONITOR: the logo on the splash screen, drawn from its shapes (docs/assets/logo-dark.svg: the six rings in the
 * circle, the lettering of bars, rings and slants) -- no image kept. Each pixel's colour is each shape's coverage of
 * it (its signed distance, half a pixel either way), over black. Distances in 1/16 of the SVG's unit. */

typedef struct {
    int32_t x0, y0;              /* the pixel the region starts at */
    int32_t u0, v0;              /* .. and the SVG unit there */
    int32_t num, den;            /* pixels a unit: num / den */
} logo_map_t;

static int32_t lg_max(int32_t a, int32_t b) { return a > b ? a : b; }
static int32_t lg_min(int32_t a, int32_t b) { return a < b ? a : b; }
static int32_t lg_abs(int32_t a) { return a < 0 ? -a : a; }
static int32_t lg_dist(int32_t x, int32_t y, int32_t cx, int32_t cy)   /* (x, y in 1/16 unit; the centre in units) */
{
    int32_t dx = x - cx * 16, dy = y - cy * 16;
    return (int32_t)isqrt32((uint32_t)(dx * dx + dy * dy));
}
static int32_t lg_rect(int32_t x, int32_t y, int32_t rx, int32_t ry, int32_t w, int32_t h)
{
    return lg_max(lg_max(rx * 16 - x, x - (rx + w) * 16), lg_max(ry * 16 - y, y - (ry + h) * 16));
}
/* a slant: the parallelogram (ax, top) (ax + w, top) (bx + w, bottom) (bx, bottom), as the N's and the R's leg */
static int32_t lg_slant(int32_t x, int32_t y, int32_t ax, int32_t bx, int32_t w, int32_t top, int32_t bot)
{
    int32_t ex = bx - ax, ey = bot - top, len = (int32_t)isqrt32((uint32_t)(ex * ex + ey * ey));
    int32_t s = (ey * (x - ax * 16) - ex * (y - top * 16)) / len;   /* (right of the left edge: > 0) */
    int32_t d = lg_max(-s, s - (w * 16 * ey) / len);
    return lg_max(d, lg_max(top * 16 - y, y - bot * 16));
}
/* coverage of a pixel (0 .. 256) by a shape d (1/16 unit) away */
static int32_t lg_cov(const logo_map_t *m, int32_t d)
{
    d = clampi(d, -4096, 4096);                  /* (256 units: past any edge; and no 64-bit division) */
    return clampi(128 - (d * 16 * m->num) / m->den, 0, 256);
}

static const uint32_t LOGO_RING_COL[6] = {0xFFD600u, 0x00D6FFu, 0xFF5CD2u, 0x60FF60u, 0xFF8C00u, 0xBEA0FFu};
#define LOGO_LIGHT 0xEDEAE4u
#define LOGO_DARK 0x4A4744u

static void lg_add(int32_t *rgb, uint32_t c, int32_t cov)
{
    rgb[0] += (int32_t)(c >> 16) * cov;
    rgb[1] += (int32_t)(c >> 8 & 255u) * cov;
    rgb[2] += (int32_t)(c & 255u) * cov;
}

/* the mark: the rings (centre 54,100) seen through the circle (100,100, r 80) right of x 92, the bar, the dot, the
 * circle round them */
static void lg_mark(const logo_map_t *m, int32_t x, int32_t y, int32_t *rgb)
{
    int32_t dc = lg_dist(x, y, 100, 100), dr = lg_dist(x, y, 54, 100), in = dc - 80 * 16, k;
    int32_t clip = lg_max(in, 92 * 16 - x);
    for (k = 0; k < 6; k++)
        lg_add(rgb, LOGO_RING_COL[k], lg_cov(m, lg_max(lg_abs(dr - (50 + 14 * k) * 16) - 56, clip)));
    lg_add(rgb, LOGO_DARK, lg_cov(m, lg_max(dr - 11 * 16, in)));
    lg_add(rgb, LOGO_LIGHT, lg_cov(m, lg_min(lg_max(lg_rect(x, y, 80, 10, 12, 180), in), lg_abs(dc - 88 * 16) - 64)));
}

/* the lettering, TONITOR: bars 16 wide and 100 high from y 50; the O's rings r 42 */
static void lg_word(const logo_map_t *m, int32_t x, int32_t y, int32_t *rgb)
{
    int32_t d = lg_rect(x, y, 250, 50, 70, 16);                                       /* T */
    d = lg_min(d, lg_rect(x, y, 277, 50, 16, 100));
    d = lg_min(d, lg_abs(lg_dist(x, y, 388, 100) - 42 * 16) - 128);                 /* O */
    d = lg_min(d, lg_rect(x, y, 456, 50, 16, 100));                                   /* N */
    d = lg_min(d, lg_rect(x, y, 520, 50, 16, 100));
    d = lg_min(d, lg_slant(x, y, 456, 518, 18, 50, 150));
    d = lg_min(d, lg_rect(x, y, 554, 50, 16, 100));                                   /* I */
    d = lg_min(d, lg_rect(x, y, 588, 50, 70, 16));                                    /* T */
    d = lg_min(d, lg_rect(x, y, 615, 50, 16, 100));
    d = lg_min(d, lg_abs(lg_dist(x, y, 726, 100) - 42 * 16) - 128);                 /* O */
    d = lg_min(d, lg_rect(x, y, 794, 50, 16, 100));                                   /* R: the stem, the bowl, the leg */
    d = lg_min(d, lg_rect(x, y, 802, 50, 30, 16));
    d = lg_min(d, lg_rect(x, y, 802, 92, 30, 16));
    d = lg_min(d, lg_max(lg_abs(lg_dist(x, y, 832, 79) - 21 * 16) - 128, 832 * 16 - x));
    d = lg_min(d, lg_slant(x, y, 818, 849, 19, 100, 150));
    lg_add(rgb, LOGO_LIGHT, lg_cov(m, d));
}

/* rows y .. y + h of the screen, region m, drawn by f */
static void lg_draw(int32_t y, int32_t h, const logo_map_t *m,
                    void (*f)(const logo_map_t *, int32_t, int32_t, int32_t *))
{
    CV_AREA(0, (uint32_t)y, 240, (uint32_t)h, C_BLACK) {
        uint32_t i, j;
        for (j = 0; j < cv_h; j++) {
            int32_t py = y + cv_oy + (int32_t)j, v = m->v0 * 16 + ((2 * (py - m->y0) + 1) * 8 * m->den) / m->num;
            for (i = 0; i < cv_w; i++) {
                int32_t u = m->u0 * 16 + ((2 * ((int32_t)i - m->x0) + 1) * 8 * m->den) / m->num, rgb[3] = {0, 0, 0};
                f(m, u, v, rgb);
                cv_px[j * cv_w + i] = swap16(RGB(clampi(rgb[0] >> 8, 0, 255), clampi(rgb[1] >> 8, 0, 255),
                                                 clampi(rgb[2] >> 8, 0, 255)));
            }
        }
    }
}

/* the splash: the mark 140 px high in the middle, the lettering across the width under it, the build below */
static void splash_draw(const char *build)
{
    static const logo_map_t MARK = {50, 4, 0, 0, 7, 10};          /* (the mark's 200 units in 140 px) */
    static const logo_map_t WORD = {8, 156, 250, 50, 224, 618};   /* (250 .. 868 in 224 px: 36 px high) */
    lcd_fill(0, 0, 240, 240, C_BLACK);
    lg_draw(4, 140, &MARK, lg_mark);
    lg_draw(154, 40, &WORD, lg_word);
    draw_text_box(0, 206, 240, &FONT_S, build, C_GREY, 1);
}
