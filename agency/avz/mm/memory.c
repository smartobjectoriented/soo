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

#include <asm/linkage.h>
#include <asm/cacheflush.h>

#include <asm/processor.h>

#include <asm/mmu.h>

#include <soo/arch-arm.h>

#define ME_MEMCHUNK_SIZE			2 * 1024 * 1024
#define ME_MEMCHUNK_NR				256    /* 256 chunks of 2 MB */

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

/* Frame table and its size in pages. */
struct page_info *frame_table;

unsigned long max_page;
unsigned long min_page;
unsigned long total_pages;

/* Limits of avz heap, used to initialise the allocator. */
unsigned long heap_phys_start, heap_phys_end;

unsigned int io_map_current_base;

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

	__sys_l1pgtable = (void *) (CONFIG_HYPERVISOR_VIRT_ADDR + TTB_L1_SYS_OFFSET);
}

extern unsigned long __bss_start, __bss_end;
extern unsigned long __vectors_start, __vectors_end;

/*
 * Clear the .bss section in the kernel memory layout.
 */
void clear_bss(void) {
	unsigned char *cp = (unsigned char *) &__bss_start;

	/* Zero out BSS */
	while (cp < (unsigned char *) &__bss_end)
		*cp++ = 0;
}

/*
 * AVZ Memory initialization - Initialize the heap, frame table and I/O-mapped area.
 */
void memory_init(void)
{
	ulong addr, frametable_phys_start, frametable_phys_end, frametable_size;
	unsigned long nxhp;
	uint32_t *l1pte;

	max_page = (__pa(HYPERVISOR_VIRT_START) + HYPERVISOR_SIZE) >> PAGE_SHIFT;
	min_page = __pa(&__end) >> PAGE_SHIFT;

#warning TO DOCUMENT

	/*
	 * Prepare the memory area devoted to the heap. We align the heap on section boundary so that
	 * we can deal with different cache attributes (no cache).
	 * Furthermore, the first MB is reserved for L2 page table allocation.
	 */
	l2pt_phys_start = (SECTION_UP(__pa(&__end)) + 1) << TTB_L1_SECT_ADDR_SHIFT;
	l2pt_current_base = (uint32_t *) __va(l2pt_phys_start);

	heap_phys_start = l2pt_phys_start + SZ_1M;

	/* Must pass a single mapped page for populating bootmem_region_list. */
	init_boot_pages(__pa(&__end), heap_phys_start);

	heap_phys_end = heap_phys_start + (HEAP_MAX_SIZE_MB << TTB_L1_SECT_ADDR_SHIFT) - 1;
	nxhp = (heap_phys_end - heap_phys_start + 1) >> PAGE_SHIFT;

	printk("AVZ Hypervisor Memory Layout\n");
	printk("------------------------------------\n");
	printk("Min page:                0x%lx (phys)\n", min_page);
	printk("Max page (not included): 0x%lx (phys)\n", max_page);

	printk("Heap start:              0x%lx (virt) / 0x%lx (phys)\n", (unsigned long) __va(heap_phys_start), heap_phys_start);
	printk("Heap end:                0x%lx (virt) / 0x%lx (phys)\n", (unsigned long) __va(heap_phys_end), heap_phys_end);
	printk("Heap size:               %luMiB (%lukiB)\n", nxhp >> (20 - PAGE_SHIFT), nxhp << (PAGE_SHIFT - 10));
	printk("\n");

	frametable_size = (max_page-min_page)*sizeof(struct page_info);
	frametable_phys_start = ALIGN_UP(heap_phys_end+1, PAGE_SIZE);
	frametable_phys_end = frametable_phys_start + frametable_size - 1;

	printk("Frame table start:       0x%lx (virt) / 0x%lx (phys)\n", (unsigned long) __va(frametable_phys_start), frametable_phys_start);
	printk("Frame table end:         0x%lx (virt) / 0x%lx (phys)\n", (unsigned long) __va(frametable_phys_end), frametable_phys_end);
	printk("Frame table size:        %d bytes\n", (int) frametable_size);
	printk("\n");

	io_map_current_base = (unsigned int) __va(ALIGN_UP(frametable_phys_end+1, TTB_SECT_SIZE));

	printk("I/O area (for ioremap):  0x%lx (virt)\n", (unsigned long) io_map_current_base);
	printk("I/O area size:           0x%lx bytes\n", (unsigned long) HYPERVISOR_VIRT_START+HYPERVISOR_SIZE-io_map_current_base);
	printk("\n");

	init_boot_pages(heap_phys_end+1, __pa(io_map_current_base));
	init_frametable();

	init_heap_pages(heap_phys_start, heap_phys_end);

	/* Now clearing the pte entries related to I/O area */
	for (addr = io_map_current_base; addr < HYPERVISOR_VIRT_START + HYPERVISOR_SIZE; addr += TTB_SECT_SIZE) {
		l1pte = (uint32_t *) l1pte_offset(__sys_l1pgtable, addr);
		*l1pte = 0;

		flush_pte_entry(l1pte);
	}
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

void dump_page(unsigned int pfn) {

	int i, j;
	unsigned int addr;

	addr = (pfn << 12);

	printk("%s: pfn %x\n\n", __func__,  pfn);

	for (i = 0; i < PAGE_SIZE; i += 16) {
		printk(" [%x]: ", i);
		for (j = 0; j < 16; j++) {
			printk("%02x ", *((unsigned char *) __lva(addr)));
			addr++;
		}
		printk("\n");
	}
}

void init_frametable(void)
{
	unsigned long p;
	unsigned long nr_pages;
	int i;

	nr_pages = PFN_UP((max_page-min_page)*sizeof(struct page_info));

	p = alloc_boot_pages(nr_pages, 1);

	if (p == 0)
		panic("Not enough memory for frame table\n");

	frame_table = (struct page_info *) phys_to_virt(p << PAGE_SHIFT);

	for (i = 0; i < nr_pages; i += 1)
		clear_page((void *) phys_to_virt(((p + i) << PAGE_SHIFT)));
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

void *ioremap(unsigned long phys_addr, unsigned int size) {
	uint32_t vaddr;
	unsigned int offset;

	/* Make sure the virtual address will be correctly aligned (either section or page aligned). */

	io_map_current_base = ALIGN_UP(io_map_current_base, ((size < SZ_1M) ? PAGE_SIZE : SZ_1M));

	/* Preserve a possible offset */
	offset = phys_addr & 0xfff;

	vaddr = (unsigned long) io_map_current_base;

	create_mapping(NULL, (unsigned long) io_map_current_base, phys_addr, size, true);

	io_map_current_base += size;

	return (void *) (vaddr + offset);

}
