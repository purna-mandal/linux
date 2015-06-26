/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2014 Joshua Henderson <joshua.henderson@microchip.com>
 */
#ifndef _ASM_PIC32_SPACES_H
#define _ASM_PIC32_SPACES_H

#include <linux/sizes.h>

#ifdef CONFIG_MIPS_PIC32MZ
#define CAC_BASE_UPPER          CKSEG2
#define UNCAC_BASE_UPPER        CKSEG3
#define MAP_BASE                (CKSEG3 + SZ_128M)
#define UPPERMEM_START          0x20000000
#define HIGHMEM_START		(~0UL)
#define UNCAC_BASE		CKSEG3
#define IO_BASE			CKSEG3
#define PAGE_OFFSET		CKSEG2
#define PHYS_OFFSET		UPPERMEM_START
#endif

#ifdef CONFIG_MIPS_PIC32MZDA
#define PHYS_OFFSET	_AC(0x08000000, UL)
#define UNCAC_BASE	_AC(0xa8000000, UL)
#endif

#include <asm/mach-generic/spaces.h>

#endif /* __ASM_PIC32_SPACES_H */
