/*
 * Support of SDHCI platform devices for Microchip ePlatform
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

#define MIN_FREQ 400000

#define PIC32_MMC_OCR ( MMC_VDD_32_33 | MMC_VDD_33_34 )

struct eplat_sdhci_pdata {
	struct clk *clk;
};

unsigned int eplat_sdhci_get_max_clock(struct sdhci_host *host) 
{
	return 25000000;
}

unsigned int eplat_sdhci_get_min_clock(struct sdhci_host *host) 
{
	return 25000000;
}

static const struct sdhci_ops eplat_sdhci_ops = {
	.get_max_clock = eplat_sdhci_get_max_clock,
	.get_min_clock = eplat_sdhci_get_min_clock,
};

#ifdef CONFIG_OF
static int eplat_sdhci_probe_dt(struct platform_device *pdev,
				struct eplat_sdhci_pdata *pdata)
{
	int ret = 0;
#ifdef EPLATFORM_SDHC_TODO
	struct sdhci_pdata *pdata = NULL;
	struct device_node *np = pdev->dev.of_node;
	int cd_gpio;

	cd_gpio = of_get_named_gpio(np, "cd-gpios", 0);
	if (!gpio_is_valid(cd_gpio))
		cd_gpio = -1;

	/* If pdata is required */
	if (cd_gpio != -1) {
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata)
			dev_err(&pdev->dev, "DT: kzalloc failed\n");
		else
			pdata->card_int_gpio = cd_gpio;
	}
	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
#endif

	return ret;
}
#else
static int eplat_sdhci_probe_dt(struct platform_device *pdev,
				struct eplat_sdhci_pdata *pdata)
{
	return -ENOSYS;
}
#endif

int eplat_sdhci_probe(struct platform_device *pdev)
{
	struct sdhci_host *host;
	struct resource *iomem;
	struct eplat_sdhci_pdata *sdhci;
	struct device *dev;
	int ret;

	dev = pdev->dev.parent ? pdev->dev.parent : &pdev->dev;
	host = sdhci_alloc_host(dev, sizeof(*sdhci));
	if (IS_ERR(host)) {
		ret = PTR_ERR(host);
		dev_dbg(&pdev->dev, "cannot allocate memory for sdhci\n");
		goto err;
	}

	iomem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	host->ioaddr = devm_ioremap_resource(&pdev->dev, iomem);
	if (IS_ERR(host->ioaddr)) {
		ret = PTR_ERR(host->ioaddr);
		dev_dbg(&pdev->dev, "unable to map iomem: %d\n", ret);
		goto err_host;
	}

	host->ops = &eplat_sdhci_ops;
	host->irq = platform_get_irq(pdev, 0);
#ifdef EPLATFORM_SDHC_TODO
	host->quirks = SDHCI_QUIRK_BROKEN_CARD_DETECTION;
#endif
	host->quirks2 = SDHCI_QUIRK2_NO_1_8_V;
	host->mmc->ocr_avail = PIC32_MMC_OCR;

	sdhci = sdhci_priv(host);

	dev->init_name = "eplat-sdhci";

	/* clk enable */
#ifdef EPLATFORM_SDHC_TODO
	sdhci->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(sdhci->clk)) {
		ret = PTR_ERR(sdhci->clk);
		dev_dbg(&pdev->dev, "Error getting clock\n");
		goto err_host;
	}

	ret = clk_prepare_enable(sdhci->clk);
	if (ret) {
		dev_dbg(&pdev->dev, "Error enabling clock\n");
		goto err_host;
	}

	ret = clk_set_rate(sdhci->clk, 25000000);
	if (ret)
		dev_dbg(&pdev->dev, "Error setting desired clk, clk=%lu\n",
				clk_get_rate(sdhci->clk));
#endif
	ret = eplat_sdhci_probe_dt(pdev, sdhci);
	if (ret) {
		dev_err(&pdev->dev, "failed to parse device tree!\n");
		goto disable_clk;
	}

#ifdef EPLATFORM_SDHC_TODO
	/*
	 * It is optional to use GPIOs for sdhci card detection. If
	 * sdhci->data is NULL, then use original sdhci lines otherwise
	 * GPIO lines. We use the built-in GPIO support for this.
	 */
	if (sdhci->data && sdhci->data->card_int_gpio >= 0) {
		ret = mmc_gpio_request_cd(host->mmc,
					  sdhci->data->card_int_gpio, 0);
		if (ret < 0) {
			dev_dbg(&pdev->dev,
				"failed to request card-detect gpio%d\n",
				sdhci->data->card_int_gpio);
			goto disable_clk;
		}
	}
#endif

	ret = sdhci_add_host(host);
	if (ret) {
		dev_dbg(&pdev->dev, "error adding host\n");
		goto disable_clk;
	} else {
		printk("Successfully added sdhci host\n");
	}

	/* EPLATFORM_SDHC_TODO: Why isn't this bit getting set in the stack? */
	*(volatile unsigned int*)0xBF8EC0E0 |= 0x00010000;

	platform_set_drvdata(pdev, host);

	return 0;

disable_clk:
	clk_disable_unprepare(sdhci->clk);
err_host:
	sdhci_free_host(host);
err:
	dev_err(&pdev->dev, "eplat-sdhci probe failed: %d\n", ret);
	return ret;
}

static int eplat_sdhci_remove(struct platform_device *pdev)
{
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct eplat_sdhci_pdata *sdhci = sdhci_priv(host);
	int dead = 0;
	u32 scratch;

	scratch = readl(host->ioaddr + SDHCI_INT_STATUS);
	if (scratch == (u32)-1)
		dead = 1;

	sdhci_remove_host(host, dead);
	clk_disable_unprepare(sdhci->clk);
	sdhci_free_host(host);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int eplat_sdhci_suspend(struct device *dev)
{
	int ret = 0;

#ifdef EPLATFORM_SDHC_TODO
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct eplat_sdhci_pdata *sdhci = sdhci_priv(host);

	ret = sdhci_suspend_host(host);
	if (!ret)
		clk_disable(sdhci->clk);
#endif

	return ret;
}

static int eplat_sdhci_resume(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);

#ifdef EPLATFORM_SDHC_TODO
	struct eplat_sdhci_pdata *sdhci = sdhci_priv(host);
	int ret = 0;

	ret = clk_enable(sdhci->clk);
	if (ret) {
		dev_dbg(dev, "Resume: Error enabling clock\n");
		return ret;
	}
#endif

	return sdhci_resume_host(host);
}
#endif

static SIMPLE_DEV_PM_OPS(sdhci_pm_ops, eplat_sdhci_suspend, eplat_sdhci_resume);

static const struct of_device_id eplat_sdhci_id_table[] = {
	{ .compatible = "microchip,sdhci-eplatform" },
	{}
};
MODULE_DEVICE_TABLE(of, eplat_sdhci_id_table);

static struct platform_driver eplat_sdhci_driver = {
	.driver = {
		.name	= "eplat-sdhci",
		.owner	= THIS_MODULE,
		.pm	= &sdhci_pm_ops,
		.of_match_table = of_match_ptr(eplat_sdhci_id_table),
	},
	.probe		= eplat_sdhci_probe,
	.remove		= eplat_sdhci_remove,
};

module_platform_driver(eplat_sdhci_driver);

MODULE_DESCRIPTION("Microchip ePlatform SDHCI driver");
MODULE_AUTHOR("Pistirica Sorin Andrei");
MODULE_LICENSE("GPL v2");
