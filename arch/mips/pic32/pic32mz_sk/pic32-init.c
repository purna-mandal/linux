/*
 * Joshua Henderson, joshua.henderson@microchip.com
 * Copyright (C) 2014 Microchip Technology Inc.  All rights reserved.
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
#include <linux/string.h>
#include <linux/kernel.h>

#include <asm/bootinfo.h>
#include <asm/io.h>
#include <asm/cacheflush.h>
#include <asm/traps.h>

#include <asm/fw/fw.h>
#include <asm/prom.h>
#include <asm/mips-boards/generic.h>
#include <asm/mach-pic32/common.h>

int prom_argc;
int *_prom_argv, *_prom_envp;

#define prom_envp(index) ((char *)(long)_prom_envp[(index)])

char *prom_getenv(char *envname)
{
	/*
	 * Return a pointer to the given environment variable.
	 * In 64-bit mode: we're using 64-bit pointers, but all pointers
	 * in the PROM structures are only 32-bit, so we need some
	 * workarounds, if we are running in 64-bit mode.
	 */
	int i, index = 0;

	i = strlen(envname);

	while (prom_envp(index)) {
		if (strncmp(envname, prom_envp(index), i) == 0)
			return prom_envp(index+1);
		index += 2;
	}

	return NULL;
}

static int __init fw_early_console_get_port_from_archcmdline(void)
{
	/*
	 * arch_mem_init() has not been called yet, so we don't have a real
	 * command line setup if using CONFIG_CMDLINE_BOOL.
	 */
#ifdef CONFIG_CMDLINE_OVERRIDE
	char *arch_cmdline = CONFIG_CMDLINE;
#else
	char *arch_cmdline = fw_getcmdline();
#endif
	char *s;
	char port = -1;

	if (*arch_cmdline == '\0')
		goto _out;

	s = strstr(arch_cmdline, "earlyprintk=");
	if (s != NULL) {
		s = strstr(s, "ttyS");
		if (s != NULL)
			s += 4;
		else
			goto _out;

		port = (*s) - '0';
	}

_out:
	return port;
}

void __init prom_init(void)
{
	char port;
	prom_argc = fw_arg0;
	_prom_argv = (int *) fw_arg1;
	_prom_envp = (int *) fw_arg2;

	fw_init_cmdline();

	port = fw_early_console_get_port_from_archcmdline();
#ifdef CONFIG_EARLY_PRINTK
	if (port != -1)
		fw_init_early_console(port);
#endif

	fw_meminit();
}
