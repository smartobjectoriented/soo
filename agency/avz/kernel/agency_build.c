/*
 * Copyright (C) 2016,2017 Daniel Rossier <daniel.rossier@soo.tech>
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

#include <config.h>
#include <percpu.h>
#include <sched.h>
#include <ctype.h>
#include <console.h>
#include <domain.h>
#include <errno.h>
#include <memory.h>
#include <libelf.h>
#include <memslot.h>
#include <heap.h>

#include <asm/processor.h>
#include <asm/mmu.h>
#include <asm/cacheflush.h>
#include <asm/setup.h>

#include <soo/uapi/logbool.h>

start_info_t *agency_start_info;

extern char hypercall_start[];

int construct_agency(struct domain *d) {
	unsigned long vstartinfo_start;
	unsigned long v_start;
	unsigned long alloc_spfn;
	struct start_info *si = NULL;
	unsigned long nr_pages;

	unsigned long domain_stack;
	extern addr_t *hypervisor_stack;
	static addr_t *__hyp_stack = (unsigned long *) &hypervisor_stack;
	static addr_t *__pseudo_usr_mode = (unsigned long *) &pseudo_usr_mode;

	printk("***************************** Loading SOO Agency Domain *****************************\n");

	/* The agency is always in slot 1 */

	/* Now the slot is busy. */
	memslot[MEMSLOT_AGENCY].busy = true;

	if (memslot[MEMSLOT_AGENCY].size == 0)
		panic("No agency image supplied\n");

	/* The following page will contain start_info information */
	vstartinfo_start = (unsigned long) memalign(PAGE_SIZE, PAGE_SIZE);
	BUG_ON(!vstartinfo_start);

	d->max_pages = ~0U;
	d->tot_pages = 0;

	nr_pages = memslot[MEMSLOT_AGENCY].size >> PAGE_SHIFT;

	printk("Max dom size %d\n", memslot[MEMSLOT_AGENCY].size);

	printk("Domain length = %lu pages.\n", nr_pages);

	ASSERT(d);

	d->tot_pages = memslot[MEMSLOT_AGENCY].size >> PAGE_SHIFT;
	alloc_spfn = memslot[MEMSLOT_AGENCY].base_paddr >> PAGE_SHIFT;

	clear_bit(_VPF_down, &d->pause_flags);
	v_start = L_PAGE_OFFSET;

	/* vstack is used when the guest has not initialized its own stack yet; put right after _end of the guest OS. */

	__setup_dom_pgtable(d, v_start, memslot[MEMSLOT_AGENCY].size, (alloc_spfn << PAGE_SHIFT));

	/* Lets switch to the page table of our new domain - required for sharing page info */

	mmu_switch(&d->addrspace);

	si = (start_info_t *) vstartinfo_start;

	agency_start_info = si;
	memset(si, 0, PAGE_SIZE);

	si->domID = d->domain_id;
	si->nr_pages = d->tot_pages;
	si->dom_phys_offset = alloc_spfn << PAGE_SHIFT;

	si->pt_vaddr = d->addrspace.pgtable_vaddr;

	/* Propagate the virtual address of the shared info page for this domain */
	si->shared_info = d->shared_info;

	si->hypercall_addr = (unsigned long) hypercall_start;
	si->logbool_ht_set_addr = (unsigned long) ht_set;

	si->fdt_paddr = memslot[MEMSLOT_AGENCY].fdt_paddr;

	si->hypervisor_vaddr = CONFIG_HYPERVISOR_VADDR;

	printk("Agency FDT device tree: 0x%lx (phys)\n", si->fdt_paddr);

	/* HW details on the CPU: processor ID, cache ID and ARM architecture version */

	si->printch = printch;

	mmu_switch(&current->addrspace);

	d->vstartinfo_start = vstartinfo_start;

	/* Set up a new domain stack for the RT domain */
	domain_stack = (unsigned long) setup_dom_stack(domains[DOMID_AGENCY_RT]);

	/* Store the stack address for further needs in hypercalls/interrupt context */
	__hyp_stack[AGENCY_RT_CPU] = domain_stack;

	/* We set the realtime domain in pseudo-usr mode since the primary domain will start it, not us. */
	__pseudo_usr_mode[AGENCY_RT_CPU] = 1;

	/*
	 * Keep a reference in the primary agency domain to its subdomain. Indeed, there is only one shared info page mapped
	 * in the guest.
	 */
	agency->shared_info->subdomain_shared_info = domains[DOMID_AGENCY_RT]->shared_info;

	/*
	 * Create the first thread associated to this domain.
	 * The initial stack of the domain is put at the top of the domain memory area.
	 */
#ifdef CONFIG_ARCH_ARM32
	/* We start at 0x8000 since ARM-32 Linux is configured as such with the 1st level page table placed at 0x4000 */
	new_thread(d, v_start + L_TEXT_OFFSET, si->fdt_paddr, v_start + memslot[MEMSLOT_AGENCY].size, vstartinfo_start);
#else
	new_thread(d, v_start, si->fdt_paddr, v_start + memslot[MEMSLOT_AGENCY].size, vstartinfo_start);
#endif

	return 0;
}

