# SPDX-License-Identifier: GPL-3.0-only
# Tonitor: reads a diagnostic build's timing reports (firmware built with TELEMETRY 1) from the FM-1 over USB-MIDI.
# Run with the venv: .venv/bin/python tools/telelog.py
import re, sys, time
import mido
EV = ["ON", "OFF", "ENGINE", "STOP", "LOOP_REC", "LOOP_PLAY", "LOOP_CLEAR", "LOOP_SET", "LOOPS", "LOOP_TRK", "LOOP_XP"]
names = [n for n in mido.get_input_names() if re.search(r"fm-1|tonitor|felucca", n, re.I)]
if not names:
    print("no FM-1 port among", mido.get_input_names(), flush=True)
    sys.exit(1)
print("listening on", names[0], flush=True)
t0 = time.time()
quiet_peak = 0
with mido.open_input(names[0]) as port:
    for m in port:
        if m.type != "sysex" or len(m.data) < 7 or m.data[0] != 0x7D or m.data[1] != 0x54:
            continue
        d = m.data[2:]
        mx = d[0] << 7 | d[1]
        lates = d[2]
        gap = (d[3] << 7 | d[4]) / 100.0
        ec = d[5:9]
        trims = d[9]
        ents = d[10:]
        t = time.time() - t0
        line = f"{t:8.2f}s  max {mx:3d}%  late {lates:2d}  max gap {gap:5.2f} ms  voice costs FM {ec[0]}% ANIMA {ec[1]}% CALIGULA {ec[2]}% RUSSOLO {ec[3]}%  tails trimmed {trims}"
        out = []
        for i in range(0, len(ents) - 13, 14):
            e = ents[i:i + 14]
            pct = e[0] << 7 | e[1]
            ev = e[5] | e[6] << 7 | e[7] << 14 | e[8] << 21
            evs = [EV[b] for b in range(len(EV)) if ev >> b & 1]
            if ev >> 20 & 1: evs.append("loop-notes")
            if ev >> 21 & 1: evs.append("wrap")
            out.append(f"    half #{e[12] | e[13] << 7}: {pct}%{' LATE' if e[2] else ''} gap {(e[3] << 7 | e[4]) / 100:.2f} ms,"
                       f" {e[9]} struck, {e[10]} voices, layers sounding {''.join(str(k + 1) for k in range(6) if e[11] >> k & 1) or '-'}  {' '.join(evs)}")
        beat = int(t) // 5 != int(t - 0.2) // 5
        if mx >= 80 or lates or gap > 6.5 or out or beat or trims:
            print(line, flush=True)
            for o in out:
                print(o, flush=True)
