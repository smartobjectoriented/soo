/*
 * Copyright (C) 2014-2016 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#if 0
#define DEBUG
#endif

#include <config.h>
#include <percpu.h>
#include <sched.h>
#include <errno.h>
#include <sched.h>
#include <bitmap.h>
#include <memory.h>
#include <memslot.h>
#include <common.h>
#include <linkage.h>
#include <heap.h>

#include <asm/cacheflush.h>
#include <asm/processor.h>
#include <asm/mmu.h>

#include <mach/uart.h>

#include <soo/arch-arm.h>

#define ME_MEMCHUNK_SIZE	2 * 1024 * 1024
#define ME_MEMCHUNK_NR		256    /* 256 chunks of 2 MB */

/*
 * Set of memslots in the RAM memory (do not confuse with memchunk !)
 * In the memslot table, the index 0 is for AVZ, the index 1 is for the two agency domains (domain 0 (non-RT) and domain 1 (RT))
 * and the indexes 2..MEMSLOT_NR are for the MEs. If the ME_slotID is provided, the index is given by ME_slotID.
 * Hence, the ME_slotID matches with the ME domID.
 */
memslot_entry_t memslot[MEMSLOT_NR];

/* Memory chunks bitmap for allocating MEs */
/* 8 bits per int int */
unsigned int memchunk_bitmap[ME_MEMCHUNK_NR/32];

/* Page-aligned kernel size (including frame table) */
static uint32_t kernel_size;

/* Current available I/O range address */
uint32_t io_mapping_current;
struct list_head io_maplist;

extern unsigned long __bss_start, __bss_end;
extern unsigned long __vectors_start, __vectors_end;

/*
 * Perform basic memory initialization, i.e. the existing memory slot.
 */
void early_memory_init(void) {
	unsigned int i;

	clear_bss();

	/* Hypervisor */
	memslot[0].base_paddr = CONFIG_RAM_BASE;
	memslot[0].size = HYPERVISOR_SIZE;
	memslot[0].busy = true;

	/* The memslot[1] is reserved for the agency (two domains although one binary image only.
	 * It will become busy during the agency construct operation.
	 */

	for (i = 0; i < MAX_ME_DOMAINS; i++)
		memslot[i+2].busy = false;

	kernel_size = __pa(&__end) - CONFIG_RAM_BASE;

}

/*
 * Clear the .bss section in the kernel memory layout.
 */
void clear_bss(void) {
	unsigned char *cp = (unsigned char *) &__bss_start;

	/* Zero out BSS */
	while (cp < (unsigned char *) &__bss_end)
		*cp++ = 0;
}

uint32_t get_kernel_size(void) {
	return kernel_size;
}

/*
 * Main memory init function
 */

void memory_init(void) {
	u64  *__new_sys_pgtable;
	addr_t vectors_paddr;
	addrspace_t __addrspace;

	/* Initialize the list of I/O virt/phys maps */
	INIT_LIST_HEAD(&io_maplist);

	/* Initialize the kernel heap */
	heap_init();

	init_io_mapping();

	/* Re-setup a system page table with a better granularity */
	__new_sys_pgtable = new_sys_pgtable();

	create_mapping(__new_sys_pgtable, CONFIG_HYPERVISOR_VIRT_ADDR, CONFIG_RAM_BASE, get_kernel_size(), false);

	/* Mapping uart I/O for debugging purposes */
	create_mapping(__new_sys_pgtable, UART_BASE, UART_BASE, PAGE_SIZE, true);

	/*
	 * Switch to the temporary page table in order to re-configure the original system page table
	 * Warning !! After the switch, we do not have ttany mapped I/O until the driver core gets initialized.
	 */

	__addrspace.ttbr1[smp_processor_id()] = __pa(__new_sys_pgtable);
	mmu_switch(&__addrspace);

	/* Re-configuring the original system page table */
	memcpy((void *) __sys_l0pgtable, (unsigned char *) __new_sys_pgtable, TTB_L1_SIZE);

	/* Finally, switch back to the original location of the system page table */
	__addrspace.ttbr1[smp_processor_id()] = __pa(__sys_l0pgtable);
	mmu_switch(&__addrspace);

#if 0
	/* Finally, prepare the vector page at its correct location */
	vectors_paddr = get_free_page();

	create_mapping(NULL, VECTOR_VADDR, vectors_paddr, PAGE_SIZE, true);

	memcpy((void *) VECTOR_VADDR, (void *) &__vectors_start, (void *) &__vectors_end - (void *) &__vectors_start);
#endif
}

/*
 * I/O address space management
 */

/* Init the I/O address space */
void init_io_mapping(void) {
	io_mapping_current = IO_MAPPING_BASE;
}

void dump_io_maplist(void) {
	io_map_t *cur = NULL;
	struct list_head *pos;

	printk("%s: ***** List of I/O mappings *****\n\n", __func__);

	list_for_each(pos, &io_maplist) {

		cur = list_entry(pos, io_map_t, list);

		printk("    - vaddr: %x  mapped on   paddr: %x\n", cur->vaddr, cur->paddr);
		printk("          with size: %d bytes\n", cur->size);
	}
}

/* Map a I/O address range to its physical range */
addr_t io_map(addr_t phys, size_t size) {
	io_map_t *io_map;
	struct list_head *pos;
	io_map_t *cur = NULL;
	uint32_t target, offset;

	/* Sometimes, it may happen than drivers try to map several devices which are located within the same page,
	 * i.e. the 4-KB page offset is not null.
	 */
	offset = phys & (PAGE_SIZE - 1);
	phys = phys & PAGE_MASK;

	io_map = find_io_map_by_paddr(phys);
	if (io_map) {
		if (io_map->size == size)
			return io_map->vaddr + offset;
		else
			BUG();
	}

	/* We are looking for a free region in the virtual address space */
	target = IO_MAPPING_BASE;

	/* Re-adjust the address according to the alignment */

	list_for_each(pos, &io_maplist) {
		cur = list_entry(pos, io_map_t, list);
		if (target + size <= cur->vaddr)
			break;
		else {
			target = cur->vaddr + cur->size;
			target = ALIGN_UP(target, ((size < SZ_1M) ? PAGE_SIZE : SZ_1M));

			/* If we reach the end of the list, we can detect it. */
			cur = NULL;
		}
	}

	io_map = (io_map_t *) malloc(sizeof(io_map_t));
	ASSERT(io_map != NULL);

	io_map->vaddr = target;
	io_map->paddr = phys;
	io_map->size = size;

	/* Insert the new entry before <cur> or if NULL at the tail of the list. */
	if (cur != NULL) {

		io_map->list.prev = pos->prev;
		io_map->list.next = pos;

		pos->prev->next = &io_map->list;
		pos->prev = &io_map->list;

	} else
		list_add_tail(&io_map->list, &io_maplist);


	create_mapping(NULL, io_map->vaddr, io_map->paddr, io_map->size, true);

	return io_map->vaddr + offset;

}

/*
 * Try to find an io_map entry corresponding to a specific pvaddr .
 */
io_map_t *find_io_map_by_paddr(uint32_t paddr) {
	struct list_head *pos;
	io_map_t *io_map;

	list_for_each(pos, &io_maplist) {
		io_map = list_entry(pos, io_map_t, list);
		if (io_map->paddr == paddr)
			return io_map;
	}

	return NULL;
}

/*
 * Remove a mapping.
 */
void io_unmap(uint32_t vaddr) {
	io_map_t *cur = NULL;
	struct list_head *pos, *q;

	/* If we have an 4 KB offset, we do not have the mapping at this level. */
	vaddr = vaddr & PAGE_MASK;

	list_for_each_safe(pos, q, &io_maplist) {

		cur = list_entry(pos, io_map_t, list);

		if (cur->vaddr == vaddr) {
			list_del(pos);
			break;
		}
		cur = NULL;
	}

	if (cur == NULL) {
		lprintk("io_unmap failure: did not find entry for vaddr %x\n", vaddr);
		kernel_panic();
	}

	release_mapping(NULL, cur->vaddr, cur->size);

	free(cur);
}


/*
 * Returns the power of 2 (order) which matches the size
 */
unsigned int get_power_from_size(unsigned int bits_NR) {
	unsigned int order;

	/* Find the power of 2 which matches the number of bits */
	order = -1;

	do {
		bits_NR = bits_NR >> 1;
		order++;
	} while (bits_NR);

	return order;
}
/*
 * Allocate a memory slot which satisfies the request.
 *
 * Returns the physical start address or 0 if no slot available.
 */
unsigned int allocate_memslot(unsigned int order) {
  int pos;

  pos = bitmap_find_free_region((unsigned long *) &memchunk_bitmap, ME_MEMCHUNK_NR, order);
  if (pos < 0)
  	return 0;

#ifdef DEBUG
  printk("allocate_memslot param %d\n", order);
  printk("allocate_memslot pos %d\n", pos);
  printk("allocate_memslot memslot1start %08x\n", (unsigned int) memslot[1].start);
  printk("allocate_memslot memslot1size %d\n", memslot[1].size);
  printk("allocate_memslot pos*MEMCHUNK_SIZE %d\n", pos*ME_MEMCHUNK_SIZE);
#endif

  return memslot[1].base_paddr + memslot[1].size + pos*ME_MEMCHUNK_SIZE;
}

void release_memslot(unsigned int addr, unsigned int order) {
	int pos;

	pos = addr - memslot[1].base_paddr - memslot[1].size;
	pos /= ME_MEMCHUNK_SIZE;

#ifdef DEBUG
  printk("release_memslot addr %08x\n", addr);
  printk("release_memslot order %d\n", order);
  printk("release_memslot pos %d\n", pos);
#endif

	bitmap_release_region((unsigned long *) &memchunk_bitmap, pos, order);
}


/*
 * Get the next available memory slot for a ME hosting.
 *
 * Return the corresponding size and ME_slotID if any.
 * If no slot is available, it returns -1.
 *
 */
int get_ME_free_slot(unsigned int size) {

	unsigned int order, addr;
	int slotID;
	unsigned int bits_NR;

	/* Check for available slot */
	for (slotID = MEMSLOT_BASE; slotID < MEMSLOT_NR; slotID++)
		if (!memslot[slotID].busy)
			break;

	if (slotID == MEMSLOT_NR)
		return -1;

	/* memslot[slotID] is available */

	bits_NR = DIV_ROUND_UP(size, ME_MEMCHUNK_SIZE);

	order = get_power_from_size(bits_NR);

	addr = allocate_memslot(order);

	if (!addr)
		return -1;  /* No available memory */

	memslot[slotID].base_paddr = addr;
	memslot[slotID].size = (1 << order) * ME_MEMCHUNK_SIZE;  /* Readjust size */
	memslot[slotID].busy = true;

#ifdef DEBUG
	printk("get_ME_slot param %d\n", size);
	printk("get_ME_slot bits_NR %d\n", bits_NR);
	printk("get_ME_slot slotID %d\n", slotID);
	printk("get_ME_slot start %08x\n", (unsigned int) memslot[slotID].start);
	printk("get_ME_slot size %d\n", memslot[slotID].size);
#endif

	return slotID;
}

/*
 * Release a slot
 */
int put_ME_slot(unsigned int slotID) {

	/* Release the allocated memchunks */
	release_memslot(memslot[slotID].base_paddr, get_power_from_size(DIV_ROUND_UP(memslot[slotID].size, ME_MEMCHUNK_SIZE)));

	memslot[slotID].busy = false;

#ifdef DEBUG
  printk("put_ME_slot param %d\n", slotID);
#endif

	return 0;
}



/*
 * switch_mm() is used to perform a memory context switch between domains.
 * @d refers to the domain
 * @next_addrspace refers to the address space to be considered with this domain.
 * @current_addrspace will point to the current address space.
 */
void switch_mm(struct domain *d, addrspace_t *next_addrspace) {
	addrspace_t prev_addrspace;

	/* Preserve the current configuration of MMU registers of the running domain before doing a switch */
	get_current_addrspace(&prev_addrspace);

	if (is_addrspace_equal(next_addrspace, &prev_addrspace))
	/* Check if the current page table is identical to the next one. */
		return ;

	set_current(d);

	mmu_switch(next_addrspace);
}

void *ioremap(addr_t phys_addr, unsigned int size) {
	addr_t vaddr;
	unsigned int offset;
#if 0
	/* Make sure the virtual address will be correctly aligned (either section or page aligned). */

	io_map_current_base = ALIGN_UP(io_map_current_base, ((size < SZ_1M) ? PAGE_SIZE : SZ_1M));

	/* Preserve a possible offset */
	offset = phys_addr & 0xfff;

	vaddr = (unsigned long) io_map_current_base;

	create_mapping(NULL, (unsigned long) io_map_current_base, phys_addr, size, true);

	io_map_current_base += size;
#endif
	return (void *) (vaddr + offset);

}
