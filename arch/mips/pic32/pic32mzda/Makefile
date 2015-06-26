#
# Joshua Henderson, joshua.henderson@microchip.com
# Copyright (C) 2014 Microchip Technology, Inc.  All rights reserved.
#
obj-y			:= pic32-init.o pic32-setup.o pic32-time.o \
				pic32-time.o early_pinctrl.o

obj-$(CONFIG_EARLY_PRINTK)	+= pic32-console.o

CFLAGS_pic32-setup.o = -I$(src)/../../../../scripts/dtc/libfdt

