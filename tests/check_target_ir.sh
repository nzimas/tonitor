#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The FM-1's chip has no FPU and the firmware links no compiler runtime: no float, no double, no 64-bit
# division or modulo, no 64-bit shift by a variable. This compiles the whole firmware to LLVM IR for a
# 32-bit target (no JieLi toolchain needed: the inline asm is never assembled) and fails on any of them.
#   tests/check_target_ir.sh        (after the tables and fonts are generated in build/gen)
set -e
cd "$(dirname "$0")/.."
mkdir -p build/check
clang --target=i686-unknown-linux-gnu -O2 -std=gnu99 -ffreestanding -fno-builtin -Wall -Wno-unused-function \
    -Werror -Ifirmware/hal -Ifirmware/src -Ibuild/gen -S -emit-llvm firmware/src/tonitor.c -o build/check/tonitor.ll
bad=$(grep -nE "\b(udiv|sdiv|urem|srem) i64|\bf(add|sub|mul|div|cmp|neg)\b|fptosi|fptoui|sitofp|uitofp|fpext|fptrunc|\bdouble\b|\bfloat\b|(shl|lshr|ashr) i64 %[a-z0-9._]+, %" build/check/tonitor.ll || true)
if [ -n "$bad" ]; then
    echo "check_target_ir: the firmware needs a runtime the FM-1 does not have:"
    echo "$bad" | head -20
    exit 1
fi
echo "check_target_ir: ok (no float, no 64-bit division, no variable 64-bit shift)"
