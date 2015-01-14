/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2014 Joshua Henderson <joshua.henderson@microchip.com>
 */
#ifndef _ASM_PIC32_H
#define _ASM_PIC32_H

#include <asm/io.h>

/*
 * PIC32 register offsets for SET/CLR/INV masks.
 */
#define PIC32_CLR(reg)		(reg+0x04)
#define PIC32_SET(reg)		(reg+0x08)
#define PIC32_INV(reg)		(reg+0x0C)

/*
 * PIC32 Base Register Offsets
 */
#define PIC32_BASE_CONFIG	0xbf800000
#define PIC32_BASE_WDT		0xbf800800
#define PIC32_BASE_RTC		0xbf800c00
#define PIC32_BASE_OSC		0xbf801200
#define PIC32_BASE_RESET	0xbf801240
#define PIC32_BASE_PPS		0xbf801400
#define PIC32_BASE_INT		0xbf810000
#define PIC32_BASE_DMA		0xbf811000
#define PIC32_BASE_I2C1		0xbf820000
#define PIC32_BASE_I2C2		0xbf820200
#define PIC32_BASE_I2C3		0xbf820400
#define PIC32_BASE_I2C4		0xbf820600
#define PIC32_BASE_I2C5		0xbf820800
#define PIC32_BASE_UART		0xbf822000
#define PIC32_BASE_PORT		0xbf860000
#define PIC32_BASE_ETHER	0xbf882000
#define PIC32_BASE_PREFETCH	0xbf8e0000
#define PIC32_BASE_EBI		0xbf8e1000
#define PIC32_BASE_RNG		0xbf8e6000
#define PIC32_BASE_DEVCFG2	0xbfc4ff44

#define BIT_REG_MASK(bit, reg, mask)		\
	do {					\
		reg = bit/32;			\
		mask = 1 << (bit % 32);		\
	} while (0)

#define pic32_syskey_unlock()						\
	do {								\
		void __iomem *syskey = ioremap(PIC32_BASE_CONFIG +	\
					0x30, sizeof(u32));		\
		__raw_writel(0x00000000, syskey);			\
		__raw_writel(0xAA996655, syskey);			\
		__raw_writel(0x556699AA, syskey);			\
	} while (0)

/*
 * TODO: The following defines should go away with pinctrl and serial drivers.
 */

/* PPS */
#define REG_U2RXR	0x0070
#define REG_RPB14R	0x0178
#define REG_U1RXR	0x0068
#define REG_RPD3R	0x01CC
#define REG_RPB3R	0x0010

/* UART1-UART6 */
#define UARTx_BASE(x)	((x - 1) * 0x0200)
#define UxMODE(x)	UARTx_BASE(x)
#define UxSTA(x)	(UARTx_BASE(x) + 0x10)
#define UxTXREG(x)	(UARTx_BASE(x) + 0x20)
#define UxRXREG(x)	(UARTx_BASE(x) + 0x30)
#define UxBRG(x)	(UARTx_BASE(x) + 0x40)

/* PORTA-PORTK / PORT0-PORT10 */
#define ANSELx(x)	((x * 0x0100) + 0x00)
#define TRISx(x)	((x * 0x0100) + 0x10)
#define PORTx(x)	((x * 0x0100) + 0x20)

#endif
