#include <asm/asm-offsets.h>
#include <asm/thread_info.h>

#define PAGE_SIZE _PAGE_SIZE

#ifdef CONFIG_XIP_KERNEL
#define FASTPATH 0x200 /* From tlb-func.S */
#endif

/*
 * Put .bss..swapper_pg_dir as the first thing in .bss. This will
 * ensure that it has .bss alignment (64K).
 */
#define BSS_FIRST_SECTIONS *(.bss..swapper_pg_dir)

#include <asm-generic/vmlinux.lds.h>

#undef mips
#define mips mips
OUTPUT_ARCH(mips)
#ifdef PHYSADDR
ENTRY(phys_entry)
#define AT_LOCATION AT(PHYSADDR)
#else
ENTRY(kernel_entry)
#define AT_LOCATION
#endif
#ifndef CONFIG_XIP_KERNEL
PHDRS {
	text PT_LOAD AT_LOCATION FLAGS(7);      /* RWX */
	note PT_NOTE FLAGS(4);	/* R__ */
}
#endif
#ifdef PHYSADDR
phys_entry = kernel_entry - VMLINUX_LOAD_ADDRESS + PHYSADDR;
#endif

#ifdef CONFIG_32BIT
	#ifdef CONFIG_CPU_LITTLE_ENDIAN
		jiffies	 = jiffies_64;
	#else
		jiffies	 = jiffies_64 + 4;
	#endif
#else
	jiffies	 = jiffies_64;
#endif

SECTIONS
{
#ifdef CONFIG_BOOT_ELF64
	/* Read-only sections, merged into text segment: */
	/* . = 0xc000000000000000; */

	/* This is the value for an Origin kernel, taken from an IRIX kernel.  */
	/* . = 0xc00000000001c000; */

	/* Set the vaddr for the text segment to a value
	 *   >= 0xa800 0000 0001 9000 if no symmon is going to configured
	 *   >= 0xa800 0000 0030 0000 otherwise
	 */

	/* . = 0xa800000000300000; */
	. = 0xffffffff80300000;
#endif
	. = VMLINUX_LOAD_ADDRESS;

#ifdef CONFIG_XIP_KERNEL
	.head.text : {
		_text = .;
		HEAD_TEXT
	}

	.init : {
		. = ALIGN(16);
		VMLINUX_SYMBOL(_sinittext) = .;
		INIT_TEXT
		VMLINUX_SYMBOL(_einittext) = .;

		INIT_SETUP(16)
		INIT_CALLS
		CON_INITCALL
		SECURITY_INITCALL
		INIT_RAM_FS

		__mips_machines_start = .;
		*(.mips.machines.init)
		__mips_machines_end = .;
	}
#endif

	/* read-only */
	_text = .;	/* Text and read-only data */
	.text : {
		TEXT_TEXT
		SCHED_TEXT
		LOCK_TEXT
		KPROBES_TEXT
		IRQENTRY_TEXT
#ifdef CONFIG_XIP_KERNEL
		__start___dbe_table = .;
		*(__dbe_table)
		__stop___dbe_table = .;
		EXIT_TEXT
		EXIT_DATA
#endif
		*(.text.*)
		*(.fixup)
		*(.gnu.warning)
#ifndef CONFIG_XIP_KERNEL
	} :text = 0
	_etext = .;	/* End of text section */
#else
	}
#endif

#ifndef CONFIG_XIP_KERNEL
	EXCEPTION_TABLE(16)
#endif

#ifdef CONFIG_XIP_KERNEL
	. = ALIGN(16);
	__ex_table : {          /* Exception table      */
		__start___ex_table = .;
		*(__ex_table)
		__stop___ex_table = .;
	}
#endif

#ifndef CONFIG_XIP_KERNEL
	/* Exception table for data bus errors */
	__dbe_table : {
		__start___dbe_table = .;
		*(__dbe_table)
		__stop___dbe_table = .;
	}
#endif

#ifdef CONFIG_XIP_KERNEL
	.notes : {
		VMLINUX_SYMBOL(__start_notes) = .;
		*(.note.*)
		VMLINUX_SYMBOL(__stop_notes) = .;
	}
#else
	NOTES :text :note
#endif

	.dummy : { *(.dummy) } :text

#ifndef CONFIG_XIP_KERNEL
	_sdata = .;			/* Start of data section */
#endif
	RODATA

#ifdef CONFIG_XIP_KERNEL
    /* These mark the ABI of the kernel for debuggers.  */
	.mdebug.abi32 : {
		KEEP(*(.mdebug.abi32))
	}
	.mdebug.abi64 : {
		KEEP(*(.mdebug.abi64))
	}

	/* This is the MIPS specific mdebug section.  */
	.mdebug : {
		*(.mdebug)
	}

	_etext = .; /* End of text section */

	__data_loc = ALIGN(4);      /* location in binary */
	. = 0xffffffff80000000 + 0x5000;
#endif

#ifdef CONFIG_XIP_KERNEL
    /* writeable */
	.data : AT(__data_loc) {    /* Data */
		_data = .;
		_sdata = .;

		VMLINUX_SYMBOL(__per_cpu_load) = .;
        VMLINUX_SYMBOL(__per_cpu_start) = .;
        *(.data..percpu..first)
        . = ALIGN(PAGE_SIZE);
        *(.data..percpu..page_aligned)
        . = ALIGN(1 << CONFIG_MIPS_L1_CACHE_SHIFT);
        *(.data..percpu..readmostly)
        . = ALIGN(1 << CONFIG_MIPS_L1_CACHE_SHIFT);
        *(.data..percpu)
        *(.data..percpu..shared_aligned)
        VMLINUX_SYMBOL(__per_cpu_end) = .;

        /* will be freed after init */
        . = ALIGN(PAGE_SIZE);       /* Init code and data */
        __init_begin = .;
        INIT_DATA
        __init_end = .;
        /* freed after init ends here */

        . = ALIGN(4);
        __clear_page_start = .;
        . = . + 288;
        __clear_page_end = .;

        . = ALIGN(4);
        __copy_page_start = .;
        . = . + 1344;
        __copy_page_end = .;

        . = ALIGN(4);
        tlbmiss_handler_setup_pgd_start = .;
        . = . + 64;
        tlbmiss_handler_setup_pgd_end = .;

        . = ALIGN(4);
        handle_tlbm_start = .;
        . = . + FASTPATH;
        handle_tlbm_end = .;

        . = ALIGN(4);
        handle_tlbs_start = .;
        . = . + FASTPATH;
        handle_tlbs_end = .;

        . = ALIGN(4);
        handle_tlbl_start = .;
        . = . + FASTPATH;
        handle_tlbl_end = .;

		INIT_TASK_DATA(THREAD_SIZE)
		NOSAVE_DATA
		CACHELINE_ALIGNED_DATA(1 << CONFIG_MIPS_L1_CACHE_SHIFT)
		READ_MOSTLY_DATA(1 << CONFIG_MIPS_L1_CACHE_SHIFT)
		DATA_DATA
		CONSTRUCTORS

        _edata =  .;            /* End of data section */
    }
    _edata_loc = __data_loc + SIZEOF(.data);
    _gp = . + 0x8000;
    .lit8 : {
     	*(.lit8)
    }
    .lit4 : {
        *(.lit4)
    }
	/* We want the small data sections together, so single-instruction offsets
	   can access them all, and initialized data all before uninitialized, so
       we can shorten the on-disk segment size.  */
	.sdata : {
          *(.sdata)
    }
    VMLINUX_SYMBOL(__bss_start) = .;
    .sbss : {
        *(.sbss)
        *(.scommon)
    }
    .bss : {
	    BSS_FIRST_SECTIONS
        *(.bss..page_aligned)
        *(.dynbss)
        *(.bss)
        *(COMMON)
    }
    VMLINUX_SYMBOL(__bss_stop) = .;

    _end = . ;
#else
	/* writeable */
	.data : {	/* Data */
		. = . + DATAOFFSET;		/* for CONFIG_MAPPED_KERNEL */

		INIT_TASK_DATA(THREAD_SIZE)
		NOSAVE_DATA
		CACHELINE_ALIGNED_DATA(1 << CONFIG_MIPS_L1_CACHE_SHIFT)
		READ_MOSTLY_DATA(1 << CONFIG_MIPS_L1_CACHE_SHIFT)
		DATA_DATA
		CONSTRUCTORS
	}
	_gp = . + 0x8000;
	.lit8 : {
		*(.lit8)
	}
	.lit4 : {
		*(.lit4)
	}
	/* We want the small data sections together, so single-instruction offsets
	   can access them all, and initialized data all before uninitialized, so
	   we can shorten the on-disk segment size.  */
	.sdata : {
		*(.sdata)
	}
	_edata =  .;			/* End of data section */

	/* will be freed after init */
	. = ALIGN(PAGE_SIZE);		/* Init code and data */
	__init_begin = .;
	INIT_TEXT_SECTION(PAGE_SIZE)
	INIT_DATA_SECTION(16)

	. = ALIGN(4);
	.mips.machines.init : AT(ADDR(.mips.machines.init) - LOAD_OFFSET) {
		__mips_machines_start = .;
		*(.mips.machines.init)
		__mips_machines_end = .;
	}

	/* .exit.text is discarded at runtime, not link time, to deal with
	 * references from .rodata
	 */
	.exit.text : {
		EXIT_TEXT
	}
	.exit.data : {
		EXIT_DATA
	}

	PERCPU_SECTION(1 << CONFIG_MIPS_L1_CACHE_SHIFT)
	/*
	 * Align to 64K in attempt to eliminate holes before the
	 * .bss..swapper_pg_dir section at the start of .bss.  This
	 * also satisfies PAGE_SIZE alignment as the largest page size
	 * allowed is 64K.
	 */
	. = ALIGN(0x10000);
	__init_end = .;
	/* freed after init ends here */

	/*
	 * Force .bss to 64K alignment so that .bss..swapper_pg_dir
	 * gets that alignment.	 .sbss should be empty, so there will be
	 * no holes after __init_end. */
	BSS_SECTION(0, 0x10000, 0)

	_end = . ;

	/* These mark the ABI of the kernel for debuggers.  */
	.mdebug.abi32 : {
		KEEP(*(.mdebug.abi32))
	}
	.mdebug.abi64 : {
		KEEP(*(.mdebug.abi64))
	}

	/* This is the MIPS specific mdebug section.  */
	.mdebug : {
		*(.mdebug)
	}
#endif

	STABS_DEBUG
	DWARF_DEBUG

	/* These must appear regardless of  .  */
	.gptab.sdata : {
		*(.gptab.data)
		*(.gptab.sdata)
	}
	.gptab.sbss : {
		*(.gptab.bss)
		*(.gptab.sbss)
	}

	/* Sections to be discarded */
	DISCARDS
	/DISCARD/ : {
		/* ABI crap starts here */
		*(.MIPS.options)
		*(.options)
		*(.pdr)
		*(.reginfo)
#ifndef CONFIG_XIP_KERNEL
		*(.eh_frame)
#endif
	}
}
