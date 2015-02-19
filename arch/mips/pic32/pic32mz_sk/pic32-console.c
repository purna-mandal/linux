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
#include <asm/mach-pic32/early_pinctrl.h>
#include <asm/fw/fw.h>

/* uart bits */
#define UART_ENABLE           (1 << 15)
#define UART_ENABLE_RX        (1 << 12)
#define UART_ENABLE_TX        (1 << 10)
#define UART_TX_FULL          (1 << 9)

#define DEFAULT_EARLY_CON_BAUDRATE (115200)

static void __iomem *uart_base;
static char console_port = -1;

static void __init enable_uart_with_rate(char port, int baud)
{
	u32 pbclk = pic32_get_pbclk(2);
	void __iomem *port_base = ioremap(PIC32_BASE_PORT, 0xa00);

	BUG_ON(!port_base);

	__raw_writel(0, uart_base + UxMODE(port));
	__raw_writel(((pbclk / baud) / 16) - 1,
		uart_base + UxBRG(port));
	__raw_writel(UART_ENABLE, uart_base + UxMODE(port));
	__raw_writel(UART_ENABLE_TX|UART_ENABLE_RX,
		uart_base + PIC32_SET(UxSTA(port)));

	iounmap(port_base);
}

static void __init setup_early_console(char port, int baud)
{
	/* early pinctrl configurations for serial ports */
	if (port == 1) {
		pic32_earlyco_port0_pinctrl();
		goto _en_uart;
	} else if (port == 2) {
		pic32_earlyco_port1_pinctrl();
		goto _en_uart;
	} else
		return;

	/* enable uart and config its rate */
_en_uart:
	enable_uart_with_rate(port, baud);
}

int __init early_console_get_baud_from_archcmdline(void)
{
	char *arch_cmdline = fw_getcmdline();
	char *s;
	int baud = -1;

	if (*arch_cmdline == '\0')
		goto _out;

	s = strstr(arch_cmdline, "earlyprintk=");
	if (s != NULL) {
		s = strstr(s, "ttyS");
		if (s != NULL)
			s += 6;
		else
			goto _out;

		baud = 0;
		while (*s >= '0' && *s <= '9')
			baud = baud*10 + *s++ - '0';
	}
_out:
	return baud;
}

void __init fw_init_early_console(char port)
{
	int baud;
	if (port != 0 && port != 1)
		return;

	uart_base = ioremap(PIC32_BASE_UART, 0xc00);
	BUG_ON(!uart_base);

	baud = early_console_get_baud_from_archcmdline();
	if (baud == -1)
		baud = DEFAULT_EARLY_CON_BAUDRATE;

	/* port starts from 0, while console_port starts from 1 */
	console_port = port + 1;
	setup_early_console(console_port, baud);
}

int prom_putchar(char c)
{
	if (unlikely(console_port == -1))
		return 0;

	while (__raw_readl(uart_base + UxSTA(console_port)) & UART_TX_FULL)
		cpu_relax();

	__raw_writel(c, uart_base + UxTXREG(console_port));

	return 1;
}
