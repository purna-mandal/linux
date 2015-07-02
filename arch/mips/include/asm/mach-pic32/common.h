/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2014 Joshua Henderson <joshua.henderson@microchip.com>
 */
#ifndef PIC32_COMMON_H
#define PIC32_COMMON_H

extern struct irq_domain *evic_irq_domain;

u32 pic32_get_pbclk(int bus);
void pic32_clk_init(void);
u32 pic32_get_cpuclk(void);
void pic32_clk_add(const char *dev, u32 rate);
void __init pic32_init_early_console(void);

#if defined(CONFIG_MIPS_PIC32MZ)
#define CONSOLE_PORT    2
#elif defined(CONFIG_MIPS_PIC32MZDA)
#if defined(CONFIG_MIPS_PIC32_EPLATFORM)
#define CONSOLE_PORT    1
#else
#define CONSOLE_PORT    2
#endif
#else
#error "No Console defined!"
#endif

#endif
