#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# TONITOR: the manual's chart of the engines' costs (docs/assets/engine-load.svg), from a run of the chip's own
# benchmark (a diagnostic build, TONITOR_TELEMETRY=1; tools/chipbench.py's output):
#   tools/gen_load_chart.py [tools/chipbench-fm1-71.log]
# Each family's cost is its voice's share of an audio block on one core, measured on the chip with one note held
# ("1n voice"). A bar spans an engine's families, the mark is their median.
import re, statistics, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
log = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "tools/chipbench-fm1-71.log"
cost = {}
for ln in log.read_text().splitlines():
    if m := re.search(r"\s(\w+) (.+?) 1n voice ([\d.]+)%", ln):
        cost.setdefault(m[1], {})[m[2]] = float(m[3])

SPLIT = {"DRUID": ("HARP", "DRONE")}             # (families far from the rest: a row of their own)
ORGANISM = {"CALIGULA", "RUSSOLO"}               # (one per track, however many notes)
rows = []
for eng in ["RUSSOLO", "FM", "OBRADOR", "ANIMA", "BAYAREA", "DRUID", "HIVE", "CALIGULA"]:
    fam = cost[eng]
    apart = SPLIT.get(eng, ())
    rest = [v for k, v in fam.items() if k not in apart]
    rows.append((eng + (" (per track)" if eng in ORGANISM else ""), rest, eng in ORGANISM))
    if apart:
        rows.append((eng + " " + " and ".join(apart), [fam[k] for k in apart], False))

W, X0, X1, TOP, RH = 760, 230, 730, 96, 34
H = TOP + RH * len(rows) + 64
MAXV = 30.0
x = lambda v: X0 + (X1 - X0) * v / MAXV
FG, DIM, LINE, BG = "#edeae4", "#a8a29b", "#2c2927", "#171515"
NOTE, ORG = "#00d6ff", "#ff8c00"
o = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" role="img" '
     f'aria-label="The engines\' cost per note, in points of an audio block" '
     f'font-family="system-ui, -apple-system, Segoe UI, sans-serif">',
     f'<rect width="{W}" height="{H}" rx="10" fill="{BG}"/>',
     f'<text x="24" y="32" fill="{FG}" font-size="17" font-weight="700">Cost of a note, by engine</text>',
     f'<text x="24" y="54" fill="{DIM}" font-size="13">Points: share of an audio block on one core, measured on the '
     f'FM-1 with Tonitor build 71.</text>',
     f'<text x="24" y="72" fill="{DIM}" font-size="13">Bar: lightest to heaviest family. Mark: the median.</text>']
for v in range(0, int(MAXV) + 1, 5):
    o.append(f'<line x1="{x(v):.1f}" y1="{TOP - 6}" x2="{x(v):.1f}" y2="{TOP + RH * len(rows)}" stroke="{LINE}"/>')
    o.append(f'<text x="{x(v):.1f}" y="{TOP + RH * len(rows) + 18}" fill="{DIM}" font-size="12" '
             f'text-anchor="middle">{v}</text>')
o.append(f'<text x="{(X0 + X1) / 2:.0f}" y="{TOP + RH * len(rows) + 38}" fill="{DIM}" font-size="12" '
         f'text-anchor="middle">points per note (per track for CALIGULA and RUSSOLO)</text>')
for i, (name, vals, org) in enumerate(rows):
    y = TOP + RH * i + RH / 2
    lo, hi, med = min(vals), max(vals), statistics.median(vals)
    c = ORG if org else NOTE
    o.append(f'<text x="{X0 - 12}" y="{y + 4.5:.1f}" fill="{FG}" font-size="13.5" text-anchor="end">{name}</text>')
    o.append(f'<rect x="{x(lo):.1f}" y="{y - 7:.1f}" width="{max(x(hi) - x(lo), 3):.1f}" height="14" rx="4" '
             f'fill="{c}" fill-opacity="0.45"/>')
    o.append(f'<rect x="{x(med) - 1.5:.1f}" y="{y - 10:.1f}" width="3" height="20" fill="{c}"/>')
    lab = f"{lo:.0f}–{hi:.0f}" if round(lo) != round(hi) else f"{lo:.0f}"
    o.append(f'<text x="{x(hi) + 8:.1f}" y="{y + 4.5:.1f}" fill="{DIM}" font-size="12">{lab}</text>')
o.append("</svg>")
out = ROOT / "docs/assets/engine-load.svg"
out.write_text("\n".join(o) + "\n")
print("chart:", out.relative_to(ROOT))
