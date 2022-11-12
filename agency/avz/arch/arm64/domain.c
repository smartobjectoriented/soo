/*
 * Copyright (C) 2014-2021 Daniel Rossier <daniel.rossier@heig-vd.ch>
 * Copyright (C) 2016-2019 Baptiste Delporte <bonel@bonel.net>
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

#include <sched.h>
#include <sizes.h>
#include <types.h>

#include <asm/mmu.h>
#include <asm/processor.h>

#include <mach/uart.h>
#include <mach/gic.h>

void arch_setup_domain_frame(struct domain *d, struct cpu_regs *domain_frame, addr_t fdt_addr, addr_t start_stack, addr_t start_pc) {

	domain_frame->x21 = fdt_addr;
	domain_frame->x22 = (unsigned long) d->avz_shared;

	domain_frame->sp = start_stack;
	domain_frame->pc = start_pc;

	d->cpu_regs.sp = (unsigned long) domain_frame;
	d->cpu_regs.lr = (unsigned long) pre_ret_to_user;
}

/*
 * Setup of domain consists in setting up the 1st-level and 2nd-level page tables within the domain.
 */
/**
 * Setup the stage 2 translation page table to translate Intermediate Physical address (IPA) to to PA addresses.
 *
 * @param d
 * @param v_start
 * @param map_size
 * @param p_start
 */
void __setup_dom_pgtable(struct domain *d, addr_t ipa_start, unsigned long map_size) {
	u64 *new_pt;

	ASSERT(d);

	/* Make sure that the size is 2 MB block aligned */
	map_size = ALIGN_UP(map_size, SZ_2M);

	/* Initial L0 page table for the domain */
	new_pt = new_root_pgtable();

	printk("*** Setup page tables of the domain: ***\n");

	printk("   intermediate phys address    : 0x%lx\n", ipa_start);
	printk("   map size (bytes) 		: 0x%lx\n", map_size);
	printk("   stage-2 vttbr 		: (va) 0x%lx - (pa) 0x%lx\n", new_pt, __pa(new_pt));

	d->avz_shared->pagetable_vaddr = (addr_t) new_pt;
	d->avz_shared->pagetable_paddr = __pa(new_pt);

	/* Prepare the IPA -> PA translation for this domain */
	create_mapping(new_pt, ipa_start, ipa_start, map_size, false, S2);

	/* Map the GIC CPU and Distributor */
	create_mapping(new_pt, GIC_DIST_PHYS, GIC_DIST_PHYS, GIC_DIST_SIZE, true, S2);
	create_mapping(new_pt, GIC_CPU_PHYS, GIC_CPU_PHYS, GIC_CPU_SIZE, true, S2);

	/* Map the shared page */
	create_mapping(new_pt, virt_to_phys(d->avz_shared), virt_to_phys(d->avz_shared), PAGE_SIZE, true, S2);
	if (d->avz_shared->subdomain_shared)
		create_mapping(new_pt, d->avz_shared->subdomain_shared_paddr, d->avz_shared->subdomain_shared_paddr, PAGE_SIZE, true, S2);
}

void arch_domain_create(struct domain *d, int cpu_id) {

	if (is_idle_domain(d))
		d->avz_shared->pagetable_paddr = __pa(__sys_root_pgtable);
}

