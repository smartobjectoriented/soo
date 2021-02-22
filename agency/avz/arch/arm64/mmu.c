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
#include <string.h>
#include <heap.h>

#include <device/fdt.h>

#include <mach/uart.h>

#include <asm/mmu.h>
#include <asm/cacheflush.h>

#include <generated/autoconf.h>


void get_current_addrspace(addrspace_t *addrspace) {
	int cpu;

	cpu = smp_processor_id();
#if 0
	/* Get the current state of MMU */
	addrspace->ttbr0[cpu] = READ_CP32(TTBR0_32);
	addrspace->pgtable_paddr = addrspace->ttbr0[cpu] & TTBR0_BASE_ADDR_MASK;
#endif
}

/*
 * Check if two address space are identical regarding the MMU configuration.
 */
bool is_addrspace_equal(addrspace_t *addrspace1, addrspace_t *addrspace2) {
	return (addrspace1->pgtable_paddr == addrspace2->pgtable_paddr);
}

#if 0

/* Reference to the system 1st-level page table */
static void alloc_init_pte(uint32_t *l1pte, unsigned long addr, unsigned long end, unsigned long pfn, bool nocache)
{
	uint32_t *l2pte, *l2pgtable;
	uint32_t size;

	size = TTB_L2_ENTRIES * sizeof(uint32_t);
	
	if (!*l1pte) {

		l2pte = get_l2_pgtable();
		ASSERT(l2pte != NULL);
		 
		memset(l2pte, 0, size);

		*l1pte =__pa((uint32_t) l2pte);

		set_l1_pte_page_dcache(l1pte, (nocache ? L1_PAGE_DCACHE_OFF : L1_PAGE_DCACHE_WRITEALLOC));

		flush_pte_entry(l1pte);

		DBG("Allocating a L2 page table at %p in l1pte: %p with contents: %x\n", l2pte, l1pte, *l1pte);

	}

	l2pgtable = (uint32_t *) __va((*l1pte & TTB_L1_PAGE_ADDR_MASK));

	l2pte = l2pte_offset(l1pte, addr);

	do {

		*l2pte = pfn << PAGE_SHIFT;

		set_l2_pte_dcache(l2pte, (nocache ? L2_DCACHE_OFF : L2_DCACHE_WRITEALLOC));

		flush_pte_entry(l2pte);

		DBG("Setting l2pte %p with contents: %x\n", l2pte, *l2pte);

		pfn++;

	} while (l2pte++, addr += PAGE_SIZE, addr != end);

	mmu_page_table_flush((uint32_t) l2pgtable, (uint32_t) (l2pgtable + TTB_L2_ENTRIES));
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

	if (((addr | end | phys) & ~TTB_SECT_MASK) == 0) {

		do {
			*l1pte = phys;

			set_l1_pte_sect_dcache(l1pte, (nocache ? L1_SECT_DCACHE_OFF : L1_SECT_DCACHE_WRITEALLOC));
			DBG("Allocating a section at l1pte: %p content: %x\n", l1pte, *l1pte);

			flush_pte_entry(l1pte);

			phys += TTB_SECT_SIZE;

		} while (l1pte++, addr += TTB_SECT_SIZE, addr != end);

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

	/* If l1pgtable is NULL, we consider the system page table */
	if (l1pgtable == NULL)
		l1pgtable = __sys_l1pgtable;

	addr = virt_base & PAGE_MASK;
	length = ALIGN_UP(size + (virt_base & ~PAGE_MASK), PAGE_SIZE);

	l1pte = l1pte_offset(l1pgtable, addr);

	end = addr + length;

	do {
		next = l1sect_addr_end(addr, end);

		alloc_init_section(l1pte, addr, next, phys_base, nocache);

		phys_base += next - addr;
		addr = next;

	} while (l1pte++, addr != end);

	mmu_page_table_flush((uint32_t) l1pgtable, (uint32_t) (l1pgtable + TTB_L1_ENTRIES));
}

#endif

static void alloc_init_l3(u64 *l2pte, addr_t addr, addr_t end, addr_t phys, bool nocache)
{
	u64 *l3pte, *pte;
	u64 *l3pgtable;

	/* L3 page table already exist? */
	if (!*l2pte) {

		/* A L1 table must be created */
		l3pgtable = (u64 *) memalign(TTB_L3_SIZE, PAGE_SIZE);
		BUG_ON(!l3pgtable);

		memset(l3pgtable, 0, TTB_L3_SIZE);

		/* Attach the L2 PTE to this L3 page table */
		*l2pte = __pa((addr_t) l3pgtable)  & TTB_L2_TABLE_ADDR_MASK;

		set_pte_table(l2pte, DCACHE_WRITEALLOC);

		DBG("Allocating a L3 page table at %p in l2pte: %p with contents: %x\n", l3pte, l2pte, *l2pte);
	}

	l3pte = l3pte_offset(l2pte, addr);

	do {
		*l3pte = phys & TTB_L3_PAGE_ADDR_MASK;

		set_pte_page(l3pte, (nocache ? DCACHE_OFF : DCACHE_WRITEALLOC));

		DBG("Setting the PTE at (l3pte): %p content: %x\n", l3pte, *l3pte);

		flush_pte_entry(addr, l3pte);

		phys += PAGE_SIZE;

	} while (l3pte++, addr += PAGE_SIZE, addr != end);
}

static void alloc_init_l2(u64 *l1pte, addr_t addr, addr_t end, addr_t phys, bool nocache)
{
	u64 *l2pte, *l3pte;
	u64 *l2pgtable;
	addr_t next;

	/* L2 page table already exist? */
	if (!*l1pte) {

		/* A L1 table must be created */
		l2pgtable = (u64 *) memalign(TTB_L2_SIZE, PAGE_SIZE);
		BUG_ON(!l2pgtable);

		memset(l2pgtable, 0, TTB_L2_SIZE);

		/* Attach the L1 PTE to this L2 page table */
		*l1pte = __pa((addr_t) l2pgtable)  & TTB_L1_TABLE_ADDR_MASK;

		set_pte_table(l1pte, DCACHE_WRITEALLOC);

		DBG("Allocating a L2 page table at %p in l1pte: %p with contents: %x\n", l2pte, l1pte, *l1pte);
	}

	l2pte = l2pte_offset(l1pte, addr);

	do {
		next = l2_addr_end(addr, end);

		if (((addr | next | phys) & ~BLOCK_2M_MASK) == 0) {

			do {
				*l2pte = phys & TTB_L2_BLOCK_ADDR_MASK;

				set_pte_block(l2pte, (nocache ? DCACHE_OFF : DCACHE_WRITEALLOC));

				DBG("Allocating a 2MB block at l2pte: %p content: %x\n", l2pte, *l2pte);

				flush_pte_entry(addr, l2pte);

				phys += SZ_2M;

			} while (l2pte++, addr += SZ_2M, addr != next);

		} else
			alloc_init_l3(l2pte, addr, next, phys, nocache);

		phys += next - addr;
		addr = next;

	} while (l2pte++, addr != end);

}

static void alloc_init_l1(u64 *l0pte, addr_t addr, addr_t end, addr_t phys, bool nocache)
{
	u64 *l1pte, *l2pte;
	u64 *l1pgtable;
	addr_t next;

	/* L1 page table already exist? */
	if (!*l0pte) {

		/* A L1 table must be created */
		l1pgtable = (u64 *) memalign(TTB_L1_SIZE, PAGE_SIZE);
		BUG_ON(!l1pgtable);

		memset(l1pgtable, 0, TTB_L1_SIZE);

		/* Attach the L0 PTE to this L1 page table */
		*l0pte = __pa((addr_t) l1pgtable)  & TTB_L0_TABLE_ADDR_MASK;

		set_pte_table(l0pte, DCACHE_WRITEALLOC);

		DBG("Allocating a L1 page table at %p in l0pte: %p with contents: %x\n", l1pgtable, l0pte, *l0pte);
	}

	l1pte = l1pte_offset(l0pte, addr);

	do {
		next = l1_addr_end(addr, end);

		if (((addr | next | phys) & ~BLOCK_1G_MASK) == 0) {

			do {
				*l1pte = phys & TTB_L1_BLOCK_ADDR_MASK;

				set_pte_block(l1pte, (nocache ? DCACHE_OFF : DCACHE_WRITEALLOC));

				DBG("Allocating a 1 GB block at l1pte: %p content: %x\n", l1pte, *l1pte);

				flush_pte_entry(addr, l1pte);

				phys += SZ_1G;

			} while (l1pte++, addr += SZ_1G, addr != next);

		} else
			alloc_init_l2(l1pte, addr, next, phys, nocache);

		phys += next - addr;
		addr = next;

	} while (l1pte++, addr != end);

}

/*
 * Create a static mapping between a virtual range and a physical range
 * @l0pgtable refers to the level 0 page table - if NULL, the system page table is used
 * @virt_base is the virtual address considered for this mapping
 * @phys_base is the physical address to be mapped
 * @size is the number of bytes to be mapped
 * @nocache is true if no cache (TLB) must be used (typically for I/O)
 *
 * This function tries to do the minimal mapping, i.e. using a number of page tables as low as possible, depending
 * on the granularity of mapping. In such a configuration, the function tries to map 1 GB first, then 2 MB, and finally 4 KB.
 * Mapping of blocks at L0 level is not allowed with 4 KB granule (AArch64).
 *
 */
void create_mapping(u64 *l0pgtable, addr_t virt_base, addr_t phys_base, u64 size, bool nocache) {
	addr_t addr, end, length, next;
	u64 *l0pte;

	/* If l0pgtable is NULL, we consider the system page table */
	if (l0pgtable == NULL)
		l0pgtable = __sys_l0pgtable;

	addr = virt_base & PAGE_MASK;
	length = ALIGN_UP(size + (virt_base & ~PAGE_MASK), PAGE_SIZE);

	l0pte = l0pte_offset(l0pgtable, addr);

	end = addr + length;

	do {
		next = l0_addr_end(addr, end);

		alloc_init_l1(l0pte, addr, next, phys_base, nocache);

		phys_base += next - addr;
		addr = next;

	} while (l0pte++, addr != end);

	mmu_page_table_flush((addr_t) l0pgtable, (addr_t) (l0pgtable + TTB_L0_ENTRIES));
}

void release_mapping(u64 *pgtable, addr_t virt_base, addr_t size) {

#if 0
	uint32_t addr, end, length, next;
	uint32_t *l1pte;

	/* If l1pgtable is NULL, we consider the system page table */
	if (pgtable == NULL)
		pgtable = __sys_l1pgtable;

	addr = virt_base & PAGE_MASK;
	length = ALIGN_UP(size + (virt_base & ~PAGE_MASK), PAGE_SIZE);

	l1pte = l1pte_offset(pgtable, addr);

	end = addr + length;

	do {
		next = l1sect_addr_end(addr, end);

		free_l1_mapping(l1pte, addr, next);

		addr = next;

	} while (l1pte++, addr != end);
#endif
}

/*
 * Allocate a new L1 page table. Return NULL if it fails.
 * The page table must be 16-KB aligned.
 */
u64 *new_sys_pgtable(void) {
	u64 *pgtable;

	pgtable = memalign(TTB_L0_SIZE, PAGE_SIZE);
	if (!pgtable) {
		printk("%s: heap overflow...\n", __func__);
		kernel_panic();
	}

	/* Empty the page table */
	memset(pgtable, 0, TTB_L0_SIZE);

	return pgtable;
}

/*
 * Initial configuration of system page table
 */
void mmu_configure(addr_t fdt_addr) {
	addr_t vaddr, paddr;
	unsigned int i;
	int temp;

	icache_disable();
	dcache_disable();

	/* The initial page table is only set by CPU #0 (AGENCY_CPU).
	 * The secondary CPUs use the same page table.
	 */

	if (smp_processor_id() == AGENCY_CPU) {

		/* Empty the page table */
		memset((void *) __sys_l0pgtable, 0, TTB_L0_SIZE);
		memset((void *) __sys_idmap_l1pgtable, 0, TTB_L1_SIZE);
		memset((void *) __sys_linearmap_l1pgtable, 0, TTB_L1_SIZE);

		/* Create an identity mapping of 1 GB on running kernel so that the kernel code can go ahead right after the MMU on */
		__sys_l0pgtable[l0pte_index(CONFIG_RAM_BASE)] = (u64) __sys_idmap_l1pgtable & TTB_L0_TABLE_ADDR_MASK;
		set_pte_table(&__sys_l0pgtable[l0pte_index(CONFIG_RAM_BASE)], DCACHE_WRITEALLOC);

		__sys_idmap_l1pgtable[l1pte_index(CONFIG_RAM_BASE)] = CONFIG_RAM_BASE & TTB_L1_BLOCK_ADDR_MASK;
		set_pte_block(&__sys_idmap_l1pgtable[l1pte_index(CONFIG_RAM_BASE)], DCACHE_WRITEALLOC);

#if 0

		/* Now, create a virtual mapping in the kernel space */

		for (vaddr = L_PAGE_OFFSET, paddr = CONFIG_RAM_BASE; ((vaddr < L_PAGE_OFFSET + CONFIG_RAM_SIZE) && (vaddr < CONFIG_HYPERVISOR_VIRT_ADDR));
				vaddr += TTB_SECT_SIZE, paddr += TTB_SECT_SIZE)
		{
			*l1pte_offset(__pgtable, vaddr) = paddr;
			set_l1_pte_sect_dcache(l1pte_offset(__pgtable, vaddr), L1_SECT_DCACHE_WRITEALLOC);

		}
#endif

		/* Create the mapping of the hypervisor code area. */

		__sys_l0pgtable[l0pte_index(CONFIG_HYPERVISOR_VIRT_ADDR)] = (u64) __sys_linearmap_l1pgtable & TTB_L0_TABLE_ADDR_MASK;
		set_pte_table(&__sys_l0pgtable[l0pte_index(CONFIG_HYPERVISOR_VIRT_ADDR)], DCACHE_WRITEALLOC);

		__sys_linearmap_l1pgtable[l1pte_index(CONFIG_HYPERVISOR_VIRT_ADDR)] = CONFIG_RAM_BASE & TTB_L1_BLOCK_ADDR_MASK;
		set_pte_block(&__sys_linearmap_l1pgtable[l1pte_index(CONFIG_HYPERVISOR_VIRT_ADDR)], DCACHE_WRITEALLOC);

		/* Early mapping I/O for UART. Here, the UART is supposed to be in a different L1 entry than the RAM. */

		__sys_idmap_l1pgtable[l1pte_index(UART_BASE)] = UART_BASE & TTB_L1_BLOCK_ADDR_MASK;
		set_pte_block(&__sys_idmap_l1pgtable[l1pte_index(UART_BASE)], DCACHE_OFF);
	}

	mmu_setup(__sys_l0pgtable);

	dcache_enable();
	icache_enable();

	if (smp_processor_id() == AGENCY_CPU) {
		/* The device tree is visible in the L_PAGE_OFFSET area */
		fdt_vaddr = (uint32_t *) __lva(fdt_addr);
	}
}

#if 0
/*
 * Clear the L1 PTE used for mapping of a specific virtual address.
 */
void clear_l1pte(uint32_t *l1pgtable, uint32_t vaddr) {
	uint32_t *l1pte;

	/* If l1pgtable is NULL, we consider the system page table */
	if (l1pgtable == NULL)
		l1pgtable = __sys_l1pgtable;

	l1pte = l1pte_offset(l1pgtable, vaddr);

	*l1pte = 0;

	flush_pte_entry(l1pte);
}

#endif

/*
 * Switch the MMU to a L0 page table.
 * We *only* use ttbr1 when dealing with our hypervisor which is located in a kernel space area,
 * i.e. starting with 0xffff.... So ttbr0 is not used as soon as the id mapping in the RAM
 * is not necessary anymore.
 */
void mmu_switch(addrspace_t *aspace) {
	flush_dcache_all();

	__mmu_switch(aspace->ttbr1[smp_processor_id()]);

	invalidate_icache_all();
	__asm_invalidate_tlb_all();

}

#if 0

/* Duplicate the kernel area by doing a copy of L1 PTEs from the system page table */
void pgtable_copy_kernel_area(uint32_t *l1pgtable) {
	int i1;

	for (i1 = l1pte_index(L_PAGE_OFFSET); i1 < TTB_L1_ENTRIES; i1++)
		l1pgtable[i1] = __sys_l1pgtable[i1];

	mmu_page_table_flush((uint32_t) l1pgtable, (uint32_t) (l1pgtable + TTB_L1_ENTRIES));
}

void dump_pgtable(uint32_t *l1pgtable) {

	int i, j;
	uint32_t *l1pte, *l2pte;

	lprintk("           ***** Page table dump *****\n");

	for (i = 0; i < TTB_L1_ENTRIES; i++) {
		l1pte = l1pgtable + i;
		if (*l1pte) {
			if (l1pte_is_sect(*l1pte))
				lprintk(" - L1 pte@%p (idx %x) mapping %x is section type  content: %x\n", l1pgtable+i, i, i << (32 - TTB_L1_ORDER), *l1pte);
			else
				lprintk(" - L1 pte@%p (idx %x) is PT type   content: %x\n", l1pgtable+i, i, *l1pte);

			if (!l1pte_is_sect(*l1pte)) {

				for (j = 0; j < 256; j++) {
					l2pte = ((uint32_t *) __va(*l1pte & TTB_L1_PAGE_ADDR_MASK)) + j;
					if (*l2pte)
						lprintk("      - L2 pte@%p (i2=%x) mapping %x  content: %x\n", l2pte, j, pte_index_to_vaddr(i, j), *l2pte);
				}
			}
		}
	}
}

void dump_current_pgtable(void) {
	dump_pgtable((uint32_t *) cpu_get_l1pgtable());
}

/*
 * Get the physical address from a virtual address (valid for any virt. address).
 * The function reads the page table(s).
 */
uint32_t virt_to_phys_pt(uint32_t vaddr) {
	uint32_t *l1pte, *l2pte;
	uint32_t offset;
	addrspace_t current_addrspace;

	get_current_addrspace(&current_addrspace);

	/* Get the L1 PTE. */
	l1pte = l1pte_offset((uint32_t *) current_addrspace.pgtable_vaddr, vaddr);

	offset = vaddr & ~PAGE_MASK;
	BUG_ON(!*l1pte);
	if (l1pte_is_sect(*l1pte)) {

		return (*l1pte & TTB_L1_SECT_ADDR_MASK) | offset;

	} else {

		l2pte = l2pte_offset(l1pte, vaddr);

		return (*l2pte & TTB_L2_ADDR_MASK) | offset;
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

	memcpy(vectors_page, __vectors_start, __vectors_end - __vectors_start);
	memcpy(vectors_page + 0x200, __stubs_start, __stubs_end - __stubs_start);

	flush_dcache_range((unsigned long) vectors_page, (unsigned long) vectors_page + PAGE_SIZE);

}

#endif
