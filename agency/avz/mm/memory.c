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

#include <avz/config.h>
#include <avz/mm.h>
#include <avz/percpu.h>
#include <avz/kernel.h>
#include <avz/sched.h>
#include <avz/errno.h>
#include <avz/sched.h>
#include <avz/init.h>
#include <avz/bitmap.h>

#include <asm/linkage.h>
#include <asm/init.h>
#include <asm/page.h>
#include <asm/mach/map.h>
#include <asm/string.h>
#include <asm/debugger.h>
#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
#include <asm/proc-fns.h>
#include <asm/cpu-ops.h>

#include <asm/processor.h>
#include <asm/cpregs.h>

#include <asm/mm.h>
#include <asm/memslot.h>

#include <soo/uapi/arch-arm.h>

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
unsigned long l2pt_phys_start;

unsigned int io_map_current_base;
void *l2pt_current_base;

/*
 * Flush all tlbs and I-/D-cache
 */
void flush_all(void) {
	flush_cache_all();
	flush_tlb_all();
}
/*
 * Perform basic memory initialization, i.e. the existing memory slot.
 */
void early_memory_init(void) {
  unsigned int i;

	/* Hypervisor */
	memslot[0].base_paddr = CONFIG_RAM_BASE;
	memslot[0].size = HYPERVISOR_SIZE;
	memslot[0].busy = true;

	/* The memslot[1] is reserved for the agency (two domains although one binary image only.
	 * It will become busy during the agency construct operation.
	 */

	for (i = 0; i < MAX_ME_DOMAINS; i++)
		memslot[i+2].busy = false;
}

/*
 * AVZ Memory initialization - Initialize the heap, frame table and I/O-mapped area.
 */
void memory_init(void)
{
	ulong addr, frametable_phys_start, frametable_phys_end, frametable_size;
	unsigned long nxhp;
	pde_t *pde;

	max_page = (__pa(HYPERVISOR_VIRT_START) + HYPERVISOR_SIZE) >> PAGE_SHIFT;
	min_page = __pa(&_end) >> PAGE_SHIFT;

#warning TO DOCUMENT

	/*
	 * Prepare the memory area devoted to the heap. We align the heap on section boundary so that
	 * we can deal with different cache attributes (no cache).
	 */
	l2pt_phys_start = (SECTION_UP(__pa(&_end)) + 1) << PGD_SHIFT;
	l2pt_current_base = __va(l2pt_phys_start);

	heap_phys_start = l2pt_phys_start + SZ_1M;

	/* Must pass a single mapped page for populating bootmem_region_list. */
	init_boot_pages(__pa(&_end), heap_phys_start);

	heap_phys_end = heap_phys_start + (HEAP_MAX_SIZE_MB << PGD_SHIFT) - 1;
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

	io_map_current_base = (unsigned int) __va(ALIGN_UP(frametable_phys_end+1, SECTION_SIZE));

	printk("I/O area (for ioremap):  0x%lx (virt)\n", (unsigned long) io_map_current_base);
	printk("I/O area size:           0x%lx bytes\n", (unsigned long) HYPERVISOR_VIRT_START+HYPERVISOR_SIZE-io_map_current_base);
	printk("\n");

	init_boot_pages(heap_phys_end+1, __pa(io_map_current_base));
	init_frametable();

	init_heap_pages(heap_phys_start, heap_phys_end);

	/* Now clearing the pmd entries related to I/O area */
	for (addr = io_map_current_base; addr < HYPERVISOR_VIRT_START+HYPERVISOR_SIZE; addr += (1 << PGD_SHIFT)) {
		pde = (pde_t *) (swapper_pg_dir + pde_index(addr));
		pmd_clear(pde);
	}

}

/*
 * Get a virtual address to store a L2 page table (256 bytes).
 */
void *get_l2_pgtable(void) {
	void *l2pt_vaddr;

	l2pt_vaddr = l2pt_current_base;
	l2pt_current_base += PAGE_SIZE;   /* One page per page table */

	return l2pt_vaddr;
}

#if 0 /* Not used at the moment */
/*
 * Make the heap non cacheable (shared page info, etc.)
 */
void make_heap_noncacheable(void) {
	struct map_desc map;
	ulong addr;
	pde_t *pde;

	/* Clear the pmd related to the heap sections */
	for (addr = (unsigned int) __va(heap_phys_start); addr < (unsigned int) __va(heap_phys_start) + (HEAP_MAX_SIZE_MB << PGD_SHIFT); addr += (1 << PGD_SHIFT)) {
		pde = (pde_t *) (swapper_pg_dir + pde_index(addr));
		pmd_clear(pde);
	}

	/* Remap the heap with new attributes */
	map.pfn = phys_to_pfn(heap_phys_start);
	map.virtual = (unsigned long) __va(heap_phys_start); /* Keep the original vaddr */
	map.length = HEAP_MAX_SIZE_MB << PGD_SHIFT;

	map.type = MT_MEMORY_RWX_NONCACHED;

	create_mapping(&map, NULL);
}
#endif

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

void __init init_frametable(void)
{
	unsigned long p;
	unsigned long nr_pages;
	int i;

	nr_pages = PFN_UP((max_page-min_page)*sizeof(struct page_info));

	p = alloc_boot_pages(nr_pages, 1);

	if (p == 0)
		panic("Not enough memory for frame table\n");

	frame_table = (struct page_info *) maddr_to_virt(p << PAGE_SHIFT);

	for (i = 0; i < nr_pages; i += 1)
		clear_page((void *)maddr_to_virt(((p + i) << PAGE_SHIFT)));
}

void write_ptbase(struct vcpu *v)
{
	flush_tlb_all();
	flush_cache_all();
	cpu_do_switch_mm(pagetable_get_paddr(v->arch.guest_table));

	flush_tlb_all();
	flush_cache_all();
}


/*
 * switch_mm() is used to perform a memory context switch.
 * If prev is NULL, the switch is done between the current VCPU and next.
 * If prev is not NULL, the current VCPU will continue to execute within the memory context of next (VCPU), but
 * it is assumed that there is no VCPU context switch.
 * If next is NULL, next will be associated to the current VCPU ("back to home").
 */
void switch_mm(struct vcpu *prev, struct vcpu *next) {

	if (prev == NULL)
		prev = current;

	/* Preserve the current configuration of MMU registers of the running domain before doing a switch */
	prev->arch.guest_context.sys_regs.guest_context_id = READ_CP32(CONTEXTIDR);

	prev->arch.guest_context.sys_regs.guest_ttbr0 = READ_CP32(TTBR0_32);
	prev->arch.guest_context.sys_regs.guest_ttbr1 = READ_CP32(TTBR1_32);
	prev->arch.guest_context.sys_regs.guest_ttbcr = READ_CP32(TTBCR);

	dmb();


	if (next == NULL)
		next = current;

	flush_cache_all();
	flush_tlb_all();

	WRITE_CP32(next->arch.guest_context.sys_regs.guest_context_id, CONTEXTIDR);
	WRITE_CP32(next->arch.guest_context.sys_regs.guest_ttbr1, TTBR1_32);
	WRITE_CP32(next->arch.guest_context.sys_regs.guest_ttbcr, TTBCR);

	dmb();

	cpu_do_switch_mm(next->arch.guest_context.sys_regs.guest_ttbr0);  /* This is the page table ! */

	flush_cache_all();
	flush_tlb_all();

}

void save_ptbase(struct vcpu *v)
{
	unsigned long offset_p_v;
	unsigned long ttb_address = virt_to_phys(cpu_get_pgd());

	v->arch.guest_table.pfn   = ttb_address >> PAGE_SHIFT;
	offset_p_v = v->arch.guest_pstart - v->arch.guest_vstart;
	ttb_address = ttb_address - (unsigned long)offset_p_v;

	v->arch.guest_vtable = (pte_t *)ttb_address;

}

#define find_first_set_bit(word) (ffs(word)-1)

extern struct domain *alloc_domain(domid_t domid);
extern struct domain *alloc_domain_struct(void);
