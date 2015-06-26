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
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/timex.h>
#include <linux/mc146818rtc.h>
#include <linux/clkdev.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>

#include <asm/mipsregs.h>
#include <asm/mipsmtregs.h>
#include <asm/hardirq.h>
#include <asm/irq.h>
#include <asm/div64.h>
#include <asm/cpu.h>
#include <asm/time.h>

#include <asm/mips-boards/generic.h>
#include <asm/mach-pic32/pic32.h>
#include <asm/mach-pic32/common.h>
#include <asm/mach-pic32/pbtimer.h>

#include <asm/prom.h>
#include <dt-bindings/interrupt-controller/microchip,pic32mz-evic.h>

extern void __init of_pic32_oc_init(void);

u32 pic32_get_cpuclk(void)
{
	return 25000000;
}

u32 pic32_get_pbclk(int bus)
{
	return 25000000;
}

unsigned int get_c0_compare_int(void)
{
	int virq;

	virq = irq_create_mapping(evic_irq_domain, CORE_TIMER_INTERRUPT);
	irq_set_irq_type(virq, IRQ_TYPE_EDGE_RISING);
	return virq;
}

void __init plat_time_init(void)
{
	struct clk *clk;

	of_clk_init(NULL);
	clk = clk_get_sys("cpu_clk", NULL);
	if (IS_ERR(clk)) {
		clk = clk_get_sys("pb7_clk", NULL);
		if (IS_ERR(clk))
			panic("unable to get CPU clock, err=%ld", PTR_ERR(clk));
	}

	clk_prepare_enable(clk);
	pr_info("CPU Clock: %dMHz\n", 25000000 / 1000000);
	mips_hpt_frequency = 25000000 / 2;
	of_pic32_pb_timer_init();
	of_pic32_oc_init();
	clocksource_of_init();
}
