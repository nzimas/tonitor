/* SPDX-License-Identifier: GPL-3.0-only
 * TONITOR: FM, generation 6's families (research/dx7/README.md; its sixth, DIVE, left out by the owner: too much pitch bend) -- learned from a collection of DX7
 * voices, 9606 different ones, by the idioms its programmers used that the earlier families do not make:
 *   SHAPER   a carrier nailed to 1 .. 19 Hz under modulators that follow the key: the carrier hardly turns, so it
 *            works as a wave shaper on them, and as it turns the tone rolls over between its odd and even
 *            harmonics, a few times a second, at the same pace up and down the keyboard
 *   LADDER   four to six carriers on a ladder of ratios -- near unison, an octave stack, uneven steps -- their
 *            envelopes staggered: the partials enter one by one, or die one by one
 *   REVERSE  a sound that swells for as long as the key is held and stops when it is let go: tape run backwards
 *   CREEP    a held note leaving its pitch slowly, over seconds to tens of seconds: the pitch envelope's slowest
 *            rates
 *   CLANG    Chowning's bells as the DX7's programmers made them: carriers turned by modulators on ratios off
 *            their harmonics, the index dying with the note
 * Each draws one of its family's skeletons (algorithm, feedback, every operator's frequency and whether it is off,
 * under or full: taken from the collection, the most common first), an envelope shape for each operator (the
 * centres of the family's carriers' and modulators' envelopes, grouped twelve ways), bent, and the rest within
 * the family's ranges there (levels, scaling, detune, the LFO's habits): new sounds of that kind, never one of the
 * collection's voices again. Then each bends its own trait. The tables between the markers are written by
 * research/dx7/learn.py. */

typedef struct {
    uint8_t alg, fb;             /* the algorithm (0-based, as FP_ALG), the feedback */
    uint16_t lv;                 /* each operator's level, operator 1 in the lowest 2 bits: 0 off, 1 under, 2 full */
    int16_t f[6];                /* each operator's frequency, operator 1 first: ratio x 100, or -(1 + decade x 100
                                  * + fine): fixed, 10^decade x 10^(fine / 100) Hz */
} dx7_skel_t;
typedef struct {
    uint8_t crs0, crs1, cdt0, cdt1, mrs0, mrs1, mdt0, mdt1;   /* rate scaling, detune: 10th and 90th percentile */
    uint8_t fbw[8], lfw[6];      /* the feedback's and the LFO wave's weights */
    uint8_t lfs0, lfs1, ppit, pmd0, pmd1, pms0, pms1, pamp, amd0;
    uint8_t amd1;                /* LFO: speed; how often (%) it moves the pitch, and how deep; .. the level */
    const dx7_skel_t *sk;
    const uint8_t (*ce)[8], (*me)[8];   /* envelope shapes, carriers' and modulators': R1..R4, L1..L4 */
    uint8_t nsk, nce, nme;
} dx7_fam_t;

/* BEGIN LEARNED (research/dx7/learn.py) */
/* SHAPER: 1481 voices of the collection; 964 skeletons, the 24 kept */
static const dx7_skel_t DX_SHAPER[24] = {
    {1, 7, 0x5a6, {-11, 200, 200, 200, 800, -332}},
    {2, 7, 0x59a, {-29, 200, 400, 100, 700, 1100}},
    {1, 7, 0x566, {-29, 202, 200, 200, 800, 200}},
    {1, 7, 0x9aa, {-1, 100, -9, 100, 100, 100}},
    {1, 7, 0x5a6, {-1, 202, 200, 200, 800, 300}},
    {1, 7, 0x5aa, {-1, 500, -1, 500, 500, 1000}},
    {16, 7, 0x99a, {-41, 100, 100, 100, 200, 500}},
    {14, 7, 0x66a, {-55, 100, -54, 100, 500, 300}},
    {16, 4, 0x296, {-11, 50, 50, 600, 50, 900}},
    {2, 7, 0x68a, {-1, 100, -379, -1, 100, -35}},
    {6, 6, 0xaaa, {400, 100, -22, 700, 300, 1100}},
    {1, 7, 0x9aa, {-5, 100, -1, 100, 101, 100}},
    {14, 6, 0x96a, {-23, 100, -35, 100, 300, 500}},
    {16, 7, 0xa26, {-1, 100, 50, -365, 400, 50}},
    {1, 7, 0x5aa, {100, 100, -1, 100, 100, 200}},
    {17, 7, 0x496, {-43, 824, 200, 100, 910, 3564}},
    {1, 7, 0x5aa, {-11, 100, -9, 100, 100, 300}},
    {27, 6, 0x8aa, {-2, 100, -22, 100, 1938, 1074}},
    {8, 7, 0xaa6, {-19, 400, 100, 700, 300, 3000}},
    {16, 6, 0x95a, {-1, 100, 100, 100, 500, 500}},
    {4, 2, 0xa9a, {300, 100, -344, 100, -41, 100}},
    {1, 6, 0x6aa, {-11, 200, 200, 200, 800, -342}},
    {14, 7, 0x5aa, {-5, 100, -1, 100, 500, 200}},
    {14, 7, 0x56a, {-41, 101, -59, 200, 200, 600}},
};
static const uint8_t DC_SHAPER[12][8] = {   /* carrier envelopes: R1..R4, L1..L4 (centroids, the most common first) */
    {91, 35, 24, 53, 99, 87, 2, 0},
    {43, 27, 20, 38, 95, 95, 89, 0},
    {92, 31, 24, 57, 98, 93, 85, 2},
    {71, 75, 23, 54, 97, 94, 89, 0},
    {58, 17, 16, 55, 98, 93, 90, 0},
    {46, 33, 21, 45, 96, 93, 8, 0},
    {92, 97, 95, 92, 99, 98, 97, 1},
    {74, 84, 28, 51, 87, 95, 3, 1},
    {53, 92, 90, 44, 99, 96, 99, 0},
    {67, 39, 62, 53, 95, 93, 95, 1},
    {90, 97, 98, 47, 98, 98, 98, 0},
    {90, 44, 64, 58, 98, 5, 4, 1},
};
static const uint8_t DM_SHAPER[12][8] = {   /* modulator envelopes: R1..R4, L1..L4 (centroids, the most common first) */
    {84, 28, 7, 33, 97, 95, 90, 1},
    {49, 30, 35, 47, 94, 89, 87, 1},
    {94, 43, 47, 48, 97, 91, 88, 0},
    {80, 32, 23, 44, 97, 85, 3, 0},
    {83, 71, 33, 51, 93, 85, 3, 1},
    {73, 74, 28, 37, 86, 89, 82, 1},
    {83, 86, 91, 28, 97, 96, 94, 0},
    {89, 42, 14, 34, 94, 17, 5, 1},
    {84, 53, 37, 47, 92, 88, 87, 89},
    {93, 96, 91, 94, 98, 96, 96, 2},
    {89, 46, 24, 60, 95, 85, 2, 89},
    {87, 42, 82, 54, 98, 11, 14, 1},
};
static const dx7_fam_t DF_SHAPER = {
    0, 4, 0, 12, 0, 5, 1, 12,   /* RS low, high; DET low, high: carriers, then modulators */
    {9, 0, 2, 2, 3, 8, 15, 54}, {39, 2, 0, 2, 54, 1}, 22, 42, 9, 19, 95, 1, 3, 19, 33,
    99, DX_SHAPER, DC_SHAPER, DM_SHAPER, 24, 12, 12};

/* LADDER: 130 voices of the collection; 99 skeletons, the 24 kept */
static const dx7_skel_t DX_LADDER[24] = {
    {29, 7, 0xa6a, {100, 202, 444, 1260, 1280, 104}},
    {31, 0, 0xaaa, {800, 666, 600, 575, 545, 535}},
    {24, 3, 0x6aa, {50, 404, 101, 202, 808, 404}},
    {23, 7, 0xaaa, {50, 50, 100, 106, 60, 600}},
    {29, 7, 0xa6a, {100, 202, 444, 1260, 1280, 304}},
    {30, 0, 0xaaa, {162, 429, 572, 852, -42, 270}},
    {31, 0, 0xaaa, {100, 102, 98, 106, 96, 109}},
    {24, 1, 0x66a, {50, 100, 150, 408, 100, 400}},
    {31, 6, 0xaaa, {100, 200, 400, 800, 1600, 3193}},
    {30, 5, 0xaaa, {100, 101, 102, 103, -1, 100}},
    {29, 7, 0x96a, {100, 202, 444, 1260, 1212, 104}},
    {29, 7, 0x96a, {100, 202, 206, 444, 1212, 101}},
    {22, 7, 0x55a, {300, 151, 258, 345, 900, 1100}},
    {31, 5, 0xaaa, {101, 202, 101, 100, 200, 100}},
    {24, 6, 0x6aa, {100, 200, 200, -233, 440, 1650}},
    {21, 4, 0xaaa, {300, 500, 600, 1177, 600, 72}},
    {21, 0, 0x6aa, {50, 99, 300, 100, 303, 1575}},
    {29, 6, 0xa6a, {1188, 800, 1600, 50, 1200, 400}},
    {23, 7, 0xaaa, {100, 200, 75, 100, 100, 200}},
    {21, 7, 0x6a6, {50, 440, 50, 100, 515, 1400}},
    {20, 3, 0xa9a, {101, 100, 300, 100, 202, 100}},
    {31, 0, 0xaaa, {100, 150, 133, 200, 266, 300}},
    {21, 6, 0xaaa, {100, 107, 114, 121, 128, 135}},
    {31, 5, 0xaaa, {132, 132, 264, 396, 264, 396}},
};
static const uint8_t DC_LADDER[12][8] = {   /* carrier envelopes: R1..R4, L1..L4 (centroids, the most common first) */
    {91, 71, 38, 46, 98, 89, 1, 0},
    {34, 89, 45, 44, 63, 97, 1, 0},
    {77, 81, 32, 65, 90, 94, 95, 2},
    {94, 94, 97, 89, 98, 91, 96, 0},
    {64, 90, 92, 38, 98, 85, 94, 7},
    {79, 30, 28, 67, 97, 92, 83, 1},
    {44, 98, 63, 75, 0, 99, 5, 0},
    {70, 51, 95, 41, 5, 0, 98, 0},
    {86, 26, 60, 41, 99, 29, 8, 0},
    {62, 99, 36, 32, 11, 0, 98, 6},
    {98, 58, 12, 82, 99, 2, 2, 10},
    {91, 66, 98, 78, 99, 4, 0, 0},
};
static const uint8_t DM_LADDER[12][8] = {   /* modulator envelopes: R1..R4, L1..L4 (centroids, the most common first) */
    {83, 60, 40, 45, 99, 91, 4, 0},
    {96, 93, 27, 47, 99, 99, 99, 0},
    {91, 99, 99, 39, 99, 89, 99, 10},
    {95, 90, 99, 98, 99, 98, 97, 23},
    {69, 52, 91, 53, 97, 0, 10, 4},
    {94, 32, 40, 29, 99, 19, 3, 0},
    {77, 36, 26, 52, 99, 96, 79, 0},
    {52, 99, 41, 93, 94, 97, 6, 0},
    {40, 78, 42, 43, 94, 77, 92, 6},
    {95, 60, 21, 54, 99, 85, 0, 58},
    {98, 48, 99, 79, 99, 78, 15, 1},
    {88, 33, 20, 30, 99, 70, 0, 97},
};
static const dx7_fam_t DF_LADDER = {
    0, 3, 3, 10, 0, 5, 5, 10,   /* RS low, high; DET low, high: carriers, then modulators */
    {31, 3, 0, 9, 5, 7, 9, 32}, {51, 5, 0, 4, 35, 5}, 7, 46, 10, 13, 99, 2, 7, 25, 22,
    99, DX_LADDER, DC_LADDER, DM_LADDER, 24, 12, 12};
#define DX_LADDER_ENTER 47   /* % of them whose carriers enter one by one */
#define DX_LADDER_DIE 63     /* .. die one by one */

/* REVERSE: 123 voices of the collection; 92 skeletons, the 24 kept */
static const dx7_skel_t DX_REVERSE[24] = {
    {1, 7, 0x5aa, {-1, 100, -9, 100, 100, 400}},
    {4, 2, 0xa9a, {300, 100, -344, 100, -41, 100}},
    {6, 7, 0x5a6, {200, 500, 100, 100, 600, 700}},
    {31, 0, 0xaaa, {100, 200, 300, 400, 500, 600}},
    {4, 0, 0xaaa, {100, -1, 100, -28, 100, -33}},
    {3, 2, 0x6a6, {100, 100, 100, 100, 100, 500}},
    {31, 0, 0xaaa, {700, 800, 900, 1000, 1100, 1200}},
    {4, 3, 0xaaa, {50, -239, 50, -239, 50, -238}},
    {16, 5, 0xa02, {-1, 100, 300, 100, 100, 164}},
    {8, 5, 0xa6a, {-170, -120, 100, 3680, 50, 51}},
    {1, 7, 0x9aa, {-1, 100, -9, 100, 100, 100}},
    {5, 5, 0xaaa, {100, 200, 101, 200, 100, 200}},
    {5, 5, 0xaaa, {1050, 1100, 1150, 1111, -12, -197}},
    {0, 7, 0xaaa, {50, 51, 50, 50, 50, 107}},
    {17, 7, 0x656, {300, 109, 384, 60, -247, -304}},
    {1, 7, 0x5a6, {-6, 200, 200, 200, 800, -332}},
    {1, 7, 0x5aa, {-16, 100, -25, 100, 100, 400}},
    {14, 6, 0xa6a, {-37, 100, -1, -34, 100, 100}},
    {5, 5, 0xaa2, {100, 1500, 101, 101, 100, 400}},
    {17, 7, 0xa56, {826, 770, 1560, 668, 1530, 3618}},
    {15, 4, 0x5aa, {416, 660, 50, 105, 134, 4650}},
    {15, 4, 0x6aa, {-223, -282, -131, -133, -179, -352}},
    {8, 6, 0xaaa, {200, 101, 200, 300, 200, 101}},
    {1, 6, 0x9aa, {200, 200, 200, 200, 100, 1100}},
};
static const uint8_t DC_REVERSE[12][8] = {   /* carrier envelopes: R1..R4, L1..L4 (centroids, the most common first) */
    {82, 92, 95, 83, 98, 96, 98, 0},
    {20, 31, 24, 99, 68, 99, 78, 0},
    {52, 22, 16, 52, 93, 94, 94, 1},
    {71, 38, 58, 64, 98, 93, 98, 2},
    {50, 71, 22, 58, 96, 96, 98, 0},
    {97, 47, 35, 55, 97, 95, 96, 0},
    {14, 99, 22, 77, 94, 99, 0, 0},
    {23, 30, 66, 64, 96, 95, 92, 0},
    {11, 19, 36, 73, 99, 77, 3, 0},
    {81, 39, 14, 52, 99, 89, 10, 0},
    {16, 20, 14, 66, 56, 94, 14, 4},
    {58, 50, 23, 76, 20, 57, 99, 0},
};
static const uint8_t DM_REVERSE[12][8] = {   /* modulator envelopes: R1..R4, L1..L4 (centroids, the most common first) */
    {69, 21, 9, 38, 98, 93, 93, 1},
    {23, 30, 28, 51, 96, 93, 86, 0},
    {96, 98, 96, 97, 99, 94, 97, 31},
    {91, 58, 23, 25, 91, 90, 93, 0},
    {59, 37, 22, 40, 91, 58, 14, 7},
    {73, 48, 33, 73, 94, 80, 81, 0},
    {94, 89, 9, 48, 98, 94, 2, 2},
    {83, 92, 85, 11, 98, 96, 91, 0},
    {75, 19, 90, 74, 93, 16, 19, 6},
    {87, 49, 23, 51, 80, 91, 89, 92},
    {32, 42, 90, 71, 94, 95, 95, 0},
    {12, 99, 23, 99, 73, 99, 0, 0},
};
static const dx7_fam_t DF_REVERSE = {
    0, 3, 0, 14, 0, 4, 0, 14,   /* RS low, high; DET low, high: carriers, then modulators */
    {15, 0, 5, 4, 2, 13, 8, 48}, {41, 1, 2, 6, 47, 2}, 7, 45, 10, 14, 94, 1, 4, 22, 31,
    99, DX_REVERSE, DC_REVERSE, DM_REVERSE, 24, 12, 12};

/* CREEP: 58 voices of the collection; 51 skeletons, the 24 kept */
static const dx7_skel_t DX_CREEP[24] = {
    {16, 6, 0x56a, {84, -154, 82, -179, 398, -236}},
    {14, 6, 0xaaa, {100, 100, 800, -201, 700, 200}},
    {8, 6, 0x9aa, {50, 50, 50, 50, 50, 200}},
    {16, 7, 0x966, {100, 200, 100, 50, 1500, 500}},
    {4, 7, 0xaaa, {-4, 50, -25, 100, -50, 150}},
    {4, 7, 0xaaa, {-1, 50, -8, 100, -4, 150}},
    {18, 2, 0x565, {1592, 1122, 1038, 1592, 760, 330}},
    {18, 2, 0x965, {1592, 1104, 1020, 1592, 728, 330}},
    {18, 2, 0x549, {398, 372, 92, 1122, 346, 346}},
    {18, 2, 0x549, {2786, 360, 1110, 2244, 346, 346}},
    {1, 7, 0x5a6, {100, 200, 100, 200, 200, 400}},
    {1, 7, 0x5aa, {-17, 100, 100, 100, 100, 200}},
    {0, 7, 0x96a, {800, 100, 100, 100, 800, 400}},
    {10, 6, 0xa9a, {100, 100, 504, 100, 300, -346}},
    {17, 2, 0xa6a, {100, -301, -265, -113, -72, -160}},
    {15, 3, 0xa6a, {700, 50, -101, 600, 200, 800}},
    {15, 0, 0x55a, {-1, 50, 100, 202, 218, 52}},
    {3, 0, 0x58a, {100, -137, -165, 400, 50, 200}},
    {2, 0, 0xaaa, {-1, -1, 2222, -21, -75, 3184}},
    {8, 6, 0x9aa, {192, 192, -301, -331, -60, -291}},
    {27, 7, 0xa6a, {100, 100, 200, 400, 100, 300}},
    {16, 6, 0x9aa, {84, -154, 82, -179, 398, -247}},
    {15, 3, 0xaaa, {-201, 600, 65, 51, 840, 52}},
    {3, 6, 0xaa6, {50, 50, 50, 600, -148, -250}},
};
static const uint8_t DC_CREEP[12][8] = {   /* carrier envelopes: R1..R4, L1..L4 (centroids, the most common first) */
    {51, 76, 27, 43, 99, 90, 91, 0},
    {55, 99, 99, 72, 99, 99, 99, 0},
    {67, 33, 45, 46, 99, 4, 5, 0},
    {23, 38, 49, 51, 91, 90, 1, 0},
    {86, 47, 32, 40, 90, 91, 0, 0},
    {2, 0, 83, 34, 19, 48, 96, 87},
    {63, 21, 14, 52, 99, 95, 83, 0},
    {38, 21, 30, 34, 94, 96, 95, 0},
    {17, 27, 94, 46, 53, 29, 99, 0},
    {82, 26, 68, 62, 99, 82, 7, 0},
    {85, 16, 11, 27, 90, 76, 94, 0},
    {99, 36, 99, 34, 99, 0, 0, 0},
};
static const uint8_t DM_CREEP[12][8] = {   /* modulator envelopes: R1..R4, L1..L4 (centroids, the most common first) */
    {78, 36, 16, 29, 95, 93, 89, 15},
    {82, 45, 23, 28, 97, 90, 5, 0},
    {27, 27, 29, 31, 94, 95, 90, 0},
    {15, 33, 21, 34, 97, 94, 0, 0},
    {69, 34, 38, 94, 91, 69, 8, 8},
    {89, 87, 86, 93, 87, 87, 94, 25},
    {96, 69, 82, 37, 97, 24, 3, 0},
    {29, 33, 94, 42, 24, 43, 97, 51},
    {85, 99, 99, 35, 99, 99, 99, 0},
    {90, 25, 23, 17, 99, 21, 6, 0},
    {61, 28, 27, 47, 1, 90, 0, 0},
    {85, 0, 25, 11, 0, 99, 0, 0},
};
static const dx7_fam_t DF_CREEP = {
    0, 3, 0, 11, 0, 3, 3, 11,   /* RS low, high; DET low, high: carriers, then modulators */
    {18, 0, 13, 3, 3, 8, 22, 29}, {46, 9, 9, 5, 14, 17}, 5, 99, 24, 21, 97, 2, 7, 31, 32,
    99, DX_CREEP, DC_CREEP, DM_CREEP, 24, 12, 12};
static const uint8_t DP_CREEP[24][8] = {   /* pitch envelopes: R1..R4, L1..L4 (44 seen) */
    {82, 0, 0, 14, 50, 52, 48, 50},
    {0, 10, 0, 99, 60, 50, 22, 50},
    {35, 46, 12, 82, 53, 40, 49, 50},
    {0, 0, 99, 98, 75, 60, 50, 50},
    {0, 0, 99, 98, 75, 60, 50, 51},
    {64, 52, 7, 54, 33, 45, 50, 50},
    {99, 99, 9, 60, 50, 50, 24, 48},
    {0, 12, 41, 32, 27, 60, 37, 48},
    {6, 99, 99, 99, 50, 50, 50, 30},
    {6, 99, 99, 99, 50, 50, 50, 20},
    {99, 13, 9, 1, 62, 54, 50, 50},
    {87, 0, 0, 0, 37, 50, 50, 50},
    {5, 42, 21, 0, 51, 50, 50, 69},
    {6, 16, 69, 10, 57, 50, 50, 68},
    {10, 0, 0, 0, 50, 50, 77, 78},
    {99, 1, 1, 1, 30, 40, 50, 51},
    {0, 0, 0, 0, 50, 80, 50, 30},
    {8, 19, 19, 19, 50, 30, 50, 31},
    {1, 99, 99, 99, 50, 50, 50, 15},
    {92, 99, 2, 99, 50, 50, 63, 50},
    {80, 50, 0, 0, 49, 47, 50, 50},
    {49, 53, 1, 26, 28, 50, 45, 24},
    {8, 38, 42, 44, 50, 47, 50, 55},
    {11, 99, 99, 99, 50, 50, 50, 24},
};

/* CLANG: 194 voices of the collection; 156 skeletons, the 24 kept */
static const dx7_skel_t DX_CLANG[24] = {
    {3, 6, 0xaa6, {76, 86, 92, -287, -271, 148}},
    {4, 5, 0xaaa, {404, 1320, 100, 900, 50, 1232}},
    {15, 3, 0x96a, {50, -201, 65, 51, 840, 52}},
    {10, 6, 0x996, {50, 100, 909, 100, 300, 1111}},
    {15, 7, 0x5aa, {50, 50, 68, 88, 50, 78}},
    {6, 6, 0x966, {300, 500, 200, 700, 2040, 1620}},
    {4, 7, 0x6aa, {50, 50, 50, 314, 100, 100}},
    {15, 7, 0x9a9, {50, -201, 65, 51, 763, 52}},
    {26, 5, 0xaaa, {50, 118, 315, 118, -1, 315}},
    {5, 0, 0xaa6, {200, 300, 700, 924, 100, -238}},
    {8, 5, 0xa6a, {-170, -120, 100, 3680, 50, 51}},
    {17, 3, 0x9aa, {50, -177, -222, 185, 318, 484}},
    {16, 7, 0xa66, {100, 1122, 238, 5394, 500, 552}},
    {2, 0, 0x69a, {100, 175, 447, -317, -347, -347}},
    {10, 6, 0xaa6, {312, 100, 505, 1000, 71, 800}},
    {3, 4, 0xaaa, {-109, 50, 1100, 1177, 400, 700}},
    {27, 7, 0xaaa, {56, 100, -68, -400, -369, 50}},
    {29, 7, 0xaaa, {100, 202, 444, 1260, 1280, 104}},
    {17, 6, 0xa62, {50, 98, 66, -253, -376, 3100}},
    {7, 7, 0x6a9, {1500, 4030, 1400, -101, -101, -400}},
    {25, 4, 0x015, {91, 60, 1400, 2112, 70, 1200}},
    {4, 7, 0xaaa, {200, 1960, 400, 1458, 100, 100}},
    {15, 7, 0x9aa, {50, -201, 65, 100, 840, 52}},
    {3, 6, 0x9a6, {76, 52, 92, 164, -271, 296}},
};
static const uint8_t DC_CLANG[12][8] = {   /* carrier envelopes: R1..R4, L1..L4 (centroids, the most common first) */
    {88, 39, 26, 36, 99, 89, 1, 0},
    {34, 34, 32, 45, 89, 83, 4, 7},
    {95, 42, 91, 39, 99, 5, 18, 0},
    {87, 40, 27, 40, 94, 5, 1, 0},
    {94, 88, 36, 40, 99, 90, 0, 0},
    {91, 77, 53, 58, 86, 90, 4, 0},
    {95, 42, 50, 53, 99, 78, 1, 0},
    {41, 44, 49, 45, 93, 81, 91, 3},
    {94, 46, 40, 43, 97, 82, 82, 0},
    {90, 71, 89, 87, 97, 0, 21, 0},
    {95, 41, 54, 95, 99, 94, 4, 11},
    {42, 99, 96, 40, 99, 99, 0, 0},
};
static const uint8_t DM_CLANG[12][8] = {   /* modulator envelopes: R1..R4, L1..L4 (centroids, the most common first) */
    {93, 53, 20, 17, 97, 87, 0, 0},
    {93, 48, 47, 34, 97, 4, 3, 1},
    {89, 76, 73, 56, 96, 83, 3, 4},
    {75, 34, 18, 22, 98, 89, 88, 2},
    {92, 46, 32, 53, 99, 86, 3, 2},
    {38, 29, 21, 29, 92, 73, 8, 4},
    {80, 61, 50, 66, 99, 91, 60, 91},
    {89, 97, 83, 56, 99, 99, 98, 0},
    {88, 51, 62, 66, 96, 89, 89, 1},
    {44, 31, 38, 66, 88, 98, 18, 0},
    {72, 49, 64, 53, 94, 14, 89, 3},
    {73, 65, 52, 72, 19, 92, 11, 0},
};
static const dx7_fam_t DF_CLANG = {
    0, 4, 0, 11, 0, 5, 4, 12,   /* RS low, high; DET low, high: carriers, then modulators */
    {11, 1, 0, 8, 8, 8, 18, 41}, {43, 8, 5, 5, 28, 11}, 7, 99, 27, 21, 99, 1, 7, 25, 32,
    99, DX_CLANG, DC_CLANG, DM_CLANG, 24, 12, 12};
/* END LEARNED */

static int32_t dx_in(rnd_t *r, int32_t a, int32_t b) { return a <= b ? rnd_in(r, a, b) : rnd_in(r, b, a); }

/* a skeleton, the rest drawn within the family's ranges */
static void fm_dx7_draw(uint8_t *p, rnd_t *r, const dx7_fam_t *F, const dx7_skel_t *s)
{
    int n;
    p[FP_ALG] = s->alg;
    p[FP_FB] = (uint8_t)(rnd_chance(r, 50) ? s->fb : rnd_weighted(r, F->fbw, 8));
    for (n = 1; n <= 6; n++) {
        uint8_t *op = fm_op(p, n);
        int c = fm_is_carrier(p, n), lv = (s->lv >> (2 * (n - 1))) & 3;
        const uint8_t *e = c ? F->ce[rnd_below(r, F->nce)] : F->me[rnd_below(r, F->nme)];
        int32_t f = s->f[n - 1];
        if (f >= 0)                                    /* (off the harmonics: moved a little, now and then) */
            fm_ratio(op, f % 50 == 0 || rnd_chance(r, 50) ? f : f * rnd_in(r, 985, 1015) / 1000);
        else
            fm_fixed(op, (-f - 1) / 100, clampi((-f - 1) % 100 + rnd_in(r, -5, 5), 0, 99));
        fm_env(op, e[0] + rnd_in(r, -6, 6), e[1] + rnd_in(r, -6, 6), e[2] + rnd_in(r, -6, 6), e[3] + rnd_in(r, -6, 6),
               e[4] + rnd_in(r, -4, 4), e[5] + rnd_in(r, -4, 4), e[6] + rnd_in(r, -4, 4), e[7] < 5u ? 0 : e[7] + rnd_in(r, -4, 4));
        op[FP_OL] = (uint8_t)(lv == 0 ? rnd_below(r, 36) : lv == 1 ? rnd_in(r, 45, 79) : rnd_in(r, c ? 86 : 80, 99));
        op[FP_RS] = (uint8_t)(c ? dx_in(r, F->crs0, F->crs1) : dx_in(r, F->mrs0, F->mrs1));
        op[FP_DET] = (uint8_t)(c ? dx_in(r, F->cdt0, F->cdt1) : dx_in(r, F->mdt0, F->mdt1));
    }
    p[FP_LFW] = (uint8_t)rnd_weighted(r, F->lfw, 6);
    p[FP_LFS] = (uint8_t)dx_in(r, F->lfs0, F->lfs1);
    p[FP_LPMD] = p[FP_LAMD] = 0;
    if (rnd_chance(r, F->ppit)) {
        p[FP_LPMD] = (uint8_t)dx_in(r, F->pmd0, F->pmd1);
        p[FP_LPMS] = (uint8_t)dx_in(r, F->pms0, F->pms1);
    }
    if (rnd_chance(r, F->pamp)) {
        p[FP_LAMD] = (uint8_t)dx_in(r, F->amd0, F->amd1);
        for (n = 1; n <= 6; n++)
            if (fm_is_carrier(p, n))
                fm_op(p, n)[FP_AMS] = (uint8_t)rnd_in(r, 1, 3);
    }
}
static const dx7_skel_t *dx_pick(rnd_t *r, const dx7_fam_t *F) { return &F->sk[rnd_below(r, F->nsk)]; }

static void fm_gen_shaper(uint8_t *p, rnd_t *r)
{
    int n;
    fm_dx7_draw(p, r, &DF_SHAPER, dx_pick(r, &DF_SHAPER));
    for (n = 1; n <= 6; n++) {
        uint8_t *op = fm_op(p, n);
        if (fm_is_carrier(p, n) && op[FP_MODE] && op[FP_FC] <= 1u) {
            if (rnd_chance(r, 35))                     /* (another pace: 1 .. 9 Hz) */
                fm_fixed(op, 0, rnd_below(r, 96));
            op[FP_L1 + 2] = (uint8_t)maxi(op[FP_L1 + 2], rnd_in(r, 40, 95));   /* (held long enough to roll over) */
        }
    }
}

static void fm_gen_ladder(uint8_t *p, rnd_t *r)
{
    int n, k, enter = rnd_chance(r, DX_LADDER_ENTER), die = rnd_chance(r, DX_LADDER_DIE);
    int32_t rt[7], step = rnd_in(r, 6, 14);
    fm_dx7_draw(p, r, &DF_LADDER, dx_pick(r, &DF_LADDER));
    if (rnd_chance(r, 40)) {                           /* now and then a ladder of its own */
        int kind = (int)rnd_below(r, 3), d = kind == 0 ? rnd_in(r, 1, 12) : rnd_in(r, 20, 60);
        for (n = 1, k = 0; n <= 6; n++)
            if (fm_is_carrier(p, n)) {
                fm_ratio(fm_op(p, n), kind == 0 ? 100 + k * d                  /* near unison: beating, a cluster */
                                      : kind == 1 ? (100 << (k < 5 ? k : 4)) + (k ? rnd_in(r, -3, 3) : 0)   /* octaves */
                                                  : 100 + k * d);              /* uneven steps: a chord off the scale */
                k++;
            }
    }
    for (n = 1; n <= 6; n++)
        rt[n] = fm_op_ratio100(fm_op(p, n));
    for (n = 1; n <= 6; n++) {                         /* the higher the partial, the later it enters, the sooner it dies */
        uint8_t *op = fm_op(p, n);
        int rank = 0, j;
        if (!fm_is_carrier(p, n))
            continue;
        for (j = 1; j <= 6; j++)
            rank += fm_is_carrier(p, j) && (rt[j] < rt[n] || (rt[j] == rt[n] && j < n));
        op[FP_OL] = (uint8_t)rnd_in(r, 86, 99);
        if (enter)
            op[FP_R1] = (uint8_t)clampi(rnd_in(r, 45, 70) - rank * step, 3, 99);
        if (die) {
            op[FP_R1 + 1] = (uint8_t)clampi(rnd_in(r, 20, 35) + rank * step, 0, 99);
            op[FP_L1 + 1] = (uint8_t)rnd_in(r, 0, 60);
            op[FP_L1 + 2] = (uint8_t)rnd_in(r, 0, 30);
        }
    }
}

static void fm_gen_reverse(uint8_t *p, rnd_t *r)
{
    int n;
    fm_dx7_draw(p, r, &DF_REVERSE, dx_pick(r, &DF_REVERSE));
    for (n = 1; n <= 6; n++) {
        uint8_t *op = fm_op(p, n);
        if (fm_is_carrier(p, n)) {                     /* rising until the key is let go, held at the top; then cut */
            int two = rnd_chance(r, 40);               /* (or in two stages: up, then up again) */
            fm_env(op, rnd_in(r, 12, 34), rnd_in(r, 10, 30), rnd_in(r, 20, 60), rnd_in(r, 50, 80),
                   two ? rnd_in(r, 60, 82) : 99, 99, 99, 0);
        } else if (rnd_chance(r, 60)) {                /* .. the timbre opening with it */
            op[FP_R1] = (uint8_t)rnd_in(r, 10, 35);
            op[FP_L1 + 2] = (uint8_t)maxi(op[FP_L1 + 2], rnd_in(r, 70, 99));
            op[FP_R1 + 3] = (uint8_t)rnd_in(r, 45, 80);
        }
    }
}

static void fm_gen_creep(uint8_t *p, rnd_t *r)
{
    const uint8_t *e = DP_CREEP[rnd_below(r, sizeof DP_CREEP / sizeof DP_CREEP[0])];
    int i, n, mirror = rnd_chance(r, 40);
    fm_dx7_draw(p, r, &DF_CREEP, dx_pick(r, &DF_CREEP));
    for (i = 0; i < 4; i++) {                          /* its slow stages slower still: rates 0 .. 3, 0.6 .. 1.7 semitones */
        int32_t l = clampi(e[4 + i] + rnd_in(r, -3, 3), 0, 99);   /* a second (GLISSE's and WARP's: 3 .. 65) */
        p[FP_PR1 + i] = (uint8_t)(e[i] <= 12u ? rnd_in(r, 0, 3) : clampi(e[i] + rnd_in(r, -8, 8), 0, 99));
        p[FP_PL1 + i] = (uint8_t)(mirror ? clampi(100 - l, 0, 99) : l);
    }
    for (n = 1; n <= 6; n++)                           /* there within a second and held, so only the pitch creeps */
        if (fm_is_carrier(p, n)) {                     /* (and fm_measure, 2.5 s long, hears it at its loudest) */
            uint8_t *op = fm_op(p, n);
            op[FP_R1] = (uint8_t)maxi(op[FP_R1], rnd_in(r, 45, 80));
            op[FP_L1] = (uint8_t)maxi(op[FP_L1], rnd_in(r, 85, 99));
            op[FP_R1 + 1] = (uint8_t)maxi(op[FP_R1 + 1], rnd_in(r, 30, 60));
            op[FP_L1 + 1] = (uint8_t)maxi(op[FP_L1 + 1], rnd_in(r, 75, 96));
            op[FP_R1 + 2] = (uint8_t)maxi(op[FP_R1 + 2], rnd_in(r, 30, 60));
            op[FP_L1 + 2] = (uint8_t)maxi(op[FP_L1 + 2], rnd_in(r, 70, 96));
        }
}

static void fm_gen_clang(uint8_t *p, rnd_t *r)
{
    int n;
    fm_dx7_draw(p, r, &DF_CLANG, dx_pick(r, &DF_CLANG));
    for (n = 1; n <= 6; n++) {
        uint8_t *op = fm_op(p, n);
        if (fm_is_carrier(p, n)) {                     /* struck, dying away */
            op[FP_R1] = (uint8_t)rnd_in(r, 80, 99);
            op[FP_L1 + 2] = (uint8_t)clampi(op[FP_L1 + 2], 0, 20);
        } else                                         /* the index dying before the note: the clang mellowing */
            op[FP_R1 + 1] = (uint8_t)maxi(op[FP_R1 + 1], rnd_in(r, 30, 70));
    }
}
