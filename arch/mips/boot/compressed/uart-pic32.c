/*
 * Copyright (C) 2014 Microchip Technology Inc.  All rights reserved.
 * Author: Joshua Henderson <joshua.henderson@microchip.com>
 *
 * This program is free software; you can redistribute	it and/or modify it
 * under  the terms of	the GNU General	 Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
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

#define ioremap_fake(offset, size) ((void __iomem*)(unsigned long)offset)

#define REG_U2RXR      0x0070
#define REG_RPB14R     0x0178

static void __init setup_early_console(char port)
{
	/* TODO: hard coded */
	u32 pbclk = 100000000 /* = pic32_get_pbclk(2); */;
	void __iomem *pps_base = ioremap_fake(PIC32_BASE_PPS, 0x400);
	void __iomem *port_base = ioremap_fake(PIC32_BASE_PORT, 0xa00);

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
}

static void init_console(char port)
{
	uart_base = ioremap_fake(PIC32_BASE_UART, 0xc00);

	console_port = port;
	setup_early_console(port);
}

void putc(char c)
{
	static unsigned int init;

	if (!init) {
		init_console(CONSOLE_PORT);
		init = 1;
	}

	while (__raw_readl(uart_base + UxSTA(console_port)) & UART_TX_FULL)
		;

	__raw_writel(c, uart_base + UxTXREG(console_port));
}
