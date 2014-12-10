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
#include <asm/mach-pic32/pic32.h>

static struct resource rng_resource[]  = {
	{
		.start	= PIC32_BASE_RNG,
		.end	= PIC32_BASE_RNG + 0x1000 - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device pic32_rng_device = {
	.name		= "pic32-rng",
	.id		= -1,
	.resource	= rng_resource,
	.num_resources	= ARRAY_SIZE(rng_resource),
};

static int __init pic32mz_add_rng(void)
{
	platform_device_register(&pic32_rng_device);

	return 0;
}

device_initcall(pic32mz_add_rng);
