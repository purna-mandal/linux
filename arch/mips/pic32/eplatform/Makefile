#
# Joshua Henderson, joshua.henderson@microchip.com
# Copyright (C) 2014 Microchip Technology, Inc.  All rights reserved.
#
# Carsten Langgaard, carstenl@mips.com
# Copyright (C) 1999,2000 MIPS Technologies, Inc.  All rights reserved.
#
# Copyright (C) 2008 Wind River Systems, Inc.
#   written by Ralf Baechle <ralf@linux-mips.org>
#
obj-y			:= pic32-init.o pic32-setup.o pic32-time.o \
				pic32-clk.o pic32-platform.o

obj-$(CONFIG_MIPS_PIC32_ETHER) += pic32-platform-eth.o

obj-$(CONFIG_EARLY_PRINTK)	+= pic32-console.o

CFLAGS_pic32-setup.o = -I$(src)/../../../../scripts/dtc/libfdt

