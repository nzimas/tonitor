#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# TONITOR: what tools/fm1_install.py makes of a package, for the browser installer's test to compare against
#   python_facts.py PACKAGE.fwsc > facts.json
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import fm1_install as fi  # noqa: E402

product, image = fi.load_package(sys.argv[1], False)
print(json.dumps({"product": product, "image_len": len(image), "image_crc": fi._crc16(image),
                  "response": list(fi.response(0x12345, bytes([1, 2, 3, 250, 0x7F, 0x80]), 3))}))
