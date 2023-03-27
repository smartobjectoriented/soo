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
#include <soo/avz.h>

#include <soo/uapi/console.h>
#include <soo/uapi/logbool.h>

volatile unsigned long *HYPERVISOR_hypercall_addr;

/* Updated in kernel/head.S */
volatile avz_shared_t *avz_shared;

extern unsigned long __pv_phys_pfn_offset;
extern u64 __pv_offset;

void __init avz_setup(void)
{
	__printch = AVZ_shared->printch;

	/* Immediately prepare for hypercall processing */
	HYPERVISOR_hypercall_addr = (unsigned long *) ((unsigned long) AVZ_shared->hypercall_vaddr);

	lprintk("  - SOO Agency Virtualizer (avz) Start info :\n");
	lprintk("  - Hypercall addr: %lx\n", (unsigned long) HYPERVISOR_hypercall_addr);
	lprintk("  - Total Pages allocated to this domain : %ld\n", AVZ_shared->nr_pages);
	lprintk("  - Domain physical address : 0x%lx\n", AVZ_shared->dom_phys_offset);
	lprintk("  - FDT device tree paddr, if any: %lx\n", AVZ_shared->fdt_paddr);

	__ht_set = (ht_set_t) AVZ_shared->logbool_ht_set_addr;

#ifdef CONFIG_ARM

	__pv_phys_pfn_offset = AVZ_shared->dom_phys_offset >> PAGE_SHIFT;
	__pv_offset = (u64) (AVZ_shared->dom_phys_offset - PAGE_OFFSET);

	fixup_pv_table(&__pv_table_begin, (&__pv_table_end - &__pv_table_begin) << 2);

#endif

	AVZ_shared->domcall_vaddr = (unsigned long) domcall;
	AVZ_shared->subdomain_shared->domcall_vaddr = AVZ_shared->domcall_vaddr;

	AVZ_shared->vectors_vaddr = (addr_t) avz_linux_callback;

	lprintk("  - All right! AVZ setup successfull.\n");
}
