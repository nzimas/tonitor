/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* App entry. The SPL maps app.bin at XIP 0x02000120 and jumps to its first
 * byte with r0 = boot-parameter block (0x01C7FE08, inside the vector table).
 * The SPL expects `goto +4; rts` as the first instruction. The core state
 * the SPL left is saved first; then interrupts off and our stacks. */
	.section .text.crt0,"ax",@progbits
	.globl _start
_start:
	goto 1f
	rts
1:	r1 = icfg
	r2 = psr
	cli
	r3 = 0x1C07F00		/* STATUS (debug area) */
	[r3 + 0x20] = r1	/* icfg as handed over */
	[r3 + 0x24] = r2	/* psr */
	[r3 + 0x28] = r0	/* boot-parameter pointer */
	r1 = sp
	[r3 + 0x2C] = r1
	r1 = ssp
	[r3 + 0x30] = r1
	sp = _ustack_top
	ssp = _sstack_top
	call fm1_cstart
	goto .

/* CPU1 entry (BIGORNA build 32, from Melodee 1.4.5, after the AC79 SDK's cpu.a/startup.S.o): install separate
 * supervisor/user stacks, then return from the startup interrupt into the RAM polling loop (hal/fm1_multicore.h).
 * No application interrupt is enabled on this core. */
	.section .text.core1,"ax",@progbits
	.globl fm1_core1_start
fm1_core1_start:
	cli
	icfg = 0
	usp = _cpu1_ustack_top
	sp = _cpu1_sstack_top
	ssp = _cpu1_sstack_top
	reti = fm1_core1_main
	rti
