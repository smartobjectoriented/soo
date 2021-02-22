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

#include <asm/mmu.h>
#include <asm/bitops.h>

#define VECTORS_BASE	0xffff0000

#define IO_MAPPING_BASE		0xe0000000

struct domain;
struct page_info;
struct vcpu;

extern struct list_head io_maplist;

/* Manage the io_maplist. The list is sorted by ascending vaddr. */
typedef struct {
	uint32_t vaddr;	/* Virtual address of the mapped I/O range */
	uint32_t paddr; /* Physical address of this mapping */
	size_t size;	/* Size in bytes */

	struct list_head list;
} io_map_t;

void init_io_mapping(void);
addr_t io_map(addr_t phys, size_t size);
void io_unmap(uint32_t vaddr);
io_map_t *find_io_map_by_paddr(uint32_t paddr);
void dump_io_maplist(void);

extern int __irq_safe[];

extern struct domain *idle_domain[];

int get_ME_free_slot(unsigned int size);
int put_ME_slot(unsigned int ME_slotID);

void early_memory_init(void);
void memory_init(void);

uint32_t get_kernel_size(void);

void get_current_addrspace(addrspace_t *addrspace);
bool is_addrspace_equal(addrspace_t *addrspace1, addrspace_t *addrspace2);
void switch_mm(struct domain *d, addrspace_t *next_addrspace);

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
