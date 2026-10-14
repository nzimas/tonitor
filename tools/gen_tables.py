#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# TONITOR: fixed-point lookup tables (the target has no FPU and no float runtime).
# Parts after Felucca's tools/gen_tables.py (Leo Kuroshita, GPL-3.0): SINE, the pitch table, the FM6
# (msfa) start-up tables. The Anima tables are ported from Vascular's AnimaCore.h (the harmonic recipes
# of its 24 single cycles, with the same pseudo-random draws).
"""tools/gen_tables.py OUT.h"""
import math
import sys
from pathlib import Path

FS = 44100
CTL = 32
AN_TSIZE = 1024             # Anima wavetable points (one cycle), band-limited at four levels
AN_TABLES = 24
AN_LEVELS = 4
AN_HARM = [96, 32, 10, 3]   # harmonics kept per level (AnimaCore.h Tables::make)


def arr(name, ctype, vals, per=12):
    out = [f"static const {ctype} {name}[{len(vals)}] = {{"]
    for i in range(0, len(vals), per):
        out.append("    " + ", ".join(str(v) for v in vals[i:i + per]) + ",")
    out.append("};")
    return out


# ------------------------------------------------------------------ Anima (Vascular AnimaCore.h) ---
M64 = (1 << 64) - 1


class Random:
    """AnimaCore.h Random (splitmix64), bit for bit"""

    def __init__(self, seed=1):
        self.s = (seed * 0x9E3779B97F4A7C15 + 0x2545F4914F6CDD1D) & M64

    def next(self):
        self.s = (self.s + 0x9E3779B97F4A7C15) & M64
        z = self.s
        z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
        z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
        return z ^ (z >> 31)

    def unit(self):
        return (self.next() >> 11) * (1.0 / 9007199254740992.0)

    def in_(self, a, b):
        return a + (b - a) * self.unit()

    def chance(self, p):
        return self.unit() < p


def amp(k, n):
    fn = float(n)
    if k == 0:
        return 1.0 if n == 1 else 0.0                                       # sine
    if k == 1:
        return (1.0 / (fn * fn) * (-1.0 if (n // 2) % 2 else 1.0)) if n % 2 else 0.0   # triangle
    if k == 2:
        return 1.0 / fn ** 1.4                                              # soft saw
    if k == 3:
        return 1.0 / fn ** 1.3 if n % 2 else 0.0                            # soft square
    if k == 4:
        return math.sin(math.pi * fn * 0.25) / fn                           # pulse 25 %
    if k == 5:
        bars = [0, 1, 0.8, 0.6, 0.5, 0, 0.35, 0, 0.3]
        return bars[n] if n <= 8 else 0.0                                   # organ
    if k == 6:
        return (1.0 / fn ** 1.6 + (0.25 if n == 9 else 0.0)) if n % 2 else 0.0      # glass
    if k == 7:
        return (math.exp(-((fn - 3) / 1.5) ** 2) + 0.6 * math.exp(-((fn - 8) / 2) ** 2) +
                (0.4 if n == 1 else 0.0))                                   # vowel a
    if k in (8, 9, 10):
        r = Random(1000 + k * 77 + n)
        a = r.in_(0.1, 1.0) / fn ** 0.8
        return a * (1.0 if r.chance(0.5) else -1.0)                         # dense
    if k == 11:
        r = Random(5000 + n)
        if n > 48:
            return 0.0
        a = r.in_(0.2, 1.0) / fn ** 0.55
        return a * (1.0 if r.chance(0.5) else -1.0)                         # noisy cycle
    if k == 12:
        return {1: 1.0, 3: 0.7, 5: 0.5, 8: 0.45, 12: 0.3}.get(n, 0.0)       # bell set
    if k == 13:
        return 1.0 / fn * (0.3 if 4 < n < 9 else 1.0)                       # reed
    if k == 14:
        return 0.0 if n in (2, 4) else 1.0 / fn ** 1.2                      # hollow
    if k == 15:
        return 1.0 / fn if n % 3 == 1 else 0.0                              # comb
    if k == 16:
        return {1: 1.0, 3: 0.5}.get(n, 0.0)                                 # sine and fifth
    if k == 17:
        return {1: 1.0, 2: 0.6}.get(n, 0.0)                                 # sine and octave
    if k == 18:
        return 1.0 / fn ** 0.5 if n <= 48 else 0.0                          # bright tilt
    if k == 19:
        return math.sin(fn * 1.3) / fn ** 0.9                               # folded
    if k == 20:
        return (math.exp(-((fn - 2) / 1.2) ** 2) + 0.5 * math.exp(-((fn - 5) / 1.5) ** 2) +
                (0.5 if n == 1 else 0.0))                                   # vowel o
    if k == 21:
        return 0.6 if n == 1 else (0.8 if n in (7, 11, 13) else 0.05 / fn)  # metallic
    if k == 22:
        r = Random(9000 + n)
        if 6 <= n <= 40:
            return r.in_(0.0, 0.5) / fn ** 0.3
        return 0.3 if n == 1 else 0.0                                       # breath
    return math.sin(math.pi * fn * 0.1) / fn                                # pulse 10 %


def anima_tables():
    vals = []
    for k in range(AN_TABLES):
        for lv in range(AN_LEVELS):
            d = [0.0] * AN_TSIZE
            for n in range(1, AN_HARM[lv] + 1):
                a = amp(k, n)
                if a == 0.0:
                    continue
                for i in range(AN_TSIZE):
                    d[i] += a * math.sin(2 * math.pi * n * i / AN_TSIZE)
            peak = max(abs(x) for x in d) or 1.0
            vals += [int(round(32767 * x / peak)) for x in d] + [int(round(32767 * d[0] / peak))]
    L = [f"#define AN_TSIZE {AN_TSIZE}", f"#define AN_TABLES {AN_TABLES}", f"#define AN_LEVELS {AN_LEVELS}",
         "/* the 24 single cycles of Anima, four band-limited levels each (96, 32, 10, 3 harmonics), Q15,",
         " * AN_TSIZE + 1 points a cycle (the last = the first: no wrap in the interpolation) */"]
    L += arr("AN_WT", "int16_t", vals, 16)
    return L


def main(path):
    L = ["/* generated by tools/gen_tables.py */", "#pragma once", "#include <stdint.h>",
         f"#define FS {FS}", f"#define CTL {CTL}", ""]
    # (1025 points: SINE[i + 1] needs no wrap)
    L += arr("SINE", "int16_t", [int(round(32767 * math.sin(2 * math.pi * i / 1024))) for i in range(1025)])
    # phase increment (2^32 = one cycle a sample) of pitch p in 1/16 semitone, MIDI 0..127: the top octave
    inc = []
    for p in range(128 * 16):
        f = 440.0 * 2 ** ((p / 16 - 69) / 12)
        inc.append(min(int(f / FS * 2 ** 32), 0x7FFFFFFF))
    L += arr("PITCH_OCT", "uint32_t", inc[2048 - 192:], 8)
    L += ["/* phase increment of pitch p, 1/16 semitone 0..2047: the top octave's entry, k octaves down */",
          "static inline uint32_t pitch_inc(int32_t p)",
          "{",
          "    uint32_t k;",
          "    if (p < 0)",
          "        p = 0;",
          "    if (p > 2047)",
          "        p = 2047;",
          "    k = (2047u - (uint32_t)p) / 192u;",
          "    return PITCH_OCT[(uint32_t)p + 192u * k - (2048u - 192u)] >> k;",
          "}"]
    # 2^(i / 256), Q16, i = 0..256: the fractional part of exp2 (dsp.h exp2_q16)
    L += arr("EXP2_FRAC", "uint32_t", [int(round(65536 * 2 ** (i / 256))) for i in range(257)], 8)
    # log2(1 + i / 256), Q16, i = 0..256 (dsp.h log2_q16)
    L += arr("LOG2_FRAC", "uint32_t", [int(round(65536 * math.log2(1 + i / 256))) for i in range(257)], 8)
    # tanh(x) for x = 0 .. 4 in 512 steps, Q15 (dsp.h tanh_q15)
    L += arr("TANH_Q15", "int16_t", [int(round(32767 * math.tanh(4 * i / 512))) for i in range(513)])
    # the state-variable filter's g = tan(pi f / FS), f linear 0 .. 0.45 FS in 512 steps, Q24
    fmax = 0.45 * FS
    L += [f"#define TANG_FMAX {int(fmax)}"]
    L += arr("TAN_G", "uint32_t", [int(round((1 << 24) * math.tan(math.pi * (fmax * i / 512) / FS))) for i in range(513)], 8)
    # Anima's Map slopes: v^(2^(-slope * 1.6)), slope -1 .. 1 in 16 steps, v 0 .. 1 in 64 steps, Q15
    curves = []
    for si in range(17):
        sl = -1 + si / 8
        e = 2 ** (-sl * 1.6)
        curves += [int(round(32767 * (v / 64) ** e)) for v in range(65)]
    L += ["/* AnimaCore.h Map::at: the slope curves, [17][65] */"]
    L += arr("AN_CURVE", "int16_t", curves, 13)
    # the granulator's buffers (gran.c): mu-law, 8 bits a sample. MULAW_ENC: |x| >> 4 (0 .. 2047) -> the 7-bit
    # magnitude code; MULAW_DEC: the code -> |x| (Q15)
    mu = 255.0
    enc = [min(127, int(round(127 * math.log1p(mu * (i * 16 + 8) / 32768) / math.log1p(mu)))) for i in range(2048)]
    dec = [int(round(32767 * ((1 + mu) ** (c / 127) - 1) / mu)) for c in range(128)]
    L += arr("MULAW_ENC", "uint8_t", enc, 24)
    L += arr("MULAW_DEC", "int16_t", dec, 16)
    L += fm6_tables()
    L += anima_tables()
    Path(path).write_text("\n".join(L) + "\n")
    print(f"tables: {path}")


# FM6 (src/fm6_core.c, a C port of msfa, Apache-2.0): the tables msfa built with double math at start-up,
# precomputed here so the firmware needs no float. CTL-sample blocks at 44.1 kHz. (Felucca's tools/gen_tables.py.)
FM6_LFO_SOURCE = [   # msfa lfo.cc: the LFO rates 0..99 (Hz)
    0.062541, 0.125031, 0.312393, 0.437120, 0.624610, 0.750694, 0.936330, 1.125302, 1.249609, 1.436782,
    1.560915, 1.752081, 1.875117, 2.062494, 2.247191, 2.374451, 2.560492, 2.686728, 2.873976, 2.998950,
    3.188013, 3.369840, 3.500175, 3.682224, 3.812065, 4.000800, 4.186202, 4.310716, 4.501260, 4.623209,
    4.814636, 4.930480, 5.121901, 5.315191, 5.434783, 5.617346, 5.750431, 5.946717, 6.062811, 6.248438,
    6.431695, 6.564264, 6.749460, 6.868132, 7.052186, 7.250580, 7.375719, 7.556294, 7.687577, 7.877738,
    7.993605, 8.181967, 8.372405, 8.504848, 8.685079, 8.810573, 8.986341, 9.122423, 9.300595, 9.500285,
    9.607994, 9.798158, 9.950249, 10.117361, 11.251125, 11.384335, 12.562814, 13.676149, 13.904338, 15.092062,
    16.366612, 16.638935, 17.869907, 19.193858, 19.425019, 20.833333, 21.034918, 22.502250, 24.003841, 24.260068,
    25.746653, 27.173913, 27.578599, 29.052876, 30.693677, 31.191516, 32.658393, 34.317090, 34.674064, 36.416606,
    38.197097, 38.550501, 40.387722, 40.749796, 42.625746, 44.326241, 44.883303, 46.772685, 48.590865, 49.261084]


def fm6_tables():
    L = ["/* FM6 (fm6_core.c): msfa's start-up tables, precomputed */",
         f"#define FM6_LFO_UNIT {int(CTL * 25190424 / FS + 0.5)}     /* msfa Lfo::unit_ */",
         f"#define FM6_PENV_UNIT {int(CTL * (1 << 24) / (21.3 * FS) + 0.5)}    /* msfa PitchEnv::unit_ */",
         f"#define FM6_FREQ_R {int(round(2 ** 40 / FS))}u   /* 2^40 / FS (fm6_freq) */",
         f"#define FM6_AMS_C0 {int(round(12.2 * math.log2(math.e) * 2 ** 24))}   /* log2(e^12.2), Q24 */",
         f"#define FM6_AMS_K {int(round(0.07 / 262144 * math.log2(math.e) * 2 ** 24 * 1024))}      /* Q24 / 1024 */"]
    L += arr("FM6_EXP2", "uint32_t", [int(round((1 << 30) * (2 ** (i / 1024) - 1))) for i in range(1025)], 8)
    lforatio = int(4437500000.0 * CTL / FS)
    L += arr("FM6_LFO_DELTA", "uint32_t", [int(s * lforatio) for s in FM6_LFO_SOURCE], 8)
    base, step = 50857777, (1 << 24) // 12
    det = []
    for mn in range(128):
        lf = base + step * mn
        det.append(int(round(0.0209 * math.exp(-0.396 * (lf / (1 << 24))) / 7 * lf)))
    L += arr("FM6_DETUNE", "uint16_t", det, 16)
    L += arr("FM6_FINE", "int32_t", [int(math.floor(24204406.323123 * math.log(1 + 0.01 * f) + 0.5)) for f in range(100)], 10)
    return L


if __name__ == "__main__":
    main(sys.argv[1])
