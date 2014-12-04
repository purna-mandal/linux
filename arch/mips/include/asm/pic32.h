/*
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

#ifndef __ASM_MIPS_BOARDS_PIC32_H
#define __ASM_MIPS_BOARDS_PIC32_H

struct pic32_irqmap {
	int	irq;
	int	pri;
};

extern void __init init_pic32_irqs(unsigned int base, struct pic32_irqmap *imp, int nirq);

#endif /* __ASM_MIPS_BOARDS_PIC32_H */
