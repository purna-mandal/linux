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
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <asm/mach-pic32/pic32.h>
#include <asm/mach-pic32/pic32-pps.h>

static int __init pic32mz_add_devices(void)
{
#ifdef CONFIG_SND_PIC32_SOC_I2S

#ifdef CONFIG_SND_PIC32_SOC_CODEC_PROTO
	/* SCK1 on RPD1/J1-118 */
	gpio_direction_input(97);

	/* SDI1 on RPD14/J1-94 */
	gpio_direction_input(110);
	pic32_pps_input(IN_FUNC_SDI1, IN_RPD14);

	/* SDO1 on RPB10/J1-117 */
	gpio_direction_input(42);
	pic32_pps_output(OUT_FUNC_SDO1, OUT_RPB10);

	/* SS1 on RPB15/J1-90 */
	gpio_direction_input(47);
	pic32_pps_input(IN_FUNC_SS1, IN_RPB15);
#endif

#ifdef CONFIG_SND_PIC32_SOC_CODEC_AK4953A
	/* SDA2 - RA3/J1-82 */
	/* SCL2 - RA2/J1-112 */

	/* REFCLK - RD15/J1-96 */
	gpio_direction_output(111, 0);
	pic32_pps_output(OUT_FUNC_REFCLKO1, OUT_RPD15);

	/* SS1 - RB15/J1-90 */
	gpio_direction_output(47, 0);
	pic32_pps_output(OUT_FUNC_SS1, OUT_RPB15);

	/* Audio PDN - RH3/J1-64 */
	gpio_direction_output(227, 0);
	mdelay(10);
	gpio_set_value(227, 1);
	mdelay(10);
#endif

#endif

	return 0;
}

device_initcall(pic32mz_add_devices);
