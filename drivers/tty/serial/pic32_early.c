/*
 * PIC32 Integrated earlyprintk serial driver.
 *
 * Copyright (C) 2014 Microchip Technology, Inc.
 *
 * Authors:
 *   Sorin-Andrei Pistirica <andrei.pistirica@microchip.com>
 *
 * Licensed under GPLv2 or later.
 */
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/console.h>
#include <linux/clk.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/sysrq.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <uapi/linux/serial_core.h>
#include <asm/mach-pic32/common.h> /* pic32_get_pbclk */

#include <linux/io.h>

#include "pic32_uart.h"
#include "pic32_early.h"

#define PIC32_EARLY_SDEV_NAME "pic32-early"

/* only boot memory allocator available! */
static struct pic32_sport pic32_early_sports[PIC32_EARLY_MAX_PORTS];

static struct console pic32_early_console;
static int __init parse_earlyprintk_cmdline(char *cmdline)
{
	char *s = strstr(cmdline, "ttyS");
	struct pic32_sport *sport;
	struct pic32_console_opt *opt;
	struct uart_port *port;
	int idx = 0;
	int ret = 0;

	/* parse options */
	if (s != NULL) {
		s += 4;
		idx = (*s++) - '0';
		if (idx < PIC32_EARLY_MAX_PORTS && *s++ == ',') {
			sport = &pic32_early_sports[idx];
			sport->idx = idx;

			pic32_early_console.data = sport;
			opt = pic32_get_opt(sport);
			port = pic32_get_port(sport);

			/* at least baudrate must be specified */
			uart_parse_options(s,
				&opt->baud,
				&opt->parity,
				&opt->bits,
				&opt->flow);
		} else {
			ret = -ENODEV;
			goto _out;
		}
	} else {
		ret = -ENODEV;
		goto _out;
	}

	/* set membase */
	switch (idx) {
	case 0:
		port->membase = (void __iomem *)
					PIC32_EARLY_CON_PORT0_MEMBASE;
		break;
	case 1:
		port->membase = (void __iomem *)
					PIC32_EARLY_CON_PORT1_MEMBASE;
		break;
	default:
		/* only ttyS0 or ttyS1 may be specified! */
		ret = -ENODEV;
		goto _out;
	}

_out:
	return ret;
}
early_param("earlyprintk", parse_earlyprintk_cmdline);

static void __init pic32_early_console_wait_for_xmitr(struct uart_port *port)
{
	struct pic32_sport *sport = to_pic32_sport(port);

	for(;;) {
		u32 status = pic32_uart_read(sport, PIC32_UART_STA);
		if (!(status & PIC32_UART_STA_UTXBF))
			return;

		cpu_relax();
	}
}

static void __init pic32_early_console_putc(struct uart_port *port, int c)
{
	struct pic32_sport *sport = to_pic32_sport(port);

	pic32_early_console_wait_for_xmitr(port);
	pic32_uart_write(c, sport, PIC32_UART_TX);
}

static void __init pic32_early_console_write(struct console *co,
					     const char *s, unsigned int count)
{
	struct pic32_sport *sport = co->data;
	struct uart_port *port;

	if (!sport)
		return;

	port = pic32_get_port(sport);
	uart_console_write(port, s, count, pic32_early_console_putc);
	pic32_early_console_wait_for_xmitr(port);
}

static struct console pic32_early_console __initdata = {
	.name	= PIC32_EARLY_SDEV_NAME,
	.write	= pic32_early_console_write,
	.flags	= CON_PRINTBUFFER | CON_ENABLED | CON_BOOT,
	.index	= -1,
	.data	= NULL,
};

int __init pic32_earlyprintk_setup(void)
{
	struct pic32_sport *sport = pic32_early_console.data;
	struct uart_port *port;
	struct pic32_console_opt *opt;
	u32 pbclk = pic32_get_pbclk(2);
	u32 baud = 0;
	int ret = 0;

	if (!sport) {
		ret = -ENODEV;
		goto _out;
	}

	port = pic32_get_port(sport);
	opt = pic32_get_opt(sport);

	if (!port->membase) {
		ret = -ENODEV;
		goto _out;
	}

	/* clear status and mode registers */
	pic32_uart_write(0, sport, PIC32_UART_MODE);
	pic32_uart_write(0, sport, PIC32_UART_STA);

	/* set baud rate */
	baud = ((pbclk / opt->baud) / 16) - 1;
	pic32_uart_write(baud, sport, PIC32_UART_BRG);

	/* enable serial port */
	pic32_uart_write(PIC32_UART_MODE_ON, sport, PIC32_UART_MODE);
	pic32_uart_rset(PIC32_UART_STA_UTXEN | PIC32_UART_STA_URXEN,
			sport, PIC32_UART_STA);

	register_console(&pic32_early_console);
_out:
	return ret;
}
