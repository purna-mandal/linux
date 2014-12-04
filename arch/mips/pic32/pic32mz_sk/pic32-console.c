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
#include <asm/mach-pic32/pic32.h>
#include <asm/mach-pic32/common.h>

#define UART_ENABLE           (1<<15)
#define UART_ENABLE_RX        (1<<12)
#define UART_ENABLE_TX        (1<<10)
#define UART_RX_DATA_AVAIL    (1)
#define UART_RX_OERR          (1<<1)
#define UART_TX_FULL          (1<<9)
#define UART_LOOPBACK         (1<<6)
#define DEFAULT_BAUDRATE      115200

static void __iomem *uart_base;
static char console_port;

static void __init setup_early_console(char port)
{
	u32 pbclk = pic32_get_pbclk(2);
	void __iomem *pps_base = ioremap(PIC32_BASE_PPS, 0x400);
	void __iomem *port_base = ioremap(PIC32_BASE_PORT, 0xa00);

	BUG_ON(!pps_base);
	BUG_ON(!port_base);
	BUG_ON(port != 2);

	/* PPS for U2 RX/TX */
	__raw_writel(0x4000, port_base + PIC32_CLR(ANSELx(1)));
	__raw_writel(0x0040, port_base + PIC32_CLR(ANSELx(6)));
	__raw_writel(0x02, pps_base + REG_RPB14R);
	__raw_writel(0x01, pps_base + REG_U2RXR);

	__raw_writel(0, uart_base + UxMODE(port));
	__raw_writel(((pbclk / DEFAULT_BAUDRATE) / 16) - 1,
		uart_base + UxBRG(port));
	__raw_writel(UART_ENABLE, uart_base + UxMODE(port));
	__raw_writel(UART_ENABLE_TX|UART_ENABLE_RX,
		uart_base + PIC32_SET(UxSTA(port)));

	iounmap(pps_base);
	iounmap(port_base);
}

void __init fw_init_early_console(char port)
{
	uart_base = ioremap(PIC32_BASE_UART, 0xc00);
	BUG_ON(!uart_base);

	console_port = port;
	setup_early_console(port);
}

int prom_putchar(char c)
{
	while (__raw_readl(uart_base + UxSTA(console_port)) & UART_TX_FULL)
		;

	__raw_writel(c, uart_base + UxTXREG(console_port));

	return 1;
}
