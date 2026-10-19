/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Charles Vestal
 * From X0X 00bff73, after Melodee dual-core audio by Kerem Kilic;
 * via SLOOP v2.7.1 4ec1ac829fd2bdf2c571ea215fb3d62477d01220.
 * Copyright (C) 2026 dspaudio: MC-1 entry symbols, stacks and guards. */
	.section .text.cpu1,"ax",@progbits
	.balign 4
	.globl mc_c1_entry
mc_c1_entry:
	cli
	icfg = 0
	usp = _mc_c1_ustack_top
	sp = _mc_c1_sstack_top
	ssp = _mc_c1_sstack_top
	reti = mc_c1_main
	rti

	.section .bss.c1stack,"aw",@nobits
	.balign 8
	.globl _mc_c1_guard0
	.globl _mc_c1_ustack
	.globl _mc_c1_guard1
	.globl _mc_c1_sstack
	.globl _mc_c1_guard2
_mc_c1_guard0:
	.space 32
_mc_c1_ustack:
	.space 4096
_mc_c1_ustack_top:
_mc_c1_guard1:
	.space 32
_mc_c1_sstack:
	.space 512
_mc_c1_sstack_top:
_mc_c1_guard2:
	.space 32
