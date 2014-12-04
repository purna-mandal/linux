/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2003, 2004 Chris Dearman
 * Copyright (C) 2005 Ralf Baechle (ralf@linux-mips.org)
 * Copyright (C) 2014 Joshua Henderson <joshua.henderson@microchip.com>
 */
#ifndef __ASM_MACH_MIPS_CPU_FEATURE_OVERRIDES_H
#define __ASM_MACH_MIPS_CPU_FEATURE_OVERRIDES_H

/*
 * CPU feature overrides for PIC32 boards
 */

#define cpu_dcache_line_size()          16
#define cpu_icache_line_size()          16
#define cpu_scache_line_size()          0

#define cpu_has_vint			1
#define cpu_has_veic			1
#define cpu_has_4k_cache		1
#define cpu_has_dc_aliases		1
#define cpu_has_ic_fills_f_dc		0
#define cpu_has_vtag_icache		0
#define cpu_has_inclusive_pcaches	0
#define cpu_icache_snoops_remote_store	1
#define cpu_has_mips32r1		0
#define cpu_has_mips32r2		1
#define cpu_has_mips64r1		0
#define cpu_has_mips64r2		0

#ifdef CONFIG_CPU_MIPS64
#error This platform does not support 64bit.
#endif

#endif /* __ASM_MACH_MIPS_CPU_FEATURE_OVERRIDES_H */
