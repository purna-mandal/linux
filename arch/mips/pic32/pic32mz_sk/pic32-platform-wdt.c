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

static struct resource wdt_resource[]  = {
	{
		.start	= PIC32_BASE_WDT,
		.end	= PIC32_BASE_WDT + 0x200 - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device pic32_wdt_device = {
	.name		= "pic32-wdt",
	.id		= -1,
	.resource	= wdt_resource,
	.num_resources	= ARRAY_SIZE(wdt_resource),
};

static int __init pic32mz_add_wdt(void)
{
	platform_device_register(&pic32_wdt_device);

	return 0;
}

device_initcall(pic32mz_add_wdt);
