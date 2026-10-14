#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# TONITOR: everything the build and the installer need, once (macOS or Linux).
#   - a Python environment in .venv with Pillow (the build), mido and python-rtmidi (the installer)
#   - JieLi's Linux toolchain (clang for pi32v2) in ~/.jieli/toolchain (tools/get_toolchain.sh)
#   - three files of the JieLi AC79 SDK (Apache-2.0) in ~/fw-AC79_AIoT_SDK: a sparse, shallow clone, or
#     (gitee wants an account) taken out of SLOOP's release package and checked by their SHA-256
#   - optionally the Atkinson Hyperlegible font (SIL OFL), for smoother large type: ./setup.sh --font
# On macOS the toolchain runs in Docker (Docker Desktop must be running to build).
set -e
cd "$(dirname "$0")"
if [ ! -x .venv/bin/python ]; then
    python3 -m venv .venv
fi
.venv/bin/pip install --quiet --upgrade pip
.venv/bin/pip install --quiet Pillow mido python-rtmidi
echo "setup: python environment ready (.venv)"

if [ ! -x "${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}/pi32v2/bin/clang" ]; then
    sh tools/get_toolchain.sh
fi
echo "setup: toolchain ready"

SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
if [ ! -f "$SDK/cpu/wl82/tools/uboot.boot" ]; then
    if ! GIT_TERMINAL_PROMPT=0 git clone --depth 1 --filter=blob:none --sparse \
            --branch AC79NN_SDK_V1.2.1_2023-12-13 https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git "$SDK" ||
       ! git -C "$SDK" sparse-checkout set cpu/wl82/tools; then
        # gitee asks for an account now: take the three files out of SLOOP's release package instead,
        # checked against the SDK's own SHA-256 (tools/sdk_from_package.py)
        rm -rf "$SDK"
        mkdir -p build
        curl -fL --retry 3 -o build/sloop-2.6.fwsc \
            https://raw.githubusercontent.com/isod89/sloop-fm1/main/docs/firmware/sloop-2.6.fwsc
        .venv/bin/python tools/sdk_from_package.py build/sloop-2.6.fwsc "$SDK"
    fi
fi
echo "setup: SDK files ready ($SDK)"

if [ "$1" = "--font" ] && [ ! -f assets/fonts/AtkinsonHyperlegible-Bold.ttf ]; then
    curl -fL --retry 3 -o assets/fonts/AtkinsonHyperlegible-Bold.ttf \
        https://github.com/google/fonts/raw/main/ofl/atkinsonhyperlegible/AtkinsonHyperlegible-Bold.ttf
    curl -fL --retry 3 -o assets/fonts/AtkinsonHyperlegible-OFL.txt \
        https://github.com/google/fonts/raw/main/ofl/atkinsonhyperlegible/OFL.txt
    echo "setup: Atkinson Hyperlegible ready"
fi
if [ "$(uname -s)" = Darwin ] && ! docker info >/dev/null 2>&1; then
    echo "setup: done -- start Docker Desktop before ./build.sh"
else
    echo "setup: done"
fi
