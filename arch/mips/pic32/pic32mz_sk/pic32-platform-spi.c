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
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/gpio.h>

#ifdef CONFIG_SPI_GPIO_PIC32MZ
static struct platform_device pic32_spi_device = {
	.name = "spi_gpio_pic32mz",
	.id = 0,
};
#elif defined(CONFIG_SPI_GPIO)
static struct spi_gpio_platform_data spi_gpio_data = {
	.sck = 36, /* J1-103 */
	.mosi = 163, /* J1-102 */
	.miso = 234, /* J1-110 */
	.num_chipselect = 1,
};

static struct platform_device pic32_spi_device = {
	.name = "spi_gpio",
	.id = 0,
	.dev = {
		.platform_data = &spi_gpio_data,
	},
};
#endif

static struct spi_board_info pic32_spi_devices[] = {
	{
		.modalias       = "enc28j60",
		.max_speed_hz   = 12000000,
		.bus_num        = 0,
		.chip_select    = 0,
		.mode           = SPI_MODE_0,
		.controller_data = (void *)97, /* (CS) J1-118 */
	},
};

#define INTCON		0x00
#define RPB3R		0x10
#define INT4EP		(1 << 4)
#define INT3EP		(1 << 3)
#define INT2EP		(1 << 2)
#define INT1EP		(1 << 1)
#define INT0EP		(1 << 0)

static void __init setup_spi_irq(void)
{
	void __iomem *pps_base = ioremap(PIC32_BASE_PPS, 0x400);
	void __iomem *int_base = ioremap(PIC32_BASE_INT, 0x1000);

	/* PPS for EXTERNAL_INTERRUPT_4 */
	gpio_direction_input(35);
	__raw_writel(0x8, pps_base + RPB3R);
	__raw_writel(__raw_readl(int_base + INTCON) & ~INT4EP, int_base + INTCON);
	/* configure IRQ */
	pic32_spi_devices[0].irq = EXTERNAL_INTERRUPT_4;
	irq_set_irq_type(pic32_spi_devices[0].irq, IRQ_TYPE_EDGE_FALLING);

	iounmap(pps_base);
	iounmap(int_base);
};

static int __init pic32mz_add_spi(void)
{
	setup_spi_irq();
	platform_device_register(&pic32_spi_device);
	spi_register_board_info(pic32_spi_devices,
				ARRAY_SIZE(pic32_spi_devices));

	return 0;
}

device_initcall(pic32mz_add_spi);
