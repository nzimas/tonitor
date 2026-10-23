# SPDX-License-Identifier: GPL-3.0-only
# Tonitor: prints the chip's own benchmark (a diagnostic build: TONITOR_TELEMETRY=1 ./build.sh) as it runs on the
# SMK-37, from its USB-MIDI SysEx lines (F0 7D 55 <ASCII> F7). Run with the venv: .venv/bin/python tools/chipbench.py
import re, sys, time
import mido
for _ in range(60):                                       # (the keyboard restarting: its port, when it comes)
    names = [n for n in mido.get_input_names() if re.search(r"fm-1|tonitor", n, re.I)]
    if names:
        break
    time.sleep(0.5)
if not names:
    print("no FM-1 port among", mido.get_input_names(), flush=True)
    sys.exit(1)
print("listening on", names[0], flush=True)
t0 = time.time()
with mido.open_input(names[0]) as port:
    for m in port:
        if m.type == "sysex" and len(m.data) > 2 and m.data[0] == 0x7D and m.data[1] == 0x55:
            line = bytes(m.data[2:]).decode("ascii", "replace")
            print(f"{time.time() - t0:7.1f}s  {line}", flush=True)
            if line.startswith("CHIPBENCH done"):
                break
