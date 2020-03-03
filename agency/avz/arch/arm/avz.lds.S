/*
 * Copyright (C) 2014-2016 Daniel Rossier <daniel.rossier@heig-vd.ch>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <asm-generic/vmlinux.lds.h>
#include <asm/memory.h>
#include <asm/config.h>

OUTPUT_ARCH(arm)
ENTRY(_stext)

SECTIONS
{
	 . = PAGE_OFFSET + 0x8000;

	.init : {			/* Init code and data		*/
		_start = .; /* avz head */
			*(.text.start)
		_stext = .;
			_sinittext = .;
			*(.init.text)
			_einittext = .;
		__proc_info_begin = .;
			*(.proc.info.init)
		__proc_info_end = .;
		__arch_info_begin = .;
			*(.arch.info.init)
		__arch_info_end = .;
		__tagtable_begin = .;
			*(.taglist.init)
		__tagtable_end = .;
		. = ALIGN(16);
		__setup_start = .;
			*(.init.setup)
		__setup_end = .;

		__early_begin = .;
			*(.early_param.init)
		__early_end = .;

		__initcall_start = .;
		*(.initcall1.init)
		__initcall_end = .;

	}

	/DISCARD/ : {			/* Exit code and data		*/
		*(.exit.text)
		*(.exit.data)
		*(.exitcall.exit)

		*(.fixup)
		*(__ex_table)

	}

	.text : {			/* Real text segment		*/
		_text = .;		/* Text and read-only data	*/
			*(.text)
			SCHED_TEXT
			LOCK_TEXT
			*(.fixup)
			*(.gnu.warning)
			*(.rodata)
			*(.rodata.*)
			*(.glue_7)
			*(.glue_7t)
		*(.got)			/* Global offset table		*/
	}

	RODATA

	_etext = .;			/* End of text and rodata section */

	. = ALIGN(8192);
	__data_loc = .;

	.data : AT(__data_loc) {
		__data_start = .;	/* address in memory */


		. = ALIGN(4096);
		__nosave_begin = .;
		*(.data.nosave)
		. = ALIGN(4096);
		__nosave_end = .;

		/*
		 * then the cacheline aligned data
		 */
		. = ALIGN(128);
		*(.data.cacheline_aligned)

		/*
		 * The exception fixup table (might need resorting at runtime)
		 */
		. = ALIGN(128);
		__start___ex_table = .;

		*(__ex_table)
		__stop___ex_table = .;

		/*
		 * and the usual data section
		 */
		*(.data)
		CONSTRUCTORS

		_edata = .;
	}
. = ALIGN(4096);
	.bss : {
		*(.init_task)
		__bss_start = .;			/* BSS */
		*(.bss)
		*(COMMON)
		. = ALIGN(4096);

	      __per_cpu_start = .;
	      *(.bss.percpu)
	    . = ALIGN(128);
	      *(.bss.percpu.read_mostly)
	    . = ALIGN(128);
	      __per_cpu_data_end = .;
	    . = ALIGN(128);
	      __per_cpu_end = .;

		_end = .;
	}
					/* Stabs debugging sections.	*/
	.stab 0 : { *(.stab) }
	.stabstr 0 : { *(.stabstr) }
	.stab.excl 0 : { *(.stab.excl) }
	.stab.exclstr 0 : { *(.stab.exclstr) }
	.stab.index 0 : { *(.stab.index) }
	.stab.indexstr 0 : { *(.stab.indexstr) }
	.comment 0 : { *(.comment) }
}

/*
 * These must never be empty
 * If you have to comment these two assert statements out, your
 * binutils is too old (for other reasons as well)
 */
ASSERT((__proc_info_end - __proc_info_begin), "missing CPU support")
ASSERT((__arch_info_end - __arch_info_begin), "no machine record defined")
