/*
 * Support of SDHCI platform devices for Microchip PIC32.
 *
 * Copyright (C) 2015 Microchip
 * Andrei Pistirica, Paul Thacker
 *
 * Inspired by sdhci-pltfm.c
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/highmem.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/mmc/host.h>
#include <linux/mmc/slot-gpio.h>
#include <linux/io.h>
#include "sdhci.h"

#define PIC32_MMC_OCR (MMC_VDD_32_33 | MMC_VDD_33_34)

#define DEV_NAME "pic32-sdhci"
struct pic32_sdhci_pdata {
	struct platform_device	*pdev;
	struct clk *clk;
};

unsigned int pic32_sdhci_get_max_clock(struct sdhci_host *host)
{
	struct pic32_sdhci_pdata *sdhci_pdata = sdhci_priv(host);
	unsigned int clk_rate = clk_get_rate(sdhci_pdata->clk);
	struct platform_device *pdev = sdhci_pdata->pdev;

	dev_dbg(&pdev->dev, "Sdhc max clock rate: %u\n", clk_rate);

	/* PIC32MZDA_SDHC_TODO: base clock when available */
	clk_rate = 25000000;
	return clk_rate;
}

unsigned int pic32_sdhci_get_min_clock(struct sdhci_host *host)
{
	struct pic32_sdhci_pdata *sdhci_pdata = sdhci_priv(host);
	unsigned int clk_rate = clk_get_rate(sdhci_pdata->clk);
	struct platform_device *pdev = sdhci_pdata->pdev;

	dev_dbg(&pdev->dev, "Sdhc min clock rate: %u\n", clk_rate);

	clk_rate = 25000000;
	return clk_rate;
}

static const struct sdhci_ops pic32_sdhci_ops = {
	/* PIC32MZDA_SDHC_TODO: This should be removed - leave the sdhci
	 * layer to do the job. */
	.get_max_clock = pic32_sdhci_get_max_clock,
	.get_min_clock = pic32_sdhci_get_min_clock,

	.set_clock = sdhci_set_clock,
	.set_bus_width = sdhci_set_bus_width,
	.reset = sdhci_reset,
	.set_uhs_signaling = sdhci_set_uhs_signaling,

};

void pic32_sdhci_shared_bus(struct platform_device *pdev)
{
#define SDH_SHARED_BUS_CTRL		0x000000E0
#define SDH_SHARED_BUS_NR_CLK_PINS_MASK	0x7
#define SDH_SHARED_BUS_NR_IRQ_PINS_MASK	0x30
#define SDH_SHARED_BUS_CLK_PINS		0x10
#define SDH_SHARED_BUS_IRQ_PINS		0x14
	struct sdhci_host *host = platform_get_drvdata(pdev);
	u32 bus = readl(host->ioaddr + SDH_SHARED_BUS_CTRL);
	u32 clk_pins = (bus & SDH_SHARED_BUS_NR_CLK_PINS_MASK) >> 0;
	u32 irq_pins = (bus & SDH_SHARED_BUS_NR_IRQ_PINS_MASK) >> 4;

	/* select first clock */
	if (clk_pins & 0x1)
		bus |= (0x1 << SDH_SHARED_BUS_CLK_PINS);

	/* select first interrupt */
	if (irq_pins & 0x1)
		bus |= (0x1 << SDH_SHARED_BUS_IRQ_PINS);

	writel(bus, host->ioaddr + SDH_SHARED_BUS_CTRL);
}

static int pic32_sdhci_probe_platform(struct platform_device *pdev,
				      struct pic32_sdhci_pdata *pdata)
{
#define SDH_CAPS_SDH_SLOT_TYPE_MASK	0xC0000000
#define SDH_SLOT_TYPE_REMOVABLE		0x0
#define SDH_SLOT_TYPE_EMBEDDED		0x1
#define SDH_SLOT_TYPE_SHARED_BUS	0x2
	int ret = 0;
	struct sdhci_host *host = platform_get_drvdata(pdev);
	u32 caps_slot_type = (readl(host->ioaddr + SDHCI_CAPABILITIES) &
					SDH_CAPS_SDH_SLOT_TYPE_MASK) >> 30;

	/* PIC32MZDA_SDHC_TODO: !If shared bus is used on Darlington, then
	 * the bus width, irq and clock should be set via DT */

	/* Card slot connected on shared bus. */
	if (caps_slot_type == SDH_SLOT_TYPE_SHARED_BUS)
		pic32_sdhci_shared_bus(pdev);

	return ret;
}

int pic32_sdhci_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sdhci_host *host;
	struct resource *iomem;
	struct pic32_sdhci_pdata *sdhci_pdata;
	/* unsigned int clk_rate = 0; */
	int ret;

	host = sdhci_alloc_host(dev, sizeof(*sdhci_pdata));
	if (IS_ERR(host)) {
		ret = PTR_ERR(host);
		dev_err(&pdev->dev, "cannot allocate memory for sdhci\n");
		goto err;
	}
	sdhci_pdata = sdhci_priv(host);
	sdhci_pdata->pdev = pdev;
	platform_set_drvdata(pdev, host);

	iomem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	host->ioaddr = devm_ioremap_resource(&pdev->dev, iomem);
	if (IS_ERR(host->ioaddr)) {
		ret = PTR_ERR(host->ioaddr);
		dev_err(&pdev->dev, "unable to map iomem: %d\n", ret);
		goto err_host;
	}

	host->ops = &pic32_sdhci_ops;
	host->irq = platform_get_irq(pdev, 0);

	host->quirks2 = SDHCI_QUIRK2_NO_1_8_V;
	host->mmc->ocr_avail = PIC32_MMC_OCR;

	/* SDH CLK enable */
	sdhci_pdata->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(sdhci_pdata->clk)) {
		ret = PTR_ERR(sdhci_pdata->clk);
		dev_err(&pdev->dev, "Error getting clock\n");
		goto err_host;
	}

#if defined(PIC32MZDA_SDHC_TODO)
	/* Enable clock when available! */
	ret = clk_prepare_enable(sdhci_pdata->clk);
	if (ret) {
		dev_dbg(&pdev->dev, "Error enabling clock\n");
		goto err_host;
	}

	clk_rate = clk_get_rate(sdhci_pdata->clk);
	dev_dbg(&pdev->dev, "base clock at: %u\n", clk_rate);
#endif

	ret = pic32_sdhci_probe_platform(pdev, sdhci_pdata);
	if (ret) {
		dev_err(&pdev->dev, "failed to probe platform!\n");
		goto err_host;
	}

	ret = sdhci_add_host(host);
	if (ret) {
		dev_dbg(&pdev->dev, "error adding host\n");
		goto err_host;
	}

	dev_info(&pdev->dev, "Successfully added sdhci host\n");
	return 0;

err_host:
	sdhci_free_host(host);
err:
	dev_err(&pdev->dev, "pic32-sdhci probe failed: %d\n", ret);
	return ret;
}

static int pic32_sdhci_remove(struct platform_device *pdev)
{
	struct sdhci_host *host = platform_get_drvdata(pdev);
	int dead = 0;
	u32 scratch;

	scratch = readl(host->ioaddr + SDHCI_INT_STATUS);
	if (scratch == (u32)-1)
		dead = 1;

	sdhci_remove_host(host, dead);
#ifdef PIC32MZDA_SDHC_TODO
	clk_disable_unprepare(sdhci_pdata->clk);
#endif
	sdhci_free_host(host);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int pic32_sdhci_suspend(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);

#ifdef PIC32MZDA_SDHC_TODO
	clk_disable(sdhci_pdata->clk);
#endif
	return sdhci_suspend_host(host);
}

static int pic32_sdhci_resume(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);

#ifdef PIC32MZDA_SDHC_TODO
	clk_enable(sdhci_pdata->clk);
#endif
	return sdhci_resume_host(host);
}
#endif

static SIMPLE_DEV_PM_OPS(sdhci_pm_ops, pic32_sdhci_suspend, pic32_sdhci_resume);

static const struct of_device_id pic32_sdhci_id_table[] = {
	{ .compatible = "microchip,pic32-sdhci" },
	{}
};
MODULE_DEVICE_TABLE(of, pic32_sdhci_id_table);

static struct platform_driver pic32_sdhci_driver = {
	.driver = {
		.name	= DEV_NAME,
		.owner	= THIS_MODULE,
		.pm	= &sdhci_pm_ops,
		.of_match_table = of_match_ptr(pic32_sdhci_id_table),
	},
	.probe		= pic32_sdhci_probe,
	.remove		= pic32_sdhci_remove,
};

module_platform_driver(pic32_sdhci_driver);

MODULE_DESCRIPTION("Microchip PIC32 SDHCI driver");
MODULE_AUTHOR("Pistirica Sorin Andrei");
MODULE_LICENSE("GPL v2");
