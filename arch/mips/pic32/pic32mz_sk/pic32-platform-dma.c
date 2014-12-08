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
#include <linux/dma-mapping.h>
#include <linux/sizes.h>
#include <asm/mach-pic32/pic32.h>
#include <asm/mach-pic32/pic32int.h>

static struct resource dma_resource[] = {
	{
		.start	= PIC32_BASE_DMA,
		.end	= PIC32_BASE_DMA + SZ_8K - 1,
		.flags	= IORESOURCE_MEM,
	}, {
		.start = DMA_CHANNEL_0,
		.end =   DMA_CHANNEL_0,
		.flags = IORESOURCE_IRQ,
	}, {
		.start = DMA_CHANNEL_1,
		.end =   DMA_CHANNEL_1,
		.flags = IORESOURCE_IRQ,
	}, {
		.start = DMA_CHANNEL_2,
		.end =   DMA_CHANNEL_2,
		.flags = IORESOURCE_IRQ,
	}, {
		.start = DMA_CHANNEL_3,
		.end =   DMA_CHANNEL_3,
		.flags = IORESOURCE_IRQ,
	}, {
		.start = DMA_CHANNEL_4,
		.end =   DMA_CHANNEL_4,
		.flags = IORESOURCE_IRQ,
	}, {
		.start = DMA_CHANNEL_5,
		.end =   DMA_CHANNEL_5,
		.flags = IORESOURCE_IRQ,
	}, {
		.start = DMA_CHANNEL_6,
		.end =   DMA_CHANNEL_6,
		.flags = IORESOURCE_IRQ,
	}, {
		.start = DMA_CHANNEL_7,
		.end =   DMA_CHANNEL_7,
		.flags = IORESOURCE_IRQ,
	},
};

static u64 dma_mask = DMA_BIT_MASK(32);
static struct platform_device pic32_dma_device = {
	.name		= "pic32-dma",
	.id		= 0,
	.dev		= {
				.dma_mask		= &dma_mask,
				.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
	.resource	= dma_resource,
	.num_resources	= ARRAY_SIZE(dma_resource),
};

static int __init pic32mz_add_dma(void)
{
	platform_device_register(&pic32_dma_device);

	return 0;
}

device_initcall(pic32mz_add_dma);
