/*
 * Copyright (C) 2020 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef MEMORY_H
#define MEMORY_H

#include <types.h>
#include <list.h>
#include <bitops.h>

#include <asm/mmu.h>

#include <soo/uapi/me_access.h>

/*
 * We add two functions for retrieving virt and phys address relative to
 * Linux offset according to the memory map (used to access guest mem)
 */
#define __lpa(vaddr) ((vaddr) - L_PAGE_OFFSET + CONFIG_RAM_BASE)
#define __lva(paddr) ((paddr) - CONFIG_RAM_BASE + L_PAGE_OFFSET)

#define __pa(vaddr)             (((addr_t) vaddr) - CONFIG_HYPERVISOR_VIRT_ADDR + ((addr_t) CONFIG_RAM_BASE))
#define __va(paddr)             (((addr_t) paddr) - ((addr_t) CONFIG_RAM_BASE) + CONFIG_HYPERVISOR_VIRT_ADDR)

#define virt_to_phys(x)     (__pa(x))
#define phys_to_virt(x)     (__va(x))

#define pfn_to_phys(pfn) ((pfn) << PAGE_SHIFT)
#define phys_to_pfn(phys) (((addr_t) phys) >> PAGE_SHIFT)
#define virt_to_pfn(virt) (phys_to_pfn(__va((addr_t) virt)))
#define pfn_to_virt(pfn) (phys_to_virt(pfn_to_phys(pfn)))


struct domain;
struct page_info;
struct vcpu;

extern struct list_head io_maplist;

/* Manage the io_maplist. The list is sorted by ascending vaddr. */
typedef struct {
	addr_t vaddr;	/* Virtual address of the mapped I/O range */
	addr_t paddr; 	/* Physical address of this mapping */
	size_t size;	/* Size in bytes */

	struct list_head list;
} io_map_t;

void init_io_mapping(void);
addr_t io_map(addr_t phys, size_t size);
void io_unmap(addr_t vaddr);
io_map_t *find_io_map_by_paddr(addr_t paddr);
void dump_io_maplist(void);

extern int __irq_safe[];

extern struct domain *idle_domain[];

int get_ME_free_slot(unsigned int size, ME_state_t ME_state);
void put_ME_slot(unsigned int ME_slotID);

void early_memory_init(void);
void memory_init(void);

uint32_t get_kernel_size(void);

void get_current_addrspace(addrspace_t *addrspace);
bool is_addrspace_equal(addrspace_t *addrspace1, addrspace_t *addrspace2);
void switch_mm(struct domain *d, addrspace_t *next_addrspace);
void dump_page(unsigned int pfn);

static inline int get_order_from_bytes(addr_t size)
{
	int order;

	size = (size - 1) >> PAGE_SHIFT;
	for (order = 0; size; order++)
		size >>= 1;

	return order;
}

void clear_bss(void);

#endif /* MEMORY_H */
