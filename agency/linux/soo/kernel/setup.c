/*
 * Copyright (C) 2014-2021 Daniel Rossier <daniel.rossier@soo.tech>
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

#include <linux/mm_types.h>

#include <asm/page.h>

#ifdef CONFIG_ARM
#include <asm/cp15.h>
#endif

#include <asm/cacheflush.h>

#include <soo/hypervisor.h>
#include <soo/uapi/avz.h>
#include <soo/uapi/console.h>
#include <soo/uapi/logbool.h>

start_info_t *avz_start_info;
unsigned long avz_guest_phys_offset;

volatile unsigned long *HYPERVISOR_hypercall_addr;

volatile shared_info_t *HYPERVISOR_shared_info;

extern unsigned long __pv_phys_pfn_offset;
extern u64 __pv_offset;

void __init avz_setup(void)
{
	__printch = avz_start_info->printch;

	/* Immediately prepare for hypercall processing */
	HYPERVISOR_hypercall_addr = (unsigned long *) avz_start_info->hypercall_addr;

	lprintk("SOO Agency Virtualizer (avz) Start info :\n");
	lprintk("Hypercall addr: %lx\n", (unsigned long) HYPERVISOR_hypercall_addr);
	lprintk("Total Pages allocated to this domain : %ld\n", avz_start_info->nr_pages);
	lprintk("MACHINE address of shared info struct : 0x%lx\n", avz_start_info->shared_info);
	lprintk("Domain physical address : 0x%lx\n", avz_start_info->dom_phys_offset);
	lprintk("FDT device tree paddr, if any: %lx\n", avz_start_info->fdt_paddr);
	lprintk("Start_info record address: %lx\n", (unsigned long) avz_start_info);

	__ht_set = (ht_set_t) avz_start_info->logbool_ht_set_addr;

#ifdef CONFIG_ARM

	__pv_phys_pfn_offset = avz_start_info->dom_phys_offset >> PAGE_SHIFT;
	__pv_offset = (u64) (avz_start_info->dom_phys_offset - PAGE_OFFSET);

	fixup_pv_table(&__pv_table_begin, (&__pv_table_end - &__pv_table_begin) << 2);

#endif

	/* Get the shared info page, and keep it as a separate reference */
	HYPERVISOR_shared_info = avz_start_info->shared_info;

 	lprintk("done.\n");

 	lprintk("Set HYPERVISOR_set_callbacks at %lx\n", (unsigned long) avz_linux_callback);

	hypercall_trampoline(__HYPERVISOR_set_callbacks, (unsigned long) avz_linux_callback, (unsigned long) domcall, 0, 0);

	lprintk("No problem\n");
}
