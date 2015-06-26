/*
 *  Joshua Henderson, joshua.henderson@microchip.com
 *  Copyright (C) 2014 Microchip Technology Inc.  All rights reserved.
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
#include <linux/init.h>
#include <linux/libfdt.h>
#include <linux/of_platform.h>
#include <linux/of_fdt.h>

#include <asm/bootinfo.h>
#include <asm/mips-boards/generic.h>
#include <asm/prom.h>
#include <asm/fw/fw.h>

const char *get_system_type(void)
{
	return "PIC32MZDA";
}

void __init plat_mem_setup(void)
{
	/*
	 * Load the builtin device tree. This causes the chosen node to be
	 * parsed resulting in our memory appearing.
	 */
	__dt_setup_arch(&__dtb_start);
}

void __init device_tree_init(void)
{
	if (!initial_boot_params)
		return;

	unflatten_and_copy_device_tree();
}

static int __init customize_machine(void)
{
	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);
	return 0;
}
arch_initcall(customize_machine);
