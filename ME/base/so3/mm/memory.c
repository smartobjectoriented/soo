/*
 * Copyright (C) 2014-2021 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <memory.h>
#include <common.h>
#include <types.h>
#include <memory.h>
#include <spinlock.h>
#include <sizes.h>
#include <process.h>
#include <heap.h>

#include <device/ramdev.h>
#include <device/fdt.h>

#include <asm/mmu.h>
#include <asm/cacheflush.h>

#ifdef CONFIG_SO3VIRT
#include <soo/avz.h>
#endif

extern unsigned long __vectors_start, __vectors_end;
mem_info_t mem_info;

#ifdef CONFIG_MMU

page_t *frame_table;
static spinlock_t ft_lock;

/* First pfn of available pages */
volatile addr_t pfn_start;

/* Page-aligned kernel size (including frame table) */
static uint32_t kernel_size;

/* Current available I/O range address */
struct list_head io_maplist;

/* Initialize the frame table */
void frame_table_init(addr_t frame_table_start) {
	addr_t ft_phys;
	uint32_t i, ft_length, ft_pages;
	addr_t ft_pfn_end;

	/* The frame table (ft) is placed (page-aligned) right after the kernel region. */
	ft_phys = ALIGN_UP(__pa(frame_table_start), PAGE_SIZE);

	frame_table = (page_t *) __va(ft_phys);

	printk("SO3 Memory information:\n");

	/* Size of the available memory (without the kernel region) */

	mem_info.avail_pages = ALIGN_UP(mem_info.size - (ft_phys - mem_info.phys_base), PAGE_SIZE) >> PAGE_SHIFT;

	printk("  - kernel size without frame table is: %d (0x%x) bytes, %d MB / 0x%x PFNs\n",
			(ft_phys - mem_info.phys_base),
			(ft_phys - mem_info.phys_base),
			(ft_phys - mem_info.phys_base) / SZ_1M,
			(ft_phys - mem_info.phys_base) >> PAGE_SHIFT);

	/* Determine the length of the frame table in bytes */
	ft_length = mem_info.avail_pages * sizeof(page_t);

	/* Number of pages taken by the frame table; keep the frame table with a page size granularity */
	ft_pages = ALIGN_UP(ft_length, PAGE_SIZE) >> PAGE_SHIFT;

	/* Set the pages allocated by the frame table to busy. */
	for (i = 0; i < ft_pages; i++) {
		frame_table[i].free = false;
		frame_table[i].refcount = 1;
	}

	for (i = ft_pages; i < mem_info.avail_pages; i++) {
		frame_table[i].free = true;
		frame_table[i].refcount = 0;
	}

	/* Refers to the last page frame occupied by the frame table */
	ft_pfn_end = (ft_phys >> PAGE_SHIFT) + ft_pages - 1;

	/* Set the definitive kernel size */
	kernel_size = ((ft_pfn_end + 1) << PAGE_SHIFT) - CONFIG_RAM_BASE;

	/* First available pfn (right after the frame table) */
	pfn_start = ft_pfn_end + 1;

	printk("  - kernel size including frame table is: %d (0x%x) bytes, %d MB / 0x%x PFNs\n", kernel_size, kernel_size, kernel_size / SZ_1M, kernel_size >> PAGE_SHIFT);
	printk("  - Number of available page frames: 0x%x\n", mem_info.avail_pages);
	printk("  - Frame table size is: %d bytes meaning 0x%0x page frames\n", ft_length, ft_pages);
	printk("  - Page frame number of the first available page: 0x%x\n", pfn_start);

	spin_lock_init(&ft_lock);
}

uint32_t get_kernel_size(void) {
	return kernel_size;
}

/*
 * Get a free page. Return the physical address of the page (or 0 if not available).
 */
addr_t get_free_page(void) {
	uint32_t loop_mark;
	static uint32_t __next_free_page = 0;

	spin_lock(&ft_lock);

	/* Used to detect a cycle meaning that there no free page. */
	loop_mark = __next_free_page;

	do {

		if (frame_table[__next_free_page].free) {
			frame_table[__next_free_page].free = false;

			spin_unlock(&ft_lock);

			/* Found an available page */
			return page_to_phys(&frame_table[__next_free_page]);
		}

		/* Prepare to be ready fo the next request (or increment if not available */
		__next_free_page = (__next_free_page + 1) % mem_info.avail_pages;

	} while (__next_free_page != loop_mark);

	spin_unlock(&ft_lock);

	/* No available page */
	return 0;
}

/*
 * Get a free page with a virtual mapping.
 */
addr_t get_free_vpage(void) {
	addr_t paddr, vaddr;

	paddr = get_free_page();
	ASSERT(paddr);

	vaddr = io_map(paddr, PAGE_SIZE);

	return vaddr;
}

/*
 * Release a page, mark as free.
 */
void free_page(addr_t paddr) {
	page_t *page;

	spin_lock(&ft_lock);

	page = (page_t *) phys_to_page(paddr);
	page->free = true;

	spin_unlock(&ft_lock);
}

void free_vpage(addr_t vaddr) {
	addr_t paddr;

	paddr = virt_to_phys_pt(vaddr);

	io_unmap(vaddr);
	free_page(paddr);
}

/*
 * Search for a number of contiguous pages.
 * Returns 0 if not available.
 */
addr_t get_contig_free_pages(uint32_t nrpages) {
	uint32_t i, base = 0;

	spin_lock(&ft_lock);

	for (i = 0; i < mem_info.avail_pages; i++) {
		if (frame_table[i].free) {
			if (i-base+1 == nrpages) {
				/* Set the page as busy */
				for (i = 0; i < nrpages; i++)
					frame_table[base+i].free = false;

				spin_unlock(&ft_lock);

				/* Returns the block base */
				return page_to_phys(&frame_table[base]);
			}
		} else
			base = i+1;
	}

	spin_unlock(&ft_lock);

	return 0;
}

/*
 * Search for a number of contiguous physical and virtual pages.
 * Returns 0 if not available.
 */
addr_t get_contig_free_vpages(uint32_t nrpages) {
	addr_t vaddr, paddr;

	paddr = get_contig_free_pages(nrpages);
	ASSERT(paddr);

	vaddr = io_map(paddr, nrpages * PAGE_SIZE);

	return vaddr;
}


void free_contig_pages(addr_t paddr, uint32_t nrpages) {
	uint32_t i;
	page_t *page;

	spin_lock(&ft_lock);

	page = (page_t *) phys_to_page(paddr);

	for (i = 0; i < nrpages; i++)
		(page + i)->free = true;

	spin_unlock(&ft_lock);

}

void free_contig_vpages(addr_t vaddr, uint32_t nrpages) {
	addr_t paddr;

	paddr = virt_to_phys_pt(vaddr);

	io_unmap(vaddr);
	free_contig_pages(paddr, nrpages);
}

void dump_frame_table(void) {
	int i;

	printk("** Dump of frame table contents **\n\n");

	for (i = 0; i < mem_info.avail_pages; i++)
		printk("  - Page address (phys) :%x, free: %d\n", virt_to_phys_pt((addr_t) &frame_table[i]), frame_table[i].free);
}

/*
 * I/O address space management
 */
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
	addr_t target, offset;

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
			target = ALIGN_UP(target, PAGE_SIZE);

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
 * Try to find an io_map entry corresponding to a specific paddr .
 */
io_map_t *find_io_map_by_paddr(addr_t paddr) {
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
void io_unmap(addr_t vaddr) {
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
#endif /* CONFIG_MMU */

#ifdef CONFIG_SO3VIRT

/**
 * Re-adjust PFNs used for various purposes.
 */
void readjust_io_map(long pfn_offset) {
	io_map_t *io_map;
	struct list_head *pos;
	addr_t offset;

	/*
	 * Re-adjust I/O area since it does not intend to be HW I/O in SO3VIRT, but
	 * can be used for gnttab entries or other mappings for example.
	 */
	list_for_each(pos, &io_maplist) {
		io_map = list_entry(pos, io_map_t, list);

		offset = io_map->paddr & (PAGE_SIZE - 1);
		io_map->paddr = pfn_to_phys(phys_to_pfn(io_map->paddr) + pfn_offset);
		io_map->paddr += offset;

	}

	/* Re-adjust other PFNs used for frametable management. */
	pfn_start += pfn_offset;
}

#endif /* CONFIG_SO3VIRT */

/*
 * Main memory init function
 */

void memory_init(void) {
#ifdef CONFIG_MMU

#ifndef CONFIG_SO3VIRT
#ifdef CONFIG_ARCH_ARM32
	addr_t vectors_paddr;
#endif
#endif /* !CONFIG_SO3VIRT */

	void *new_sys_root_pgtable;

#endif /* CONFIG_MMU */

#ifdef CONFIG_MMU
	/* Initialize the list of I/O virt/phys maps */
	INIT_LIST_HEAD(&io_maplist);
#endif
	/* Initialize the kernel heap */
	heap_init();

#ifdef CONFIG_MMU

	lprintk("%s: relocating the device tree from 0x%x to 0x%p (size of %d bytes)\n", __func__, __fdt_addr, __end, fdt_totalsize(__fdt_addr));

	/* Move the device after the kernel stack (at &_end according to the linker script) */
	fdt_move((const void *) __fdt_addr, __end, fdt_totalsize(__fdt_addr));
	__fdt_addr = (addr_t *) __end;

	/* Initialize the free page list */
	frame_table_init(((addr_t) __end) + fdt_totalsize(__fdt_addr));

	/* Re-setup a system page table with a better granularity */
	new_sys_root_pgtable = new_root_pgtable();

	pgtable_copy_kernel_area(new_sys_root_pgtable);

	create_mapping(new_sys_root_pgtable, CONFIG_KERNEL_VADDR, CONFIG_RAM_BASE, get_kernel_size(), false);

#ifndef CONFIG_SO3VIRT
	/* Mapping uart I/O for debugging purposes */
	create_mapping(new_sys_root_pgtable, CONFIG_UART_LL_PADDR, CONFIG_UART_LL_PADDR, PAGE_SIZE, true);
#endif

	/*
	 * Switch to the temporary page table in order to re-configure the original system page table
	 * Warning !! After the switch, we do not have any mapped I/O until the driver core gets initialized.
	 */

	mmu_switch_sys(new_sys_root_pgtable);

	/* Re-configuring the original system page table */
	copy_root_pgtable(__sys_root_pgtable, new_sys_root_pgtable);

	/* Finally, switch back to the original location of the system page table */
	mmu_switch_sys(__sys_root_pgtable);

#ifndef CONFIG_SO3VIRT
#ifdef CONFIG_ARCH_ARM32
	/* Finally, prepare the vector page at its correct location */
	vectors_paddr = get_free_page();

	create_mapping(NULL, VECTOR_VADDR, vectors_paddr, PAGE_SIZE, true);

	memcpy((void *) VECTOR_VADDR, (void *) &__vectors_start, (void *) &__vectors_end - (void *) &__vectors_start);
#endif

#else
	/* Update the system page table vaddr required for memory re-implantation after migration */
	AVZ_shared->pagetable_vaddr = (addr_t)__sys_root_pgtable;
	AVZ_shared->pagetable_paddr = __pa(__sys_root_pgtable);

#endif

	set_pgtable(__sys_root_pgtable);

#endif /* CONFIG_MMU */
}
