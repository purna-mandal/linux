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
#include <linux/platform_device.h>
#include <linux/platform_data/pic32ether.h>
#include <linux/dma-mapping.h>
#include <linux/sizes.h>
#include <asm/mach-pic32/pic32.h>
#include <asm/mach-pic32/pic32int.h>

static u64 eth_dmamask = DMA_BIT_MASK(32);
static struct pic32ether_platform_data eth_data;

static struct resource eth_resources[] = {
	[0] = {
		.start	= PIC32_BASE_ETHER,
		.end	= PIC32_BASE_ETHER + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= ETHERNET_INTERRUPT,
		.end	= ETHERNET_INTERRUPT,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device pic32_eth_device __initdata = {
	.name		= "pic32-ether",
	.id		= -1,
	.dev		= {
				.dma_mask		= &eth_dmamask,
				.coherent_dma_mask	= DMA_BIT_MASK(32),
				.platform_data		= &eth_data,
	},
	.resource	= eth_resources,
	.num_resources	= ARRAY_SIZE(eth_resources),
};

void __init pic32_add_device_eth(struct pic32ether_platform_data *data)
{
	void __iomem *port_base = ioremap(PIC32_BASE_PORT, 0xA00);

	BUG_ON(!port_base);

	if (!data)
		return;

	/*
	 * PORT D pin configuration settings
	 *
	 * Reg   Bit  I/O    Dig/Ana
	 * EMDC  RD11 Output Digital
	 * ETXEN RD6  Output Digital
	 *
	 */
	__raw_writel(0x0840, port_base + PIC32_CLR(ANSELx(3))); /* digital */
	__raw_writel(0x0840, port_base + PIC32_CLR(TRISx(3))); /* output */

	/*
	 * PORT H pin configuration settings
	 *
	 * Reg    Bit  I/O    Dig/Ana
	 * ECRSDV RH13 Input  Digital
	 * ERXD0  RH8  Input  Digital
	 * ERXD1  RH5  Input  Digital
	 * ERXERR RH4  Input  Digital
	 */
	__raw_writel(0x2130, port_base + PIC32_CLR(ANSELx(7))); /* digital */
	__raw_writel(0x2130, port_base + PIC32_SET(TRISx(7))); /* input */

	/*
	 * PORT J pin configuration settings
	 *
	 * Reg     Bit  I/O    Dig/Ana
	 * EREFCLK RJ11 Input  Digital
	 * ETXD1   RJ9  Output Digital
	 * ETXD0   RJ8  Output Digital
	 * EMDIO   RJ1  Input  Digital
	 *
	 */
	__raw_writel(0x0b02, port_base + PIC32_CLR(ANSELx(8))); /* digital */
	__raw_writel(0x0300, port_base + PIC32_CLR(TRISx(8))); /* output */
	__raw_writel(0x0802, port_base + PIC32_SET(TRISx(8))); /* input */

	eth_data = *data;
	platform_device_register(&pic32_eth_device);

	iounmap(port_base);
}

static struct pic32ether_platform_data eth_data __initdata = {
	.is_rmii	= 1,
	.phy_mask	= 0xfffffff0,
};

static int __init pic32mz_add_eth(void)
{
	pic32_add_device_eth(&eth_data);

	return 0;
}

device_initcall(pic32mz_add_eth);
