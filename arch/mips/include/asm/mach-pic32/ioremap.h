/*
 * include/asm/mach-pic32/ioremap.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * Copyright (C) 2014 Joshua Henderson <joshua.henderson@microchip.com>
 */
#ifndef __ASM_MACH_PIC32_IOREMAP_H
#define __ASM_MACH_PIC32_IOREMAP_H

#include <linux/types.h>

#define PIC32_SFR_BASE 0xbf800000

/*
 * Allow physical addresses to be fixed up to help peripherals located
 * outside the low 32-bit range -- generic pass-through version.
 */
static inline phys_addr_t fixup_bigphys_addr(phys_addr_t phys_addr, phys_addr_t size)
{
	return phys_addr;
}

static inline void __iomem *plat_ioremap(phys_addr_t offset, unsigned long size,
	unsigned long flags)
{
	if (offset >= PIC32_SFR_BASE &&
	    offset < PIC32_SFR_BASE + 0x100000)
		return (void __iomem *)(unsigned long)offset;

	return NULL;
}

static inline int plat_iounmap(const volatile void __iomem *addr)
{
	return (unsigned long)addr >= PIC32_SFR_BASE &&
		(unsigned long)addr < PIC32_SFR_BASE + 0x100000;
}

#endif /* __ASM_MACH_PIC32_IOREMAP_H */
