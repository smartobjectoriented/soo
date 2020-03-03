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

#include <avz/config.h>
#include <avz/lib.h>
#include <avz/percpu.h>
#include <avz/sched.h>
#include <avz/init.h>
#include <avz/ctype.h>
#include <avz/console.h>

#include <avz/domain.h>
#include <avz/errno.h>
#include <asm/processor.h>
#include <asm/cputype.h>
#include <asm/pgtable.h>
#include <asm/mach/map.h>
#include <asm/memslot.h>

#include <soo/uapi/arch-arm.h>

#include <avz/mm.h>
#include <asm/mm.h>
#include <avz/libelf.h>
#include <asm/cacheflush.h>

#include <soo/uapi/logbool.h>

#define L_TEXT_OFFSET	0x8000

start_info_t *agency_start_info;

/*
 * setup_page_table_guestOS() is setting up the 1st-level and 2nd-level page tables within the domain.
 */
int setup_page_table_guestOS(struct vcpu *v, unsigned long v_start, unsigned long map_size, unsigned long p_start, unsigned long vpt_start) {

	struct map_desc map;
	unsigned long addr, new_pt;
	struct vcpu temp_vcpu;

	ASSERT(v);

	/* Make sure that the size is 1 MB-aligned */
	map_size = ALIGN_UP(map_size, SECTION_SIZE);

	printk("*** Setup page tables of the domain: ***\n");
	printk("   v_start          : 0x%lx\n", v_start);
	printk("   map size (bytes) : 0x%lx\n", map_size);
	printk("   phys address     : 0x%lx\n", p_start);
	printk("   vpt_start        : 0x%lx\n", vpt_start);

	new_pt = (unsigned long) __lva(vpt_start - v_start + p_start);

	/* copy page table of idle domain to guest domain */
	memcpy((unsigned long *) new_pt, swapper_pg_dir, sizeof(swapper_pg_dir));

	/* guest start address (phys/virtual addr) */
	v->arch.guest_pstart = p_start;
	v->arch.guest_vstart = v_start;

	/* guest page table address (phys addr) */
	v->arch.guest_table = mk_pagetable(vpt_start - v_start + p_start);
	v->arch.guest_vtable = (pte_t *) vpt_start;

	/* We have to change the ptes of the page table in our new page table :-) */
	addr = (unsigned long) pgd_offset_priv((pde_t *) new_pt, vpt_start);

	((pde_t *) addr)->l2 &= ~SECTION_MASK; /* Reset the pfn */
	((pde_t *) addr)->l2 |= (vpt_start - v_start + p_start) & SECTION_MASK;

	flush_all();

	/* Immediately switch to our newly created page table in order to work in the domain address space. */
	save_ptbase(&temp_vcpu);
	write_ptbase(v);

	/* Clear the area below the I/Os, but preserve of course the page table itself which is located within the first MB */
	for (addr = v_start + SECTION_SIZE; addr < VMALLOC_END; addr += SECTION_SIZE)
		pmd_clear(pgd_offset_priv((pde_t *) vpt_start, addr));

	/* And now, re-do the mapping including the remaining sections of the guest domain */
	map.pfn = phys_to_pfn(p_start);
	map.virtual = v_start;
	map.length = map_size;
	map.type = MT_MEMORY_RWX;

	create_mapping(&map, (pde_t *) vpt_start);

	write_ptbase(&temp_vcpu);

	return 0;
}

extern char hypercall_start[];

int construct_agency(struct domain *d) {
	struct vcpu *v;
	unsigned long vstartinfo_start;
	unsigned long v_start;
	unsigned long alloc_spfn;
	unsigned long vpt_start;
	struct start_info *si = NULL;
	unsigned long nr_pages;

	printk("***************************** Loading SOO Agency Domain *****************************\n");

	/* The agency is always in slot 1 */

	/* Now the slot is busy. */
	memslot[MEMSLOT_AGENCY].busy = true;

	if (memslot[MEMSLOT_AGENCY].size == 0)
		panic("No agency image supplied\n");

	/* The following page will contain start_info information */
	vstartinfo_start = (unsigned long) alloc_heap_page();

	d->max_pages = ~0U;
	d->tot_pages = 0;

	nr_pages = memslot[MEMSLOT_AGENCY].size >> PAGE_SHIFT;

	printk("Max dom size %d\n", memslot[MEMSLOT_AGENCY].size);

	printk("Domain length = %lu pages.\n", nr_pages);

	ASSERT(d);

	v = d->vcpu[0];
	BUG_ON(d->vcpu[0] == NULL);

	ASSERT(v);

	d->tot_pages = memslot[MEMSLOT_AGENCY].size >> PAGE_SHIFT;
	alloc_spfn = memslot[MEMSLOT_AGENCY].base_paddr >> PAGE_SHIFT;

	clear_bit(_VPF_down, &v->pause_flags);
	v_start = L_PAGE_OFFSET;

	vpt_start = v_start + L_TEXT_OFFSET - 0x4000;  /* Location of the system page table (see head.S). */

	/* vstack is used when the guest has not initialized its own stack yet; put right after _end of the guest OS. */

	setup_page_table_guestOS(v, v_start, memslot[MEMSLOT_AGENCY].size, (alloc_spfn << PAGE_SHIFT), vpt_start);

	/* Lets switch to the page table of our new domain - required for sharing page info */

	save_ptbase(current);
	write_ptbase(v);

	si = (start_info_t *) vstartinfo_start;

	agency_start_info = si;
	memset(si, 0, PAGE_SIZE);

	si->domID = d->domain_id;
	si->nr_pages = d->tot_pages;
	si->min_mfn = alloc_spfn;

	/* Propagate the virtual address of the shared info page for this domain */
	si->shared_info = d->shared_info;

	si->hypercall_addr = (unsigned long) hypercall_start;
	si->logbool_ht_set_addr = (unsigned long) ht_set;

	si->fdt_paddr = memslot[MEMSLOT_AGENCY].fdt_paddr;

	printk("Agency FDT device tree: 0x%lx (phys)\n", si->fdt_paddr);

	/* HW details on the CPU: processor ID, cache ID and ARM architecture version */

	si->printch = printch;
	si->pt_base = vpt_start;

	write_ptbase(current);

	d->arch.vstartinfo_start = vstartinfo_start;

	{
		unsigned long domain_stack;
		extern unsigned long *hypervisor_stack;
		extern unsigned long *pseudo_usr_mode;
		static unsigned long *__hyp_stack = (unsigned long *) &hypervisor_stack;
		static unsigned long *__pseudo_usr_mode = (unsigned long *) &pseudo_usr_mode;

	  /* Set up a new domain stack for the RT domain */
	  domain_stack = (unsigned long) setup_dom_stack(domains[DOMID_AGENCY_RT]->vcpu[0]);

	  /* Store the stack address for further needs in hypercalls/interrupt context */
	  __hyp_stack[AGENCY_RT_CPU] = domain_stack;

	  /* We set the realtime domain in pseudo-usr mode since the primary domain will start it, not us. */
	  __pseudo_usr_mode[AGENCY_RT_CPU] = 1;

	  /*
	   * Keep a reference in the primary agency domain to its subdomain. Indeed, there is only one shared info page mapped
	   * in the guest.
	   */
	  agency->shared_info->subdomain_shared_info = domains[DOMID_AGENCY_RT]->shared_info;

	  /* Create the first thread associated to this domain. */
	  new_thread(v, L_PAGE_OFFSET + L_TEXT_OFFSET, si->fdt_paddr, v_start + memslot[MEMSLOT_AGENCY].size, vstartinfo_start);
	}

	return 0;
}

