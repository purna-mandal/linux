/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2014 Joshua Henderson <joshua.henderson@microchip.com>
 */
#ifndef _ASM_PIC32_SPACES_H
#define _ASM_PIC32_SPACES_H

#ifdef CONFIG_PIC32MZ_UPPER_MEMORY

#define CAC_BASE_UPPER          _AC(0xc0000000, UL)
#define UNCAC_BASE_UPPER        _AC(0xe0000000, UL)
#define KSEG0_SIZE              _AC(0x20000000, UL)
#define KSEG1_SIZE              _AC(0x20000000, UL)
#define MAP_BASE                _AC(0xe0800000, UL)
#define PIC32_MAX_UPPER_MB      _AC(8, UL)
#define UPPERMEM_START          _AC(0x20000000, UL)
#define HIGHMEM_START           (UPPERMEM_START + (PIC32_MAX_UPPER_MB << 20))

#endif

#ifdef CONFIG_MIPS_PIC32_EPLATFORM
#define PHYS_OFFSET	_AC(0x08000000, UL)
#define UNCAC_BASE	_AC(0xa8000000, UL)
#endif

#include <asm/mach-generic/spaces.h>

#endif /* __ASM_PIC32_SPACES_H */
