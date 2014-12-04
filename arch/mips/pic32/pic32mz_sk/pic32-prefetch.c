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
#include <asm/mach-pic32/pic32.h>
#include <asm/mach-pic32/common.h>

#define PIC32_PRECON		0x00
#define PIC32_CFGCON		0x00

void __init setup_prefetch(void)
{
	bool ecc;
	int ws;
	void __iomem *prefetch_base =
		ioremap(PIC32_BASE_PREFETCH, sizeof(u32));
	void __iomem *config_base =
		ioremap(PIC32_BASE_CONFIG, sizeof(u32));
	u32 sysclk = pic32_get_cpuclk();

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

	iounmap(prefetch_base);
	iounmap(config_base);
}
