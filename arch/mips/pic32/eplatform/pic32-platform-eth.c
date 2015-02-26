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

static int __init pic32mz_add_eth(void)
{
	void __iomem *port_base = ioremap(PIC32_BASE_PORT, 0xA00);

	BUG_ON(!port_base);

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

	iounmap(port_base);

	return 0;
}

device_initcall(pic32mz_add_eth);
