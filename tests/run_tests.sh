#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# TONITOR host tests (no toolchain, no FM-1 needed): the generated tables and fonts, the whole firmware
# checked for a runtime the chip lacks, then the DSP, the sounds, the layers and the screen run on the
# computer. Renders land in build/sounds (WAV) and build/screens (PNG).
set -e
cd "$(dirname "$0")/.."
PY=python3
[ -x .venv/bin/python ] && PY=.venv/bin/python
mkdir -p build/gen build/sounds build/screens
$PY tools/gen_tables.py build/gen/tonitor_tables.h >/dev/null
$PY tools/gen_font.py build/gen/tonitor_font.h >/dev/null
tests/check_target_ir.sh
clang -O2 -std=gnu99 -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function -Werror \
    -Ifirmware/src -Ibuild/gen tests/host/host.c -o build/host
build/host check
clang -O2 -std=gnu99 -w -Ifirmware/src -Ibuild/gen tests/golden/golden.c -o build/golden
for g in 0 1 2; do
    build/golden $g | diff -q - tests/golden/gen$g.txt >/dev/null || { echo "FAIL golden: generation $g no longer draws its sounds again"; exit 1; }
done
echo "golden: generations 0, 1 and 2 as they were"
build/host flows
build/host latch
build/host presets build/screens
build/host macros build/screens
build/host overdubs build/screens
build/host transpose
build/host ties
build/host budget
build/host clicks
build/host voices
build/host compat
build/host paths build/sounds
build/host reverb build/sounds
build/host sessions build/screens
build/host filter build/screens
build/host crush build/screens
build/host caligula build/sounds
build/host russolo build/sounds
build/host grain build/sounds && mv build/sounds/screen-*.ppm build/screens/
build/host render build/sounds 3 > build/sounds/stats.txt
build/host mix build/sounds
build/host screens build/screens
if [ -f build/tonitor.fwsc ] && command -v node >/dev/null; then    # the browser installer (after a build)
    $PY tests/web/python_facts.py build/tonitor.fwsc > build/facts.json
    node tests/web/test_fm1ota.mjs build/tonitor.fwsc build/facts.json
fi
$PY - <<'PY'
from pathlib import Path
from PIL import Image
d = Path("build/screens")
for p in sorted(d.glob("*.ppm")):
    Image.open(p).resize((480, 480), Image.NEAREST).save(p.with_suffix(".png"))
    p.unlink()
print("screens: build/screens/*.png")
PY
echo "tests: ok"
