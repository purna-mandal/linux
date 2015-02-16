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
#include <linux/gpio.h>

static struct resource i2s_resource0[] = {
	{
		.start	= PIC32_BASE_SPI1,
		.end	= PIC32_BASE_SPI1 + 512 - 1,
		.flags	= IORESOURCE_MEM,
	}, {
		.start = SPI1_RECEIVE_DONE,
		.end =   SPI1_RECEIVE_DONE,
		.flags = IORESOURCE_IRQ,
	}, {
		.start = SPI1_TRANSFER_DONE,
		.end =   SPI1_TRANSFER_DONE,
		.flags = IORESOURCE_IRQ,
	},
};

static struct resource i2s_resource1[] = {
	{
		.start	= PIC32_BASE_SPI2,
		.end	= PIC32_BASE_SPI2 + 512 - 1,
		.flags	= IORESOURCE_MEM,
	}, {
		.start = SPI2_RECEIVE_DONE,
		.end =   SPI2_RECEIVE_DONE,
		.flags = IORESOURCE_IRQ,
	}, {
		.start = SPI2_TRANSFER_DONE,
		.end =   SPI2_TRANSFER_DONE,
		.flags = IORESOURCE_IRQ,
	},
};

static struct resource i2s_resource2[] = {
	{
		.start	= PIC32_BASE_SPI3,
		.end	= PIC32_BASE_SPI3 + 512 - 1,
		.flags	= IORESOURCE_MEM,
	}, {
		.start = SPI3_RECEIVE_DONE,
		.end =   SPI3_RECEIVE_DONE,
		.flags = IORESOURCE_IRQ,
	}, {
		.start = SPI3_TRANSFER_DONE,
		.end =   SPI3_TRANSFER_DONE,
		.flags = IORESOURCE_IRQ,
	},
};

static struct resource i2s_resource3[] = {
	{
		.start	= PIC32_BASE_SPI4,
		.end	= PIC32_BASE_SPI4 + 512 - 1,
		.flags	= IORESOURCE_MEM,
	}, {
		.start = SPI4_RECEIVE_DONE,
		.end =   SPI4_RECEIVE_DONE,
		.flags = IORESOURCE_IRQ,
	}, {
		.start = SPI4_TRANSFER_DONE,
		.end =   SPI4_TRANSFER_DONE,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device pic32_i2s_device0 = {
	.name		= "pic32-i2s",
	.id		= 0,
	.resource	= i2s_resource0,
	.num_resources	= ARRAY_SIZE(i2s_resource0),
};

static struct platform_device pic32_i2s_device1 = {
	.name		= "pic32-i2s",
	.id		= 1,
	.resource	= i2s_resource1,
	.num_resources	= ARRAY_SIZE(i2s_resource1),
};

static struct platform_device pic32_i2s_device2 = {
	.name		= "pic32-i2s",
	.id		= 2,
	.resource	= i2s_resource2,
	.num_resources	= ARRAY_SIZE(i2s_resource2),
};

static struct platform_device pic32_i2s_device3 = {
	.name		= "pic32-i2s",
	.id		= 3,
	.resource	= i2s_resource3,
	.num_resources	= ARRAY_SIZE(i2s_resource3),
};

#ifdef CONFIG_SND_PIC32_SOC_CODEC_PROTO
static struct platform_device snd_proto_device = {
	.name = "snd-pic32-proto",
	.id = 0,
	.num_resources = 0,
};
#endif

#ifdef CONFIG_SND_PIC32_SOC_CODEC_AK4953A
static struct platform_device snd_ak4953a_device = {
	.name = "snd-pic32-ak4953a",
	.id = 0,
	.num_resources = 0,
};
#endif

static struct platform_device *pic32_i2s_devices[] __initdata = {
	&pic32_i2s_device0,
	&pic32_i2s_device1,
	&pic32_i2s_device2,
	&pic32_i2s_device3,
#ifdef CONFIG_SND_PIC32_SOC_CODEC_PROTO
	&snd_proto_device,
#endif
#ifdef CONFIG_SND_PIC32_SOC_CODEC_AK4953A
	&snd_ak4953a_device,
#endif
};

#define SDI1R 0x9C
#define RPB10R 0x168
#define SS1R 0xA0
#define RPB15R 0x57c
#define RPB3R 0x14c
#define RPD11R 0x1ec
#define RPD15R 0x1fc

static int __init pic32mz_add_i2s(void)
{
	void __iomem *pps_base = (void __iomem*)(unsigned long)(PIC32_BASE_PPS);

	/* SCK1 on RPD1/J1-118 */
	gpio_direction_input(97);

	/* SDI1 on RPD14/J1-94 */
	gpio_direction_input(110);
	__raw_writel(0xB, pps_base + SDI1R);

	/* SDO1 on RPB10/J1-117 */
	gpio_direction_input(42);
	__raw_writel(0x5, pps_base + RPB10R);

	/* SS1 on RPB15/J1-90 */
	gpio_direction_input(47);
	__raw_writel(0x3, pps_base + SS1R);

	platform_add_devices(pic32_i2s_devices,
			ARRAY_SIZE(pic32_i2s_devices));

	return 0;
}

device_initcall(pic32mz_add_i2s);
