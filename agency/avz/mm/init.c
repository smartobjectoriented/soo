/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#define DEBUG

#include <common.h>
#include <memory.h>
#include <heap.h>
#include <sizes.h>

#include <device/fdt.h>

#include <asm/mmu.h>
#include <asm/cacheflush.h>

#include <mach/uart.h>

#include <generated/autoconf.h>

extern unsigned long __vectors_start, __vectors_end;

/*
 * Main memory init function
 */

void memory_init(void) {
	uint32_t *new_sys_pgtable;
	uint32_t vectors_paddr;

	/* Initialize the list of I/O virt/phys maps */
	INIT_LIST_HEAD(&io_maplist);

	/* Initialize the kernel heap */
	heap_init();

	init_io_mapping();


#if 0
	/* Re-setup a system page table with a better granularity */
	new_sys_pgtable = new_l1pgtable();

	create_mapping(new_sys_pgtable, CONFIG_KERNEL_VIRT_ADDR, CONFIG_RAM_BASE, get_kernel_size(), false);

	/* Mapping uart I/O for debugging purposes */
	create_mapping(new_sys_pgtable, UART_BASE, UART_BASE, PAGE_SIZE, true);

	/*
	 * Switch to the temporary page table in order to re-configure the original system page table
	 * Warning !! After the switch, we do not have any mapped I/O until the driver core gets initialized.
	 */

	mmu_switch(new_sys_pgtable);

	/* Re-configuring the original system page table */
	memcpy((void *) __sys_01pgtable, (unsigned char *) new_sys_pgtable, TTB_L1_SIZE);

	/* Finally, switch back to the original location of the system page table */
	mmu_switch(__sys_l1pgtable);

	/* Finally, prepare the vector page at its correct location */
	vectors_paddr = get_free_page();

	create_mapping(NULL, VECTOR_VADDR, vectors_paddr, PAGE_SIZE, true);

	memcpy((void *) VECTOR_VADDR, (void *) &__vectors_start, (void *) &__vectors_end - (void *) &__vectors_start);

	set_pgtable(__sys_l1pgtable);
#endif
}

