/* SPDX-License-Identifier: GPL-3.0-only
 * TONITOR: the sounds of generation 0 (the generators as they were up to 0.20), fingerprinted, so that every
 * later firmware draws them again bit for bit from their seeds -- the sessions and presets that keep only seeds
 * depend on it. Prints one line per (engine, kind, path): an FNV-1a hash over many seeds of the design's
 * fields as generation 0 had them. Built with the host harness (tests/run_tests.sh). */
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
#define GEN0_FM(p, k, s, pa) fm_generate(p, k, s, pa, 0)
#define GEN0_AN(d, k, s, pa) an_design(d, k, s, pa, 0)
#define GEN0_CAL(d, k, s, pa) cal_design(d, k, s, pa, 0)
#define GEN0_AN_BYTES 380u       /* an_design_t and cal_design_t as generation 0 laid them out: later fields go after */
#define GEN0_CAL_BYTES 200u

int main(void)
{
    static fm_patch_t fp;
    static an_design_t ad;
    static cal_design_t cd;
    int32_t k;
    uint32_t pa, s;
    for (pa = 0; pa < PATH_N; pa++) {
        for (k = -1; k < 9; k++) {           /* (generation 0's nine families) */
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
        for (k = -1; k < 8; k++) {
            uint32_t h = 2166136261u;
            for (s = 0; s < (k < 0 ? 400u : 40u); s++) {
                GEN0_AN(&ad, k, 1234u + s * 7919u, pa);
                h = fnv(h, &ad, GEN0_AN_BYTES);
            }
            printf("an %d %u %08x\n", k, pa, h);
        }
        for (k = -1; k < 6; k++) {
            uint32_t h = 2166136261u;
            for (s = 0; s < (k < 0 ? 400u : 40u); s++) {
                GEN0_CAL(&cd, k, 1234u + s * 7919u, pa);
                h = fnv(h, &cd, GEN0_CAL_BYTES);
            }
            printf("cal %d %u %08x\n", k, pa, h);
        }
    }
    return 0;
}
