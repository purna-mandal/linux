/*
 * Joshua Henderson, joshua.henderson@microchip.com
 * Copyright (C) 2014 Microchip Technology Inc.  All rights reserved.
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 */
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/bootmem.h>
#include <linux/pfn.h>
#include <linux/string.h>
#include <linux/export.h>
#include <linux/sizes.h>

#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/bootinfo.h>
#include <asm/page.h>
#include <asm/sections.h>

#include <asm/fw/fw.h>
#include <asm/mach-pic32/pic32.h>
#include <asm/mach-pic32/common.h>

#include <spaces.h>

static fw_memblock_t mdesc[FW_MAX_MEMBLOCKS];

#if defined(CONFIG_PIC32MZ_PLANC) || defined(CONFIG_PIC32MZ_PLAND)
#define EBI_SRAM_SIZE SZ_16M
#else
#define EBI_SRAM_SIZE SZ_8M
#endif

fw_memblock_t * __init fw_getmdesc(int eva)
{
	static int init;

	if (!init) {
		mdesc[0].type = BOOT_MEM_RAM;
		mdesc[0].base = 0;
		mdesc[0].size = 0x00080000;
		mdesc[0].valid = 1;

		mdesc[1].type = BOOT_MEM_RAM;
		mdesc[1].base = UPPERMEM_START;
		mdesc[1].size = EBI_SRAM_SIZE;
		mdesc[1].valid = 1;

		init = 1;
	}

	return &mdesc[0];
}

void __init fw_meminit(void)
{
	fw_memblock_t *p;

	p = fw_getmdesc(0);

	while (p->size) {
		unsigned long type, base, size;

		type = p->type;
		base = p->base;
		size = p->size;

		add_memory_region(base, size, type);
		p++;
	}
}

void __init prom_free_prom_memory(void)
{

}
