/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Chris Dearman (chris@mips.com)
 * Copyright (C) 2007 Mips Technologies, Inc.
 * Copyright (C) 2014 Joshua Henderson <joshua.henderson@microchip.com>
 */
#ifndef __ASM_MACH_MIPS_KERNEL_ENTRY_INIT_H
#define __ASM_MACH_MIPS_KERNEL_ENTRY_INIT_H

#define C0_CONFIG	$16
#define C0_TAGLO	$28

#define K0_MASK		0x07
#define CACHE_MODE	0x03

#ifdef CONFIG_MIPS_PIC32MZDA
#define INDEX_BASE 0x8800
#else
#define INDEX_BASE 0x8000
#endif

	.macro	kernel_entry_setup

#ifndef CONFIG_SYS_SUPPORTS_ZBOOT

	move a2, zero
	lui a2, INDEX_BASE /* Get a KSeg0 address for cache ops */

	/* Clear TagLo register */
	mtc0 zero, C0_TAGLO /* write C0_ITagLo */

	move a3, zero
	add a3, 0x800

next_icache_tag:
	/* Index Store Tag Cache Op */
	/* Will invalidate the tag entry, clear the lock bit, and clear */
	/* the LRF bit */
	cache 0x8, 0(a2)

	add a3, -1 /* Decrement set counter */
	add a2, v1 /* Get next line address */
	bne a3, zero, next_icache_tag
	nop

done_icache:
	move a2, zero
	lui a2, INDEX_BASE /* Get a KSeg0 address for cache ops */

	/* Clear TagLo register */
	mtc0 zero, C0_TAGLO /* write C0_TagLo */

	move a3, zero
	add a3, 0x800

next_dcache_tag:
	cache 0x9, 0(a2)

	add a3, -1 /* Decrement set counter */
	add a2, v1 /* Get next line address */
	bne a3, zero, next_dcache_tag
	nop

done_dcache:

	mfc0 v0, C0_CONFIG
	ori v0, K0_MASK
	xori v0, K0_MASK
	ori v0, CACHE_MODE
	mtc0 v0, C0_CONFIG
#endif /* CONFIG_SYS_SUPPORTS_ZBOOT */
	.endm

/*
 * Do SMP slave processor setup necessary before we can safely execute C code.
 */
	.macro	smp_slave_setup
	.endm

#endif /* __ASM_MACH_MIPS_KERNEL_ENTRY_INIT_H */
