/*
 * Copyright (C) 2016-2019 Daniel Rossier <daniel.rossier@soo.tech>
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
#include <memslot.h>
#include <domain.h>
#include <errno.h>
#include <libelf.h>
#include <heap.h>

#include <asm/processor.h>

#include <soo/arch-arm.h>

#include <soo/uapi/logbool.h>

#include <asm/cacheflush.h>

#define L_TEXT_OFFSET	0x8000

extern char hypercall_start[];

/*
 * construct_ME sets up a new Mobile Entity.
 */
int construct_ME(struct domain *d) {
	unsigned int slotID;
	unsigned long vstartinfo_start;
	unsigned long v_start;
	unsigned long alloc_spfn;
	unsigned long vpt_start;
	struct start_info *si = NULL;
	unsigned long nr_pages;
	addrspace_t prev_addrspace;

	slotID = d->domain_id;

	printk(	"***************************** Loading Mobile Entity (ME) *****************************\n");

	if (memslot[slotID].size == 0)
		panic("No domU image supplied\n");

	/* We are already on the swapper_pg_dir page table to have full access to RAM */

	/* The following page will contain start_info information */
	vstartinfo_start = (unsigned long) memalign(PAGE_SIZE, PAGE_SIZE);
	BUG_ON(!vstartinfo_start);

	d->max_pages = ~0U;
	d->tot_pages = 0;

	nr_pages = memslot[slotID].size >> PAGE_SHIFT;
	printk("Max dom size %d\n", memslot[slotID].size);

	printk("Domain length = %lu pages.\n", nr_pages);

	ASSERT(d);

	d->tot_pages = memslot[slotID].size >> PAGE_SHIFT;
	alloc_spfn = memslot[slotID].base_paddr >> PAGE_SHIFT;

	clear_bit(_VPF_down, &d->pause_flags);

	v_start = L_PAGE_OFFSET;

	vpt_start = v_start + TTB_L1_SYS_OFFSET; /* Location of the system page table (see head.S). */

	__setup_dom_pgtable(d, v_start, memslot[slotID].size, (alloc_spfn << PAGE_SHIFT));

	/* Lets switch to the page table of our new domain - required for sharing page info */
	get_current_addrspace(&prev_addrspace);

	/* We do this trick to access the right address space linked to the current CPU. */
	d->addrspace.ttbr0[smp_processor_id()] = d->addrspace.ttbr0[ME_CPU];

	mmu_switch(&d->addrspace);

	si = (start_info_t*) vstartinfo_start;

	memset(si, 0, PAGE_SIZE);

	si->domID = d->domain_id;

	si->nr_pages = d->tot_pages;
	si->dom_phys_offset = alloc_spfn << PAGE_SHIFT;

	si->shared_info = d->shared_info;
	si->hypercall_addr = (unsigned long) hypercall_start;

	si->logbool_ht_set_addr = (unsigned long) ht_set;

	si->fdt_paddr = memslot[slotID].fdt_paddr;

	printk("ME FDT device tree: 0x%lx (phys)\n", si->fdt_paddr);

	si->printch = printch;

	si->pt_vaddr = d->addrspace.pgtable_vaddr;

	mmu_switch(&prev_addrspace);

	d->vstartinfo_start = vstartinfo_start;

	/* Create the first thread associated to this domain. */
	new_thread(d, L_PAGE_OFFSET + L_TEXT_OFFSET, si->fdt_paddr, v_start + memslot[slotID].size, vstartinfo_start);

	return 0;
}

