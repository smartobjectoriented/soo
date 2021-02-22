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

void __arch_domain_create(struct domain *d) {
                
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

