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

#ifdef CONFIG_ARM
pgd_t *swapper_pg_dir;
#endif

volatile shared_info_t *HYPERVISOR_shared_info;

extern unsigned long __pv_phys_pfn_offset;
extern u64 __pv_offset;

void __init avz_setup(void)
{
	int ret;

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

	init_mm.pgd = swapper_pg_dir;

	__pv_phys_pfn_offset = avz_start_info->min_mfn;
	__pv_offset = (u64) (avz_guest_phys_offset - PAGE_OFFSET);

	fixup_pv_table(&__pv_table_begin, (&__pv_table_end - &__pv_table_begin) << 2);

	/*
	 * We changing not only the virtual to physical mapping, but also
	 * the physical addresses used to access memory.  We need to flush
	 * all levels of cache in the system with caching disabled to
	 * ensure that all data is written back, and nothing is prefetched
	 * into the caches.  We also need to prevent the TLB walkers
	 * allocating into the caches too.  Note that this is ARMv7 LPAE
	 * specific.
	 */
	cr = get_cr();
	set_cr(cr & ~(CR_I | CR_C));
	asm("mrc p15, 0, %0, c2, c0, 2" : "=r" (ttbcr));
	asm volatile("mcr p15, 0, %0, c2, c0, 2"
		: : "r" (ttbcr & ~(3 << 8 | 3 << 10)));

	flush_cache_all();

	/* Re-enable the caches and cacheable TLB walks */
	asm volatile("mcr p15, 0, %0, c2, c0, 2" : : "r" (ttbcr));
	set_cr(cr);
#endif

	/* Get the shared info page, and keep it as a separate reference */
	HYPERVISOR_shared_info = avz_start_info->shared_info;

 	printk("done.\n");

 	printk("Set HYPERVISOR_set_callbacks at %lx\n", (unsigned long) avz_linux_callback);

	ret = hypercall_trampoline(__HYPERVISOR_set_callbacks, (unsigned long) avz_linux_callback, (unsigned long) domcall, 0, 0);
	BUG_ON(ret < 0);

	lprintk("No problem\n");
}
