/*
 * Purna Chandra Mandal purna.mandal@microchip.com
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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/clkdev.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/delay.h>

#include <asm/mips-boards/generic.h>
#include <asm/mach-pic32/pic32.h>
#include <asm/mach-pic32/common.h>

#define __MHZ(x)	((x) /  1000000)
#define __KHZ(x)	((x) /  1000)

#define UART_ENABLE           BIT(15)
#define UART_ENABLE_RX        BIT(12)
#define UART_ENABLE_TX        BIT(10)
#define CONSOLE_UART 2
#define DEFAULT_BAUDRATE	19200

#warning "Configure UART-2 for baud 19200n8."
#warning "Modify baud in early_console_init(), if required."

static void uart_set_baud_clk(unsigned long new_rate)
{
	u32 v;
	int port = CONSOLE_UART;
	void __iomem *iobase = (void __iomem *)PIC32_BASE_UART;
	unsigned long flags;
	local_irq_save(flags);

	writel(0, iobase + UxMODE(port));
	v = ((new_rate / DEFAULT_BAUDRATE) / 16) - 1;
	writel(v, iobase + UxBRG(port));
	writel(UART_ENABLE, iobase + UxMODE(port));
	writel(UART_ENABLE_TX|UART_ENABLE_RX, iobase + PIC32_SET(UxSTA(port)));
	local_irq_restore(flags);
}

static int pic32_serial_clk_notifier(struct notifier_block *nb,
				     unsigned long action, void *data)
{
	struct clk_notifier_data *cnd = (struct clk_notifier_data *)data;
	switch (action) {
	case PRE_RATE_CHANGE:
		pr_info("serial: pre-rate, old_rate %lu, new_rate %lu\n\n",
			cnd->old_rate, cnd->new_rate);
		uart_set_baud_clk(cnd->new_rate);
		break;
	case POST_RATE_CHANGE:
		uart_set_baud_clk(cnd->new_rate);
		pr_info("serial: post-rate, old_rate %lu, new_rate %lu\n",
			cnd->old_rate, cnd->new_rate);
		break;
	case ABORT_RATE_CHANGE:
		uart_set_baud_clk(cnd->old_rate);
		pr_info("serial: abort-rate, old_rate %lu, new_rate %lu\n",
			cnd->old_rate, cnd->new_rate);
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block notify_serial = {
	.notifier_call = pic32_serial_clk_notifier,
	.priority = U16_MAX,
};

static const struct clk *uart_clk;
static int pic32_serial_init(struct device_node *np)
{
	struct clk *clk;
	uart_clk = clk = of_clk_get(np, NULL);
	if (IS_ERR(clk))
		goto out;

	/* enable clk and register notifier */
	clk_prepare_enable(clk);
	clk_notifier_register(clk, &notify_serial);
out:
	return 0;
}

static int pic32_serial_exit(void)
{
	struct clk *clk = uart_clk;

	clk_disable_unprepare(clk);
	clk_notifier_unregister(clk, &notify_serial);
out:
	return 0;
}

static inline int dbg_clk_sw_rate(struct clk *c, unsigned long new_rate,
	const char *fname, const unsigned long line)
{
	int rate;
	clk_set_rate(c, new_rate);
	rate = clk_get_rate(c);
	if (rate != new_rate) {
		pr_err("%s:%d, %s, rate = %lu MHz, expected %lu Mhz\n",
			fname, line,
			__clk_get_name(c), __MHZ(rate), __MHZ(new_rate));
	}
	return rate != new_rate;
}
#define pic32_clk_switch_rate(c, r) dbg_clk_sw_rate(c, r, __func__, __LINE__)


struct clk_rate_table {
	unsigned long rate;
	unsigned long flags;
};

#define __CLK_RATE(_x, _y)	{.rate = _x, .flags = _y,}
static struct clk_rate_table refoclk_rate_table[] = {
	__CLK_RATE(125000000, 1),
	__CLK_RATE(100000000, 0),
	__CLK_RATE(50000000, 0),
	__CLK_RATE(27004219, 0),
	__CLK_RATE(25000000, 0),
	__CLK_RATE(20000000, 0),
	__CLK_RATE(16000000, 0),
	__CLK_RATE(10000000, 0),
	__CLK_RATE(8000000, 0),
	__CLK_RATE(4000000, 0),
	__CLK_RATE(5000000, 0),
	__CLK_RATE(1000000, 0),
	__CLK_RATE(32000, 1),
};

static int pic32_test_refoclk(int idx)
{
	struct clk *roclk, *pclk;
	int ret, i;
	unsigned long rate, new_rate, old_rate;
	char clk_name[64];

	snprintf(clk_name, sizeof(clk_name), "refo%d_clk", idx);
	pr_info("------------- perform %s test ------------\n", clk_name);

	roclk = clk_get_sys(clk_name, NULL);
	if (IS_ERR(roclk)) {
		pr_err("%s: not found\n", clk_name);
		return -EINVAL;
	}

	/* enable clk */
	clk_prepare(roclk);
	if (__clk_is_enabled(roclk))
		clk_disable(roclk);

	/* set parent clk */
	pclk = __clk_lookup("sys_clk");
	if (!IS_ERR(pclk)) {
		clk_set_parent(roclk, pclk);

		if (clk_get_parent(roclk) != pclk) {
			pr_err("%s:%d, %s, set_parent(%s) failed\n", __func__,
				__LINE__, clk_name, __clk_get_name(pclk));
			ret = -EINVAL;
			goto out;
		}
	}

	clk_disable(roclk);
	clk_enable(roclk);
	old_rate = clk_get_rate(roclk);
	for (i = 0; i < ARRAY_SIZE(refoclk_rate_table); i++) {
		msleep(1000);
		old_rate = clk_get_rate(roclk);
		new_rate = refoclk_rate_table[i].rate;
		if (new_rate == 0)
			break;
		pr_info("%s:%d, %s, set_rate(%lu khz -> %lu khz)\n",
			__func__, __LINE__, clk_name,
			__KHZ(old_rate), __KHZ(new_rate));

		if (!pic32_clk_switch_rate(roclk, new_rate))
			continue;

		rate = clk_get_rate(roclk);
		if (refoclk_rate_table[i].flags) {
			pr_info("%s:%d, %s, rate = %lu, expected %lu. NEGATIVE test\n",
				__func__, __LINE__, clk_name, rate, new_rate);
			ret = 0;
		} else {
			pr_err("%s:%d, %s, rate = %lu, expected %lu. FAILED\n",
				__func__, __LINE__, clk_name, rate, new_rate);
			ret = -EINVAL;
			goto out;
		}
	}
out:
	clk_unprepare(roclk);
	clk_put(roclk);
	pr_info("%s: test %s!\n", clk_name, ret ? "FAILED" : "PASSED");
	return 0;
}

static int pic32_test_pbclk(int idx)
{
	struct clk *clk;
	char name[20];
	unsigned long saved_rate, new_rate, rate;

	pr_info("------------- perform pb%d_clk test ------------\n", idx);

	/* get clk */
	snprintf(name, sizeof(name), "pb%d_clk", idx);
	clk = clk_get_sys(name, NULL);
	if (IS_ERR(clk)) {
		pr_err("%s: not found\n", name);
		return -EINVAL;
	}

	/* preprate for operation */
	clk_prepare(clk);

	/* enable clk */
	pr_info("%s: _clk_is_enabled %d\n", name, __clk_is_enabled(clk));
	clk_enable(clk);
	if (!__clk_is_enabled(clk)) {
		pr_err("%s: clk is not enabled, although it should be\n", name);
		return -EINVAL;
	}

	/* get rate */
	saved_rate = rate = clk_get_rate(clk);
	pr_info("%s: current rate %lu MHZ\n", name, __MHZ(rate));

	/* switch rate: in disabled state */
	new_rate = 50000000;
	clk_disable(clk);
	clk_set_rate(clk, new_rate);

	clk_enable(clk);
	rate = clk_get_rate(clk);
	if (new_rate != rate) {
		pr_err("%s: failed, set_rate %lu MHZ, current rate %lu MHZ\n",
			name, __MHZ(new_rate), __MHZ(rate));
		goto out;
	}

	/* switch rate 2: in enabled state */
	new_rate = 25000000;
	if (pic32_clk_switch_rate(clk, new_rate))
		goto out;

	/* switch rate 3 in enabled state */
	new_rate = 10000000;
	if (pic32_clk_switch_rate(clk, new_rate))
		goto out;

	/* switch back to original rate */
	new_rate = saved_rate;
	if (pic32_clk_switch_rate(clk, new_rate))
		goto out;


	pr_info("%s(%s): test PASSED\n", __func__, name);
out:
	/* unwind */
	clk_disable(clk);
	clk_unprepare(clk);
	clk_put(clk);
	return 0;
}

static int pic32_test_sclk_switch(void)
{
	int ret;
	struct clk *new_parent_clk, *sclk;
	const char *cur_parent_name, *new_parent_name;

	pr_info("------------- perform SCLK test ------------\n");

	sclk = __clk_lookup("sys_clk");
	if (IS_ERR(sclk)) {
		pr_err("sclk: not found. Test FAILED.\n");
		return 0;
	}

	new_parent_clk = __clk_lookup("posc_clk");
	if (IS_ERR(new_parent_clk)) {
		pr_err("posc_clk: not found. Test FAILED.\n");
		return 0;
	}

	new_parent_name = __clk_get_name(new_parent_clk);
	pr_info("--- sclk: & %s both found\n", new_parent_name);

	/* prepare sclk */
	__clk_get(sclk);
	clk_prepare_enable(sclk);

	/* prepare new parent clk */
	__clk_get(new_parent_clk);
	clk_prepare_enable(new_parent_clk);

	/* get current parent */
	cur_parent_name = __clk_get_name(__clk_get_parent(sclk));
	pr_info("--- sys_clk: get_parent %s ----\n", cur_parent_name);

	/* set new parent */
	pr_info("--- sys_clk: set_parent %s ----\n", new_parent_name);
	ret = clk_set_parent(sclk, new_parent_clk);
	if (ret) {
		pr_err("sysclk: set_parent failed\n");
		goto out_clk_put;
	}

	/* verify current parent */
	cur_parent_name = __clk_get_name(__clk_get_parent(sclk));
	pr_info("--- sys_clk: get_parent %s\n", cur_parent_name);
	if (clk_get_parent(sclk) != new_parent_clk)
		pr_info("--- sys_clk: mismatch %s %s\n",
			cur_parent_name, new_parent_name);
out_clk_put:
	clk_put(new_parent_clk);
	clk_put(sclk);
	return ret;
}

static int pic32_spll_test(void)
{
	struct clk *pllclk;
	unsigned long old_rate, rate;
	struct clk *sclk;

	pr_info("------------- perform SPLL rate-change ------------\n");
	pllclk = __clk_lookup("sys_pll");
	if (IS_ERR(pllclk)) {
		pr_info("spll: not found. Test FAILED\n");
		return 0;
	}

	sclk = __clk_lookup("sys_clk");
	if (IS_ERR(sclk)) {
		pr_info("sclk: not found. Test FAILED\n");
		return 0;
	}

	/* prepare pll clk */
	__clk_get(pllclk);
	clk_prepare_enable(pllclk);
	pr_info("---- spll: found clk.\n");

	/* parepare sclk */
	__clk_get(sclk);
	clk_prepare_enable(sclk);
	pr_info("---- sclk: found clk.\n");

	/* check whether pllclk rate change is allowed */
	if (clk_get_parent(sclk) == pllclk)
		pr_info("---- sclk: using spll\n");

	/* clk get rate */
	old_rate = rate = clk_get_rate(pllclk);
	pr_info("---- spll: get_rate %luMhz\n", __MHZ(rate));

	/* clk switch rate: new rate */
	rate = 175000000;
	pr_info("---- spll: set_rate %luMhz\n", __MHZ(rate));
	pic32_clk_switch_rate(pllclk, rate);

	/* clk switch rate: new rate */
	rate = 150000000;
	pr_info("---- spll: set_rate %luMhz\n", __MHZ(rate));
	pic32_clk_switch_rate(pllclk, rate);

	/* clk switch rate: new rate */
	rate = 135000000;
	pr_info("---- spll: set_rate %luMhz\n", __MHZ(rate));
	pic32_clk_switch_rate(pllclk, rate);

	/* clk switch rate: restore old rate */
	pr_info("---- spll: restore saved rate, %luKhz\n",
		__KHZ(old_rate));
	clk_set_rate(pllclk, old_rate);

	pr_info("---- sclk: set parent to pllclk\n");
	if (clk_set_parent(sclk, pllclk))
		pr_err("---- sclk: set_parent failed\n");

	clk_disable_unprepare(pllclk);
	__clk_put(pllclk);
	return 0;
}

static int pic32_sosc_test(void)
{
	struct clk *clk;

	pr_info("------------- perform SOSC en(/dis)able test -----------\n");

	clk = __clk_lookup("sosc_clk");
	if (IS_ERR(clk)) {
		pr_info("sosc: not found. So test FAILED.\n");
		return 0;
	}
	__clk_get(clk);
	clk_prepare(clk);
	pr_info("sosc: pre-enable: is_clk_enabeld ? %d\n",
		__clk_is_enabled(clk));
	clk_enable(clk);
	pr_info("sosc: post-enable: is_clk_enabeld ? %d, current rate %lu\n",
		__clk_is_enabled(clk), clk_get_rate(clk));
	clk_disable(clk);
	clk_unprepare(clk);
	__clk_put(clk);
	return 0;
}

static int pic32_test_few_clks(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int testcode = 0;

	if (np) {
		if (of_find_property(np, "microchip,pic32-refclk-test", NULL))
			testcode |= BIT(0);

		if (of_find_property(np, "microchip,pic32-pb1clk-test", NULL))
			testcode |= BIT(1);

		if (of_find_property(np, "microchip,pic32-pb2clk-test", NULL))
			testcode |= BIT(2);

		if (of_find_property(np, "microchip,pic32-pb4clk-test", NULL))
			testcode |= BIT(3);

		if (of_find_property(np, "microchip,pic32-sclk-test", NULL))
			testcode |= BIT(4);

		if (of_find_property(np, "microchip,pic32-spll-test", NULL))
			testcode |= BIT(5);

		if (of_find_property(np, "microchip,pic32-sosc-test", NULL))
			testcode |= BIT(6);

		/* console init */
		pic32_serial_init(np);
	} else {
		testcode = BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5);
	}

#if 0
	if ((testcode & BIT(0))) {
		/* Configure PPS */
		unsigned long flags;
		uint32_t v;

		local_irq_save(flags);

		/* sysunlock */
		pic32_syskey_unlock();

		/* iounlock */
		writel(BIT(13), (void __iomem *)PIC32_CLR(0xbf800000));

		/* PortA_ANSEL: digital */
		writel(BIT(15)|BIT(14), (void __iomem *)PIC32_CLR(0xbf860000));
		/* PortA_TRIS: output */
		writel(BIT(14)|BIT(15), (void __iomem *)PIC32_CLR(0xbf860010));
		/* PORTA_ODC: normal digital output */
		writel(BIT(15)|BIT(14), (void __iomem *)PIC32_SET(0xbf860040));

		/* REFCLKO1 -> SoC 96(RPA15) -> J1-126 -> MEB J4 (5-6) */
		writel(0xf, (void __iomem *)0xbf80153c);
		v = readl((void __iomem *)0xbf80153c);
		if (v != 0xf)
			pr_info("PPS(RPA15) is not working 0x%x\n", v);

		/* REFCLKO4 -> SoC 95(RPA14) -> J1-124 -> MEB J4(3-4) */
		writel(0xd, (void __iomem *)0xbf801538);
		v = readl((void __iomem *)0xbf801538);
		if (v != 0xd)
			pr_info("PPS(RPA14) is not working 0x%x\n", v);

		/* OC1 -> SoC-23(RPE8) -> J1-93 -> MEB J10 (13-14) */
		writel(BIT(8), (void __iomem *)PIC32_CLR(0xbf860500));/*ANSEL5*/
		writel(BIT(8), (void __iomem *)PIC32_CLR(0xbf860510)); /*TRIS5*/
		writel(0xc, (void __iomem *)0xbf801620);

		/*writel(BIT(13), (void __iomem *)PIC32_SET(0xbf800000));*/
		local_irq_restore(flags);
	}
#endif
	/* testcode */
	if (testcode & BIT(0)) {
		pic32_test_refoclk(1);
		pic32_test_refoclk(2);
		pic32_test_refoclk(3);
		pic32_test_refoclk(4);
	}

	if (testcode & BIT(1))
		pic32_test_pbclk(2);

	if (testcode & BIT(2))
		pic32_test_pbclk(3);

	if (testcode & BIT(3))
		pic32_test_pbclk(4);

	if (testcode & BIT(4))
		pic32_test_sclk_switch();

	if (testcode & BIT(5))
		pic32_spll_test();

	if (testcode & BIT(6))
		pic32_sosc_test();

	/* console init */
	pic32_serial_exit();

	return 0;
}

static struct of_device_id pic32_clk_tests_id[] = {
	{ .compatible = "microchip,pic32-clk-test",},
	{ },
};
MODULE_DEVICE_TABLE(of, pic32_clk_tests_id);

static struct platform_driver pic32_clk_driver_test = {
	.driver = {
		.name		= "pic32-clk-test",
		.owner		= THIS_MODULE,
		.of_match_table = of_match_ptr(pic32_clk_tests_id),
	},
	.probe = pic32_test_few_clks,
};
module_platform_driver(pic32_clk_driver_test);

