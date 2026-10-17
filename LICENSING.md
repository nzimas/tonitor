# Tonitor licensing

The Tonitor **firmware** is free software, licensed under the GNU General Public License, version 3 only
(`GPL-3.0-only`, full text in `LICENSE`). If you distribute it, or firmware built from it, give your
recipients its complete corresponding source under the same licence.

The **website, logo and documentation** written for Tonitor are under the MIT licence
(`LICENSES/MIT.txt`): see the table below for exactly which files.

It is a work based on **Felucca** (Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton
Instruments) and **SLOOP** (isod89), both GPL-3.0-only. Files taken from them keep their copyright
headers; changes are noted in each file.

| What | Licence | Where |
| --- | --- | --- |
| Tonitor's own code: `firmware/src/{tonitor,anima,fm,gran,reverb,layers,ui,sessions,gfx,main}.c`, `dsp.h`, `tools/{build,gen_tables,gen_font,sdk_from_package}.py`, `tests/`, the scripts | GPL-3.0-only | |
| The browser installer, a port of `tools/fm1_install.py` (Felucca / SLOOP) to JavaScript | GPL-3.0-only | `docs/js/fm1ota.js` |
| The firmware package the installer writes, built from this tree (with the JieLi SDK files below) | GPL-3.0-only | `docs/firmware/tonitor.fwsc` |
| The website, the logo, the documentation's text | MIT | `docs/` (except the two rows above), `site/`, `README.md`, `CHANGELOG.md`, `LICENSES/MIT.txt` |
| From Felucca / SLOOP: `firmware/hal/`, `firmware/loader/`, `firmware/crt0.S`, `firmware/app.ld`, `firmware/src/{usb,ota,recovery,lcd,libc}.c`, `bootguard.h`, `tools/{fm1pkg_make,lz4blk,fm1_install}.py`, `tools/get_toolchain.sh`, parts of `tools/build.py` and `tools/gen_tables.py` | GPL-3.0-only | |
| msfa (Google Inc., Pascal Gauthier), from Dexed, ported to integer C by Leo Kuroshita for Felucca; for TONITOR the modulation buses moved to the stack | Apache-2.0 | `firmware/src/fm6_core.c`, `LICENSES/Apache-2.0-msfa.txt` |
| The ANIMA engine: a fixed-point port of the anima opcode (`AnimaCore.h`, `AnimaDesign.h`) of Vascular, Copyright (C) Nuno Zimas, contributed by its author to this firmware | GPL-3.0-only (in this firmware) | `firmware/src/anima.c`, `tools/gen_tables.py` |
| CALIGULA follows the architecture of LIRA-8 (Copyright (C) Miguel Moreno), a Pure Data emulation of SOMA's LYRA-8; rebuilt in fixed-point C, no Pd code copied | BSD 3-clause (LIRA-8's) | `firmware/src/cal.c`, `LICENSES/BSD-LIRA-8.txt` |
| RUSSOLO re-makes the techniques of Noisferatu (Copyright (C) Robert Heel, rob-scape/noisferatu) in fixed-point C for the FM-1 | GPL-3.0 (Noisferatu's, as the firmware's) | `firmware/src/russolo.c` |
| DRUID: a fixed-point port of the resonators of Mutable Instruments Rings (Copyright 2015 Emilie Gillet, pichenettes/eurorack, `rings/dsp`), in this GPL-3.0 firmware | MIT (Rings') | `firmware/src/druid.c`, `LICENSES/MIT-rings.txt` |
| Terminus Bold 8x16 (the screen's fallback font) | SIL OFL 1.1 | `assets/fonts/ter-u16b.bdf`, `assets/fonts/Terminus-LICENSE.txt` |
| Atkinson Hyperlegible Bold (the screen's type) | SIL OFL 1.1 | `assets/fonts/AtkinsonHyperlegible-Bold.ttf`, `assets/fonts/AtkinsonHyperlegible-OFL.txt` |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK file is in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |
