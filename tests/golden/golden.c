/* SPDX-License-Identifier: GPL-3.0-only
 * TONITOR: the sounds of each past generation of the generators -- 0 (up to 0.20), 1 (0.21), 2 (0.22, and RUSSOLO from 0.26), 3 (0.29) -- fingerprinted, so
 * that every later firmware draws them again bit for bit from their seeds: the sessions and presets that keep only
 * seeds depend on it. `golden G` prints one line per (engine, kind, path): an FNV-1a hash over many seeds of the
 * design's fields as generation G had them. Built with the host harness (tests/run_tests.sh). */
#define main host_main
#include "../host/host.c"
#undef main

static uint32_t fnv(uint32_t h, const void *p, uint32_t n)
{
    const uint8_t *b = p;
    while (n--)
        h = (h ^ *b++) * 16777619u;
    return h;
}
#define GEN0_FM(p, k, s, pa) fm_generate(p, k, s, pa, gen)
#define GEN0_AN(d, k, s, pa) an_design(d, k, s, pa, gen)
#define GEN0_CAL(d, k, s, pa) cal_design(d, k, s, pa, gen)
#define GEN0_AN_BYTES 380u       /* an_design_t and cal_design_t as generation 0 laid them out: later fields go after */
#define GEN0_CAL_BYTES 200u

int main(int argc, char **argv)
{
    uint32_t gen = argc > 1 ? (uint32_t)atoi(argv[1]) : 0u;
    /* each generation's kinds, and the bytes of its designs (later fields go after them) */
    int32_t nf = gen >= 6u ? FF_N : gen >= 3u ? FF_N5 : gen ? 17 : 9, na = gen >= 3u ? AK_N : gen ? 14 : 8, nc = gen ? 10 : 6;
    uint32_t anb = gen ? 384u : GEN0_AN_BYTES, calb = gen ? 212u : GEN0_CAL_BYTES;
    static fm_patch_t fp;
    static an_design_t ad;
    static cal_design_t cd;
    int32_t k;
    uint32_t pa, s;
    for (pa = 0; pa < PATH_N; pa++) {
        for (k = -1; k < nf; k++) {
            uint32_t h = 2166136261u;
            for (s = 0; s < (k < 0 ? 400u : 40u); s++) {
                memset(&fp, 0, sizeof fp);
                GEN0_FM(&fp, k, 1234u + s * 7919u, pa);
                h = fnv(h, fp.p, sizeof fp.p);
                h = fnv(h, &fp.family, 1);
                h = fnv(h, &fp.seed, 4);
            }
            printf("fm %d %u %08x\n", k, pa, h);
        }
        for (k = -1; k < na; k++) {
            uint32_t h = 2166136261u;
            for (s = 0; s < (k < 0 ? 400u : 40u); s++) {
                GEN0_AN(&ad, k, 1234u + s * 7919u, pa);
                h = fnv(h, &ad, anb);
            }
            printf("an %d %u %08x\n", k, pa, h);
        }
        for (k = -1; k < nc; k++) {
            uint32_t h = 2166136261u;
            for (s = 0; s < (k < 0 ? 400u : 40u); s++) {
                GEN0_CAL(&cd, k, 1234u + s * 7919u, pa);
                h = fnv(h, &cd, calb);
            }
            printf("cal %d %u %08x\n", k, pa, h);
        }
    }
    if (gen >= 2)                /* RUSSOLO came with generation 2: its lines go after, the earlier ones unchanged */
        for (pa = 0; pa < PATH_N; pa++)
            for (k = -1; k < RK_N; k++) {
                static rus_design_t rd;
                uint32_t h = 2166136261u;
                for (s = 0; s < (k < 0 ? 400u : 40u); s++) {
                    memset(&rd, 0, sizeof rd);
                    rus_design(&rd, k, 1234u + s * 7919u, pa, gen);
                    if (s < 4u)
                        rus_measure(&rd);                  /* (and the level a few of them are given) */
                    h = fnv(h, &rd, sizeof rd);
                }
                printf("rus %d %u %08x\n", k, pa, h);
            }
    if (gen >= 6)                /* OBRADOR came with generation 6's builds (62): its lines go after */
        for (pa = 0; pa < PATH_N; pa++)
            for (k = -1; k < OBK_N; k++) {
                static ob_design_t od;
                uint32_t h = 2166136261u;
                for (s = 0; s < (k < 0 ? 400u : 40u); s++) {
                    memset(&od, 0, sizeof od);
                    ob_design(&od, k, 1234u + s * 7919u, pa, gen);
                    if (s < 4u)
                        ob_measure(&od);                   /* (and the level a few of them are given) */
                    h = fnv(h, &od, sizeof od);
                }
                printf("ob %d %u %08x\n", k, pa, h);
            }
    return 0;
}
