/*
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * Copyright (c) 2004 MIPS Inc
 * Author: chris@mips.com
 *
 * Copyright (C) 2004, 06 Ralf Baechle <ralf@linux-mips.org>
 * Copyright (C) 2014 Joshua Henderson <joshua.henderson@microchip.com>
 */
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kernel_stat.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/traps.h>
#include <asm/mach-pic32/pic32.h>
#include <asm/pic32.h>

#define IFSx(x)		(0x0040 + (x * 0x10))
#define IECx(x)		(0x00C0 + (x * 0x10))
#define IPCx(x)		(0x0140 + (x * 0x10))
#define OFFx(x)		(0x0540 + (x * 0x04))

static void __iomem *int_base;

/* mask off an interrupt */
static inline void mask_pic32_irq(struct irq_data *d)
{
	u32 reg;
	u32 mask;

	BIT_REG_MASK(d->irq, reg, mask);

	__raw_writel(mask, int_base + PIC32_CLR(IECx(reg)));
}

/* unmask an interrupt */
static inline void unmask_pic32_irq(struct irq_data *d)
{
	u32 reg;
	u32 mask;

	BIT_REG_MASK(d->irq, reg, mask);

	__raw_writel(mask, int_base + PIC32_SET(IECx(reg)));
}

static void edge_mask_and_ack_pic32_irq(struct irq_data *d)
{
	u32 reg;
	u32 mask;

	BIT_REG_MASK(d->irq, reg, mask);

	__raw_writel(mask, int_base + PIC32_CLR(IFSx(reg)));
}

static void pic32_bind_eic_interrupt(int irq, int set)
{
	__raw_writel(set, int_base + OFFx(irq));
}

static struct irq_chip pic32_edgeirq_type = {
	.name = "pic32",
	.irq_ack = edge_mask_and_ack_pic32_irq,
	.irq_mask = mask_pic32_irq,
	.irq_mask_ack = edge_mask_and_ack_pic32_irq,
	.irq_unmask = unmask_pic32_irq,
	.irq_eoi = unmask_pic32_irq,
};

static void set_irq_priority(int irq, int priority)
{
	u32 reg;
	u32 shift = 0;

	reg = irq / 4;
	switch (irq % 4) {
	case 0:
		shift = 2;
		break;
	case 1:
		shift = 10;
		break;
	case 2:
		shift = 18;
		break;
	case 3:
		shift = 26;
		break;
	}

	/* set priority */
	__raw_writel(3 << shift, int_base + PIC32_CLR(IPCx(reg)));
	__raw_writel(priority << shift, int_base + PIC32_SET(IPCx(reg)));
}

void __init init_pic32_irqs(unsigned int irqbase, struct pic32_irqmap *imp, int nirq)
{
	int_base = ioremap(PIC32_BASE_INT, 0x1000);
	if (!int_base) {
		pr_err("Failed to map irq regs\n");
		return;
	}

	board_bind_eic_interrupt = &pic32_bind_eic_interrupt;

	for (; nirq > 0; nirq--, imp++) {
		u32 reg;
		u32 mask;
		int irq = imp->irq;

		irq_set_chip_and_handler_name(irqbase + irq,
					&pic32_edgeirq_type,
					handle_edge_irq,
					"edge");

		BIT_REG_MASK(irq, reg, mask);

		/* disable */
		__raw_writel(mask, int_base + PIC32_CLR(IECx(reg)));

		/* clear flag */
		__raw_writel(mask, int_base + PIC32_CLR(IFSx(reg)));

		set_irq_priority(irq, imp->pri);
	}
}
