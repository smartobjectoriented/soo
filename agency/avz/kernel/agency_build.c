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

int construct_agency(struct domain *d) {
	unsigned long v_start;
	unsigned long alloc_spfn;

	unsigned long domain_stack;
	extern addr_t *hypervisor_stack;
	static addr_t *__hyp_stack = (unsigned long *) &hypervisor_stack;

	printk("***************************** Loading SOO Agency Domain *****************************\n");

	/* The agency is always in slot 1 */

	/* Now the slot is busy. */
	memslot[MEMSLOT_AGENCY].busy = true;

	if (memslot[MEMSLOT_AGENCY].size == 0)
		panic("No agency image supplied\n");

	d->max_pages = ~0U;

	ASSERT(d);

	d->avz_shared->nr_pages = memslot[MEMSLOT_AGENCY].size >> PAGE_SHIFT;

	alloc_spfn = memslot[MEMSLOT_AGENCY].base_paddr >> PAGE_SHIFT;

	clear_bit(_VPF_down, &d->pause_flags);
	v_start = AGENCY_VOFFSET;

#ifdef CONFIG_ARM64VT
	__setup_dom_pgtable(d, memslot[MEMSLOT_AGENCY].base_paddr, memslot[MEMSLOT_AGENCY].size);
	d->avz_shared->dom_phys_offset = alloc_spfn << PAGE_SHIFT;
#else
	__setup_dom_pgtable(d, v_start, memslot[MEMSLOT_AGENCY].size, (alloc_spfn << PAGE_SHIFT) - L_TEXT_OFFSET);
	d->avz_shared->dom_phys_offset = (alloc_spfn << PAGE_SHIFT) - L_TEXT_OFFSET;

#endif

	/* Propagate the virtual address of the shared info page for this domain */

	d->avz_shared->hypercall_vaddr = (unsigned long) hypercall_entry;
	d->avz_shared->logbool_ht_set_addr = (unsigned long) ht_set;
	d->avz_shared->fdt_paddr = memslot[MEMSLOT_AGENCY].fdt_paddr;
	d->avz_shared->hypervisor_vaddr = CONFIG_HYPERVISOR_VADDR;

	printk("Agency FDT device tree: 0x%lx (phys)\n", d->avz_shared->fdt_paddr);

	/* HW details on the CPU: processor ID, cache ID and ARM architecture version */

	d->avz_shared->printch = printch;

	/* Set up a new domain stack for the RT domain */
	domain_stack = (unsigned long) setup_dom_stack(domains[DOMID_AGENCY_RT]);

	/* Store the stack address for further needs in hypercalls/interrupt context */
	__hyp_stack[AGENCY_RT_CPU] = domain_stack;

	/*
	 * Keep a reference in the primary agency domain to its subdomain. Indeed, there is only one shared info page mapped
	 * in the guest.
	 */
	agency->avz_shared->subdomain_shared = domains[DOMID_AGENCY_RT]->avz_shared;
	agency->avz_shared->subdomain_shared_paddr = virt_to_phys(agency->avz_shared);

	/* Domain related information */
	domains[DOMID_AGENCY_RT]->avz_shared->nr_pages = d->avz_shared->nr_pages;
	domains[DOMID_AGENCY_RT]->avz_shared->hypercall_vaddr = d->avz_shared->hypercall_vaddr;
	domains[DOMID_AGENCY_RT]->avz_shared->fdt_paddr = d->avz_shared->fdt_paddr;
	domains[DOMID_AGENCY_RT]->avz_shared->dom_phys_offset = d->avz_shared->dom_phys_offset;
	domains[DOMID_AGENCY_RT]->avz_shared->pagetable_paddr = d->avz_shared->pagetable_paddr;
	domains[DOMID_AGENCY_RT]->avz_shared->logbool_ht_set_addr = d->avz_shared->logbool_ht_set_addr;
	domains[DOMID_AGENCY_RT]->avz_shared->hypervisor_vaddr = d->avz_shared->hypervisor_vaddr;
	domains[DOMID_AGENCY_RT]->avz_shared->printch = d->avz_shared->printch;

	/*
	 * Create the first thread associated to this domain.
	 * The initial stack of the domain is put at the top of the domain memory area.
	 */
#ifdef CONFIG_ARCH_ARM32
	/* We start at 0x8000 since ARM-32 Linux is configured as such with the 1st level page table placed at 0x4000 */
	new_thread(d, v_start + L_TEXT_OFFSET, d->avz_shared->fdt_paddr, v_start + memslot[MEMSLOT_AGENCY].size);
#else

#ifdef CONFIG_ARM64VT
	new_thread(d, memslot[MEMSLOT_AGENCY].ipa_addr,
		   phys_to_ipa(memslot[MEMSLOT_AGENCY], d->avz_shared->fdt_paddr),
		   memslot[MEMSLOT_AGENCY].ipa_addr + memslot[MEMSLOT_AGENCY].size);

#else
	new_thread(d, v_start, d->avz_shared->fdt_paddr, v_start + memslot[MEMSLOT_AGENCY].size);
#endif

#endif

	return 0;
}

