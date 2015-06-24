/*
 * Copyright (C) 2014 Microchip Technology Inc.  All rights reserved.
 * Author: Joshua Henderson <joshua.henderson@microchip.com>
 *
 * Copyright 2001 MontaVista Software Inc.
 * Author: Matt Porter <mporter@mvista.com>
 *
 * Copyright (C) 2009 Lemote, Inc.
 * Author: Wu Zhangjin <wuzhangjin@gmail.com>
 *
 * This program is free software; you can redistribute	it and/or modify it
 * under  the terms of	the GNU General	 Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sizes.h>

#include <asm/addrspace.h>

#include <asm/mach-pic32/pic32.h>
#include <asm/mach-pic32/pic32.h>
#include <asm/mach-pic32/common.h>

/*
 * These two variables specify the free mem region
 * that can be used for temporary malloc area
 */
unsigned long free_mem_ptr;
unsigned long free_mem_end_ptr;

/* The linker tells us where the image is. */
extern unsigned char __image_begin, __image_end;

/* debug interfaces  */
extern void puts(const char *s);
extern void puthex(unsigned long long val);

void error(char *x)
{
	puts("\n\n");
	puts(x);
	puts("\n\n -- System halted");

	while (1)
		;	/* Halt */
}

/* activate the code for pre-boot environment */
#define STATIC static

#ifdef CONFIG_KERNEL_GZIP
#include "../../../../lib/decompress_inflate.c"
#endif

#ifdef CONFIG_KERNEL_BZIP2
#include "../../../../lib/decompress_bunzip2.c"
#endif

#ifdef CONFIG_KERNEL_LZ4
#include "../../../../lib/decompress_unlz4.c"
#endif

#ifdef CONFIG_KERNEL_LZMA
#include "../../../../lib/decompress_unlzma.c"
#endif

#ifdef CONFIG_KERNEL_LZO
#include "../../../../lib/decompress_unlzo.c"
#endif

#ifdef CONFIG_KERNEL_XZ
#include "../../../../lib/decompress_unxz.c"
#endif

/* Config Registers */
#define CFGEBIA		0xC0
#define CFGEBIC		0xD0

/* EBI Registers */
#define EBICS0		0x14
#define EBICS1		0x18
#define EBICS2		0x1C
#define EBICS3		0x20
#define EBIMSK0		0x54
#define EBIMSK1		0x58
#define EBIMSK2		0x5C
#define EBIMSK3		0x60
#define EBISMT0		0x94
#define EBISMT1		0x98
#define EBISMT2		0x9C
#define EBISMCON	0xA4

#define ioremap_fake(offset, size) ((void __iomem *)(unsigned long)offset)

#if defined(CONFIG_PIC32MZ_PLANC) || defined(CONFIG_PIC32MZ_PLAND)
#define CHIP_SIZE SZ_4M
#define MEMSIZE 7 /* 4MB */
#else
#define CHIP_SIZE SZ_2M
#define MEMSIZE 6 /* 2MB */
#endif

static void ebi_setup(void)
{
	void __iomem *ebi_base = ioremap_fake(PIC32_BASE_EBI, 0x1000);
	void __iomem *config_base = ioremap_fake(PIC32_BASE_CONFIG, 0x400);

	BUG_ON(!config_base);
	BUG_ON(!ebi_base);

	/*
	 * Enable address lines [0:23]
	 * Controls access of pins shared with PMP
	 */
	__raw_writel(0x80FFFFFF, config_base + CFGEBIA);

	/*
	 * Enable write enable pin
	 * Enable output enable pin
	 * Enable byte select pin 0
	 * Enable byte select pin 1
	 * Enable byte select pin 2
	 * Enable byte select pin 3
	 * Enable Chip Select 0
	 * Enable data pins [0:15]
	 */
	__raw_writel(0x000033F3, config_base + CFGEBIC);

	/*
	 * Connect CS0/CS1/CS2/CS3 to physical address
	 */
	__raw_writel(0x20000000 + (CHIP_SIZE * 0), ebi_base + EBICS0);
	__raw_writel(0x20000000 + (CHIP_SIZE * 1), ebi_base + EBICS1);
	__raw_writel(0x20000000 + (CHIP_SIZE * 2), ebi_base + EBICS2);
	__raw_writel(0x20000000 + (CHIP_SIZE * 3), ebi_base + EBICS3);

	/*
	 * Memory size is set as 2/4 MB
	 * Memory type is set as SRAM
	 * Uses timing numbers in EBISMT0
	 */
	__raw_writel(1 << 5 | MEMSIZE, ebi_base + EBIMSK0);
	__raw_writel(1 << 5 | MEMSIZE, ebi_base + EBIMSK1);
#if defined(CONFIG_PIC32MZ_PLAND)
	__raw_writel(1 << 8 | 1 << 5 | MEMSIZE, ebi_base + EBIMSK2);
	__raw_writel(1 << 8 | 1 << 5 | MEMSIZE, ebi_base + EBIMSK3);
#else
	__raw_writel(1 << 5 | MEMSIZE, ebi_base + EBIMSK2);
	__raw_writel(1 << 5 | MEMSIZE, ebi_base + EBIMSK3);
#endif

	/*
	 * Configure EBISMT0
	 * ISSI device has read cycles time
	 * ISSI device has address setup time
	 * ISSI device has address/data hold time
	 * ISSI device has Write Cycle Time
	 * Bus turnaround time
	 * No page mode
	 * No page size
	 * No RDY pin
	 */
#if defined(CONFIG_PIC32MZ_PLANB)
	__raw_writel(1 << 10 | 1 << 8 | 1 << 6 | 2, ebi_base + EBISMT0);
	__raw_writel(0, ebi_base + EBISMT1);
#elif defined(CONFIG_PIC32MZ_PLAND)
	__raw_writel(1 << 10 | 1 << 8 | 1 << 6 | 3, ebi_base + EBISMT0);
	__raw_writel(1 << 10 | 1 << 8 | 1 << 6 | 4, ebi_base + EBISMT1);
#else
	__raw_writel(2 << 10 | 1 << 8 | 1 << 6 | 7, ebi_base + EBISMT0);
	__raw_writel(0, ebi_base + EBISMT1);
#endif
	__raw_writel(0, ebi_base + EBISMT2);

	/*
	 * Keep default data width to 16-bits
	 */
	__raw_writel(0x00000000, ebi_base + EBISMCON);
}

struct tlb_entry {
	unsigned long entrylo0;
	unsigned long entrylo1;
	unsigned long entryhi;
	unsigned long pagemask;
};

/* (PFN << 6) | GLOBAL | VALID | DIRTY | cacheability */
#define ENTRYLO_CACHED(paddr)	(((paddr) >> 6) | (0x07) | (0x03 << 3))
#define ENTRYLO_UNCACHED(paddr)	(((paddr) >> 6) | (0x07) | (0x02 << 3))

#define SZ_12M (SZ_8M + SZ_4M)

static struct tlb_entry wired_mappings[] = {

	{
		.entrylo0	= ENTRYLO_CACHED(UPPERMEM_START),
		.entrylo1	= ENTRYLO_CACHED(UPPERMEM_START + SZ_4M),
		.entryhi	= CAC_BASE_UPPER,
		.pagemask	= PM_4M,
	},
	{
		.entrylo0	= ENTRYLO_UNCACHED(UPPERMEM_START),
		.entrylo1	= ENTRYLO_UNCACHED(UPPERMEM_START + SZ_4M),
		.entryhi	= UNCAC_BASE_UPPER,
		.pagemask	= PM_4M,
	},
#if defined(CONFIG_PIC32MZ_PLANC) || defined(CONFIG_PIC32MZ_PLAND)
	{
		.entrylo0	= ENTRYLO_CACHED(UPPERMEM_START + SZ_8M),
		.entrylo1	= ENTRYLO_CACHED(UPPERMEM_START + SZ_12M),
		.entryhi	= CAC_BASE_UPPER + SZ_8M,
		.pagemask	= PM_4M,
	},
	{
		.entrylo0	= ENTRYLO_UNCACHED(UPPERMEM_START + SZ_8M),
		.entrylo1	= ENTRYLO_UNCACHED(UPPERMEM_START + SZ_12M),
		.entryhi	= UNCAC_BASE_UPPER + SZ_8M,
		.pagemask	= PM_4M,
	},
#endif
};

/*
 * This function is used instead of add_wired_entry(), because it does not
 * have any external dependencies and is not marked __init
 */
static inline void pic32mz_add_wired_entry(unsigned long entrylo0,
	unsigned long entrylo1, unsigned long entryhi, unsigned long pagemask)
{
	int i = read_c0_wired();

	write_c0_wired(i + 1);
	write_c0_index(i);
	tlbw_use_hazard();
	write_c0_pagemask(pagemask);
	write_c0_entryhi(entryhi);
	write_c0_entrylo0(entrylo0);
	write_c0_entrylo1(entrylo1);
	mtc0_tlbw_hazard();
	tlb_write_indexed();
	tlbw_use_hazard();
}

#define UNIQUE_ENTRYHI(idx) (CKSEG0 + ((idx) << (PAGE_SHIFT + 1)))

static void tlb_setup(void)
{
	int i, tlbsz;

	/* Flush TLB.  local_flush_tlb_all() is not available yet. */
	write_c0_entrylo0(0);
	write_c0_entrylo1(0);
	write_c0_pagemask(PM_DEFAULT_MASK);
	write_c0_wired(0);

	tlbsz = (read_c0_config1() >> 25) & 0x3f;
	for (i = 0; i <= tlbsz; i++) {
		write_c0_entryhi(UNIQUE_ENTRYHI(i));
		write_c0_index(i);
		mtc0_tlbw_hazard();
		tlb_write_indexed();
		tlbw_use_hazard();
	}

	write_c0_wired(0);
	mtc0_tlbw_hazard();

	for (i = 0; i < ARRAY_SIZE(wired_mappings); i++) {
		struct tlb_entry *e = &wired_mappings[i];
		pic32mz_add_wired_entry(e->entrylo0, e->entrylo1, e->entryhi,
			e->pagemask);
	}

	write_c0_pagemask(PM_DEFAULT_MASK);
}

#define PIC32_PRECON		0x00
#define PIC32_CFGCON		0x00

static void prefetch_setup(void)
{
	bool ecc;
	int ws;
	void __iomem *prefetch_base =
		ioremap(PIC32_BASE_PREFETCH, sizeof(u32));
	void __iomem *config_base =
		ioremap(PIC32_BASE_CONFIG, sizeof(u32));
	/* TODO: hard coded */
	u32 sysclk = 200000000 /* pic32_get_pbclk(2) */;

	ecc = (((__raw_readl(config_base + PIC32_CFGCON) &
				0x00000030) >> 4) < 2) ? true : false;
	if (sysclk <= (ecc ? 66000000 : 83000000))
		ws = 0;
	else if (sysclk <= (ecc ? 133000000 : 166000000))
		ws = 1;
	else
		ws = 2;

	/* set wait states */
	__raw_writel(ws, prefetch_base + PIC32_PRECON);

	/* enable */
	__raw_writel(0x30, prefetch_base + PIC32_SET(PIC32_PRECON));
}

extern void run_ebi_sram_test(u32 sram_size);

#if defined(CONFIG_PIC32MZ_PLANC) || defined(CONFIG_PIC32MZ_PLAND)
#define EBI_SRAM_SIZE SZ_16M
#else
#define EBI_SRAM_SIZE SZ_8M
#endif

void decompress_kernel(unsigned long boot_heap_start)
{
	unsigned long zimage_start, zimage_size;

	prefetch_setup();
	ebi_setup();
	tlb_setup();

/* #define EBI_SRAM_TEST */
#ifdef EBI_SRAM_TEST
	run_ebi_sram_test(EBI_SRAM_SIZE);
#endif

	zimage_start = (unsigned long)(&__image_begin);
	zimage_size = (unsigned long)(&__image_end) -
	    (unsigned long)(&__image_begin);

	puts("zimage at:     ");
	puthex(zimage_start);
	puts(" ");
	puthex(zimage_size + zimage_start);
	puts("\n");

	/* This area are prepared for mallocing when decompressing */
	free_mem_ptr = boot_heap_start;
	free_mem_end_ptr = boot_heap_start + BOOT_HEAP_SIZE;

	/* Display standard Linux/MIPS boot prompt */
	puts("Uncompressing Linux at load address ");
	puthex(VMLINUX_LOAD_ADDRESS_ULL);
	puts("\n");

	/* Decompress the kernel with according algorithm */
	decompress((char *)zimage_start, zimage_size, 0, 0,
		   (void *)VMLINUX_LOAD_ADDRESS_ULL, 0, error);

	/* FIXME: should we flush cache here? */
	puts("Now, booting the kernel...\n");
}
