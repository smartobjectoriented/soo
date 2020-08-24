/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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
#include <memory.h>

#include <mach/uart.h>

#include <asm/mmu.h>
#include <asm/cacheflush.h>

#include <generated/autoconf.h>

uint32_t *swapper_pg_dir;
uint32_t *l2pt_current_base;
unsigned long l2pt_phys_start;

unsigned int get_dacr(void)
{
	unsigned int domain;

	asm("mrc p15, 0, %0, c3, c0	@ get domain" : "=r" (domain) :);

	return domain;
}

void set_dacr(uint32_t val)
{
	asm volatile("mcr p15, 0, %0, c3, c0	@ set domain" : : "r" (val) : "memory");
	isb();
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


/* Reference to the system 1st-level page table */
static void alloc_init_pte(uint32_t *l1pte, unsigned long addr, unsigned long end, unsigned long pfn, bool nocache)
{
	uint32_t *l2pte;
	uint32_t size;

	if (!*l1pte) {
		size = L2_PAGETABLE_ENTRIES * sizeof(uint32_t);

		l2pte = get_l2_pgtable();
		ASSERT(l2pte != NULL);
		 
		memset(l2pte, 0, size);

		*l1pte = (__pa((uint32_t) l2pte) & L1DESC_L2PT_BASE_ADDR_MASK) | L1DESC_TYPE_PT;

		DBG("Allocating a L2 page table at %p in l1pte: %p with contents: %x\n", l2pte, l1pte, *l1pte);
	}

	l2pte = l2pte_offset(l1pte, addr);

	do {
		*l2pte = (pfn << PAGE_SHIFT) | L2DESC_SMALL_PAGE_AP01 | L2DESC_SMALL_PAGE_AP2 | L2DESC_PAGE_TYPE_SMALL;

		/*
		 * One word about the following attributes: on Vexpress, DESC_CACHE (DESC_CACHEABLE | DESC_BUFFERABLE) was
		 * good to run in kernel and user space. But with Rpi4, it is another story; it works well in the kernel space
		 * but executing instructions in other pages than the first one allocate to application code in user space
		 * led to an undefined instruction exception (reading/writing in these different pages were successful).
		 * It has been figured out by using DESC_CACHEABLE attribute only (BUFFERABLE is now disabled).
		 * It works on both Rpi4 and vExpress.
		 */
		*l2pte |= (nocache ? 0 : DESC_CACHE);

		*l2pte &= ~L1DESC_PT_DOMAIN_MASK;
		*l2pte |= PTE_DESC_DOMAIN_0;

		DBG("Setting l2pte %p with contents: %x\n", l2pte, *l2pte);

		pfn++;
	} while (l2pte++, addr += PAGE_SIZE, addr != end);
}

/*
 * Allocate a section (only L1 PTE) or page table (L1 & L2 page tables)
 * @nocache indicates if the page can be cache or not (true means no support for cached page)
 */
static void alloc_init_section(uint32_t *l1pte, uint32_t addr, uint32_t end, uint32_t phys, bool nocache)
{
	/*
	 * Try a section mapping - end, addr and phys must all be aligned
	 * to a section boundary.
	 */

	if (((addr | end | phys) & ~L1_SECT_MASK) == 0) {

		do {
			*l1pte = (phys & L1_SECT_MASK) | L1DESC_SECT_AP01 | L1DESC_SECT_AP2 | L1DESC_TYPE_SECT;

			*l1pte |= (nocache ? 0 : DESC_CACHE);

			*l1pte &= ~L1DESC_SECT_DOMAIN_MASK;
			*l1pte |= PTE_DESC_DOMAIN_0;

			DBG("Allocating a section at l1pte: %p content: %x\n", l1pte, *l1pte);

			phys += L1_SECT_SIZE;

		} while (l1pte++, addr += L1_SECT_SIZE, addr != end);


	} else {
		/*
		 * No need to loop; L2 pte's aren't interested in the
		 * individual L1 entries.
		 */
		alloc_init_pte(l1pte, addr, end, phys >> PAGE_SHIFT, nocache);
	}
}

/*
 * Create a static mapping between a virtual range and a physical range
 * @l1pgtable refers to the level 1 page table - if NULL, the system page table is used
 * @virt_base is the virtual address considered for this mapping
 * @phys_base is the physical address to be mapped
 * @size is the number of bytes to be mapped
 * @nocache is true if no cache (TLB) must be used (typically for I/O)
 */
void create_mapping(uint32_t *l1pgtable, uint32_t virt_base, uint32_t phys_base, uint32_t size, bool nocache) {

	uint32_t addr, end, length, next;
	uint32_t *l1pte;

	/* If l1pgtable is NULL, we consider the system page table (hence in our case, the hypervisor page table. */
	if (l1pgtable == NULL)
		l1pgtable = swapper_pg_dir;

	addr = virt_base & PAGE_MASK;
	length = ALIGN_UP(size + (virt_base & ~PAGE_MASK), PAGE_SIZE);

	l1pte = l1pte_offset(l1pgtable, addr);

	end = addr + length;

	do {
		next = pgd_addr_end(addr, end);

		alloc_init_section(l1pte, addr, next, phys_base, nocache);

		phys_base += next - addr;
		addr = next;

	} while (l1pte++, addr != end);

	flush_tlb_all();
}

/*
 * Initial configuration of system page table
 */
void configure_l1pgtable(uint32_t l1pgtable, uint32_t fdt_addr) {
	unsigned int i;
	uint32_t vaddr, paddr;
	uint32_t *__pgtable = (uint32_t *) l1pgtable;

	/* Empty the page table */
	for (i = 0; i < 4096; i++)
		__pgtable[i] = 0;

	/*
	 * The kernel mapping has to be done with "normal memory" attribute, i.e. using cacheable mappings.
	 * This is required for the use of ldrex/strex instructions in recent core such as Cortex-A72 (or armv8 in general).
	 * Otherwise, strex has weird behaviour -> updated memory resulting with the value of 1 in the destination register (failure).
	 */

	/* Create an identity mapping of 1 MB on running kernel so that the kernel code can go ahead right after the MMU on */
	*l1pte_offset(__pgtable, CONFIG_RAM_BASE) = CONFIG_RAM_BASE  | L1DESC_SECT_AP01 | L1DESC_SECT_AP2 | L1DESC_TYPE_SECT | DESC_CACHEABLE;

	/* Now, create a virtual mapping in the kernel space */
	for (vaddr = L_PAGE_OFFSET, paddr = CONFIG_RAM_BASE; ((vaddr < L_PAGE_OFFSET + CONFIG_RAM_SIZE) && (vaddr < CONFIG_HYPERVISOR_VIRT_ADDR));
		vaddr += L1_SECT_SIZE, paddr += L1_SECT_SIZE)
	{
		*l1pte_offset(__pgtable, vaddr) = paddr | L1DESC_SECT_AP01 | L1DESC_SECT_AP2 | L1DESC_TYPE_SECT | DESC_CACHEABLE;
	}

	/* Create the mapping of the hypervisor code area. */
	for (vaddr = CONFIG_HYPERVISOR_VIRT_ADDR, paddr = CONFIG_RAM_BASE; vaddr < CONFIG_HYPERVISOR_VIRT_ADDR + HYPERVISOR_SIZE; vaddr += L1_SECT_SIZE, paddr += L1_SECT_SIZE)
	{
		*l1pte_offset(__pgtable, vaddr) = paddr | L1DESC_SECT_AP01 | L1DESC_SECT_AP2 | L1DESC_TYPE_SECT | DESC_CACHEABLE;
	}

	/* Early mapping I/O for UART */
	__pgtable[UART_BASE >> L1_PAGETABLE_SHIFT] = (UART_BASE & L1_SECT_MASK) | L1DESC_SECT_AP01 | L1DESC_SECT_AP2 | L1DESC_TYPE_SECT;

	flush_all();
}

/*
 * Clear the L1 PTE used for mapping of a specific virtual address.
 */
void clear_l1pte(uint32_t *l1pgtable, uint32_t vaddr) {
	uint32_t *l1pte;

	/* If l1pgtable is NULL, we consider the system page table */
	if (l1pgtable == NULL)
		l1pgtable = swapper_pg_dir;

	l1pte = l1pte_offset(l1pgtable, vaddr);

	*l1pte = 0;
}

/*
 * Switch the MMU to a L1 page table
 */
void mmu_switch(uint32_t *l1pgtable) {

	__mmu_switch(__pa((uint32_t) l1pgtable));

	flush_all();
}

/* Duplicate the kernel area by doing a copy of L1 PTEs from the system page table */
void pgtable_copy_kernel_area(uint32_t *l1pgtable) {
	int i1;

	for (i1 = L_PAGE_OFFSET >> L1_PAGETABLE_SHIFT; i1 < L1_PAGETABLE_ENTRIES; i1++)
		l1pgtable[i1] = ((uint32_t *) swapper_pg_dir)[i1];
}

void dump_pgtable(uint32_t *l1pgtable) {

	int i, j;
	uint32_t *l1pte, *l2pte;

	lprintk("           ***** Page table dump *****\n");

	for (i = 0; i < L1_PAGETABLE_ENTRIES; i++) {
		l1pte = l1pgtable + i;
		if (*l1pte) {
			if ((*l1pte & L1DESC_TYPE_MASK) == L1DESC_TYPE_SECT)
				lprintk(" - L1 pte@%p (idx %x) mapping %x is section type  content: %x\n", l1pgtable+i, i, i << L1_PAGETABLE_SHIFT, *l1pte);
			else
				lprintk(" - L1 pte@%p (idx %x) is PT type   content: %x\n", l1pgtable+i, i, *l1pte);

			if ((*l1pte & L1DESC_TYPE_MASK) == L1DESC_TYPE_PT) {
				for (j = 0; j < 256; j++) {
					l2pte = ((uint32_t *) __va(*l1pte & L1DESC_L2PT_BASE_ADDR_MASK)) + j;
					if (*l2pte)
						lprintk("      - L2 pte@%p (i2=%x) mapping %x  content: %x\n", l2pte, j, (i << 20) | (j << 12), *l2pte);
				}
			}
		}
	}
}

void dump_current_pgtable(void) {
	dump_pgtable((uint32_t *) cpu_get_l1pgtable());
}

/*
 * Flush all caches
 */
void flush_all(void) {
	flush_tlb_all();
	cache_clean_flush();
}

/*
 * Get the physical address from a virtual address (valid for any virt. address).
 * The function reads the page table(s).
 */
uint32_t virt_to_phys_pt(uint32_t vaddr) {
	uint32_t *l1pte, *l2pte;
	uint32_t offset;

	/* Get the L1 PTE. */
	l1pte = l1pte_offset(get_l2_pgtable(), vaddr);

	BUG_ON(!*l1pte);
	if ((*l1pte & L1DESC_TYPE_MASK) == L1DESC_TYPE_SECT) {

		offset = vaddr & ~L1_SECT_MASK;

		return (*l1pte & L1_SECT_MASK) | offset;

	} else {

		offset = vaddr & ~PAGE_MASK;

		l2pte = l2pte_offset(l1pte, vaddr);

		return (*l2pte & L2DESC_SMALL_PAGE_ADDR_MASK) | offset;
	}

}

void vectors_init(void) {

	extern char __stubs_start[], __stubs_end[];
	extern char __vectors_start[], __vectors_end[];
	void *vectors_page;

	memset(&pseudo_usr_mode, 0, NR_CPUS * sizeof(unsigned int));

	/* Allocate a page for the vectors page */
	vectors_page = alloc_heap_page();
	BUG_ON(!vectors_page);

	create_mapping(NULL, VECTORS_BASE, virt_to_phys(vectors_page), PAGE_SIZE, false);

	flush_all();

	memcpy(vectors_page, __vectors_start, __vectors_end - __vectors_start);
	memcpy(vectors_page + 0x200, __stubs_start, __stubs_end - __stubs_start);

	flush_icache_range((unsigned long) vectors_page, (unsigned long) vectors_page + PAGE_SIZE);

}


