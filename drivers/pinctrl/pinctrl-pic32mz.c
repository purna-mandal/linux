/*
 * pic32mz pinctrl driver.
 *
 * Copyright (C) 2014 Microchip Technology, Inc.
 * Author: Sorin-Andrei Pistirica <andrei.pistirica@microchip.com>
 *
 * Licensed under GPLv2 or later.
 */

#include <linux/err.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>

#include "pinctrl-pic32.h"
#include "dt-bindings/pinctrl/pic32.h"

/* PIC32MZ PORT I/O: register offsets */
static unsigned pic32mz_pio_lookup_off[PIC32_LAST] = {
	[PIC32_ANSEL]	= 0 * PIC32_PIO_REGS * PIC32_REG_SIZE,
	[PIC32_TRIS]	= 1 * PIC32_PIO_REGS * PIC32_REG_SIZE,
	[PIC32_PORT]	= 2 * PIC32_PIO_REGS * PIC32_REG_SIZE,
	[PIC32_LAT]	= 3 * PIC32_PIO_REGS * PIC32_REG_SIZE,
	[PIC32_ODC]	= 4 * PIC32_PIO_REGS * PIC32_REG_SIZE,
	[PIC32_CNPU]	= 5 * PIC32_PIO_REGS * PIC32_REG_SIZE,
	[PIC32_CNPD]	= 6 * PIC32_PIO_REGS * PIC32_REG_SIZE,
	[PIC32_CNCON]	= 7 * PIC32_PIO_REGS * PIC32_REG_SIZE,
	[PIC32_CNEN]	= 8 * PIC32_PIO_REGS * PIC32_REG_SIZE,
	[PIC32_CNSTAT]	= 9 * PIC32_PIO_REGS * PIC32_REG_SIZE
};

static void pic32mz_build_pio_lookup_off(
		unsigned (*pio_lookup_off)[PIC32_LAST])
{
	int i;

	for (i = 0; i < PIC32_LAST; i++) {
		if ((*pio_lookup_off)[i] == 0 && i != PIC32_ANSEL)
			(*pio_lookup_off)[i] = PIC32_OFF_UNSPEC;
	}
}

static int pic32mz_gpio_probe(struct platform_device *pdev)
{
	pic32mz_build_pio_lookup_off(&pic32mz_pio_lookup_off);

	return pic32_gpio_probe(pdev,
				&pic32mz_pio_lookup_off,
				PIC32_LAST);
}

static struct of_device_id pic32mz_gpio_of_match[] = {
	{ .compatible = "microchip,pic32-gpio" },
	{ /* sentinel */ }
};

static struct platform_driver pic32mz_gpio_driver = {
	.driver = {
		.name = "gpio-pic32mz",
		.owner = THIS_MODULE,
		.of_match_table = pic32mz_gpio_of_match,
	},
	.probe = pic32mz_gpio_probe,
};

/* Peripheral Pin Select (input): register offsets */
static unsigned pic32mz_ppsin_lookup_off[PP_MAX] = {
	[PP_INT1]	= 0  * PIC32_REG_SIZE,
	[PP_INT2]	= 1  * PIC32_REG_SIZE,
	[PP_INT3]	= 2  * PIC32_REG_SIZE,
	[PP_INT4]	= 3  * PIC32_REG_SIZE,
	/* rsv		= 4  */
	[PP_T2CK]	= 5  * PIC32_REG_SIZE,
	[PP_T3CK]	= 6  * PIC32_REG_SIZE,
	[PP_T4CK]	= 7  * PIC32_REG_SIZE,
	[PP_T5CK]	= 8  * PIC32_REG_SIZE,
	[PP_T6CK]	= 9  * PIC32_REG_SIZE,
	[PP_T7CK]	= 10 * PIC32_REG_SIZE,
	[PP_T8CK]	= 11 * PIC32_REG_SIZE,
	[PP_T9CK]	= 12 * PIC32_REG_SIZE,
	[PP_IC1]	= 13 * PIC32_REG_SIZE,
	[PP_IC2]	= 14 * PIC32_REG_SIZE,
	[PP_IC3]	= 15 * PIC32_REG_SIZE,
	[PP_IC4]	= 16 * PIC32_REG_SIZE,
	[PP_IC5]	= 17 * PIC32_REG_SIZE,
	[PP_IC6]	= 18 * PIC32_REG_SIZE,
	[PP_IC7]	= 19 * PIC32_REG_SIZE,
	[PP_IC8]	= 20 * PIC32_REG_SIZE,
	[PP_IC9]	= 21 * PIC32_REG_SIZE,
	/* rsv		= 22 */
	[PP_OCFA]	= 23 * PIC32_REG_SIZE,
	/* rsv		= 24 */
	[PP_U1RX]	= 25 * PIC32_REG_SIZE,
	[PP_U1CTS]	= 26 * PIC32_REG_SIZE,
	[PP_U2RX]	= 27 * PIC32_REG_SIZE,
	[PP_U2CTS]	= 28 * PIC32_REG_SIZE,
	[PP_U3RX]	= 29 * PIC32_REG_SIZE,
	[PP_U3CTS]	= 30 * PIC32_REG_SIZE,
	[PP_U4RX]	= 31 * PIC32_REG_SIZE,
	[PP_U4CTS]	= 32 * PIC32_REG_SIZE,
	[PP_U5RX]	= 33 * PIC32_REG_SIZE,
	[PP_U5CTS]	= 34 * PIC32_REG_SIZE,
	[PP_U6RX]	= 35 * PIC32_REG_SIZE,
	[PP_U6CTS]	= 36 * PIC32_REG_SIZE,
	/* rsv		= 37 */
	[PP_SDI1]	= 38 * PIC32_REG_SIZE,
	[PP_SS1]	= 39 * PIC32_REG_SIZE,
	/* rsv		= 40 */
	[PP_SDI2]	= 41 * PIC32_REG_SIZE,
	[PP_SS2]	= 42 * PIC32_REG_SIZE,
	/* rsv		= 43 */
	[PP_SDI3]	= 44 * PIC32_REG_SIZE,
	[PP_SS3]	= 45 * PIC32_REG_SIZE,
	/* rsv		= 46 */
	[PP_SDI4]	= 47 * PIC32_REG_SIZE,
	[PP_SS4]	= 48 * PIC32_REG_SIZE,
	/* rsv		= 49 */
	[PP_SDI5]	= 50 * PIC32_REG_SIZE,
	[PP_SS5]	= 51 * PIC32_REG_SIZE,
	/* rsv		= 52 */
	[PP_SDI6]	= 53 * PIC32_REG_SIZE,
	[PP_SS6]	= 54 * PIC32_REG_SIZE,
	[PP_C1RX]	= 55 * PIC32_REG_SIZE,
	[PP_C2RX]	= 56 * PIC32_REG_SIZE,
	[PP_REFCLKI1]	= 57 * PIC32_REG_SIZE,
	/* rsv		= 58 */
	[PP_REFCLKI3]	= 59 * PIC32_REG_SIZE,
	[PP_REFCLKI4]	= 60 * PIC32_REG_SIZE
};

static void pic32mz_build_ppsin_lookup_off(
		unsigned (*ppsin_lookup_off)[PP_MAX])
{
	int i;

	for (i = 0; i < PP_MAX; i++) {
		if ((*ppsin_lookup_off)[i] == 0 && i != PP_INT1)
			(*ppsin_lookup_off)[i] = PIC32_OFF_UNSPEC;
	}
}

#define PIC32MZ_PPSOUT_MAP_PIN_BANK_START	PORT_A
#define PIC32MZ_PPSOUT_MAP_PIN_BANK_END		PORT_G
#define PIC32MZ_PPSOUT_MAP_PINS_PER_BANK	16
#define PIC32MZ_PPSOUT_MAP_PIN_START		14
#define PIC32MZ_PPSOUT_MAP_PIN_END		10
static unsigned pic32mz_ppsout_lookup_off[MAX_PIO_BANKS][PINS_PER_BANK];

static void pic32mz_build_ppsout_lookup_off(
		unsigned (*ppsout_lookup_off)[MAX_PIO_BANKS][PINS_PER_BANK])
{
	unsigned off_start = PIC32MZ_PPSOUT_MAP_PIN_START;
	unsigned bank, pin;

	memset((*ppsout_lookup_off), PIC32_OFF_UNSPEC,
			MAX_PIO_BANKS * PINS_PER_BANK * sizeof(unsigned));

	for (bank = PIC32MZ_PPSOUT_MAP_PIN_BANK_START;
	     bank <= PIC32MZ_PPSOUT_MAP_PIN_BANK_END; bank++){
		unsigned s_pin = 0;
		unsigned l_pin = PIC32MZ_PPSOUT_MAP_PINS_PER_BANK;

		if (bank == PIC32MZ_PPSOUT_MAP_PIN_BANK_START)
			s_pin = PIC32MZ_PPSOUT_MAP_PIN_START;

		if (bank == PIC32MZ_PPSOUT_MAP_PIN_BANK_END)
			l_pin = PIC32MZ_PPSOUT_MAP_PIN_END;

		for (pin = s_pin; pin < l_pin; pin++) {
			(*ppsout_lookup_off)[bank][pin] =
				(bank * PIC32MZ_PPSOUT_MAP_PINS_PER_BANK -
				 off_start + pin) * PIC32_REG_SIZE;
		}
	}
}

static struct pic32_pps_off pic32mz_pps_off = {
	.ppsout_lookup_off = &pic32mz_ppsout_lookup_off,
	.ppsin_lookup_off = &pic32mz_ppsin_lookup_off,
};

static struct pic32_caps pic32mz_caps = {
	.pinconf_caps = 0,
	.pinconf_incaps = 0,
	.pinconf_outcaps = 0
};

static void pic32mz_build_caps(struct pic32_caps *caps)
{
	(caps->pinconf_caps)	= PIC32_PIN_CONF_NONE |
				  PIC32_PIN_CONF_OD |
				  PIC32_PIN_CONF_PU |
				  PIC32_PIN_CONF_PD |
				  PIC32_PIN_CONF_AN |
				  PIC32_PIN_CONF_DG;
	(caps->pinconf_outcaps) = PIC32_PIN_CONF_DOUT |
				  PIC32_PIN_CONF_OD_OUT |
				  PIC32_PIN_CONF_DG_OUT;
	(caps->pinconf_incaps) = PIC32_PIN_CONF_DIN |
				 PIC32_PIN_CONF_PU_IN |
				 PIC32_PIN_CONF_PD_IN |
				 PIC32_PIN_CONF_AN_IN |
				 PIC32_PIN_CONF_DG_IN;
}

static int pic32mz_pinctrl_probe(struct platform_device *pdev)
{
	pic32mz_build_caps(&pic32mz_caps);

	return pic32_pinctrl_probe(pdev,
				   &pic32mz_pps_off,
				   &pic32mz_caps);
}

static struct of_device_id pic32mz_pinctrl_of_match[] = {
	{ .compatible = "microchip,pic32-pinctrl"},
	{ /* sentinel */ }
};

static struct platform_driver pic32mz_pinctrl_driver = {
	.driver = {
		.name = "pinctrl-pic32mz",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(pic32mz_pinctrl_of_match),
	},
	.probe = pic32mz_pinctrl_probe,
};

static int __init pic32mz_pinctrl_init(void)
{
	int ret = 0;

	pic32mz_build_ppsin_lookup_off(&pic32mz_ppsin_lookup_off);
	pic32mz_build_ppsout_lookup_off(&pic32mz_ppsout_lookup_off);

	ret = platform_driver_register(&pic32mz_gpio_driver);
	if (ret)
		return ret;

	return platform_driver_register(&pic32mz_pinctrl_driver);
}
arch_initcall(pic32mz_pinctrl_init);

static void __exit pic32mz_pinctrl_exit(void)
{
	platform_driver_unregister(&pic32mz_gpio_driver);
	platform_driver_unregister(&pic32mz_pinctrl_driver);
}
module_exit(pic32mz_pinctrl_exit);

MODULE_AUTHOR("Sorin-Andrei Pistirica <andrei.pistirica@microchip.com>");
MODULE_DESCRIPTION("Microchop pic32mz pinctrl driver");
MODULE_LICENSE("GPL v2");
