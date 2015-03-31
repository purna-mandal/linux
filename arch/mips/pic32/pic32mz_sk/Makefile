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
obj-y			:= pic32-init.o pic32-setup.o \
				pic32-ebi.o pic32-prefetch.o \
				pic32-time.o early_pinctrl.o

ifdef CONFIG_SYS_SUPPORTS_ZBOOT
obj-y			+= pic32-memory-zboot.o
else
obj-y			+= pic32-memory.o
endif

obj-$(CONFIG_EARLY_PRINTK)	+= pic32-console.o

obj-y += pic32-clk-test.o
CFLAGS_pic32-clk-test.o := -w

CFLAGS_pic32-setup.o = -I$(src)/../../../../scripts/dtc/libfdt

