/*
 * PIC32 pinctrl/pinmux debug driver.
 *
 * Copyright (c) 2014, Microchip Technology Inc.
 *      Purna Chandra Mandal <purna.mandal@microchip.com>
 *
 * Licensed under GPLv2.
 */
#include <linux/types.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/slab.h>

static int pinmux_summary_show(struct seq_file *s, void *data)
{
	u32 p;

	for (p = 0xbf801444; p < 0xbf8016a4; p += 8) {
		seq_printf(s, "0x%08x: 0x%08x",
			p, readl((void __iomem *)p));
		seq_printf(s, "0x%08x: 0x%08x\n",
			p + 4, readl((void __iomem *)p + 4));
	}

	seq_puts(s, "\n");
	return 0;
}

static int pinmux_summary_open(struct inode *inode, struct file *file)
{
	return single_open(file, pinmux_summary_show, inode->i_private);
}

static const struct file_operations pinmux_summary_fops = {
	.open		= pinmux_summary_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static struct debugfs_reg32 pps_port_regs_debug[] = {
	{ .name = "ANSELx", .offset = 0,},
	{ .name = "TRISx", .offset = 0x10,},
	{ .name = "PORTx", .offset = 0x20,},
	{ .name = "LATx", .offset = 0x30,},
	{ .name = "ODCx", .offset = 0x40,},
	{ .name = "CNPUx", .offset = 0x50,},
	{ .name = "CNPDx", .offset = 0x60,},
	{ .name = "CNCONx", .offset = 0x70,},
	{ .name = "CNENx", .offset = 0x80,},
	{ .name = "CNSTAx", .offset = 0x90,},
};

static int pinctrl_port_summary_init(int port, struct dentry *dentry)
{
	struct dentry *file;
	char *name;
	struct debugfs_regset32 *regset;

	regset = kzalloc(sizeof(*regset) + 20, GFP_KERNEL);
	regset->base = (void __iomem *)0xbf860000 + (port * 0x100);
	regset->regs = pps_port_regs_debug;
	regset->nregs = ARRAY_SIZE(pps_port_regs_debug);

	name = ((void *)regset + sizeof(*regset));
	snprintf(name, 20, "port-%c", port + 'A');

	file = debugfs_create_regset32(name, S_IRUGO, dentry, regset);
	if (IS_ERR(file))
		return PTR_ERR(file);

	return 0;
}

static int pinctrl_debug_init(void)
{
	int i = 0;
	struct dentry *pinctrl_debug_root;

	pinctrl_debug_root = debugfs_create_dir("pps", NULL);
	if (IS_ERR(pinctrl_debug_root))
		return PTR_ERR(pinctrl_debug_root);

	for (i = 0; i < 8; i++)
		pinctrl_port_summary_init(i, pinctrl_debug_root);

	debugfs_create_file("mux_summary", S_IRUGO, pinctrl_debug_root, NULL,
			&pinmux_summary_fops);

	return 0;
}
late_initcall(pinctrl_debug_init);
