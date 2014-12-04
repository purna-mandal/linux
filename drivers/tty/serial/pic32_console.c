/*
 * PIC32 Serial Driver
 *
 * Joshua Henderson, joshua.henderson@microchip.com
 * Copyright (C) 2014 Microchip Technology Inc.  All rights reserved.
 *
 * This program is free software; you can distribute it and/or modify it
 * under the terms of the GNU General Public License (Version 2) as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/console.h>
#include <linux/module.h>
#include <linux/sysrq.h>
#include <linux/circ_buf.h>
#include <linux/serial_reg.h>
#include <linux/delay.h> /* for mdelay */
#include <linux/miscdevice.h>
#include <linux/serial_core.h>

#include <asm/io.h>
#include <asm/mach-pic32/common.h>
#include <asm/mach-pic32/pic32.h>

/*
 * This is a very basic polled (no interrupts) console/uart driver that doesn't
 * even completely setup the port and is hard coded to a single port.
 */

#define UART_ENABLE           (1<<15)
#define UART_ENABLE_RX        (1<<12)
#define UART_ENABLE_TX        (1<<10)
#define UART_RX_DATA_AVAIL    (1)
#define UART_RX_OERR          (1<<1)
#define UART_TX_FULL          (1<<9)
#define UART_LOOPBACK         (1<<6)

#define CONSOLE_UART CONSOLE_PORT

/* number of characters we can transmit to the PIC32 console at a time */
#define PIC32_MAX_CHARS 120

/* pic32_transmit_chars() calling args */
#define TRANSMIT_BUFFERED	0
#define TRANSMIT_RAW		1

/* To use dynamic numbers only and not use the assigned major and minor,
 * define the following.. */
				  /* #define USE_DYNAMIC_MINOR 1 *//* use dynamic minor number */
#define USE_DYNAMIC_MINOR 0	/* Don't rely on misc_register dynamic minor */

/* Device name we're using */
#define DEVICE_NAME "ttySG"
#define DEVICE_NAME_DYNAMIC "ttySG0"	/* need full name for misc_register */
/* The major/minor we are using, ignored for USE_DYNAMIC_MINOR */
#define DEVICE_MAJOR 204
#define DEVICE_MINOR 40

/*
 * Port definition - this kinda drives it all
 */
struct pic32_cons_port {
	struct timer_list sc_timer;
	struct uart_port sc_port;
	struct pic32_ops {
		int (*pic32_puts_raw) (const char *s, int len);
		int (*pic32_puts) (const char *s, int len);
		int (*pic32_getc) (void);
		int (*pic32_input_pending) (void);
		void (*pic32_wakeup_transmit) (struct pic32_cons_port *, int);
	} *sc_ops;
	unsigned long sc_interrupt_timeout;
	int sc_is_asynch;
};

static struct pic32_cons_port pic32_console_port;
static int pic32_process_input;
static void __iomem *uart_base;

/* Only used if USE_DYNAMIC_MINOR is set to 1 */
static struct miscdevice misc;	/* used with misc_register for dynamic */

#ifdef DEBUG
static int pic32_debug_printf(const char *fmt, ...);
#define DPRINTF(x...) pic32_debug_printf(x)
#else
#define DPRINTF(x...) do { } while (0)
#endif

/* Prototypes */
static int pic32_hw_puts_raw(const char *, int);
static int pic32_poll_getc(void);
static int pic32_poll_input_pending(void);
static void pic32_transmit_chars(struct pic32_cons_port *, int);
static void puts_raw_fixed(int (*puts_raw) (const char *s, int len),
			   const char *s, int count);

static struct pic32_ops poll_ops = {
	.pic32_puts_raw = pic32_hw_puts_raw,
	.pic32_puts = pic32_hw_puts_raw,
	.pic32_getc = pic32_poll_getc,
	.pic32_input_pending = pic32_poll_input_pending
};

/* the console does output in two distinctly different ways:
 * synchronous (raw) and asynchronous (buffered).  initally, early_printk
 * does synchronous output.  any data written goes directly to the PIC32
 * to be output (incidentally, it is internally buffered by the PIC32)
 * after interrupts and timers are initialized and available for use,
 * the console init code switches to asynchronous output.  this is
 * also the earliest opportunity to begin polling for console input.
 * after console initialization, console output and tty (serial port)
 * output is buffered and sent to the PIC32 asynchronously (either by
 * timer callback or by UART interrupt) */

/* routines for running the console in polling mode */

/**
 * pic32_poll_getc - Get a character from the console in polling mode
 *
 */
static int pic32_poll_getc(void)
{
	char ch;
	ch = readl(uart_base + UxRXREG(CONSOLE_UART));
	return ch;
}

/**
 * pic32_poll_input_pending - Check if any input is waiting - polling mode.
 *
 */
static int pic32_poll_input_pending(void)
{
	char __attribute__((unused)) throwaway;

	/* check if rcv buf overrun error has occurred */
	if (readl(uart_base + UxSTA(CONSOLE_UART)) & UART_RX_OERR) {

		/* throw away this character */
		throwaway      = readl(uart_base + UxRXREG(CONSOLE_UART));

		/* clear OERR to keep receiving */
		writel(UART_RX_OERR, uart_base + PIC32_CLR(UxSTA(CONSOLE_UART)));
	}

	return (readl(uart_base + UxSTA(CONSOLE_UART)) & UART_RX_DATA_AVAIL);
}

/**
 * pic32_hw_puts_raw - Send raw string to the console, polled or interrupt mode
 * @s: String
 * @len: Length
 *
 */
static int pic32_hw_puts_raw(const char *s, int len)
{
	int size = len;
	while (size--) {
		while (readl(uart_base + UxSTA(CONSOLE_UART)) & UART_TX_FULL)
			cpu_relax();
		writel(*s, uart_base + UxTXREG(CONSOLE_UART));
		s++;
	}
	return len;
}

/**
 * snp_type - What type of console are we?
 * @port: Port to operate with (we ignore since we only have one port)
 *
 */
static const char *snp_type(struct uart_port *port)
{
	return "PIC32";
}

/**
 * snp_tx_empty - Is the transmitter empty?  We pretend we're always empty
 * @port: Port to operate on (we ignore since we only have one port)
 *
 */
static unsigned int snp_tx_empty(struct uart_port *port)
{
	return 1;
}

/**
 * snp_stop_tx - stop the transmitter - no-op for us
 * @port: Port to operat eon - we ignore - no-op function
 *
 */
static void snp_stop_tx(struct uart_port *port)
{
}

/**
 * snp_release_port - Free i/o and resources for port - no-op for us
 * @port: Port to operate on - we ignore - no-op function
 *
 */
static void snp_release_port(struct uart_port *port)
{
}

/**
 * snp_enable_ms - Force modem status interrupts on - no-op for us
 * @port: Port to operate on - we ignore - no-op function
 *
 */
static void snp_enable_ms(struct uart_port *port)
{
}

/**
 * snp_shutdown - shut down the port - free irq and disable - no-op for us
 * @port: Port to shut down - we ignore
 *
 */
static void snp_shutdown(struct uart_port *port)
{
}

/**
 * snp_set_mctrl - set control lines (dtr, rts, etc) - no-op for our console
 * @port: Port to operate on - we ignore
 * @mctrl: Lines to set/unset - we ignore
 *
 */
static void snp_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
}

/**
 * snp_get_mctrl - get contorl line info, we just return a static value
 * @port: port to operate on - we only have one port so we ignore this
 *
 */
static unsigned int snp_get_mctrl(struct uart_port *port)
{
	return TIOCM_CAR | TIOCM_RNG | TIOCM_DSR | TIOCM_CTS;
}

/**
 * snp_stop_rx - Stop the receiver - we ignor ethis
 * @port: Port to operate on - we ignore
 *
 */
static void snp_stop_rx(struct uart_port *port)
{
}

/**
 * snp_start_tx - Start transmitter
 * @port: Port to operate on
 *
 */
static void snp_start_tx(struct uart_port *port)
{
	if (pic32_console_port.sc_ops->pic32_wakeup_transmit)
		pic32_console_port.sc_ops->pic32_wakeup_transmit(&pic32_console_port,
							     TRANSMIT_BUFFERED);
}

/**
 * snp_break_ctl - handle breaks - ignored by us
 * @port: Port to operate on
 * @break_state: Break state
 *
 */
static void snp_break_ctl(struct uart_port *port, int break_state)
{
}

/**
 * snp_startup - Start up the serial port - always return 0 (We're always on)
 * @port: Port to operate on
 *
 */
static int snp_startup(struct uart_port *port)
{
	return 0;
}

/**
 * snp_set_termios - set termios stuff - we ignore these
 * @port: port to operate on
 * @termios: New settings
 * @termios: Old
 *
 */
static void
snp_set_termios(struct uart_port *port, struct ktermios *termios,
		struct ktermios *old)
{
}

/**
 * snp_request_port - allocate resources for port - ignored by us
 * @port: port to operate on
 *
 */
static int snp_request_port(struct uart_port *port)
{
	return 0;
}

/**
 * snp_config_port - allocate resources, set up - we ignore,  we're always on
 * @port: Port to operate on
 * @flags: flags used for port setup
 *
 */
static void snp_config_port(struct uart_port *port, int flags)
{
}

/* Associate the uart functions above - given to serial core */

static struct uart_ops pic32_console_ops = {
	.tx_empty = snp_tx_empty,
	.set_mctrl = snp_set_mctrl,
	.get_mctrl = snp_get_mctrl,
	.stop_tx = snp_stop_tx,
	.start_tx = snp_start_tx,
	.stop_rx = snp_stop_rx,
	.enable_ms = snp_enable_ms,
	.break_ctl = snp_break_ctl,
	.startup = snp_startup,
	.shutdown = snp_shutdown,
	.set_termios = snp_set_termios,
	.pm = NULL,
	.type = snp_type,
	.release_port = snp_release_port,
	.request_port = snp_request_port,
	.config_port = snp_config_port,
	.verify_port = NULL,
};

/* End of uart struct functions and defines */

#ifdef DEBUG

/**
 * pic32_debug_printf - close to hardware debugging printf
 * @fmt: printf format
 *
 * This is as "close to the metal" as we can get, used when the driver
 * itself may be broken.
 *
 */
static int pic32_debug_printf(const char *fmt, ...)
{
	static char printk_buf[1024];
	int printed_len;
	va_list args;

	va_start(args, fmt);
	printed_len = vsnprintf(printk_buf, sizeof(printk_buf), fmt, args);
	puts_raw_fixed(pic32_hw_puts_raw,printk_buf, printed_len);
	va_end(args);
	return printed_len;
}
#endif				/* DEBUG */

/*
 * Interrupt handling routines.
 */

/**
 * pic32_receive_chars - Grab characters, pass them to tty layer
 * @port: Port to operate on
 * @flags: irq flags
 *
 * Note: If we're not registered with the serial core infrastructure yet,
 * we don't try to send characters to it...
 *
 */
static void
pic32_receive_chars(struct pic32_cons_port *port, unsigned long flags)
{
	int ch;
	struct tty_port *tty;

	if (!port) {
		printk(KERN_ERR "pic32_receive_chars - port NULL so can't receieve\n");
		return;
	}

	if (!port->sc_ops) {
		printk(KERN_ERR "pic32_receive_chars - port->sc_ops  NULL so can't receieve\n");
		return;
	}

	if (port->sc_port.state) {
		/* The serial_core stuffs are initilized, use them */
		tty = &port->sc_port.state->port;
	}
	else {
		/* Not registered yet - can't pass to tty layer.  */
		tty = NULL;
	}

	while (port->sc_ops->pic32_input_pending()) {
		ch = port->sc_ops->pic32_getc();
		if (ch < 0) {
			printk(KERN_ERR "pic32_console: An error occured while "
			       "obtaining data from the console (0x%0x)\n", ch);
			break;
		}

		/* record the character to pass up to the tty layer */
		if (tty) {
			if(tty_insert_flip_char(tty, ch, TTY_NORMAL) == 0)
				break;
		}
		port->sc_port.icount.rx++;
	}

	if (tty)
		tty_flip_buffer_push(tty);
}

/**
 * pic32_transmit_chars - grab characters from serial core, send off
 * @port: Port to operate on
 * @raw: Transmit raw or buffered
 *
 * Note: If we're early, before we're registered with serial core, the
 * writes are going through pic32_console_write because that's how
 * register_console has been set up.  We currently could have asynch
 * polls calling this function due to pic32_switch_to_asynch but we can
 * ignore them until we register with the serial core stuffs.
 *
 */
static void pic32_transmit_chars(struct pic32_cons_port *port, int raw)
{
	int xmit_count, tail, head, loops, ii;
	int result;
	char *start;
	struct circ_buf *xmit;

	if (!port)
		return;

	BUG_ON(!port->sc_is_asynch);

	if (port->sc_port.state) {
		/* We're initilized, using serial core infrastructure */
		xmit = &port->sc_port.state->xmit;
	} else {
		/* Probably pic32_switch_to_asynch has been run but serial core isn't
		 * initilized yet.  Just return.  Writes are going through
		 * pic32_console_write (due to register_console) at this time.
		 */
		return;
	}


	if (uart_circ_empty(xmit) || uart_tx_stopped(&port->sc_port)) {
		/* Nothing to do. */
		return;
	}

	head = xmit->head;
	tail = xmit->tail;
	start = &xmit->buf[tail];

	/* twice around gets the tail to the end of the buffer and
	 * then to the head, if needed */
	loops = (head < tail) ? 2 : 1;

	for (ii = 0; ii < loops; ii++) {
		xmit_count = (head < tail) ?
		    (UART_XMIT_SIZE - tail) : (head - tail);

		if (xmit_count > 0) {
			if (raw == TRANSMIT_RAW)
				result =
				    port->sc_ops->pic32_puts_raw(start,
							       xmit_count);
			else
				result =
				    port->sc_ops->pic32_puts(start, xmit_count);
#ifdef DEBUG
			if (!result)
				DPRINTF("`");
#endif
			if (result > 0) {
				xmit_count -= result;
				port->sc_port.icount.tx += result;
				tail += result;
				tail &= UART_XMIT_SIZE - 1;
				xmit->tail = tail;
				start = &xmit->buf[tail];
			}
		}
	}

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(&port->sc_port);

	if (uart_circ_empty(xmit))
		snp_stop_tx(&port->sc_port);	/* no-op for us */
}

/**
 * pic32_timer_poll - this function handles polled console mode
 * @data: A pointer to our pic32_cons_port (which contains the uart port)
 *
 * data is the pointer that init_timer will store for us.  This function is
 * associated with init_timer to see if there is any console traffic.
 * Obviously not used in interrupt mode
 *
 */
static void pic32_timer_poll(unsigned long data)
{
	struct pic32_cons_port *port = (struct pic32_cons_port *)data;
	unsigned long flags;

	if (!port)
		return;

	if (!port->sc_port.irq) {
		spin_lock_irqsave(&port->sc_port.lock, flags);
		if (pic32_process_input)
			pic32_receive_chars(port, flags);
		pic32_transmit_chars(port, TRANSMIT_RAW);
		spin_unlock_irqrestore(&port->sc_port.lock, flags);
		mod_timer(&port->sc_timer,
			  jiffies + port->sc_interrupt_timeout);
	}
}

/*
 * Boot-time initialization code
 */

/**
 * pic32_switch_to_asynch - Switch to async mode (as opposed to synch)
 * @port: Our pic32_cons_port (which contains the uart port)
 *
 * So this is used by pic32_serial_console_init (early on, before we're
 * registered with serial core).  It's also used by pic32_module_init
 * right after we've registered with serial core.  The later only happens
 * if we didn't already come through here via pic32_serial_console_init.
 *
 */
static void __init pic32_switch_to_asynch(struct pic32_cons_port *port)
{
	unsigned long flags;

	if (!port)
		return;

	DPRINTF("pic32_console: about to switch to asynchronous console\n");

	/* without early_printk, we may be invoked late enough to race
	 * with other cpus doing console IO at this point, however
	 * console interrupts will never be enabled */
	spin_lock_irqsave(&port->sc_port.lock, flags);

	/* early_printk invocation may have done this for us */
	if (!port->sc_ops)
		port->sc_ops = &poll_ops;

	/* we can't turn on the console interrupt (as request_irq
	 * calls kmalloc, which isn't set up yet), so we rely on a
	 * timer to poll for input and push data from the console
	 * buffer.
	 */
	init_timer(&port->sc_timer);
	port->sc_timer.function = pic32_timer_poll;
	port->sc_timer.data = (unsigned long)port;

	port->sc_interrupt_timeout = 6;

	mod_timer(&port->sc_timer, jiffies + port->sc_interrupt_timeout);

	port->sc_is_asynch = 1;
	spin_unlock_irqrestore(&port->sc_port.lock, flags);

	DPRINTF("pic32_console: in asynchronous console\n");
}

/*
 * Kernel console definitions
 */

static void pic32_console_write(struct console *, const char *, unsigned);
static int pic32_console_setup(struct console *, char *);
static struct uart_driver pic32_console_uart;
extern struct tty_driver *uart_console_device(struct console *, int *);

static struct console pic32_console = {
	.name = DEVICE_NAME,
	.write = pic32_console_write,
	.device = uart_console_device,
	.setup = pic32_console_setup,
	.index = -1,		/* unspecified */
	.data = &pic32_console_uart,
};

#define PIC32_CONSOLE	&pic32_console

static struct uart_driver pic32_console_uart = {
	.owner = THIS_MODULE,
	.driver_name = "pic32_console",
	.dev_name = DEVICE_NAME,
	.major = 0,		/* major/minor set at registration time per USE_DYNAMIC_MINOR */
	.minor = 0,
	.nr = 1,		/* one port */
	.cons = PIC32_CONSOLE,
};

/**
 * pic32_module_init - When the kernel loads us, get us rolling w/ serial core
 *
 * Before this is called, we've been printing kernel messages in a special
 * early mode not making use of the serial core infrastructure.  When our
 * driver is loaded for real, we register the driver and port with serial
 * core and try to enable interrupt driven mode.
 *
 */
static int __init pic32_module_init(void)
{
	int retval;

	printk(KERN_DEBUG "pic32_console: Console driver init\n");

	if (USE_DYNAMIC_MINOR == 1) {
		misc.minor = MISC_DYNAMIC_MINOR;
		misc.name = DEVICE_NAME_DYNAMIC;
		retval = misc_register(&misc);
		if (retval != 0) {
			printk(KERN_WARNING "Failed to register console "
			       "device using misc_register.\n");
			return -ENODEV;
		}
		pic32_console_uart.major = MISC_MAJOR;
		pic32_console_uart.minor = misc.minor;
	} else {
		pic32_console_uart.major = DEVICE_MAJOR;
		pic32_console_uart.minor = DEVICE_MINOR;
	}

	/* We register the driver and the port before switching to interrupts
	 * or async above so the proper uart structures are populated */

	if (uart_register_driver(&pic32_console_uart) < 0) {
		printk
		    ("ERROR pic32_module_init failed uart_register_driver, line %d\n",
		     __LINE__);
		return -ENODEV;
	}

	spin_lock_init(&pic32_console_port.sc_port.lock);

	/* Setup the port struct with the minimum needed */
	pic32_console_port.sc_port.membase = (char *)1;	/* just needs to be non-zero */
	pic32_console_port.sc_port.type = PORT_8250;
	pic32_console_port.sc_port.fifosize = PIC32_MAX_CHARS;
	pic32_console_port.sc_port.ops = &pic32_console_ops;
	pic32_console_port.sc_port.line = 0;

	if (uart_add_one_port(&pic32_console_uart, &pic32_console_port.sc_port) < 0) {
		/* error - not sure what I'd do - so I'll do nothing */
		printk(KERN_ERR "%s: unable to add port\n", __func__);
	}

	/* when this driver is compiled in, the console initialization
	 * will have already switched us into asynchronous operation
	 * before we get here through the module initcalls */
	if (!pic32_console_port.sc_is_asynch) {
		pic32_switch_to_asynch(&pic32_console_port);
	}

	pic32_process_input = 1;

	return 0;
}

/**
 * pic32_module_exit - When we're unloaded, remove the driver/port
 *
 */
static void __exit pic32_module_exit(void)
{
	del_timer_sync(&pic32_console_port.sc_timer);
	uart_remove_one_port(&pic32_console_uart, &pic32_console_port.sc_port);
	uart_unregister_driver(&pic32_console_uart);
	misc_deregister(&misc);
}

module_init(pic32_module_init);
module_exit(pic32_module_exit);

/**
 * puts_raw_fixed - pic32_console_write helper for adding \r's as required
 * @puts_raw : puts function to do the writing
 * @s: input string
 * @count: length
 *
 * We need a \r ahead of every \n for direct writes through
 * ia64_pic32_console_putb (what pic32_puts_raw below actually does).
 *
 */

static void puts_raw_fixed(int (*puts_raw) (const char *s, int len),
			   const char *s, int count)
{
	unsigned int i;

	for (i = 0; i < count; i++, s++) {
		if (*s == '\n')
			puts_raw("\r", 1);
		puts_raw(s, 1);
	}
}

/**
 * pic32_console_write - Print statements before serial core available
 * @console: Console to operate on - we ignore since we have just one
 * @s: String to send
 * @count: length
 *
 * This is referenced in the console struct.  It is used for early
 * console printing before we register with serial core and for things
 * such as kdb.  The console_lock must be held when we get here.
 *
 * This function has some code for trying to print output even if the lock
 * is held.  We try to cover the case where a lock holder could have died.
 * We don't use this special case code if we're not registered with serial
 * core yet.  After we're registered with serial core, the only time this
 * function would be used is for high level kernel output like magic sys req,
 * kdb, and printk's.
 */
static void
pic32_console_write(struct console *co, const char *s, unsigned count)
{
	unsigned long flags = 0;
	struct pic32_cons_port *port = &pic32_console_port;
	static int stole_lock = 0;

	BUG_ON(!port->sc_is_asynch);

	/* We can't look at the xmit buffer if we're not registered with serial core
	 *  yet.  So only do the fancy recovery after registering
	 */
	if (!port->sc_port.state) {
		/* Not yet registered with serial core - simple case */
		puts_raw_fixed(port->sc_ops->pic32_puts_raw, s, count);
		return;
	}

	/* somebody really wants this output, might be an
	 * oops, kdb, panic, etc.  make sure they get it. */
	if (spin_is_locked(&port->sc_port.lock)) {
		int lhead = port->sc_port.state->xmit.head;
		int ltail = port->sc_port.state->xmit.tail;
		int counter, got_lock = 0;

		/*
		 * We attempt to determine if someone has died with the
		 * lock. We wait ~20 secs after the head and tail ptrs
		 * stop moving and assume the lock holder is not functional
		 * and plow ahead. If the lock is freed within the time out
		 * period we re-get the lock and go ahead normally. We also
		 * remember if we have plowed ahead so that we don't have
		 * to wait out the time out period again - the asumption
		 * is that we will time out again.
		 */

		for (counter = 0; counter < 150; mdelay(125), counter++) {
			if (!spin_is_locked(&port->sc_port.lock)
			    || stole_lock) {
				if (!stole_lock) {
					spin_lock_irqsave(&port->sc_port.lock,
							  flags);
					got_lock = 1;
				}
				break;
			} else {
				/* still locked */
				if ((lhead != port->sc_port.state->xmit.head)
				    || (ltail !=
					port->sc_port.state->xmit.tail)) {
					lhead =
						port->sc_port.state->xmit.head;
					ltail =
						port->sc_port.state->xmit.tail;
					counter = 0;
				}
			}
		}
		/* flush anything in the serial core xmit buffer, raw */
		pic32_transmit_chars(port, 1);
		if (got_lock) {
			spin_unlock_irqrestore(&port->sc_port.lock, flags);
			stole_lock = 0;
		} else {
			/* fell thru */
			stole_lock = 1;
		}
		puts_raw_fixed(port->sc_ops->pic32_puts_raw, s, count);
	} else {
		stole_lock = 0;
		spin_lock_irqsave(&port->sc_port.lock, flags);
		pic32_transmit_chars(port, 1);
		spin_unlock_irqrestore(&port->sc_port.lock, flags);

		puts_raw_fixed(port->sc_ops->pic32_puts_raw, s, count);
	}
}


/**
 * pic32_console_setup - Set up console for early printing
 * @co: Console to work with
 * @options: Options to set
 *
 * Altix console doesn't do anything with baud rates, etc, anyway.
 *
 * This isn't required since not providing the setup function in the
 * console struct is ok.  However, other patches like KDB plop something
 * here so providing it is easier.
 *
 */
static int pic32_console_setup(struct console *co, char *options)
{
	return 0;
}

/**
 * pic32_serial_console_init - Early console output - set up for register
 *
 * This function is called when regular console init happens.  Because we
 * support even earlier console output with pic32_serial_console_early_setup
 * (called from setup.c directly), this function unregisters the really
 * early console.
 *
 * Note: Even if setup.c doesn't register pic32_console_early, unregistering
 * it here doesn't hurt anything.
 *
 */
static int __init pic32_serial_console_init(void)
{
	uart_base = ioremap(PIC32_BASE_UART, 0xc00);
	if (!uart_base)
		return -ENOMEM;

	pic32_switch_to_asynch(&pic32_console_port);
	DPRINTF("pic32_serial_console_init : register console\n");
	register_console(&pic32_console);
	return 0;
}

console_initcall(pic32_serial_console_init);
