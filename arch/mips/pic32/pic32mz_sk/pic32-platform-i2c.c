/*
 * Joshua Henderson, <joshua.henderson@microchip.com>
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
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <asm/mach-pic32/pic32.h>
#include <asm/mach-pic32/pic32int.h>
#include <linux/i2c.h>
#include <linux/platform_data/at24.h>
#include <linux/bma150.h>
#include <linux/gpio.h>
#include <asm/mach-pic32/pic32-pps.h>

static struct resource i2c_resource0[] = {
	{
		.start	= PIC32_BASE_I2C1,
		.end	= PIC32_BASE_I2C1 + 512 - 1,
		.flags	= IORESOURCE_MEM,
	}, {
		.start = I2C1_MASTER_EVENT,
		.end =   I2C1_MASTER_EVENT,
		.flags = IORESOURCE_IRQ,
	},
};

static struct resource i2c_resource1[] = {
	{
		.start	= PIC32_BASE_I2C2,
		.end	= PIC32_BASE_I2C2 + 512 - 1,
		.flags	= IORESOURCE_MEM,
	}, {
		.start = I2C2_MASTER_EVENT,
		.end =   I2C2_MASTER_EVENT,
		.flags = IORESOURCE_IRQ,
	},
};

static struct resource i2c_resource2[] = {
	{
		.start	= PIC32_BASE_I2C3,
		.end	= PIC32_BASE_I2C3 + 512 - 1,
		.flags	= IORESOURCE_MEM,
	}, {
		.start = I2C3_MASTER_EVENT,
		.end =   I2C3_MASTER_EVENT,
		.flags = IORESOURCE_IRQ,
	},
};

static struct resource i2c_resource3[] = {
	{
		.start	= PIC32_BASE_I2C4,
		.end	= PIC32_BASE_I2C4 + 512 - 1,
		.flags	= IORESOURCE_MEM,
	}, {
		.start = I2C4_MASTER_EVENT,
		.end =   I2C4_MASTER_EVENT,
		.flags = IORESOURCE_IRQ,
	},
};

static struct resource i2c_resource4[] = {
	{
		.start	= PIC32_BASE_I2C5,
		.end	= PIC32_BASE_I2C5 + 512 - 1,
		.flags	= IORESOURCE_MEM,
	}, {
		.start = I2C5_MASTER_EVENT,
		.end =   I2C5_MASTER_EVENT,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device pic32_i2c_device0 = {
	.name		= "pic32-i2c",
	.id		= 0,
	.resource	= i2c_resource0,
	.num_resources	= ARRAY_SIZE(i2c_resource0),
};

static struct platform_device pic32_i2c_device1 = {
	.name		= "pic32-i2c",
	.id		= 1,
	.resource	= i2c_resource1,
	.num_resources	= ARRAY_SIZE(i2c_resource1),
};

static struct platform_device pic32_i2c_device2 = {
	.name		= "pic32-i2c",
	.id		= 2,
	.resource	= i2c_resource2,
	.num_resources	= ARRAY_SIZE(i2c_resource2),
};

static struct platform_device pic32_i2c_device3 = {
	.name		= "pic32-i2c",
	.id		= 3,
	.resource	= i2c_resource3,
	.num_resources	= ARRAY_SIZE(i2c_resource3),
};

static struct platform_device pic32_i2c_device4 = {
	.name		= "pic32-i2c",
	.id		= 4,
	.resource	= i2c_resource4,
	.num_resources	= ARRAY_SIZE(i2c_resource4),
};

static struct platform_device *pic32_i2c_devices[] __initdata = {
	&pic32_i2c_device0,
	&pic32_i2c_device1,
	&pic32_i2c_device2,
	&pic32_i2c_device3,
	&pic32_i2c_device4,
};

static struct at24_platform_data at24_plat = {
	.byte_len = 256 * 1024,
	.page_size = 64,
	.flags = AT24_FLAG_ADDR16,
};

static struct i2c_board_info pic32_i2c_board_info0[] = {
	{
		/* External RTC */
		I2C_BOARD_INFO("mcp7940", 0x6F)
	},
	{
		/* EEPROM */
		I2C_BOARD_INFO("at24", 0x50), /* E0=0, E1=0, E2=0 */
		.platform_data = &at24_plat,
	},
	{
		/* Accelerometer */
		I2C_BOARD_INFO("bma150", 0x38),
	},
#if defined(CONFIG_SND_SOC_WM8731)
	{
		/* Audio Codec */
                I2C_BOARD_INFO("wm8731", 0x1a)
	},
#endif
#if defined(CONFIG_SND_SOC_AK4953)
	{
		/* Audio Codec */
		I2C_BOARD_INFO("ak4953a", 0x12),
	},
#endif
};

static struct i2c_board_info pic32_i2c_board_info1[] = {
};

static int __init pic32mz_add_i2c(void)
{
	platform_add_devices(pic32_i2c_devices,
			ARRAY_SIZE(pic32_i2c_devices));

	i2c_register_board_info(0, pic32_i2c_board_info0,
				ARRAY_SIZE(pic32_i2c_board_info0));

	i2c_register_board_info(1, pic32_i2c_board_info1,
				ARRAY_SIZE(pic32_i2c_board_info1));

	/* SDA2 - RA3/J1-82 */
	/* SCL2 - RA2/J1-112 */

	/* REFCLK - RD15/J1-96 */
	gpio_direction_output(111,0);
	pic32_pps_output(OUT_FUNC_REFCLKO1, OUT_RPD15);

	/* SS1 - RB15/J1-90 */
	gpio_direction_output(47,0);
	pic32_pps_output(OUT_FUNC_SS1, OUT_RPB15);

	/* Audio PDN - RH3/J1-64 */
	gpio_direction_output(227, 0);
	udelay(10000);
	gpio_set_value(227, 1);
	udelay(10000);

	return 0;
}

device_initcall(pic32mz_add_i2c);
