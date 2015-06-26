/*
 * PIC32 Early pinctrl configurations.
 *
 * Copyright (C) 2014 Microchip Technology, Inc.
 *
 * Authors:
 *   Sorin-Andrei Pistirica <andrei.pistirica@microchip.com>
 *
 * Licensed under GPLv2 or later.
 */
#include <asm/io.h>
#include <asm/mach-pic32/early_pinctrl.h>

enum pic32_ports {
	PIC32_PORTA = 0,
	PIC32_PORTB = 1,
	PIC32_PORTC = 2,
	PIC32_PORTD = 3,
	PIC32_PORTE = 4,
	PIC32_PORTF = 5,
	PIC32_PORTG = 6,
	PIC32_PORTH = 7,
	PIC32_PORTJ = 8,
	PIC32_PORTK = 9
};

static inline struct pic32_pio __iomem *
	pic32_map_port_addr(enum pic32_ports port)
{
	return ioremap_nocache(PORTS_BASE + (port * PORT_SIZE),
		sizeof(struct pic32_pio));
}

void __init pic32_earlyco_port0_pinctrl(void)
{
	struct pic32_ppsinr __iomem *ppsinr =
		ioremap_nocache(PPSIN_BASE, sizeof(*ppsinr));
	struct pic32_ppsoutr __iomem *ppsoutr =
		ioremap_nocache(PPSOUT_BASE, sizeof(*ppsoutr));
	struct pic32_pio __iomem *piob = pic32_map_port_addr(PIC32_PORTB);
	struct pic32_pio __iomem *piod = pic32_map_port_addr(PIC32_PORTD);
	struct pic32_pio __iomem *piog = pic32_map_port_addr(PIC32_PORTG);

	BUG_ON(!ppsinr);
	BUG_ON(!ppsoutr);
	BUG_ON(!piob);
	BUG_ON(!piod);
	BUG_ON(!piod);

	/* pins linkage */
	writel(0x01, &ppsinr->u1rxr);     /* RX (RPG8 :J1-81)  */
	writel(0x03, &ppsinr->u1ctsr);    /* CTS(RPB15:J1-90)  */
	writel(0x01, &ppsoutr->rpg0r[7]); /* TX (RPG7 :J1-91)  */
	writel(0x01, &ppsoutr->rpd0r[1]); /* RTS(RPD1 :J1-118) */

	/* pins type: digital */
	writel((1 << 8), &piog->ansel.clr); /* RX (RPG8 :J1-81)  */
	writel((1 << 15), &piob->ansel.clr);/* CTS(RPB15:J1-90)  */
	writel((1 << 7), &piog->ansel.clr); /* TX (RPG7 :J1-91)  */
	writel((1 << 8), &piod->ansel.clr); /* RTS(RPD1 :J1-118) */

	iounmap(ppsinr);
	iounmap(ppsoutr);
	iounmap(piob);
	iounmap(piod);
	iounmap(piog);
}

void __init pic32_earlyco_port1_pinctrl(void)
{
	struct pic32_ppsinr __iomem *ppsinr =
		ioremap_nocache(PPSIN_BASE, sizeof(*ppsinr));
	struct pic32_ppsoutr __iomem *ppsoutr =
		ioremap_nocache(PPSOUT_BASE, sizeof(*ppsoutr));
	struct pic32_pio __iomem *piob = pic32_map_port_addr(PIC32_PORTB);
	struct pic32_pio __iomem *piod = pic32_map_port_addr(PIC32_PORTD);
	struct pic32_pio __iomem *piog = pic32_map_port_addr(PIC32_PORTD);

	BUG_ON(!ppsinr);
	BUG_ON(!ppsoutr);
	BUG_ON(!piob);
	BUG_ON(!piod);
	BUG_ON(!piog);

	/* pins linkage */
	writel(0x01, &ppsinr->u2rxr);     /* RX (RPG6)         */
	writel(0x06, &ppsinr->u2ctsr);    /* CTS(RPB10:J1-117) */
	writel(0x02, &ppsoutr->rpb0r[14]);/* TX (RPB14)        */
	writel(0x02, &ppsoutr->rpd0r[7]); /* RTS(RPD7:J1-122)  */

	/* pins type: digital */
	writel((1 << 6), &piog->ansel.clr); /* RX (RPG6)  */
	writel((1 << 10), &piob->ansel.clr);/* CTS(RPB10) */
	writel((1 << 14), &piob->ansel.clr);/* TX (RPB14) */
	writel((1 << 7), &piod->ansel.clr); /* RTS(RPD7)  */

	iounmap(ppsinr);
	iounmap(ppsoutr);
	iounmap(piob);
	iounmap(piod);
	iounmap(piog);
}
