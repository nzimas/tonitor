#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# TONITOR: build build/tonitor.fwsc (./setup.sh first).   ./build.sh [--release X.Y]
set -e
cd "$(dirname "$0")"
PY=.venv/bin/python
[ -x "$PY" ] || { echo "build.sh: run ./setup.sh first"; exit 1; }
export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
if [ "$(uname -s)" != Linux ] || [ "$(uname -m)" != x86_64 ]; then
    docker info >/dev/null 2>&1 || { echo "build.sh: Docker is not running (start Docker Desktop)"; exit 1; }
fi
exec "$PY" tools/build.py "$@"
