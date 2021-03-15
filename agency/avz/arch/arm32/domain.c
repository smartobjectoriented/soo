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

#include <domain.h>

#include <asm/processor.h>

void arch_setup_domain_frame(struct domain *d, struct cpu_user_regs *domain_frame, addr_t fdt_addr, addr_t start_info, addr_t start_stack, addr_t start_pc) {
	struct cpu_user_regs *regs = &d->arch.guest_context.user_regs;

	domain_frame->r2 = fdt_addr;
	domain_frame->r12 = start_info;

	domain_frame->r13 = start_stack;
	domain_frame->r15 = start_pc;

	domain_frame->psr = 0x93;  /* IRQs disabled initially */

	regs->r13 = (unsigned long) domain_frame;
	regs->r14 = (unsigned long) pre_ret_to_user;
}

/*
 * Setup of domain consists in setting up the 1st-level and 2nd-level page tables within the domain.
 */
void __setup_dom_pgtable(struct domain *d, addr_t v_start, unsigned long map_size, addr_t p_start) {
	uint32_t vaddr, *new_pt;
	addr_t vpt_start = vstart + TTB_L1_SYS_OFFSET;

	ASSERT(d);

	/* Make sure that the size is 1 MB-aligned */
	map_size = ALIGN_UP(map_size, TTB_SECT_SIZE);

	printk("*** Setup page tables of the domain: ***\n");
	printk("   v_start          : 0x%lx\n", v_start);
	printk("   map size (bytes) : 0x%lx\n", map_size);
	printk("   phys address     : 0x%lx\n", p_start);
	printk("   vpt_start        : 0x%lx\n", vpt_start);

	/* guest page table address (phys addr) */
	d->addrspace.pgtable_paddr = (vpt_start - v_start + p_start);
	d->addrspace.pgtable_vaddr = vpt_start;

	d->addrspace.ttbr0[d->processor] = cpu_get_ttbr0() & ~TTBR0_BASE_ADDR_MASK;
	d->addrspace.ttbr0[d->processor] |= d->addrspace.pgtable_paddr;

	/* Manage the new system page table dedicated to the domain. */
	new_pt = (uint32_t *) __lva(vpt_start - v_start + p_start); /* Ex.: 0xc0c04000 */

	/* copy page table of idle domain to guest domain */
	memcpy(new_pt, __sys_l1pgtable, TTB_L1_SIZE);

	/* Clear the area below the I/Os, but preserve of course the page table itself which is located within the first MB */
	for (vaddr = 0; vaddr < CONFIG_HYPERVISOR_VIRT_ADDR; vaddr += TTB_SECT_SIZE)
		*((uint32_t *) l1pte_offset(new_pt, vaddr)) = 0;

	/* Do the mapping of new domain at its virtual address location */
	create_mapping(new_pt,  v_start, p_start, map_size, false);

	/* We have to change the ptes of the page table in our new page table :-) (currently pointing the hypervisor page table. */
	vaddr = (uint32_t) l1pte_offset(new_pt, vpt_start);

	*((uint32_t *) vaddr) &= ~TTB_L1_SECT_ADDR_MASK; /* Reset the pfn */
	*((uint32_t *) vaddr) |= ((uint32_t ) d->addrspace.pgtable_paddr) & TTB_L1_SECT_ADDR_MASK;

	mmu_page_table_flush((uint32_t) new_pt, ((uint32_t) new_pt) + TTB_L1_SIZE);
}

void __arch_domain_create(struct domain *d, int cpu_id) {
                
	/* Will be used during the context_switch (cf kernel/entry-armv.S */

	d->arch.guest_context.sys_regs.vdacr = domain_val(DOMAIN_USER, DOMAIN_MANAGER) | domain_val(DOMAIN_KERNEL, DOMAIN_MANAGER) |
		domain_val(DOMAIN_IO, DOMAIN_MANAGER);

	d->arch.guest_context.sys_regs.vusp = 0x0; /* svc stack hypervisor at the beginning */

	if (is_idle_domain(d)) {
		d->addrspace.pgtable_paddr = (CONFIG_RAM_BASE + TTB_L1_SYS_OFFSET);
		d->addrspace.pgtable_vaddr = (CONFIG_HYPERVISOR_VIRT_ADDR + TTB_L1_SYS_OFFSET);

		d->addrspace.ttbr0[cpu_id] = cpu_get_ttbr0() & ~TTBR0_BASE_ADDR_MASK;
		d->addrspace.ttbr0[cpu_id] |= d->addrspace.pgtable_paddr;
	}
}

