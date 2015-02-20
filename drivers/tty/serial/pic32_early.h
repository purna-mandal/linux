/*
 * PIC32 Integrated earlyprintk serial driver.
 *
 * Copyright (C) 2014 Microchip Technology, Inc.
 *
 * Authors:
 *   Sorin-Andrei Pistirica <andrei.pistirica@microchip.com>
 *
 * Licensed under GPLv2 or later.
 */
#ifndef __DT_PIC32_EARLY_CONSOLE_H__
#define __DT_PIC32_EARLY_CONSOLE_H__

#define PIC32_EARLY_CON_PORT0_MEMBASE 0xbf822000
#define PIC32_EARLY_CON_PORT1_MEMBASE 0xbf822200
#define PIC32_EARLY_MAX_PORTS 2

int __init pic32_earlyprintk_setup(void);

#endif /* __DT_PIC32_EARLY_CONSOLE_H__ */
