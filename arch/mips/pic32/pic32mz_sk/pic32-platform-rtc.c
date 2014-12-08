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
#include <asm/mach-pic32/pic32int.h>

static struct resource rtc_resource[] = {
	{
		.start	= PIC32_BASE_RTC,
		.end	= PIC32_BASE_RTC + 0x60 - 1,
		.flags	= IORESOURCE_MEM,
	}, {
		.start = REAL_TIME_CLOCK,
		.end = REAL_TIME_CLOCK,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device pic32_rtc_device = {
	.name		= "pic32-rtc",
	.id		= -1,
	.resource	= rtc_resource,
	.num_resources	= ARRAY_SIZE(rtc_resource),
};

static int __init pic32mz_add_rtc(void)
{
	platform_device_register(&pic32_rtc_device);

	return 0;
}

device_initcall(pic32mz_add_rtc);
