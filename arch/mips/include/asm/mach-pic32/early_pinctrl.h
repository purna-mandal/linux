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
#ifndef __DT_PIC32_EARLY_PINCTRL_H__
#define __DT_PIC32_EARLY_PINCTRL_H__

#define TO_VA(x) ((void __iomem *)(x))

#define PORTS_BASE  0xBF860000
#define PORT_SIZE   0x00000100

#define PPSIN_BASE  0xBF801404
#define PPSOUT_BASE 0xBF801538

struct pic32r {
	u32 val;
	u32 clr;
	u32 set;
	u32 inv;
} __packed;

struct pic32_pio {
	struct pic32r ansel;
	struct pic32r tris;
	struct pic32r port;
	struct pic32r lat;
	struct pic32r odc;
	struct pic32r cnpu;
	struct pic32r cnpd;
	struct pic32r cncon;
	struct pic32r cnen;
	struct pic32r cnstat;
} __packed;

struct pic32_ppsinr {
	u32 int1r;
	u32 int2r;
	u32 int3r;
	u32 int4r;
	u32 rsv0;
	u32 t2ckr;
	u32 t3ckr;
	u32 t4ckr;
	u32 t5ckr;
	u32 t6ckr;
	u32 t7ckr;
	u32 t8ckr;
	u32 t9ckr;
	u32 ic1r;
	u32 ic2r;
	u32 ic3r;
	u32 ic4r;
	u32 ic5r;
	u32 ic6r;
	u32 ic7r;
	u32 ic8r;
	u32 ic9r;
	u32 rsv1;
	u32 ocfar;
	u32 rsv2;
	u32 u1rxr;
	u32 u1ctsr;
	u32 u2rxr;
	u32 u2ctsr;
	u32 u3rxr;
	u32 u3ctsr;
	u32 u4rxr;
	u32 u4ctsr;
	u32 u5rxr;
	u32 u5ctsr;
	u32 u6rxr;
	u32 u6ctsr;
	u32 rsv3;
	u32 sdi1r;
	u32 ss1r;
	u32 rsv4;
	u32 sdi2r;
	u32 ss2r;
	u32 rsv5;
	u32 sdi3r;
	u32 ss3r;
	u32 rsv6;
	u32 sdi4r;
	u32 ss4r;
	u32 rsv7;
	/* unavailable on 64pin-chip */
	u32 sdi5r;
	/* unavailable on 64pin-chip */
	u32 ss5r;
	u32 rsv8;
	/* unavailable on 64pin-chip */
	u32 sdi6r;
	/* unavailable on 64pin-chip */
	u32 ss6r;
	/* unavailable on chips without CAN */
	u32 c1rxr;
	/* unavailable on chips without CAN */
	u32 c2rxr;
	u32 refclki1r;
	u32 rsv9;
	u32 refclki3r;
	u32 refclki4r;
} __packed;

struct pic32_ppsoutr {
	/* unavailable on 64pin-chip */
	u32 rpa14r[2];
	/* reserved: 4, 11 to 13
	 * unavailable on 64pin-chip: 1 to 4 */
	u32 rpb0r[16];
	/* reserved: 0, 5 to 12, 15 */
	u32 rpc0r[16];
	/* reserved: 8, 13
	 * unavailable on 64pin-and-100pin-chip:6, 7
	 * unavailable on 64pin-chip: 12, 14, 15 */
	u32 rpd0r[16];
	/* reserved: 0 to 2, 4, 6, 7, 10 to 15
	 * unavailable on 64pin-chip: 8, 9 */
	u32 rpe0r[16];
	/* reserved: 6, 7, 9 to 11, 14, 15
	 * unavailable on 64pin-chip: 2, 8, 12, 13 */
	u32 rpf0r[16];
	/* reserved: 2 to 5
	 * unavailable on 64pin-chip: 0, 1 */
	u32 rpg0r[10];
} __packed;

void __init pic32_earlyco_port0_pinctrl(void);
void __init pic32_earlyco_port1_pinctrl(void);

#endif /* __DT_PIC32_EARLY_PINCTRL_H__ */
