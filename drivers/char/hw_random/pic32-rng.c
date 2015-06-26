/*
 * Copyright (c) 2014 Joshua Henderson <joshua.henderson@microchip.com>
 *
 * This file is licensed under  the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/hw_random.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/clk.h>
#include <linux/clkdev.h>

#define DRIVER_NAME "pic32-rng"

#define RNGCON		0x04
#define RNGCON_TRNGEN	BIT(8)
#define RNGCON_RNGEN	BIT(9)
#define RNGCON_CONT	BIT(10)
#define RNGCON_LOAD	BIT(12)
#define RNGPOLY1	0x08
#define RNGPOLY2	0x0C
#define RNGNUMGEN1	0x10
#define RNGNUMGEN2	0x14
#define RNGSEED1	0x18
#define RNGSEED2	0x1C
#define RNGRCNT		0x20

struct pic32_rng {
	void __iomem *base;
	struct hwrng rng;
	struct clk *clk;
};

static int pic32_rng_read(struct hwrng *rng, void *buf, size_t max,
			   bool wait)
{
	struct pic32_rng *prng = container_of(rng, struct pic32_rng, rng);
	u64 *data = buf;

	*data = ((u64)readl(prng->base + RNGNUMGEN2) << 32) +
		readl(prng->base + RNGNUMGEN1);
	return 8;
}

static const struct of_device_id pic32_rng_of_match[] = {
	{ .compatible	= "microchip,pic32-rng", },
	{},
};
MODULE_DEVICE_TABLE(of, pic32_rng_of_match);

static int pic32_rng_probe(struct platform_device *pdev)
{
	struct pic32_rng *prng;
	struct resource *res;
	u32 v;
	int ret;

	prng = devm_kzalloc(&pdev->dev, sizeof(*prng), GFP_KERNEL);
	if (!prng)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	prng->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(prng->base))
		return PTR_ERR(prng->base);

	prng->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(prng->clk))
		return PTR_ERR(prng->clk);

	clk_prepare_enable(prng->clk);

	/* enable TRNG & wait for seed */
	writel(0x7f, prng->base + RNGRCNT);
	v = readl(prng->base + RNGCON);
	v |= RNGCON_TRNGEN;
	writel(v, prng->base + RNGCON);
	__udelay(1);

	/* load initial seed & start PRNG */
	v |= 0xff|RNGCON_RNGEN|RNGCON_CONT|RNGCON_LOAD;
	writel(v, prng->base + RNGCON);

	prng->rng.name = pdev->name;
	prng->rng.read = pic32_rng_read;

	ret = hwrng_register(&prng->rng);
	if (ret)
		goto err_register;

	platform_set_drvdata(pdev, prng);

	dev_info(&pdev->dev, "PIC32 Random Number Generator\n");

	return 0;

err_register:
	return ret;
}

static int pic32_rng_remove(struct platform_device *pdev)
{
	struct pic32_rng *rng = platform_get_drvdata(pdev);

	hwrng_unregister(&rng->rng);
	writel(0, rng->base + RNGCON);
	clk_disable_unprepare(rng->clk);
	return 0;
}

static struct platform_driver pic32_rng_driver = {
	.probe		= pic32_rng_probe,
	.remove		= pic32_rng_remove,
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(pic32_rng_of_match),
	},
};

module_platform_driver(pic32_rng_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Joshua Henderson <joshua.henderson@microchip.com>");
MODULE_DESCRIPTION("PIC32 pseudo random number generator driver");
