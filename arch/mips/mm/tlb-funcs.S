/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Micro-assembler generated tlb handler functions.
 *
 * Copyright (C) 2013  Broadcom Corporation.
 *
 * Based on mm/page-funcs.c
 * Copyright (C) 2012  MIPS Technologies, Inc.
 * Copyright (C) 2012  Ralf Baechle <ralf@linux-mips.org>
 */
#include <asm/asm.h>
#include <asm/regdef.h>

#define FASTPATH_SIZE	128

#ifdef CONFIG_XIP_KERNEL

FEXPORT(tlbmiss_handler_setup_pgd)
1:	la	k0, tlbmiss_handler_setup_pgd_start
	j	k0
	nop

FEXPORT(handle_tlbm)
1:	la	k0, handle_tlbm_start
	j	k0
	nop

FEXPORT(handle_tlbs)
1:	la	k0, handle_tlbs_start
	j	k0
	nop

FEXPORT(handle_tlbl)
1:	la	k0, handle_tlbl_start
	j	k0
	nop

#else

EXPORT(tlbmiss_handler_setup_pgd_start)
LEAF(tlbmiss_handler_setup_pgd)
1:	j       1b              /* Dummy, will be replaced. */
	.space  64
END(tlbmiss_handler_setup_pgd)
EXPORT(tlbmiss_handler_setup_pgd_end)

LEAF(handle_tlbm)
	.space		FASTPATH_SIZE * 4
END(handle_tlbm)
EXPORT(handle_tlbm_end)

LEAF(handle_tlbs)
	.space		FASTPATH_SIZE * 4
END(handle_tlbs)
EXPORT(handle_tlbs_end)

LEAF(handle_tlbl)
	.space		FASTPATH_SIZE * 4
END(handle_tlbl)
EXPORT(handle_tlbl_end)

#endif
