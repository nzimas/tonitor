#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# TONITOR: the three JieLi AC79 SDK files a package needs (uboot.boot, cfg_tool.bin, cfg/eq_cfg_hw.bin,
# Apache-2.0), taken out of an existing FM-1 package built with them -- a SLOOP release, for example --
# for when the SDK's repository cannot be reached. They are checked against the SHA-256 of
# AC79NN_SDK_V1.2.1_2023-12-13 (the version SLOOP's build records) and written where tools/build.py looks:
#   tools/sdk_from_package.py PACKAGE.fwsc SDK_DIR      -> SDK_DIR/cpu/wl82/tools/...
import hashlib
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fm1pkg_make as pk  # noqa: E402

SHA256 = {
    "uboot.boot": "4e3b4c220dc96641cb5a723f41e68ce41d5261ae9434bb33fbd7f2c59976ded4",
    "cfg_tool.bin": "276579954f076886a6a7694f65dc71c034a63a2c204b76749065c0ac7b010d1b",
    "cfg/eq_cfg_hw.bin": "41167491bffed4651750719c973d2758adeb9021a5670d02d6a53c85ed80ea7d",
}


def entry(b):
    """a 32-byte JLFS entry (decrypted): offset, size, name"""
    _, _, off, size, _, _, _, name = struct.unpack("<HHIIBBH16s", b)
    return off, size, name.split(b"\0")[0].decode("latin-1")


def main(pkg, sdk):
    raw = Path(pkg).read_bytes()
    logical = bytearray()
    for i in range(20):                              # drop the identity's marker bytes
        logical += raw[i * 0x30:i * 0x30 + 0x2F]
    logical += raw[20 * 0x30:]
    lst = bytearray(logical[0x40:0x40 + 0x50])
    pk.enc(lst, 0, 0x50)
    off, size = struct.unpack_from("<II", lst, 8)
    flash = bytearray(logical[off:off + size])
    out = {}
    head = bytearray(flash[0:0x100])
    for o in range(0, 0x100, 32):
        pk.enc(head, o, 32)
    for o in range(32, 0x100, 32):
        eo, es, nm = entry(bytes(head[o:o + 32]))
        if nm == "uboot.boot":
            out["uboot.boot"] = bytes(flash[eo:eo + es])
    region = bytearray(flash[0x4000:])
    pk.sfc(region, 0, len(region) - len(region) % 32, 0, pk.KEY)
    blk = None
    for o in range(32, 32 * 8, 32):
        eo, es, nm = entry(bytes(region[o:o + 32]))
        if nm == "cfg_tool.bin":
            out["cfg_tool.bin"] = bytes(region[eo:eo + es])
            blk = eo + es
    if blk is not None:                              # the cfg directory right after the block
        _, _, cname = entry(bytes(region[blk:blk + 32]))
        eo, es, nm = entry(bytes(region[blk + 32:blk + 64]))
        if cname == "cfg" and nm == "eq_cfg_hw.bin":
            out["cfg/eq_cfg_hw.bin"] = bytes(region[blk + eo:blk + eo + es])
    ok = True
    for rel, sha in SHA256.items():
        d = out.get(rel)
        if d is None or hashlib.sha256(d).hexdigest() != sha:
            print(f"sdk_from_package: {rel}: {'not found' if d is None else 'checksum differs'}")
            ok = False
            continue
        dst = Path(sdk) / "cpu" / "wl82" / "tools" / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_bytes(d)
        print(f"sdk_from_package: {rel} ({len(d)} B) ok")
    return 0 if ok else 1


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit(__doc__ or "sdk_from_package.py PACKAGE.fwsc SDK_DIR")
    sys.exit(main(sys.argv[1], sys.argv[2]))
