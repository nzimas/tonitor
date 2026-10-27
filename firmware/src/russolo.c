/* SPDX-License-Identifier: GPL-3.0-only
 * TONITOR: RUSSOLO, the fourth engine -- noise, logic and broken buffers, after the techniques of Noisferatu by
 * Robert Heel (rob-scape/noisferatu, GPL-3.0): latched and dusty noise, bitwise logic between oscillators, generated
 * buffers read at a speed and with their address bits bent, chance-driven tones. Re-made here in fixed point and
 * played from the keys: a key gates it (an envelope) and sets its pitch (the oscillators' frequency, the buffers'
 * speed); a second key on top moves it there at once; the last let go, it fades.
 *
 * One organism a layer (as CALIGULA), rendered at a third of the rate (FS / 3, ~14.7 kHz) in 10-bit values, held
 * between its samples -- the grit of a small DAC -- then bit-reduced and sample-held further as the design says.
 * Its kinds:
 *   noise    HOLD (sample-and-hold noise, its rate from the key), DUST (clicks through a low-pass on the key),
 *            GATES (noise opened by two triangles, one wandering), CRACKLE (sparse clicks ringing on short pitched
 *            decays, over hiss)
 *   logic    NOR, XOR (two oscillators combined bit by bit), RING (multiplied), CASCADE (squares modulating
 *            squares), CLANG (two triangles at an inharmonic ratio, XOR'd; the ratio redrawn when two clocks
 *            meet)
 *   buffer   CHUNKS (a buffer of noise, saw and silent chunks with a blip, read at the key's speed, rebuilt in
 *            parts), JUMP (the read jumping at random), WANDER (a window wandering through it), BEND (an address
 *            bit forced to 0 or 1, flipped or held, the bit and how wandering on a clock), QUAD (four address
 *            manipulations at once, each switched on and off by chance)
 *   tones    BLIPS (decaying triangles fired by chance on the key's harmonics), WALK (a random walk over a scale's
 *            steps from the key), CHORD (triangles on the key, slowly beating and swelling)
 * Each has two parameters (the first two of EDIT's macros, named for what they do), and every one CRUSH (bits and
 * sample hold) and CHAOS (how much chance drives it; above its middle, stutters too: its sample frozen a moment). */

#define RUS_BUF 4096u            /* the generated buffer, 8-bit (10-bit >> 2) */
#define RUS_SUB 3u               /* one sample of its own every 3 of the output */

enum { RK_HOLD, RK_DUST, RK_GATES, RK_CRACKLE, RK_NOR, RK_XOR, RK_RING, RK_CASCADE, RK_CLANG, RK_CHUNKS, RK_JUMP,
       RK_WANDER, RK_BEND, RK_QUAD, RK_BLIPS, RK_WALK, RK_CHORD, RK_N };
static const char *const RK_NAME[RK_N] = {"HOLD", "DUST", "GATES", "CRACKLE", "NOR", "XOR", "RING", "CASCADE", "CLANG",
                                          "CHUNKS", "JUMP", "WANDER", "BEND", "QUAD", "BLIPS", "WALK", "CHORD"};
/* the two parameters' names, a kind each */
static const char *const RK_P[RK_N][2] = {
    {"RATE", "CHANCE"}, {"TONE", "DENSITY"}, {"WIDTH", "WANDER"}, {"HISS", "DENSITY"}, {"RATIO", "DRIFT"},
    {"RATIO", "DRIFT"}, {"RATIO", "DRIFT"}, {"RATIO", "SUB"}, {"SPREAD", "CLOCK"}, {"DENSITY", "CHANGE"},
    {"SILENCE", "JUMPS"}, {"WINDOW", "WANDER"}, {"DENSITY", "CLOCK"}, {"DENSITY", "CLOCK"}, {"SPREAD", "RATE"},
    {"STEP", "RATE"}, {"DEPTH", "RATE"}};
/* the kinds' weights on each path */
static const uint8_t RK_W[3][RK_N] = {
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 3, 1, 3, 0, 0, 0, 0, 0, 1, 0, 2, 0, 0, 3, 3, 4},
    {2, 1, 2, 1, 3, 3, 2, 2, 3, 2, 2, 1, 3, 3, 1, 0, 0},
};

typedef struct {
    uint32_t seed;               /* what made it: rus_design(kind, seed, path, gen) + rus_measure give it again */
    uint8_t kind, path, gen;
    uint8_t bits;                /* the bit depth it is reduced to, 3 .. 10 */
    uint8_t hold;                /* its own samples held 1 .. 8 times more */
    uint8_t chord;               /* CHORD's: 0 minor, 1 major, 2 open fifths, 3 sus */
    uint8_t rsv[2];
    int32_t p1, p2;              /* its two parameters, Q15 */
    int32_t chaos;               /* how much chance drives it, Q15 */
    int32_t att, rel;            /* the envelope, ms */
    int32_t out;                 /* Q12: the measured level brought to the target */
} rus_design_t;

typedef struct {
    rnd_t r;
    uint8_t live, nheld, sub, held_n;
    uint8_t note[8];
    int8_t mac[NMAC];            /* EDIT's macros (layers.c sets them each block) */
    int32_t env;                 /* Q23 */
    int32_t base;                /* the pitch now, 1/16 semitone */
    uint32_t inc;                /* the key's frequency, a phase step at FS / 3 */
    int32_t y, last;             /* its sample now (10-bit), the one before (for the hold between) */
    uint32_t ph[3], clk[2];
    int32_t a, b, c, lp;         /* the kinds' state */
    uint32_t cnt, cnt2, rpos, rwin, held_bits;
    uint8_t bitpos, bitmode, quad, ratio_ix, built, walk_ix, hold_left, trig;   /* (trig: a key from silence) */
    int32_t built_p1;            /* the density its buffer was built with (DENSITY turned: rebuilt, a part a block) */
    uint32_t rebuild, stutter;   /* where the rebuild is; samples a stutter still holds */
    int32_t walk;
    int8_t buf[RUS_BUF];
} rus_t;

static int32_t rus_noise(rnd_t *r) { return (int32_t)(rnd_next(r) & 0x3FFu) - 512; }
static int32_t rus_tri(uint32_t ph)              /* -512 .. 511, from the phase's top 10 bits */
{
    uint32_t p = ph >> 22;
    return p < 512u ? (int32_t)(p << 1) - 512 : 1535 - (int32_t)(p << 1);
}
static int32_t rus_saw(uint32_t ph) { return (int32_t)(ph >> 22) - 512; }
/* a frequency in Hz Q8 as a phase step at FS / 3 */
static uint32_t rus_hz(int32_t hz8) { return (uint32_t)(((uint64_t)(uint32_t)hz8 * 292200u) >> 8); }   /* 2^32 / 14700 */
/* x in the middle of [lo, hi] by q (Q15) */
static int32_t rus_lerp(int32_t lo, int32_t hi, int32_t q) { return lo + mulq15(hi - lo, q); }
/* an event, on average hz8 (Hz Q8) times a second x CHAOS (Q15), at this sample (of FS / 3) */
static int rus_chance(rus_t *c, int32_t hz8, int32_t chaos)
{
    uint32_t p20 = (uint32_t)mulq15((int32_t)((uint32_t)hz8 * 285u >> 10), chaos);   /* hz / 14700, Q20 */
    return (rnd_next(&c->r) & 0xFFFFFu) < p20;
}
/* a step x k (k small), saturating */
static uint32_t rus_mul(uint32_t inc, uint32_t k)
{
    uint64_t x = (uint64_t)inc * k;
    return x > 0xF0000000u ? 0xF0000000u : (uint32_t)x;
}

/* ------------------------------------------------------------- buffers --- */
/* a part of the buffer [from, to) rebuilt: chunks of latched noise, saw or silence (density: the share of sound);
 * now and then a short triangle blip */
static void rus_fill(rus_t *c, uint32_t from, uint32_t to, int32_t density, uint32_t saws)
{
    static const uint16_t CH[3] = {8, 32, 64};
    uint32_t w = from;
    while (w < to) {
        uint32_t n = CH[rnd_below(&c->r, 3)], i;
        int sound = rnd_q15(&c->r) < density;
        if (w + n > to)
            n = to - w;
        if (!sound)
            for (i = 0; i < n; i++)
                c->buf[w + i] = 0;
        else if (saws && rnd_chance(&c->r, 40)) {
            uint32_t ph = 0, inc = rus_hz(rnd_in(&c->r, 40 * 256, 800 * 256));
            for (i = 0; i < n; i++, ph += inc)
                c->buf[w + i] = (int8_t)(rus_saw(ph) >> 2);
        } else {
            int32_t v = rus_noise(&c->r);
            for (i = 0; i < n; i++) {
                if (rnd_below(&c->r, 10) < 3)
                    v = rus_noise(&c->r);
                c->buf[w + i] = (int8_t)(v >> 2);
            }
        }
        w += n;
    }
    if (to - from > 64u && rnd_chance(&c->r, 60)) {       /* a blip */
        uint32_t at = from + (uint32_t)rnd_below(&c->r, (int32_t)(to - from - 48u)), i, ph = 0;
        uint32_t inc = rus_hz(rnd_in(&c->r, 300 * 256, 3000 * 256));
        for (i = 0; i < 48u; i++, ph += inc)
            c->buf[at + i] = (int8_t)(rus_tri(ph) >> 2);
    }
}

/* ------------------------------------------------------------- the voice --- */
static void rus_init(rus_t *c, uint32_t seed)
{
    memset(c, 0, sizeof *c);
    rnd_seed(&c->r, seed);
}
static void rus_note_on(rus_t *c, const rus_design_t *d, uint32_t note)
{
    uint32_t k;
    int fresh = !c->nheld;
    for (k = 0; k < c->nheld; k++)                        /* (already held: to the top) */
        if (c->note[k] == note) {
            for (; k + 1u < c->nheld; k++)
                c->note[k] = c->note[k + 1u];
            c->nheld--;
            break;
        }
    if (c->nheld == 8u) {
        for (k = 0; k + 1u < 8u; k++)
            c->note[k] = c->note[k + 1u];
        c->nheld--;
    }
    c->note[c->nheld++] = (uint8_t)note;
    c->base = (int32_t)note * 16;
    c->live = 1;
    if (fresh)
        c->trig = 1;                                      /* (the tones: a key fires one at once) */
    if (fresh && !c->built) {                             /* its buffer, the first time it sounds */
        rus_fill(c, 0, RUS_BUF, rus_lerp(LV(0.15), LV(0.9), d->p1), d->kind == RK_CHUNKS || d->kind == RK_BEND);
        c->built = 1;
        c->built_p1 = d->p1;
    }
}
static void rus_note_off(rus_t *c, uint32_t note)
{
    uint32_t k;
    for (k = 0; k < c->nheld; k++)
        if (c->note[k] == note) {
            for (; k + 1u < c->nheld; k++)
                c->note[k] = c->note[k + 1u];
            c->nheld--;
            if (c->nheld)
                c->base = (int32_t)c->note[c->nheld - 1u] * 16;
            return;
        }
}
static void rus_release(rus_t *c) { c->nheld = 0; }

/* one of its own samples (10-bit) */
static int32_t rus_step(rus_t *c, const rus_design_t *d, int32_t p1, int32_t p2, int32_t chaos)
{
    uint32_t inc = c->inc;
    switch (d->kind) {
    case RK_HOLD: {                                       /* sample-and-hold: the key's rate x 1/4 .. 4 */
        uint32_t r = (uint32_t)(((uint64_t)inc * (uint32_t)exp2_q16(mulq15(p1, R16(4.0)) - R16(2.0))) >> 16);
        int32_t prob = rus_lerp(LV(0.05), Q15, p2);
        if (chaos < Q15)
            prob = mulq15(prob, chaos) + LV(0.05);         /* (CHAOS down: it changes less often) */
        c->ph[0] += r;
        if (c->ph[0] < r && rnd_q15(&c->r) < prob)
            c->y = rus_noise(&c->r);
        return c->y;
    }
    case RK_DUST: {                                       /* clicks through a low-pass on the key x 1 .. 16 */
        int32_t x = 0;
        if (c->trig || rus_chance(c, rus_lerp(8 * 256, 900 * 256, mulq15(p2, p2)), chaos)) {
            c->trig = 0;
            x = rus_noise(&c->r);
        }
        int32_t a = clampi((int32_t)(((uint64_t)inc * (uint32_t)exp2_q16(mulq15(p1, R16(4.0)))) >> 30), 200, Q15);
        c->lp += mulq15(x - c->lp, a);
        return c->lp * 3;
    }
    case RK_GATES: {                                      /* noise through two triangles' gates, one wandering */
        int32_t thr = rus_lerp(300, 500, p1), n = rus_noise(&c->r), g = 0;
        if (++c->cnt >= 1838u) {                          /* (8 times a second) */
            c->cnt = 0;
            c->walk = clampi(c->walk + (rnd_next(&c->r) & 1u ? 1 : -1) * mulq15(mulq15(p2, chaos), (int32_t)(inc >> 6)),
                             -(int32_t)(inc >> 1), (int32_t)(inc >> 1));
        }
        c->ph[0] += inc;
        c->ph[1] += (uint32_t)((int32_t)inc + c->walk);
        g += rus_tri(c->ph[0]) > thr ? n : 0;
        g += rus_tri(c->ph[1]) > thr ? n : 0;
        return g;
    }
    case RK_CRACKLE: {                                    /* clicks ringing on short decays at the key's harmonics */
        if (rus_chance(c, rus_lerp(1 * 256, 60 * 256, p2), chaos)) {
            c->a = rus_noise(&c->r) * 2;                  /* the ring's level */
            c->ph[1] = 0;
            c->clk[0] = rus_mul(inc, (uint32_t)rnd_in(&c->r, 1, 8));
        }
        c->ph[1] += c->clk[0];
        c->a -= c->a >> 7;
        return mulq15(rus_tri(c->ph[1]), c->a << 5) + mulq15(rus_noise(&c->r), mulq15(p1, LV(0.25)));
    }
    case RK_NOR: case RK_XOR: case RK_RING: {             /* two oscillators, combined */
        uint32_t i2 = (uint32_t)(((uint64_t)inc * (uint32_t)(R16(1.0) + mulq15(p1, R16(3.0)))) >> 16);
        int32_t a, b;
        if (++c->cnt >= 3675u) {                          /* (a drift, 4 times a second) */
            c->cnt = 0;
            c->walk = clampi(c->walk + (rnd_next(&c->r) & 1u ? 1 : -1) * mulq15(mulq15(p2, chaos), (int32_t)(inc >> 7)),
                             -(int32_t)(inc >> 3), (int32_t)(inc >> 3));
        }
        c->ph[0] += inc;
        c->ph[1] += (uint32_t)((int32_t)i2 + c->walk);
        if (d->kind == RK_NOR) {                          /* saws: ~(a | b), clipped at the top */
            a = rus_saw(c->ph[0]) + 512;
            b = rus_saw(c->ph[1]) + 512;
            return clampi((int32_t)(~(uint32_t)(a | b) & 0x3FFu), 0, 511) * 2 - 511;
        }
        a = rus_tri(c->ph[0]);
        b = rus_tri(c->ph[1]);
        return d->kind == RK_XOR ? (a ^ b) : (a * b) >> 9;
    }
    case RK_CASCADE: {                                    /* squares modulating squares */
        uint32_t i2 = (uint32_t)(((uint64_t)inc * (uint32_t)(R16(1.0) + mulq15(p1, R16(7.0)))) >> 16);
        uint32_t i3 = (uint32_t)(((uint64_t)inc * (uint32_t)(R16(0.0625) + mulq15(p2, R16(0.4)))) >> 16);
        int32_t y;
        c->ph[0] += inc;
        c->ph[1] += i2;
        c->ph[2] += i3;
        y = c->ph[0] >> 31 ? 511 : -512;                  /* the square, halved by the second, quartered by the third */
        if (!(c->ph[1] >> 31))
            y >>= 1;
        if (!(c->ph[2] >> 31))
            y >>= 2;
        return y;
    }
    case RK_CLANG: {                                      /* triangles at an inharmonic ratio, XOR'd; redrawn on a clock */
        static const int32_t RT[8] = {R16(1.41), R16(1.618), R16(1.73), R16(2.11), R16(2.37), R16(2.81), R16(3.14), R16(4.0)};
        uint32_t ck = rus_hz(rus_lerp(51, 12 * 256, mulq15(p2, chaos > Q15 ? Q15 : chaos))), o1 = c->clk[0];
        int32_t ratio = R16(1.0) + mulq16(RT[c->ratio_ix & 7u] - R16(1.0), R16(0.3) + mulq15(p1, R16(1.0)));
        c->clk[0] += ck;
        c->clk[1] += (uint32_t)(((uint64_t)ck * (uint32_t)R16(2.37)) >> 16);
        if (!(o1 >> 31) && (c->clk[0] >> 31) && (c->clk[1] >> 31))
            c->ratio_ix = (uint8_t)rnd_below(&c->r, 8);
        c->ph[0] += inc;
        c->ph[1] += (uint32_t)(((uint64_t)inc * (uint32_t)ratio) >> 16);
        return rus_tri(c->ph[0]) ^ rus_tri(c->ph[1]);
    }
    case RK_CHUNKS: case RK_JUMP: case RK_WANDER: case RK_BEND: case RK_QUAD: {   /* the buffer, read at the key's speed */
        uint32_t spd = inc >> 9;                          /* (A2, 110 Hz: about one buffer sample a sample) */
        uint32_t a, mask;
        c->rpos += spd;
        a = (c->rpos >> 16) & (RUS_BUF - 1u);
        if (++c->cnt2 >= 7350u) {                         /* every half second: a part rebuilt, or not */
            c->cnt2 = 0;
            if (d->kind == RK_CHUNKS ? rnd_q15(&c->r) < mulq15(p2, chaos > Q15 ? Q15 : chaos) : rnd_q15(&c->r) < LV(0.2)) {
                uint32_t at = (uint32_t)rnd_below(&c->r, RUS_BUF / 512u) * 512u;
                rus_fill(c, at, at + 512u, rus_lerp(LV(0.15), LV(0.9), d->kind == RK_JUMP ? Q15 - p1 : p1), 1);
            }
        }
        switch (d->kind) {
        case RK_JUMP:                                     /* jumps at random */
            if (rus_chance(c, rus_lerp(26, 30 * 256, p2), chaos))
                c->rpos = (uint32_t)rnd_below(&c->r, RUS_BUF) << 16;
            break;
        case RK_WANDER:                                   /* a window wandering */
            if (rus_chance(c, rus_lerp(3, 50 * 256, mulq15(p2, p2)), chaos))
                c->rwin = (c->rwin + (uint32_t)(rnd_next(&c->r) & 1u ? 20 : RUS_BUF - 20u)) & (RUS_BUF - 1u);
            a = (c->rwin + (a % (64u + (uint32_t)mulq15(p1, 2000)))) & (RUS_BUF - 1u);
            break;
        case RK_BEND:                                     /* an address bit bent, the bit and the how on a clock */
            if (rus_chance(c, rus_lerp(128, 20 * 256, p2), chaos)) {
                if (rnd_next(&c->r) & 1u)
                    c->bitmode = (uint8_t)rnd_below(&c->r, 4);
                c->bitpos = (uint8_t)((c->bitpos + (rnd_next(&c->r) & 1u ? 1u : 11u)) % 12u);
                if (c->bitmode == 3u)
                    c->held_bits = a;
            }
            mask = (1u << c->bitpos) - 1u;
            a = c->bitmode == 0u ? a & ~mask : c->bitmode == 1u ? a | mask : c->bitmode == 2u ? a ^ mask
                                                                           : (a & ~mask) | (c->held_bits & mask);
            a &= RUS_BUF - 1u;
            break;
        case RK_QUAD:                                     /* four bendings at once, each on or off by chance */
            if (rus_chance(c, rus_lerp(128, 25 * 256, p2), chaos)) {
                c->quad = (uint8_t)(rnd_next(&c->r) & 15u);
                c->held_bits = a;
            }
            if (c->quad & 1u)
                a ^= 3u;
            if (c->quad & 2u)
                a = (a & ~12u) | (c->held_bits & 12u);
            if (c->quad & 4u)
                a &= ~0x70u;
            if (c->quad & 8u)
                a |= 0x380u;
            a &= RUS_BUF - 1u;
            break;
        default:
            break;
        }
        return c->buf[a] * 4;
    }
    case RK_BLIPS: {                                      /* triangles fired by chance on the key's harmonics */
        static const uint8_t H[8] = {2, 4, 6, 3, 8, 5, 10, 12};   /* (halves of the key) */
        if (c->trig || rus_chance(c, rus_lerp(2 * 256, 30 * 256, p2), chaos)) {
            c->trig = 0;
            c->a = 511 << 6;
            c->clk[0] = rus_mul(inc, H[rnd_below(&c->r, 2 + mulq15(p1, 6))]) >> 1;
        }
        c->ph[0] += c->clk[0];
        c->a -= c->a >> 9;
        return (rus_tri(c->ph[0]) * (c->a >> 6)) >> 9;
    }
    case RK_WALK: {                                       /* a random walk over a scale's steps, from the key */
        static const int8_t ST[8] = {0, 1, 3, 5, 7, 8, 10, 12};   /* (phrygian: dark) */
        if (c->trig || rus_chance(c, rus_lerp(128, 12 * 256, p2), chaos)) {
            int32_t step = 1 + mulq15(p1, 3);
            c->trig = 0;
            c->walk_ix = (uint8_t)clampi((int32_t)c->walk_ix + (rnd_next(&c->r) & 1u ? step : -step), 0, 15);
            c->a = 511 << 6;
        }
        {
            int32_t oct = c->walk_ix >> 3, s = ST[c->walk_ix & 7u] + 12 * oct - 7;
            c->ph[0] += (uint32_t)(((uint64_t)inc * exp2_q16(s * (R16(1.0) / 12))) >> 16);
        }
        c->a -= c->a >> 11;
        return (rus_tri(c->ph[0]) * (c->a >> 6)) >> 9;
    }
    default: {                                            /* RK_CHORD: triangles on the key, beating, swelling */
        static const int8_t CH[4][3] = {{0, 3, 7}, {0, 4, 7}, {0, 7, 12}, {0, 5, 7}};
        uint32_t k;
        int32_t s = 0;
        for (k = 0; k < 3u; k++) {
            c->ph[k] += (uint32_t)(((uint64_t)inc * exp2_q16(CH[d->chord & 3u][k] * (R16(1.0) / 12) + (int32_t)k * 40)) >> 16);
            s += rus_tri(c->ph[k]);
        }
        c->clk[0] += rus_hz(rus_lerp(26, 6 * 256, p2));
        return mulq15(s / 3, Q15 - mulq15(p1, (rus_tri(c->clk[0]) + 512) << 5));
    }
    }
}

/* one block (CTL samples), Q15 into out; 0 when it has gone quiet for good */
static int rus_render(rus_t *c, const rus_design_t *d, int32_t *out)
{
    uint32_t i;
    int32_t p1 = clampi(d->p1 + c->mac[0] * (Q15 / MAC_MAX), 0, Q15), p2 = clampi(d->p2 + c->mac[1] * (Q15 / MAC_MAX), 0, Q15);
    int32_t chaos = clampi(d->chaos + c->mac[3] * (Q15 / MAC_MAX), LV(0.1), 2 * Q15);   /* (never none: it thins, not stops) */
    int32_t bits = clampi(d->bits - c->mac[2] * 6 / MAC_MAX, 4, 10), hold = clampi(d->hold + c->mac[2] * 6 / MAC_MAX, 1, 12);
    if ((d->kind == RK_CHUNKS || d->kind == RK_JUMP || d->kind == RK_BEND || d->kind == RK_QUAD) && c->built &&
        (c->rebuild || absi(p1 - c->built_p1) > LV(0.06))) {   /* DENSITY turned: the buffer rebuilt, a part a block */
        int32_t dens = d->kind == RK_JUMP ? Q15 - p1 : p1;
        rus_fill(c, c->rebuild, c->rebuild + 512u, rus_lerp(LV(0.15), LV(0.9), dens), 1);
        c->rebuild = (c->rebuild + 512u) & (RUS_BUF - 1u);
        if (!c->rebuild)
            c->built_p1 = p1;
    }
    int32_t e_t = c->nheld ? Q15 << 8 : 0, k, e1, de;
    if (!c->live)
        return 0;
    c->inc = pitch_inc(c->base) * RUS_SUB;
    k = e_t > c->env ? cal_ex_fp(d->att) : cal_ex_fp(d->rel);
    e1 = e_t + mulq16(c->env - e_t, k);
    if (absi(e1 - e_t) < 32 << 8)
        e1 = e_t;
    de = (e1 - c->env) / (int32_t)CTL;
    for (i = 0; i < CTL; i++) {
        int32_t y;
        if (++c->sub >= RUS_SUB) {                       /* its own sample (it goes on, held or not) */
            int32_t x = clampi(rus_step(c, d, p1, p2, chaos), -512, 511);
            c->sub = 0;
            if (c->stutter)                              /* CHAOS above the middle: now and then a stutter */
                c->stutter--;
            else if (c->hold_left)                       /* CRUSH: the output held, the engine running on */
                c->hold_left--;
            else {
                int32_t m = (int32_t)(0x3FFu >> (10 - bits)) << (10 - bits);
                if (chaos > Q15 / 2 && rus_chance(c, mulq15(chaos - Q15 / 2, 8 * 256), Q15))
                    c->stutter = (uint32_t)rnd_in(&c->r, 150, 2500);
                c->last = (int32_t)((uint32_t)(x + 512) & (uint32_t)m) - 512;   /* (the bits kept) */
                c->hold_left = (uint8_t)(hold - 1);
            }
        }
        c->env += de;
        y = mulq15(c->last << 6, c->env >> 8);
        out[i] = mulq12(y, d->out);
    }
    c->env = e1;
    if (!c->nheld && !e1) {
        c->live = 0;
        return 0;
    }
    return 1;
}

/* ------------------------------------------------------------- designs --- */
static void rus_design(rus_design_t *d, int32_t kind, uint32_t seed, uint32_t path, uint32_t gen)
{
    rnd_t rr, *r = &rr;
    path = path < PATH_N ? path : PATH_OPEN;
    rnd_seed(r, seed ^ 0x8055u);
    {   /* (the family drawn always, a family given or not: the seed and the family then make the sound again, however
         * the family was come to -- SHIFT + PRESETS, a preset, a session) */
        int32_t drawn = (int32_t)rnd_weighted(r, RK_W[path], RK_N);
        if (kind < 0 || kind >= RK_N)
            kind = drawn;
    }
    memset(d, 0, sizeof *d);
    d->seed = seed;
    d->kind = (uint8_t)kind;
    d->path = (uint8_t)path;
    d->gen = (uint8_t)gen;
    d->p1 = rnd_q15(r);
    d->p2 = rnd_q15(r);
    d->chaos = rnd_in(r, LV(0.5), Q15);
    d->chord = (uint8_t)rnd_below(r, 4);
    if (path == PATH_AMBIENT) {                           /* clean, slow to come and go, calm */
        d->bits = (uint8_t)rnd_in(r, 9, 10);
        d->hold = 1;
        d->att = rnd_in(r, 200, 1500);
        d->rel = rnd_in(r, 1500, 5000);
        d->p2 = mulq15(d->p2, LV(0.6));
        d->chaos = mulq15(d->chaos, LV(0.7));
        if (d->chord == 1u)
            d->chord = 0;                                 /* (no major chords) */
    } else if (path == PATH_EXP) {                        /* crushed, abrupt */
        d->bits = (uint8_t)rnd_in(r, 4, 9);
        d->hold = (uint8_t)rnd_in(r, 1, 6);
        d->att = rnd_in(r, 2, 60);
        d->rel = rnd_in(r, 40, 600);
        d->chaos = rnd_in(r, LV(0.8), 2 * Q15);
    } else {
        d->bits = (uint8_t)rnd_in(r, 6, 10);
        d->hold = (uint8_t)rnd_in(r, 1, 3);
        d->att = rnd_in(r, 5, 600);
        d->rel = rnd_in(r, 150, 2500);
    }
    d->out = IX(1.0);
}

static const char *rus_mac_name(const rus_design_t *d, uint32_t j)
{
    return j < 2u ? RK_P[d->kind % RK_N][j] : j == 2u ? "CRUSH" : "RANDOMNESS";
}

/* the design's level from how loud it usually is: C3 held 1.5 s, the 75th percentile of 12 ms windows brought to
 * 0.1 RMS (as cal_measure) -- but no more than keeps its loudest click under half of full scale, and never above
 * x1 (dust and crackle are mostly silence: by their usual level alone they would be raised until their clicks
 * broke; at x1 nothing it makes can) */
static rus_t rus_probe;
static void rus_measure(rus_design_t *d)
{
    static int32_t win[2067u / 16u + 1u];
    int32_t out[CTL];
    uint32_t b, k, nwin = 0;
    int64_t acc = 0;
    int32_t peak = 1;
    d->out = IX(1.0);
    rus_init(&rus_probe, d->seed ^ 0x77u);
    rus_note_on(&rus_probe, d, 48);
    for (b = 0; b < 2067u; b++) {
        uint32_t i;
        rus_render(&rus_probe, d, out);
        for (i = 0; i < CTL; i++) {
            acc += ((int64_t)out[i] * out[i]) >> 6;
            if (absi(out[i]) > peak)
                peak = absi(out[i]);
        }
        if ((b & 15u) == 15u) {
            if (nwin < NELEM(win))
                win[nwin++] = (int32_t)((acc >> 9) > 0x7FFFFFFF ? 0x7FFFFFFF : (acc >> 9));
            acc = 0;
        }
#ifndef HOST
        if ((b & 255u) == 255u)
            fm1_wdt_feed();
#endif
    }
    for (b = 1; b < nwin; b++)
        for (k = b; k > 0 && win[k - 1] > win[k]; k--) {
            int32_t t = win[k];
            win[k] = win[k - 1];
            win[k - 1] = t;
        }
    {
        uint32_t root = nwin ? isqrt32((uint32_t)win[nwin * 3u / 4u]) : 0;
        int32_t g = root ? (int32_t)(1677722u / root) : 4096 * 8;
        int32_t gp = (int32_t)((uint32_t)(Q15 / 2) * 4096u / (uint32_t)peak);   /* (the loudest at -6 dBFS) */
        d->out = clampi(g < gp ? g : gp, 300, 4096);       /* (at most x1: its 10 bits never reach full scale) */
    }
}
