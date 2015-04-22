/*
 * PIC32 MUSB "glue layer"
 *
 * Cristian Birsan <cristian.birsan@microchip.com>
 * Copyright (C) 2014 Microchip Technology Inc.  All rights reserved.
 *
 * Based on the am35x and dsps "glue layer" code.
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

#include <linux/module.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>

#include <linux/platform_device.h>
#include <linux/dma-mapping.h>

#include <linux/usb/of.h>
#include <linux/gpio.h>
#include "musb_core.h"

#define PIC32_TX_EP_MASK	0xffff		/* EP0 + 15 Tx EPs */
#define PIC32_RX_EP_MASK	0xfffe		/* 15 Rx EPs */

#define	POLL_SECONDS		2

struct pic32_glue {
	struct device		*dev;
	struct platform_device	*musb;
	struct clk		*clk;
	int			oc_irq;
	int			vbus_on_pin;
	int			usb_id_pin;
	int			usb_id_irq;
	struct of_phandle_args	usb_id_oirq;
	struct timer_list	timer;		/* otg_workaround timer */
	unsigned long		last_timer;	/* last timer data for */
};


static irqreturn_t pic32_over_current(int irq, void *d);

static irqreturn_t pic32_usb_id_change(int irq, void *d);

/*
 * pic32_musb_enable - enable interrupts
 */
static void pic32_musb_enable(struct musb *musb)
{
	struct device *dev = musb->controller;
	struct pic32_glue *glue = dev_get_drvdata(dev->parent);

	/* Enable additional interrupts */
	enable_irq(glue->oc_irq);

	if (musb->port_mode == MUSB_PORT_MODE_DUAL_ROLE)
		enable_irq(glue->usb_id_irq);
}

/*
 * pic32_musb_disable - disable HDRC and flush interrupts
 */
static void pic32_musb_disable(struct musb *musb)
{
	struct device *dev = musb->controller;
	struct pic32_glue *glue = dev_get_drvdata(dev->parent);

	musb_writeb(musb->mregs, MUSB_DEVCTL, 0);

	/* Disable additional interrupts */
	disable_irq(glue->oc_irq);

	if (musb->port_mode == MUSB_PORT_MODE_DUAL_ROLE)
		disable_irq(glue->usb_id_irq);
}

static void pic32_musb_set_vbus(struct musb *musb, int is_on)
{
	WARN_ON(is_on && is_peripheral_active(musb));
}

static void otg_timer(unsigned long _musb)
{
	struct musb *musb = (void *)_musb;
	void __iomem *mregs = musb->mregs;
	struct device *dev = musb->controller;
	struct pic32_glue *glue = dev_get_drvdata(dev->parent);

	u8 devctl;
	unsigned long flags;
	int skip_session = 0;

	/*
	 * We poll because IP's won't expose several OTG-critical
	 * status change events (from the transceiver) otherwise.
	 */
	devctl = musb_readb(mregs, MUSB_DEVCTL);
	dev_dbg(dev, "Poll devctl %02x (%s)\n", devctl,
			usb_otg_state_string(musb->xceiv->otg->state));

	spin_lock_irqsave(&musb->lock, flags);
	switch (musb->xceiv->otg->state) {
	case OTG_STATE_A_WAIT_BCON:
		musb_writeb(musb->mregs, MUSB_DEVCTL, 0);
		skip_session = 1;
		/* fall */

	case OTG_STATE_A_IDLE:
	case OTG_STATE_B_IDLE:
		if (devctl & MUSB_DEVCTL_BDEVICE) {
			musb->xceiv->otg->state = OTG_STATE_B_IDLE;
			MUSB_DEV_MODE(musb);
		} else {
			musb->xceiv->otg->state = OTG_STATE_A_IDLE;
			MUSB_HST_MODE(musb);
		}
		if (!(devctl & MUSB_DEVCTL_SESSION) && !skip_session)
			musb_writeb(mregs, MUSB_DEVCTL, MUSB_DEVCTL_SESSION);
		mod_timer(&glue->timer, jiffies + POLL_SECONDS * HZ);
		break;
	case OTG_STATE_A_WAIT_VFALL:
		musb->xceiv->otg->state = OTG_STATE_A_WAIT_VRISE;
		/*musb_writel(musb->ctrl_base, wrp->coreintr_set,
			    MUSB_INTR_VBUSERROR << wrp->usb_shift); */
		break;
	default:
		break;
	}
	spin_unlock_irqrestore(&musb->lock, flags);
}

static void pic32_musb_try_idle(struct musb *musb, unsigned long timeout)
{
	struct device *dev = musb->controller;
	struct pic32_glue *glue = dev_get_drvdata(dev);

	if (timeout == 0)
		timeout = jiffies + msecs_to_jiffies(3);

	/* Never idle if active, or when VBUS timeout is not set as host */
	if (musb->is_active || (musb->a_wait_bcon == 0 &&
				musb->xceiv->otg->state == OTG_STATE_A_WAIT_BCON)) {
		dev_dbg(dev, "%s active, deleting timer\n",
				usb_otg_state_string(musb->xceiv->otg->state));
		del_timer(&glue->timer);
		glue->last_timer = jiffies;
		return;
	}
	if (musb->port_mode != MUSB_PORT_MODE_DUAL_ROLE)
		return;

	if (!musb->g.dev.driver)
		return;

	if (time_after(glue->last_timer, timeout) &&
				timer_pending(&glue->timer)) {
		dev_dbg(dev,
			"Longer idle timer already pending, ignoring...\n");
		return;
	}
	glue->last_timer = timeout;

	dev_dbg(dev, "%s inactive, starting idle timer for %u ms\n",
		usb_otg_state_string(musb->xceiv->otg->state),
			jiffies_to_msecs(timeout - jiffies));
	mod_timer(&glue->timer, timeout);
}

static irqreturn_t pic32_musb_interrupt(int irq, void *hci)
{
	struct musb  *musb = hci;
	void __iomem *reg_base = musb->ctrl_base;
	struct device *dev = musb->controller;
	struct pic32_glue *glue = dev_get_drvdata(dev->parent);
	unsigned long flags;
	irqreturn_t ret = IRQ_NONE;

	u16 epintr_rx, epintr_tx;
	u8 usbintr;

	spin_lock_irqsave(&musb->lock, flags);

	/* Get endpoint interrupts */
	epintr_rx = musb_readw(reg_base, MUSB_INTRRX);
	epintr_tx = musb_readw(reg_base, MUSB_INTRTX);

	if (epintr_rx)
		musb->int_rx = epintr_rx & PIC32_RX_EP_MASK;

	if (epintr_tx)
		musb->int_tx = epintr_tx & PIC32_TX_EP_MASK;

	/* Get usb core interrupts */
	usbintr = musb_readb(reg_base, MUSB_INTRUSB);
	if (!usbintr && !(epintr_rx || epintr_tx)) {
		dev_info(dev, "Got USB spurious interrupt !\n");
		goto eoi;
	}

	if (usbintr)
		musb->int_usb = usbintr;

	if (is_host_active(musb) && usbintr & MUSB_INTR_BABBLE)
		pr_info("CAUTION: musb: Babble Interrupt Occurred\n");

	/* Use ID change IRQ to switch appropriately between halves of the OTG
	 * state machine. Managing DEVCTL.SESSION per Mentor docs requires that
	 * we know its value but DEVCTL.BDEVICE is invalid without
	 * DEVCTL.SESSION set. Also, DRVVBUS pulses for SRP (but not at 5V) ...
	 */

	if (0 /*TODO: update when OTG is required*/) {

		void __iomem *mregs = musb->mregs;
		u8 devctl = musb_readb(mregs, MUSB_DEVCTL);
		int err;
		int usb_id = 0;
		err = musb->int_usb & MUSB_INTR_VBUSERROR;
		if (err) {
			/*
			 * The Mentor core doesn't debounce VBUS as needed
			 * to cope with device connect current spikes. This
			 * means it's not uncommon for bus-powered devices
			 * to get VBUS errors during enumeration.
			 *
			 * This is a workaround, but newer RTL from Mentor
			 * seems to allow a better one: "re"-starting sessions
			 * without waiting for VBUS to stop registering in
			 * devctl.
			 */
			musb->int_usb &= ~MUSB_INTR_VBUSERROR;
			musb->xceiv->otg->state = OTG_STATE_A_WAIT_VFALL;
			mod_timer(&glue->timer,
					jiffies + POLL_SECONDS * HZ);
			WARNING("VBUS error workaround (delay coming)\n");
		} else if (usb_id == 0) {
			MUSB_HST_MODE(musb);
			musb->xceiv->otg->default_a = 1;
			musb->xceiv->otg->state = OTG_STATE_A_WAIT_VRISE;
			del_timer(&glue->timer);
		} else {
			musb->is_active = 0;
			MUSB_DEV_MODE(musb);
			musb->xceiv->otg->default_a = 0;
			musb->xceiv->otg->state = OTG_STATE_B_IDLE;
		}

		/* NOTE: this must complete power-on within 100 ms. */
		dev_dbg(dev, "VBUS %s (%s)%s, devctl %02x\n",
				usb_id ? "on" : "off",
				usb_otg_state_string(musb->xceiv->otg->state),
				err ? " ERROR" : "",
				devctl);
		ret = IRQ_HANDLED;
	}

	/* Drop spurious RX and TX if device is disconnected */
	if (musb->int_usb & MUSB_INTR_DISCONNECT) {
		musb->int_tx = 0;
		musb->int_rx = 0;
	}

	if (musb->int_tx || musb->int_rx || musb->int_usb)
		ret |= musb_interrupt(musb);

eoi:
	/* Poll for ID change in OTG port mode */
	if (musb->xceiv->otg->state == OTG_STATE_B_IDLE &&
			musb->port_mode == MUSB_PORT_MODE_DUAL_ROLE)
		mod_timer(&glue->timer, jiffies + POLL_SECONDS * HZ);

	spin_unlock_irqrestore(&musb->lock, flags);

	return ret;
}

static int pic32_musb_set_mode(struct musb *musb, u8 mode)
{
	struct device *dev = musb->controller;
	struct pic32_glue *glue = dev_get_drvdata(dev->parent);

	switch (mode) {
	case MUSB_HOST:

		/* if we're setting mode to host-only or device-only, we're
		 * going to force ID pin status by SW */
		pic32_pinconf_pullup_runtime(glue->usb_id_pin, 0);
		pic32_pinconf_pulldown_runtime(glue->usb_id_pin, 1);
		dev_dbg(dev, "MUSB Host mode enabled\n");

		break;
	case MUSB_PERIPHERAL:

		/* if we're setting mode to host-only or device-only, we're
		 * going to force ID pin status by SW */
		pic32_pinconf_pulldown_runtime(glue->usb_id_pin, 0);
		pic32_pinconf_pullup_runtime(glue->usb_id_pin, 1);

		dev_dbg(dev, "MUSB Device mode enabled\n");

		break;
	case MUSB_OTG:

		/* TODO: Enable OTG mode using usb_id irq*/
		dev_dbg(dev, "MUSB OTG mode enabled\n");
		break;
	default:
		dev_err(glue->dev, "unsupported mode %d\n", mode);
		return -EINVAL;
	}

	return 0;
}

static int pic32_musb_init(struct musb *musb)
{
	struct device *dev = musb->controller;
	struct pic32_glue *glue = dev_get_drvdata(dev->parent);
	int ret;

	u16 hwvers;

	/* Returns zero if e.g. not clocked */
	hwvers = musb_read_hwvers(musb->mregs);
	if (!hwvers)
		return -ENODEV;

	/* The PHY transceiver is registered using device tree */
	musb->xceiv = usb_get_phy(USB_PHY_TYPE_USB2);
	if (IS_ERR_OR_NULL(musb->xceiv))
		return -EPROBE_DEFER;

	setup_timer(&glue->timer, otg_timer, (unsigned long) musb);

	/* Start the on-chip PHY and its PLL - Enabled by default*/

	musb->isr = pic32_musb_interrupt;

	/* Request other interrupts*/
	irq_set_status_flags(glue->oc_irq, IRQ_NOAUTOEN);
	ret = devm_request_irq(dev, glue->oc_irq, pic32_over_current, 0,
			dev_name(dev), dev);
	if (ret) {
		dev_err(dev, "failed to request irq: %d\n", ret);
		return ret;
	}

	switch (musb->port_mode) {
	case MUSB_PORT_MODE_DUAL_ROLE:

		glue->usb_id_irq = irq_create_of_mapping(&glue->usb_id_oirq);
		irq_set_status_flags(glue->usb_id_irq, IRQ_NOAUTOEN);
		ret = devm_request_irq(dev, glue->usb_id_irq,
				pic32_usb_id_change, 0, dev_name(dev), dev);
		if (ret) {
			dev_err(dev, "failed to request irq: %d\n", ret);
			return ret;
		}
		break;

	case MUSB_PORT_MODE_HOST:
	case MUSB_PORT_MODE_GADGET:
		if (gpio_is_valid(glue->usb_id_pin)) {

			ret = devm_gpio_request(dev, glue->usb_id_pin,
					"usb_id");
			if (ret) {
				dev_err(dev, "error requesting USB_ID GPIO\n");
				return -EINVAL;
			}

			ret = gpio_direction_input(glue->usb_id_pin);
			if (ret) {
				dev_err(dev, "error setting USB_ID GPIO\n");
				return -EINVAL;
			}
		}
		break;
	default:
		dev_err(dev, "unsupported mode %d\n", musb->port_mode);
		return -EINVAL;
	}
	return 0;
}

static int pic32_musb_exit(struct musb *musb)
{
	struct device *dev = musb->controller;
	struct pic32_glue *glue = dev_get_drvdata(dev->parent);

	del_timer_sync(&glue->timer);

	/* Shutdown the on-chip PHY and its PLL. - Enabled by default. Can be
	 * disabled by setting USBMD bit */

	usb_put_phy(musb->xceiv);
	return 0;
}

static irqreturn_t pic32_over_current(int irq, void *d)
{
	struct device *dev = d;
	dev_info(dev, "USB Host over-current detected !\n");

	return IRQ_HANDLED;
}

static irqreturn_t pic32_usb_id_change(int irq, void *d)
{
	struct device *dev = d;
	dev_info(dev, "USB ID Changed !\n");

	return IRQ_HANDLED;
}

/* We support only 32bit read operation */
void pic32_read_fifo(struct musb_hw_ep *hw_ep, u16 len, u8 *dst)
{
	void __iomem *fifo = hw_ep->fifo;
	u32		val;
	int		i;

	/* Read for 32bit-aligned destination address */
	if (likely((0x03 & (unsigned long) dst) == 0) && len >= 4) {
		readsl(fifo, dst, len >> 2);
		dst += len & ~0x03;
		len &= 0x03;
	}
	/*
	 * Now read the remaining 1 to 3 byte or complete length if
	 * unaligned address.
	 */
	if (len > 4) {
		for (i = 0; i < (len >> 2); i++) {
			*(u32 *) dst = musb_readl(fifo, 0);
			dst += 4;
		}
		len &= 0x03;
	}
	if (len > 0) {
		val = musb_readl(fifo, 0);
		memcpy(dst, &val, len);
	}
}

static const struct musb_platform_ops pic32_ops = {
	.init		= pic32_musb_init,
	.exit		= pic32_musb_exit,

	.read_fifo	= pic32_read_fifo,
	.enable		= pic32_musb_enable,
	.disable	= pic32_musb_disable,

	.set_mode	= pic32_musb_set_mode,
	.try_idle	= pic32_musb_try_idle,

	.set_vbus	= pic32_musb_set_vbus,

	.fifo_mode = 2,
};

/* static u64 musb_dmamask = DMA_BIT_MASK(32); TODO: use it with DMA */

static const struct of_device_id pic32_musb_of_match[] = {
	{ .compatible = "microchip,pic32-usb" },
	{  }
};
MODULE_DEVICE_TABLE(of, pic32_musb_of_match);

static const struct platform_device_info pic32_dev_info = {
	.name		= "musb-hdrc",
	.id		= PLATFORM_DEVID_AUTO,
	.dma_mask	= DMA_BIT_MASK(32),
};

static int get_int_prop(struct device_node *dn, const char *s)
{
	int ret;
	u32 val;

	ret = of_property_read_u32(dn, s, &val);
	if (ret)
		return 0;
	return val;
}
static int get_musb_port_mode(struct device *dev)
{
	enum usb_dr_mode mode;

	mode = of_usb_get_dr_mode(dev->of_node);
	switch (mode) {
	case USB_DR_MODE_HOST:
		return MUSB_PORT_MODE_HOST;

	case USB_DR_MODE_PERIPHERAL:
		return MUSB_PORT_MODE_GADGET;

	case USB_DR_MODE_UNKNOWN:
	case USB_DR_MODE_OTG:
	default:
		return MUSB_PORT_MODE_DUAL_ROLE;
	}
}

int pic32_create_musb_pdev(struct pic32_glue *glue,
		struct platform_device *parent)
{
	struct musb_hdrc_platform_data pdata;
	struct resource	resources[2];
	struct resource	*res;
	struct device *dev = &parent->dev;
	struct musb_hdrc_config	*config;
	struct platform_device *musb;
	struct device_node *dn = parent->dev.of_node;
	int ret;

	memset(resources, 0, sizeof(resources));
	res = platform_get_resource_byname(parent, IORESOURCE_MEM, "mc");
	if (!res) {
		dev_err(dev, "failed to get memory.\n");
		return -EINVAL;
	}
	resources[0] = *res;

	resources[0].name = "mc";

	res = platform_get_resource_byname(parent, IORESOURCE_IRQ, "mc");
	if (!res) {
		dev_err(dev, "failed to get irq.\n");
		return -EINVAL;
	}
	resources[1] = *res;
	resources[1].name = "mc";

	/* Allocate the child platform device */
	musb = platform_device_alloc("musb-hdrc", PLATFORM_DEVID_AUTO);
	if (!musb) {
		dev_err(dev, "failed to allocate musb device\n");
		return -ENOMEM;
	}

	musb->dev.parent		= dev;
	musb->dev.dma_mask		= 0;	/* TODO: use it with DMA*/
	musb->dev.coherent_dma_mask	= 0;	/* TODO: use it with DMA*/

	glue->musb = musb;

	ret = platform_device_add_resources(musb, resources,
			ARRAY_SIZE(resources));
	if (ret) {
		dev_err(dev, "failed to add resources\n");
		goto err_pdev;
	}

	config = devm_kzalloc(&parent->dev, sizeof(*config), GFP_KERNEL);
	if (!config) {
		dev_err(dev, "failed to allocate musb hdrc config\n");
		ret = -ENOMEM;
		goto err_pdev;
	}
	pdata.config = config;
	pdata.platform_ops = &pic32_ops;

	config->num_eps = get_int_prop(dn, "mentor,num-eps");
	config->ram_bits = get_int_prop(dn, "mentor,ram-bits");
	config->host_port_deassert_reset_at_resume = 1;
	pdata.mode = get_musb_port_mode(dev);

	/* DT keeps this entry in mA, musb expects it as per USB spec */
	pdata.power = get_int_prop(dn, "mentor,power") / 2;
	config->multipoint = of_property_read_bool(dn, "mentor,multipoint");

	glue->clk = devm_clk_get(dev, "usb_clk");
	if (IS_ERR(glue->clk)) {
		ret = PTR_ERR(glue->clk);
		dev_err(dev, "failed to get usb_clk (%u)\n", ret);
		goto err_pdev;
	}

	ret = clk_prepare_enable(glue->clk);
	if (ret) {
		dev_err(dev, "failed to enable usb_clk (%u)\n", ret);
		goto err_pdev;
	}

	ret = platform_device_add_data(musb, &pdata, sizeof(pdata));
	if (ret) {
		dev_err(dev, "failed to add platform_data\n");
		goto err_disable_clk;
	}

	ret = platform_device_add(musb);
	if (ret) {
		dev_err(dev, "failed to register musb device\n");
		goto err_disable_clk;
	}

	return 0;

err_disable_clk:
	clk_disable_unprepare(glue->clk);
err_pdev:
	platform_device_put(musb);
	return ret;
}

static int pic32_remove(struct platform_device *pdev)
{
	struct pic32_glue	*glue = platform_get_drvdata(pdev);

	platform_device_unregister(glue->musb);
	clk_disable_unprepare(glue->clk);
	devm_clk_put(&pdev->dev, glue->clk);
	devm_kfree(&pdev->dev, glue);

	return 0;
}

#ifdef CONFIG_PM
static int pic32_suspend(struct device *dev)
{
	return 0;
}

static int pic32_resume(struct device *dev)
{
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(pic32_pm_ops, pic32_suspend, pic32_resume);

static int pic32_musb_probe(struct platform_device *pdev)
{
	int ret;
	struct pic32_glue		*glue;
	struct device_node		*dev_node_id, *dev_node_oc;
	const struct of_device_id	*match;


	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "MUSB device tree configuration is needed !\n");
		/* TODO: CB: Check if right return value is used for error ! */
		return -ENXIO;
	}

	match = of_match_node(pic32_musb_of_match, pdev->dev.of_node);
	if (!match) {
		dev_err(&pdev->dev, "fail to get matching of_match struct\n");
		return -EINVAL;
	}

	/* allocate glue */
	glue = devm_kzalloc(&pdev->dev, sizeof(*glue), GFP_KERNEL);
	if (!glue) {
		dev_err(&pdev->dev, "unable to allocate glue memory\n");
		return -ENOMEM;
	}

	/* TODO: Add runtime power management support code when needed */

	glue->vbus_on_pin = of_get_named_gpio(pdev->dev.of_node,
			"vbus_on-gpios", 0);

	if (gpio_is_valid(glue->vbus_on_pin)) {
		ret = devm_gpio_request(&pdev->dev,
				glue->vbus_on_pin, "vbus_on");
		if (ret) {
			dev_err(&pdev->dev, "error requesting VBUS_ON GPIO\n");
			ret = -EINVAL;
			goto err_probe;
		}

		/* Enable VBUS for HOST */
		ret = gpio_direction_output(glue->vbus_on_pin, 1);
		if (ret) {
			dev_err(&pdev->dev, "error setting VBUS_ON GPIO\n");
			ret = -EINVAL;
			goto err_probe;
		}

		gpio_set_value(glue->vbus_on_pin, 1);
	}

	glue->usb_id_pin = of_get_named_gpio(pdev->dev.of_node,
			"usb_id-gpios", 0);

	dev_node_oc = of_get_child_by_name(pdev->dev.of_node,
			"usb_overcurrent");
	if (!dev_node_oc) {
		dev_err(&pdev->dev, "error usb_overcurrent property missing\n");
		ret = -EINVAL;
		goto err_probe;
	}

	glue->oc_irq = irq_of_parse_and_map(dev_node_oc, 0);
	if (!glue->oc_irq) {
		dev_err(&pdev->dev, "cannot get over current irq!\n");
		ret = -EINVAL;
		goto err_put_oc;
	}

	dev_node_id = of_get_child_by_name(pdev->dev.of_node, "usb_id_pin");
	if (!dev_node_id) {
		dev_err(&pdev->dev, "error usb_id property missing\n");
		ret = -EINVAL;
		goto err_put_oc;
	}

	ret = of_irq_parse_one(dev_node_id, 0, &glue->usb_id_oirq);
	if (ret) {
		dev_err(&pdev->dev, "cannot get usb id irq!\n");
		ret = -EINVAL;
		goto err_put_id;
	}

	/* Set the glue code */
	glue->dev = &pdev->dev;
	platform_set_drvdata(pdev, glue);

	ret = pic32_create_musb_pdev(glue, pdev);
	if (ret)
		goto err_put_id;

	return 0;

err_put_id:
	of_node_put(dev_node_id);
err_put_oc:
	of_node_put(dev_node_oc);
err_probe:
	return ret;

}

static struct platform_driver pic32_musb_driver = {
	.probe		= pic32_musb_probe,
	.remove		= pic32_remove,
	.driver		= {
		.name	= "musb-pic32mz",
		.owner = THIS_MODULE,
		.of_match_table	= pic32_musb_of_match,
	},
};

MODULE_DESCRIPTION("PIC32 MUSB Glue Layer");
MODULE_AUTHOR("Cristian Birsan <cristian.birsan@microchip.com>");
MODULE_LICENSE("GPL v2");

module_platform_driver(pic32_musb_driver);
