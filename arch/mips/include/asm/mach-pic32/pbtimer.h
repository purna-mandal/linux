/*
 * Purna Chandra Mandal, purna.mandal@microchip.com
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
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 */

#ifndef __PIC32_TIMER_H
#define __PIC32_TIMER_H

#include <linux/debugfs.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/of.h>

/* PIC32 Timer capability flags */
#define PIC32_TIMER_BUSY	0x01
#define PIC32_TIMER_CLASS_A	0x02
#define PIC32_TIMER_32BIT	0x04
#define PIC32_TIMER_ASYNC	0x08
#define PIC32_TIMER_CLK_EXT	0x10
#define PIC32_TIMER_GATED	0x20
#define PIC32_TIMER_TRIG_ADC	0x40

/* By default, timers run continuously until disabled.
 * To simulate oneshot mode timer_isr() on timer match
 * has to disable the timer.
 */
#define PIC32_TIMER_ONESHOT	0x0100

/* set_timeout() might change prescaler to modify clk rate, whenever required */
#define PIC32_TIMER_MAY_RATE	0x0200

/* Best match mode; set_timeout() will try to achieve best rate */
#define PIC32_TIMER_PRESCALE_BEST_MATCH	1

/* Higest resolution mode; set_timeout() will try to achieve nearer rate */
#define PIC32_TIMER_PRESCALE_HIGH_RES	2

/* Timer prescaler(divider) fixups */
/* Prescalers resets to zero on timer count write */
#define PIC32_TIMER_PS_FIXUP_COUNT	0x0400

/* Prescalers resets to zero on timer disable */
#define PIC32_TIMER_PS_FIXUP_DISABLE	0x0800

/* Timer Registers */
#define PIC32_TIMER_CTRL	0x00
#define PIC32_TIMER_CTRL_CLR	0x04
#define PIC32_TIMER_CTRL_SET	0x08
#define PIC32_TIMER_COUNT	0x10
#define PIC32_TIMER_PERIOD	0x20

/* Timer Control Register fields */
#define TIMER_CS		0x0002 /* 1: clk-external, 0: clk-internal */
#define TIMER_SYNC		0x0004 /* sync mode */
#define TIMER_32BIT		0x0008 /* 32bit mode */
#define TIMER_CKPS		0x0030 /* pre-scalers */
#define TIMER_GATE		0x0080 /* gated mode */
#define TIMER_WIP		0x0800 /* write-in-progress for async mode */
#define TIMER_WDIS		0x1000
#define TIMER_SIDL		0x2000 /* module enabled */
#define TIMER_ON		0x8000
#define TIMER_CKPS_SHIFT	4
#define TIMER_CS_SHIFT		1

#define PIC32_TIMER_CLK_SRC_MAX	2

struct timer_ops;

struct pic32_pb_timer {
	struct list_head link;
	struct device_node *np;
	void __iomem *base;
	unsigned int irq;
	unsigned int id;
	unsigned int flags;
	unsigned int capability;
	unsigned int *dividers;
	unsigned int num_dividers;
	struct clk_hw hw;
	struct clk *clk;
	u32 *clk_idx;
	struct timer_ops *ops;
	unsigned long overrun;
	u8 save_prescaler;
	struct dentry *dentry;
	struct debugfs_regset32 regs;
	spinlock_t lock;
	struct mutex mutex;
	unsigned long enable_count;
};

#define timer_cap_32bit(timer)	((timer)->capability & PIC32_TIMER_32BIT)
#define timer_cap_class_a(timer)((timer)->capability & PIC32_TIMER_CLASS_A)
#define timer_cap_async(timer)	((timer)->capability & PIC32_TIMER_ASYNC)
#define timer_cap_gated(timer)	((timer)->capability & PIC32_TIMER_GATED)

#define timer_is_busy(timer)	((timer)->flags & PIC32_TIMER_BUSY)
#define timer_is_32bit(timer)	((timer)->flags & PIC32_TIMER_32BIT)
#define timer_is_async(timer)	((timer)->flags & PIC32_TIMER_ASYNC)
#define timer_is_gated(timer)	((timer)->flags & PIC32_TIMER_GATED)
#define timer_is_type_a(timer)	timer_cap_class_a(timer)
#define timer_is_oneshot(timer) ((timer)->flags & PIC32_TIMER_ONESHOT)

static inline uint32_t pbt_readl(struct pic32_pb_timer *tmr, unsigned long offs)
{
	return __raw_readl(tmr->base + offs);
}

static inline uint16_t pbt_readw(struct pic32_pb_timer *tmr, unsigned long offs)
{
	return __raw_readw(tmr->base + offs);
}

static int __pbt_wait_for_write_done(struct pic32_pb_timer *timer,
	unsigned long offs)
{
	uint16_t conf;

	/* This is special wait semantic is required only when Timer1.COUNT
	 * register is written in asynchronous mode.
	 */
	if (timer_is_type_a(timer) == 0)
		return 0;

	if (offs != PIC32_TIMER_COUNT)
		return 0;

	for (;;) {
		conf = pbt_readl(timer, PIC32_TIMER_CTRL);
		if (conf & TIMER_WIP)
			continue;
	}
}

static inline void pbt_writel(uint32_t val, struct pic32_pb_timer *timer,
		 unsigned long offs)
{
	if (timer_is_async(timer))
		__pbt_wait_for_write_done(timer, offs);

	__raw_writel(val, timer->base + offs);
}

static inline void pbt_writew(uint16_t val, struct pic32_pb_timer *timer,
		 unsigned long offs)
{
	if (timer_is_async(timer))
		__pbt_wait_for_write_done(timer, offs);

	__raw_writew(val, timer->base + offs);
}

static inline void pbt_write_prescaler(uint8_t v, struct pic32_pb_timer *timer)
{
	pbt_writew(TIMER_CKPS, timer, PIC32_TIMER_CTRL_CLR);
	pbt_writew(v << TIMER_CKPS_SHIFT, timer, PIC32_TIMER_CTRL_SET);
}

static inline uint8_t pbt_read_prescaler(struct pic32_pb_timer *timer)
{
	uint16_t v = pbt_readw(timer, PIC32_TIMER_CTRL);
	return (uint8_t)((v & TIMER_CKPS) >> TIMER_CKPS_SHIFT);
}

static inline void pbt_enable(struct pic32_pb_timer *timer)
{
	pbt_writew(TIMER_ON, timer, PIC32_TIMER_CTRL_SET);
}

static inline void pbt_disable(struct pic32_pb_timer *timer)
{
	pbt_writew(TIMER_ON, timer, PIC32_TIMER_CTRL_CLR);

	/* Note: clearing TMRCTRL.ON resets prescalers to zero.
	 * So take proper care to restore prescaler once enabled.
	 */
	if (timer->capability & PIC32_TIMER_PS_FIXUP_DISABLE)
		pbt_write_prescaler(timer->save_prescaler, timer);
}

static int __used pbt_is_enabled(struct pic32_pb_timer *timer)
{
	uint16_t v = pbt_readw(timer, PIC32_TIMER_CTRL);
	return !!(v & TIMER_ON);
}

static inline void pbt_enable_gate(struct pic32_pb_timer *timer)
{
	pbt_writew(TIMER_GATE, timer, PIC32_TIMER_CTRL_SET);
}

static inline void pbt_disable_gate(struct pic32_pb_timer *timer)
{
	pbt_writew(TIMER_GATE, timer, PIC32_TIMER_CTRL_CLR);
}

static __used int pbt_is_gate_enabled(struct pic32_pb_timer *timer)
{
	uint16_t v = pbt_readw(timer, PIC32_TIMER_CTRL);
	return !!(v & TIMER_GATE);
}

static inline void pbt_set_32bit(int enable, struct pic32_pb_timer *timer)
{
	if (enable)
		pbt_writew(TIMER_32BIT, timer, PIC32_TIMER_CTRL_SET);
	else
		pbt_writew(TIMER_32BIT, timer, PIC32_TIMER_CTRL_CLR);
}

uint32_t pbt_read_count(struct pic32_pb_timer *timer);

uint32_t pbt_read_period(struct pic32_pb_timer *timer);

void pbt_write_count(uint32_t val, struct pic32_pb_timer *timer);

void pbt_write_period(uint32_t val, struct pic32_pb_timer *timer);

/* pic32_pb_timer_get_irq - get irq number of the associated PIC32 timer.
 *
 * PIC32 timer framework does not handle interrupt internally, So client
 * drivers have to install irq_handler and handle the specific operation
 * as required/specified by the reference manual.
 */
static inline int pic32_pb_timer_get_irq(struct pic32_pb_timer *timer)
{
	if (likely(timer))
		return timer->irq;

	return -EINVAL;
}

/* pic32_pb_timer_get_clk - get clk of PIC32 timer. */
static inline struct clk *pic32_pb_timer_get_clk(struct pic32_pb_timer *timer)
{
	if (!timer)
		return NULL;
	return timer->clk;
}

static inline void pic32_pb_timer_put_clk(struct pic32_pb_timer *timer)
{
}

struct pic32_pb_timer *pic32_pb_timer_request_specific(int id);

struct pic32_pb_timer *pic32_pb_timer_request_by_cap(int cap);

struct pic32_pb_timer *pic32_pb_timer_request_by_node(struct device_node *np);

struct pic32_pb_timer *pic32_pb_timer_request_any(void);

int pic32_pb_timer_free(struct pic32_pb_timer *timer);

static inline unsigned long __clk_timeout_ns_to_period(u64 timeout_ns,
	unsigned long rate)
{
	u32 T = NSEC_PER_SEC / rate;
	do_div(timeout_ns, T);
	return (unsigned long) timeout_ns;
}

static inline u64 __clk_period_to_timeout_ns(unsigned long period,
	unsigned long rate)
{
	u32 T = NSEC_PER_SEC / rate;
	return period * T;
}

static inline uint64_t
pb_timer_clk_get_max_timeout(struct pic32_pb_timer *timer, unsigned long rate)
{
	uint64_t count;
	if (timer_is_32bit(timer))
		count = (u64) U32_MAX;
	else
		count = (u64) U16_MAX;
	return __clk_period_to_timeout_ns(count, rate);
}

int pic32_pb_timer_settime(struct pic32_pb_timer *timer,
	unsigned long flags, uint64_t timeout_nsec);

int pic32_pb_timer_gettime(struct pic32_pb_timer *timer, uint64_t *timeout,
	uint64_t *elapsed);

static inline int pic32_pb_timer_getoverrun(struct pic32_pb_timer *timer)
{
	if (!timer)
		return 0;
	return timer->overrun;
}

int pic32_pb_timer_start(struct pic32_pb_timer *timer);
int pic32_pb_timer_stop(struct pic32_pb_timer *timer);

struct pbtimer_platform_data {
	u32 timer_capability;
	const char **parent_names;
	int num_parents;
};

void __init of_pic32_pb_timer_init(void);

extern int pic32_of_clk_get_parent_indices(struct device_node *np,
	u32 **table_p, int count);

#endif /* __PIC32_TIMER_H */

