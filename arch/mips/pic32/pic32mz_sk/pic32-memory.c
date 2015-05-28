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
#include <linux/mm.h>
#include <linux/bootmem.h>
#include <linux/pfn.h>
#include <linux/string.h>
#include <linux/export.h>
#include <linux/sizes.h>

#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/bootinfo.h>
#include <asm/page.h>
#include <asm/sections.h>

#include <asm/fw/fw.h>
#include <asm/mach-pic32/pic32.h>
#include <asm/mach-pic32/common.h>

#include <spaces.h>

#include <linux/dma-mapping.h>
#include <asm/mipsregs.h>
#include <asm/barrier.h>
#include <asm/cacheflush.h>
#include <asm/r4kcache.h>
#include <asm/asm-offsets.h>
#include <asm/inst.h>
#include <asm/fpu.h>
#include <asm/hazards.h>
#include <asm/cpu-features.h>

static fw_memblock_t mdesc[FW_MAX_MEMBLOCKS];
static char cmdline[COMMAND_LINE_SIZE];

/* determined physical memory size, not overridden by command line args  */
unsigned long physical_memsize = 0L;

#if defined(CONFIG_PIC32MZ_PLANC) || defined(CONFIG_PIC32MZ_PLAND)
#define EBI_SRAM_SIZE SZ_16M
#else
#define EBI_SRAM_SIZE SZ_8M
#endif

fw_memblock_t * __init fw_getmdesc(int eva)
{
	static int init_done;
	char *memsize_str;
	unsigned int memsize;
	char *ptr;

	if (init_done)
		goto out;

	/* otherwise look in the environment */
	memsize_str = fw_getenv("memsize");
	if (!memsize_str) {
		pr_warn("memsize not set in boot prom, set to default (512K)\n");
		physical_memsize = 0x00080000;
	} else {
		physical_memsize = kstrtol(memsize_str, 0, 0);
	}

	/* Check the command line for a memsize directive
	 * that might override the physical/default amount.
	 */
	strcpy(cmdline, arcs_cmdline);
	ptr = strstr(cmdline, "memsize=");
	if (ptr && (ptr != cmdline) && (*(ptr - 1) != ' '))
		ptr = strstr(ptr, " memsize=");
	if (ptr)
		memsize = memparse(ptr + 8, &ptr);
	else
		memsize = physical_memsize;


	memset(mdesc, 0, sizeof(mdesc));

	/* on-chip SRAM (code segment) */
	mdesc[0].type = BOOT_MEM_RESERVED;
	mdesc[0].base = 0x00000000;
	mdesc[0].size = PFN_ALIGN(__bss_stop - KSEG0);
	mdesc[0].valid = 1;

	/* on-chip SRAM */
	mdesc[1].type = BOOT_MEM_RAM;
	mdesc[1].base = PFN_ALIGN(__bss_stop - KSEG0);
	mdesc[1].size = memsize - mdesc[1].base;
	mdesc[1].valid = 1;

	/* EBI SRAM */
	mdesc[2].type = BOOT_MEM_RAM;
	mdesc[2].base = UPPERMEM_START;
	mdesc[2].size = EBI_SRAM_SIZE;
	mdesc[2].valid = 1;

	init_done = 1;
out:
	return &mdesc[0];
}

struct tlb_entry {
	unsigned long entrylo0;
	unsigned long entrylo1;
	unsigned long entryhi;
	unsigned long pagemask;
};

/* (PFN << 6) | GLOBAL | VALID | DIRTY | cacheability */
#define ENTRYLO_CAC(pa) (((pa) >> 6)|0x07|(CONF_CM_CACHABLE_NONCOHERENT << 3))
#define ENTRYLO_UNC(pa) (((pa) >> 6)|0x07|(CONF_CM_UNCACHED << 3))

#ifdef CONFIG_PIC32MZ_UPPER_MEMORY
static struct tlb_entry wired_mappings[] = {
	{
		.entrylo0	= ENTRYLO_CAC(UPPERMEM_START),
		.entrylo1	= ENTRYLO_CAC(UPPERMEM_START + SZ_4M),
		.entryhi	= CAC_BASE_UPPER,
		.pagemask	= PM_4M,
	},
	{
		.entrylo0	= ENTRYLO_UNC(UPPERMEM_START),
		.entrylo1	= ENTRYLO_UNC(UPPERMEM_START + SZ_4M),
		.entryhi	= UNCAC_BASE_UPPER,
		.pagemask	= PM_4M,
	},
#if defined(CONFIG_PIC32MZ_PLANC) || defined(CONFIG_PIC32MZ_PLAND)
#define SZ_12M	0x00C00000
	{
		.entrylo0	= ENTRYLO_CAC(UPPERMEM_START + SZ_8M),
		.entrylo1	= ENTRYLO_CAC(UPPERMEM_START + SZ_12M),
		.entryhi	= CAC_BASE_UPPER + SZ_8M,
		.pagemask	= PM_4M,
	},
	{
		.entrylo0	= ENTRYLO_UNC(UPPERMEM_START + SZ_8M),
		.entrylo1	= ENTRYLO_UNC(UPPERMEM_START + SZ_12M),
		.entryhi	= UNCAC_BASE_UPPER + SZ_8M,
		.pagemask	= PM_4M,
	},
#undef SZ_12M
#endif
};
#endif

/*
 * This function is used instead of add_wired_entry(), because it does not
 * have any external dependencies and is not marked __init
 */
static inline void __cpuinit pic32mz_add_wired_entry(unsigned long entrylo0,
	unsigned long entrylo1, unsigned long entryhi, unsigned long pagemask)
{
	int i = read_c0_wired();

	write_c0_wired(i + 1);
	write_c0_index(i);
	tlbw_use_hazard();
	write_c0_pagemask(pagemask);
	write_c0_entryhi(entryhi);
	write_c0_entrylo0(entrylo0);
	write_c0_entrylo1(entrylo1);
	mtc0_tlbw_hazard();
	tlb_write_indexed();
	tlbw_use_hazard();
}

#define UNIQUE_ENTRYHI(idx) (CKSEG0 + ((idx) << (PAGE_SHIFT + 1)))

void pic32mz_upper_tlb_setup(void)
{
#ifdef CONFIG_PIC32MZ_UPPER_MEMORY
	int i, tlbsz;

	/* Flush TLB.  local_flush_tlb_all() is not available yet. */
	write_c0_entrylo0(0);
	write_c0_entrylo1(0);
	write_c0_pagemask(PM_DEFAULT_MASK);
	write_c0_wired(0);

	tlbsz = (read_c0_config1() >> 25) & 0x3f;
	for (i = 0; i <= tlbsz; i++) {
		write_c0_entryhi(UNIQUE_ENTRYHI(i));
		write_c0_index(i);
		mtc0_tlbw_hazard();
		tlb_write_indexed();
		tlbw_use_hazard();
	}

	write_c0_wired(0);
	mtc0_tlbw_hazard();

	for (i = 0; i < ARRAY_SIZE(wired_mappings); i++) {
		struct tlb_entry *e = &wired_mappings[i];
		pic32mz_add_wired_entry(e->entrylo0, e->entrylo1, e->entryhi,
			e->pagemask);
	}

	write_c0_pagemask(PM_DEFAULT_MASK);
#endif
}

extern u32 __init setup_prefetch(void);
extern void __init setup_ebi_sram(void);
extern void run_ebi_sram_test(u32 sram_size);

void __init fw_meminit(void)
{
	int i;
	fw_memblock_t *p;

	setup_prefetch();

	setup_ebi_sram();
	pic32mz_upper_tlb_setup();

#ifdef EBI_SRAM_TEST
	run_ebi_sram_test(EBI_SRAM_SIZE);
#endif

	p = fw_getmdesc(0);
	for (i = 0; i < FW_MAX_MEMBLOCKS; i++, p++) {
		unsigned long type, base, size;

		if (!p->valid)
			continue;

		type = p->type;
		base = p->base;
		size = p->size;

		add_memory_region(base, size, type);
	}
}

void __init prom_free_prom_memory(void)
{
	unsigned long addr;
	int i;

	for (i = 0; i < boot_mem_map.nr_map; i++) {
		if (boot_mem_map.map[i].type != BOOT_MEM_ROM_DATA)
			continue;

		addr = boot_mem_map.map[i].addr;
		free_init_pages("prom memory",
				addr, addr + boot_mem_map.map[i].size);
	}

}

extern void tlb_init(void);
extern void build_tlb_refill_handler(void);

void __cpuinit pic32mz_tlb_init(void)
{
#ifdef CONFIG_PIC32MZ_UPPER_MEMORY
	int i;

	BUG_ON(smp_processor_id() != 0);

	tlb_init();

	for (i = 0; i < ARRAY_SIZE(wired_mappings); i++) {
		struct tlb_entry *e = &wired_mappings[i];
		pic32mz_add_wired_entry(e->entrylo0, e->entrylo1,
					e->entryhi, e->pagemask);
	}

	write_c0_pagemask(PM_DEFAULT_MASK);
#else
	tlb_init();
#endif
}

/*
 * Returns index if the supplied range falls entirely within a bmem region
 */
int bmem_find_region(unsigned long addr, unsigned long size)
{
	int idx = 0;
	fw_memblock_t *p = &mdesc[0];

	while (p->size) {
		if (!p->valid)
			continue;
		if ((addr >= p->base) &&
		    ((addr + size) <=
			(p->base + p->size))) {
			return idx;
		}
		idx++;
		p++;
	}
	return -ENOENT;
}
EXPORT_SYMBOL(bmem_find_region);

/*
 * Special handling for __get_user_pages() on BMEM reserved memory:
 *
 * 1) Override the VM_IO | VM_PFNMAP sanity checks
 * 2) No cache flushes (this is explicitly under application control)
 * 3) vm_normal_page() does not work on these regions
 * 4) Don't need to worry about any kinds of faults; pages are always present
 */
int bmem_get_page(struct mm_struct *mm, struct vm_area_struct *vma,
	unsigned long start, struct page **page)
{
	unsigned long pg = start & PAGE_MASK, pfn;
	int ret = -EFAULT;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	pgd = pgd_offset(mm, pg);
	BUG_ON(pgd_none(*pgd));
	pud = pud_offset(pgd, pg);
	BUG_ON(pud_none(*pud));
	pmd = pmd_offset(pud, pg);
	if (pmd_none(*pmd))
		return ret;

	pte = pte_offset_map(pmd, pg);
	if (pte_none(*pte))
		goto out;

	pfn = pte_pfn(*pte);
	if (likely(bmem_find_region(pfn << PAGE_SHIFT, PAGE_SIZE) < 0))
		goto out;

	if (page) {
		*page = pfn_to_page(pfn);
		get_page(*page);
	}
	ret = 0;

out:
	pte_unmap(pte);
	return ret;
}

static int bmem_defaults_set __initdata;

static void __init pic32mz_set_default_bmem(void)
{
	memset(mdesc, 0, sizeof(mdesc));

	mdesc[0].type = BOOT_MEM_RESERVED;
	mdesc[0].base = 0x00000000;
	mdesc[0].size = PFN_ALIGN(__bss_stop - KSEG0);
	mdesc[0].valid = 1;
}

/*
 * Invokes free_bootmem(), but truncates ranges where necessary to
 * avoid releasing the bmem region(s) back to the VM
 */
void __init pic32mz_free_bootmem(unsigned long addr, unsigned long size)
{
	int i;
	if (!bmem_defaults_set) {
		pic32mz_set_default_bmem();
		bmem_defaults_set = 1;
	}

	while (size) {
		unsigned long chunksize = size;
		fw_memblock_t *r = NULL;
		fw_memblock_t *p = &mdesc[0];

		/*
		 * Find the first bmem region (if any) that fits entirely
		 * within the current bootmem address range.
		 */
		for (i = 0; i < FW_MAX_MEMBLOCKS; i++, p++) {
			if (!p->valid)
				continue;

			if ((p->base >= addr) &&
			    ((p->base + p->size) <= (addr + size))) {
				if (!r || (r->base > p->base))
					r = p;
			}
			p++;
		}

		/*
		 * Skip over every bmem region; call free_bootmem() for
		 * every Linux region.  A Linux region is created for
		 * each chunk of the memory map that is not reserved
		 * for bmem.
		 */
		if (r && r == &mdesc[0]) {
			if (addr == r->base) {
				pr_info("%s: adding %u MB "
				       "RESERVED region at %lu MB "
				       "(0x%08x@0x%08lx)\n",
				       "bmem",
				       r->size >> 20, r->base >> 20,
				       r->size, r->base);
				chunksize = r->size;
				r->valid = 1;
				goto skip;
			} else {
				BUG_ON(addr > r->base);
				chunksize = r->base - addr;
			}
		}
		BUG_ON(chunksize > size);

		pr_info("bmem: adding %lu MB LINUX region at %lu MB "
		       "(0x%08lx@0x%08lx)\n", chunksize >> 20, addr >> 20,
		       chunksize, addr);
		free_bootmem(addr, chunksize);

skip:
		addr += chunksize;
		size -= chunksize;
	}
}
