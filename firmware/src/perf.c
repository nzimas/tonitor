/* SPDX-License-Identifier: GPL-3.0-only */
/* PERF FX, the performance effects (ui.c): black key 2 held, the effects drawn anew at random -- each track's filter, granulator,
 * crush, distortion and sends, and the master reverb and delay -- and let go, back as they were. The settings
 * themselves (ui.flt .. ui.dlp) never change, so the knobs, the snapshots and the sessions keep them: what the
 * ISR is given is each moved from its setting towards its draw by MIX percent, gliding in and out over
 * PF_GLIDE ms. A track's draws lean to its generative path, the master's to the chosen track's; each value is
 * drawn or left as it is by chance, so that not everything moves at once. SHIFT + black key 2: the PERF FX
 * page (ui.fx 2 on the REVERB page's rows): MIX, TRACKS (CUR, the current track, or ALL), and whether it moves the TRACK
 * effects and the MASTER's. */
enum { PF_FLT, PF_GR, PF_CR, PF_DIST, PF_SEND, PF_DSEND, PF_NT };
enum { PF_MIX, PF_ALL, PF_TRK, PF_MST };
#define PF_NM 8                  /* the master's: the reverb SIZE DECAY DAMP MIX, the delay TIME REPEATS DAMP MIX */
#define PF_GLIDE 50u
static const int8_t PF_DEF[4] = {50, 0, 1, 1};
static const char *const PF_NAME[4] = {"MIX", "TRACKS", "TRACK", "MASTER"};   /* (TRACKS CUR, the current track, or ALL; TRACK, MASTER: their
                                                                                * effects ON or OFF) */
/* the ranges drawn in, by path (OPEN, AMBIENT, EXPERIMENTAL): AMBIENT darker, longer, wetter; EXPERIMENTAL
 * crushed, driven, scattered, its echoes short and bending */
static const int8_t PF_TR[PATH_N][PF_NT][2] = {
    {{-FLT_MAX, FLT_MAX}, {-FLT_MAX, FLT_MAX}, {-FLT_MAX, FLT_MAX}, {0, 100}, {0, 100}, {0, 100}},
    {{-FLT_MAX, FLT_MAX / 8}, {-FLT_MAX, 0}, {-FLT_MAX / 6, FLT_MAX / 6}, {0, 20}, {50, 100}, {30, 90}},
    {{-FLT_MAX, FLT_MAX}, {0, FLT_MAX}, {-FLT_MAX, FLT_MAX}, {40, 100}, {0, 70}, {20, 100}},
};
static const int8_t PF_MR[PATH_N][PF_NM][2] = {
    {{0, 100}, {20, 90}, {0, 100}, {20, 90}, {10, 90}, {10, 85}, {0, 100}, {20, 90}},
    {{60, 100}, {60, 95}, {30, 80}, {50, 100}, {55, 95}, {40, 85}, {40, 90}, {30, 80}},
    {{0, 100}, {20, 85}, {0, 40}, {20, 80}, {0, 55}, {50, 92}, {0, 40}, {40, 100}},
};
static const uint8_t PF_P[PATH_N] = {60, 55, 70};   /* the chance each value is drawn, percent */
static struct {
    int8_t set[4];               /* the page: MIX 0 .. 100, TRACKS (1: all), TRACK, MASTER (1: on) */
    uint8_t held;                /* the key down (since it drew) */
    uint8_t mask, mst;           /* the tracks it moves, and the master (as the key went down) */
    uint16_t amt;                /* how far in, 0 .. 256 (the glide) */
    uint32_t at;                 /* (the glide's last step) */
    int8_t t[NLAYER][PF_NT];     /* each track's draws */
    int8_t m[PF_NM];             /* the master's */
} pf;

/* the four values the REVERB page's rows show: the reverb's, the delay's, or PERF FX's (ui.fx 2) */
static int8_t *rv_vals(void) { return ui.fx == 2u ? pf.set : ui.fx ? ui.dlp : ui.rvp; }
static int32_t pf_mix(int32_t u, int32_t t) { return u + (t - u) * pf.set[PF_MIX] * (int32_t)pf.amt / 25600; }
/* track l's value k as the ISR hears it, its setting u */
static int32_t pf_trk(uint32_t l, uint32_t k, int32_t u)
{
    return pf.amt && (pf.mask >> l & 1u) ? pf_mix(u, pf.t[l][k]) : u;
}
static int32_t pf_mst(uint32_t k, int32_t u) { return pf.amt && pf.mst ? pf_mix(u, pf.m[k]) : u; }
static void rv_apply(void) { rv_param(pf_mst(0, ui.rvp[0]), pf_mst(1, ui.rvp[1]), pf_mst(2, ui.rvp[2]), pf_mst(3, ui.rvp[3])); }
static void dl_apply(void) { dl_param(pf_mst(4, ui.dlp[0]), pf_mst(5, ui.dlp[1]), pf_mst(6, ui.dlp[2]), pf_mst(7, ui.dlp[3])); }
static void pf_defaults(void) { memcpy(pf.set, PF_DEF, sizeof pf.set); }
static void pf_check(void)
{
    pf.set[PF_MIX] = (int8_t)clampi(pf.set[PF_MIX], 0, 100);
    pf.set[PF_ALL] = (int8_t)!!pf.set[PF_ALL];
    pf.set[PF_TRK] = (int8_t)!!pf.set[PF_TRK];
    pf.set[PF_MST] = (int8_t)!!pf.set[PF_MST];
}

/* the key down: new draws, from the settings as they are (a value left out is drawn as its setting) */
static void pf_draw(uint32_t sel, uint32_t seed)
{
    rnd_t r;
    uint32_t l, k, n;
    rnd_seed(&r, seed);
    pf.mask = (uint8_t)(!pf.set[PF_TRK] ? 0u : pf.set[PF_ALL] ? (1u << NLAYER) - 1u : 1u << sel);
    pf.mst = (uint8_t)pf.set[PF_MST];
    for (l = 0; l < NLAYER; l++) {
        uint32_t pa = ui.path[l] < PATH_N ? ui.path[l] : PATH_OPEN;
        const int8_t u[PF_NT] = {ui.flt[l], ui.gr[l], ui.cr[l], ui.dist[l], ui.send[l], ui.dsend[l]};
        for (k = n = 0; k < PF_NT; k++) {
            int d = rnd_chance(&r, PF_P[pa]);
            pf.t[l][k] = (int8_t)(d ? rnd_in(&r, PF_TR[pa][k][0], PF_TR[pa][k][1]) : u[k]);
            n += (uint32_t)d;
        }
        if (!n)                                  /* (none drawn: at least the filter) */
            pf.t[l][PF_FLT] = (int8_t)rnd_in(&r, PF_TR[pa][PF_FLT][0], PF_TR[pa][PF_FLT][1]);
    }
    {
        uint32_t pa = ui.path[sel] < PATH_N ? ui.path[sel] : PATH_OPEN;
        for (k = 0; k < PF_NM; k++)
            pf.m[k] = (int8_t)(rnd_chance(&r, PF_P[pa]) ? rnd_in(&r, PF_MR[pa][k][0], PF_MR[pa][k][1])
                                                        : (k < 4u ? ui.rvp[k] : ui.dlp[k - 4u]));
    }
}
static void level_pan_apply(uint32_t l);
/* the main loop, each time: the glide in or out, a step on */
static void pf_tick(void)
{
    uint32_t want = pf.held ? 256u : 0u, step, l;
    if (pf.amt == want)
        return;
    step = (uint32_t)(fm1_ms - pf.at) * 256u / PF_GLIDE;
    if (!step)
        return;
    pf.at = fm1_ms;
    pf.amt = (uint16_t)(want > pf.amt ? (pf.amt + step >= want ? want : pf.amt + step)
                                      : (pf.amt <= step ? 0u : pf.amt - step));
    for (l = 0; l < NLAYER; l++)
        if (pf.mask >> l & 1u)
            level_pan_apply(l);
    if (pf.mst) {
        rv_apply();
        dl_apply();
    }
}
static void pf_on(uint32_t sel, uint32_t seed)
{
    if (!pf.set[PF_TRK] && !pf.set[PF_MST]) {
        say("FX OFF", 900);                      /* (both off on its page: nothing to move) */
        return;
    }
    if (pf.amt) {                                /* (still gliding out: back to the settings first) */
        uint32_t l;
        pf.amt = 0;
        for (l = 0; l < NLAYER; l++)
            if (pf.mask >> l & 1u)
                level_pan_apply(l);
        if (pf.mst) {
            rv_apply();
            dl_apply();
        }
    }
    pf_draw(sel, seed);
    pf.held = 1;
    pf.at = fm1_ms;
    say("PERF FX", 900);
}
