/*
 * Joshua Henderson, joshua.henderson@microchip.com
 * Copyright (C) 2014 Microchip Technology Inc.  All rights reserved.
 *
 * This program is free software; you can distribute it and/or modify it
 * under the terms of the GNU General Public License (Version 2) as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
#include <asm/mach-pic32/pic32.h>

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

#define EBI_SRAM	0x20 /* memory type */

void __init setup_ebi_sram(void)
{
	void __iomem *ebi_base = ioremap(PIC32_BASE_EBI, 0x1000);
	void __iomem *config_base = ioremap(PIC32_BASE_CONFIG, 0x400);
	int mem_sz = SZ_2M;
	int mem_sz_mask = 6;

	BUG_ON(!config_base);
	BUG_ON(!ebi_base);

#if defined(CONFIG_PIC32MZ_PLANC) || defined(CONFIG_PIC32MZ_PLAND)
	mem_sz = SZ_4M;
	mem_sz_mask = 7;
#endif
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
	__raw_writel(0x20000000, ebi_base + EBICS0);
	__raw_writel(0x20000000 + (mem_sz * 1), ebi_base + EBICS1);
	__raw_writel(0x20000000 + (mem_sz * 2), ebi_base + EBICS2);
	__raw_writel(0x20000000 + (mem_sz * 3), ebi_base + EBICS3);

	/*
	 * Memory size is set as 2 MB
	 * Memory type is set as SRAM
	 * Uses timing numbers in EBISMT0
	 */
	__raw_writel(EBI_SRAM|mem_sz_mask, ebi_base + EBIMSK0);
	__raw_writel(EBI_SRAM|mem_sz_mask, ebi_base + EBIMSK1);
	__raw_writel(EBI_SRAM|mem_sz_mask, ebi_base + EBIMSK2);
	__raw_writel(EBI_SRAM|mem_sz_mask, ebi_base + EBIMSK3);

	/*
	 * Configure EBISMT0
	 * ISSI device has read cycles time of 6 ns
	 * ISSI device has address setup time of 0ns
	 * ISSI device has address/data hold time of 2.5 ns
	 * ISSI device has Write Cycle Time of 6 ns
	 * Bus turnaround time is 0 ns
	 * No page mode
	 * No page size
	 * No RDY pin
	 */
#if defined(CONFIG_PIC32MZ_PLANB)
	__raw_writel(1 << 10 | 1 << 8 | 1 << 6 | 2, ebi_base + EBISMT0);
	__raw_writel(0, ebi_base + EBISMT1);
#elif defined(CONFIG_PIC32MZ_PLAND)
	__raw_writel(1 << 10 | 1 << 8 | 1 << 6 | 2, ebi_base + EBISMT0);
	__raw_writel(1 << 10 | 1 << 8 | 1 << 6 | 3, ebi_base + EBISMT1);
#else
	__raw_writel(2 << 10 | 1 << 8 | 1 << 6 | 7, ebi_base + EBISMT0);
	__raw_writel(0, ebi_base + EBISMT1);
#endif
	__raw_writel(0, ebi_base + EBISMT2);

	/*
	 * Keep default data width to 16-bits
	 */
	__raw_writel(0x00000000, ebi_base + EBISMCON);

	iounmap(config_base);
	iounmap(ebi_base);
}

static __init void write_pattern(u32 p, u32 sram_size)
{
	u32 *addr = (u32 *)KSEG2;
	u32 loop;

	pr_info("EBI SRAM write test pattern 0x%x ...", p);

	for (loop = 0; loop < sram_size / 4; loop++)
		*addr++ = p;

	pr_cont("finished\n");
}

static __init void read_pattern(u32 p, u32 sram_size)
{
	u32 *addr = (u32 *)KSEG2;
	u32 loop;
	u32 val;

	pr_info("EBI SRAM read test pattern 0x%x ...", p);

	for (loop = 0; loop < sram_size / 4; loop++) {
		val = *addr++;
		if (val != p) {
			pr_cont("pattern 0x%x failed at 0x%x\n",
				p, loop * 4);
			panic("EBI SRAM is bad");
		}
	}
	pr_cont("success\n");
}

void __init run_ebi_sram_test(u32 sram_size)
{
	u32 *addr;
	u32 loop;
	u32 val;
	u32 count = 0;

	write_pattern(0xFFFFFFFF, sram_size);
	read_pattern(0xFFFFFFFF, sram_size);

	write_pattern(0x0, sram_size);
	read_pattern(0x0, sram_size);

	pr_info("EBI SRAM write test running...");

	count = 0;
	addr = (u32 *)KSEG2;

	for (loop = 0; loop < sram_size / 4; loop++)
		*addr++ = count++;

	pr_cont("finished\n");
	pr_info("EBI SRAM read test running...");

	count = 0;
	addr = (u32 *)KSEG2;
	for (loop = 0 ; loop < sram_size / 4; loop++) {
		val = *addr++;
		if (val != count) {
			pr_cont("failed at 0x%x: 0x%x != 0x%x\n",
				loop*4, val, count);
			panic("EBI SRAM is bad");
		}
		count++;
	}

	pr_cont("success\n");
}
