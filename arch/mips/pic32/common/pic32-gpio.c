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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <asm/mach-pic32/pic32.h>

/*
 * Name    x   GPIO Numbers
 * PORTA   0   0-31
 * PORTB   1   32-63
 * PORTC   2   64-95
 * PORTD   3   96-127
 * PORTE   4   128-159
 * PORTF   5   160-191
 * PORTG   6   192-223
 * PORTH   7   224-255
 * PORTI   8   256-287
 * PORTJ   9   288-319
 * PORTK   10  320-351
 */

static void __iomem *port_base;
static DEFINE_SPINLOCK(pic32mz_gpio_lock);

#define DIR_IN 1
#define DIR_OUT 0

static void pic32mz_gpio_set(struct gpio_chip *chip,
			     unsigned gpio, int val)
{
	u32 reg;
	u32 mask;
	unsigned long flags;

	if (gpio >= chip->ngpio)
		BUG();

	BIT_REG_MASK(gpio, reg, mask);

	spin_lock_irqsave(&pic32mz_gpio_lock, flags);
	if (val)
		__raw_writel(mask, port_base + PIC32_SET(PORTx(reg)));
	else
		__raw_writel(mask, port_base + PIC32_CLR(PORTx(reg)));
	spin_unlock_irqrestore(&pic32mz_gpio_lock, flags);
}

static int pic32mz_gpio_get(struct gpio_chip *chip, unsigned gpio)
{
	u32 reg;
	u32 mask;

	if (gpio >= chip->ngpio)
		BUG();

	BIT_REG_MASK(gpio, reg, mask);

	return __raw_readl(port_base + PORTx(reg)) & mask;
}

static int pic32mz_gpio_set_direction(struct gpio_chip *chip,
				      unsigned gpio, int dir)
{
	u32 reg;
	u32 mask;
	unsigned long flags;

	if (gpio >= chip->ngpio)
		BUG();

	BIT_REG_MASK(gpio, reg, mask);

	spin_lock_irqsave(&pic32mz_gpio_lock, flags);

	__raw_writel(mask, port_base + PIC32_CLR(ANSELx(reg)));

	if (dir)
		__raw_writel(mask, port_base + PIC32_SET(TRISx(reg)));
	else
		__raw_writel(mask, port_base + PIC32_CLR(TRISx(reg)));

	spin_unlock_irqrestore(&pic32mz_gpio_lock, flags);

	return 0;
}

static int pic32mz_gpio_direction_input(struct gpio_chip *chip, unsigned gpio)
{
	return pic32mz_gpio_set_direction(chip, gpio, DIR_IN);
}

static int pic32mz_gpio_direction_output(struct gpio_chip *chip,
					 unsigned gpio, int value)
{
	return pic32mz_gpio_set_direction(chip, gpio, DIR_OUT);
}

static struct gpio_chip pic32mz_gpio_chip = {
	.label			= "pic32mz-gpio",
	.direction_input	= pic32mz_gpio_direction_input,
	.direction_output	= pic32mz_gpio_direction_output,
	.get			= pic32mz_gpio_get,
	.set			= pic32mz_gpio_set,
	.base			= 0,
};

int __init pic32mz_gpio_init(void)
{
	port_base = ioremap(PIC32_BASE_PORT, 0xA00);
	if (!port_base)
		return -ENOMEM;
	pic32mz_gpio_chip.ngpio = ARCH_NR_GPIOS;
	pr_info("registering %d GPIOs\n", pic32mz_gpio_chip.ngpio);
	return gpiochip_add(&pic32mz_gpio_chip);
}

arch_initcall(pic32mz_gpio_init);
