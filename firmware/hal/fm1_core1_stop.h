/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * (BIGORNA: from Melodee 1.4.5, keremimo/melodee, unchanged but always built) */
/* Safe CPU1 hold, adapted from Felucca 1.6.1 fm1_cpu1_halt:
 * Release commit a53ecf428c29c780108fd70a31b71a32a6b5c313;
 * related dual-core PR: https://github.com/hugelton/Felucca/pull/247
 * Holding CPU1 can abort an outstanding bus access and raise a delayed
 * bus-invalid flag on CPU0. Mask those traps, hold, drain and clear the
 * flags, then restore the debug enables and write-lock state. Registers
 * only: also used before application RAM is initialized. */
#pragma once
#include <stdint.h>
#include "fm1_cc.h"

#ifndef FM1_CORE1_STOP_TEST
#define FM1_C1_READ(a) (*(volatile uint32_t *)(a))
#define FM1_C1_WRITE(a, v) (*(volatile uint32_t *)(a) = (v))
#define FM1_C1_SYNC() __asm__ volatile("csync" ::: "memory")
#endif

/* (TONITOR: one copy, called -- seven inlined copies cost ~0.9 KB of a full flash. Registers only and no RAM code
 * calls it, so it may be out of line: it runs from the flash, before the RAM is set up too) */
static __attribute__((noinline, unused)) void fm1_core1_stop(void)
{
    uint32_t locked = !(FM1_C1_READ(0x1EEE240u) & 1u), en, bus = 0x003F0030u;
    volatile uint32_t n;
    /* Do not fold ~bus into a stock-ROM-looking address (build.py check). */
    __asm__("" : "+r"(bus));
    if (locked)
        FM1_C1_WRITE(0x1EEE240u, 0xE7u);
    en = FM1_C1_READ(0x1EEE340u);
    FM1_C1_WRITE(0x1EEE340u, en & ~bus);
    FM1_C1_SYNC();
    FM1_C1_WRITE(0x1EEE004u, FM1_C1_READ(0x1EEE004u) | 2u);
    FM1_C1_WRITE(0x1EEE004u, FM1_C1_READ(0x1EEE004u) & ~8u);
    FM1_C1_SYNC();
    for (n = 0; n < 256u; n++)            /* let the delayed bus flag arrive */
        ;
    FM1_C1_WRITE(0x1EEE248u, bus);
    FM1_C1_SYNC();
    FM1_C1_WRITE(0x1EEE340u, en);
    if (locked && (FM1_C1_READ(0x1EEE240u) & 1u))
        FM1_C1_WRITE(0x1EEE240u, 0xE7u);
    FM1_C1_SYNC();
}

#undef FM1_C1_READ
#undef FM1_C1_WRITE
#undef FM1_C1_SYNC
