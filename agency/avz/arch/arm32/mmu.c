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

void *__current_pgtable = NULL;

void *__sys_root_pgtable;

void *current_pgtable(void) {
	return __current_pgtable;
}

unsigned int get_ttbr0(void) {
	unsigned int ttbr0;

	asm("mrc	p15, 0, %0, c2, c0, 1	@ get ttbr0" : "=r" (ttbr0) :);

	return ttbr0;
}

/**
 * Retrieve the current physical address of the page table
 *
 * @param pgtable_paddr
 */
void get_current_pgtable(addr_t *pgtable_paddr) {
	int cpu;

	cpu = smp_processor_id();

	*pgtable_paddr = cpu_get_ttbr0();
}

/* Reference to the system 1st-level page table */
static void alloc_init_pte(uint32_t *l1pte, addr_t addr, addr_t end, addr_t pfn, bool nocache)
{
	uint32_t *l2pte, *l2pgtable;
	uint32_t size;

	size = TTB_L2_ENTRIES * sizeof(uint32_t);
	
	if (!*l1pte) {
	
		l2pte = memalign(size, SZ_1K);
		ASSERT(l2pte != NULL);
		 
		memset(l2pte, 0, size);

		*l1pte =__pa((uint32_t) l2pte);
		set_l1_pte_page_dcache(l1pte, (nocache ? L1_PAGE_DCACHE_OFF : L1_PAGE_DCACHE_WRITEALLOC));

		DBG("Allocating a L2 page table at %p in l1pte: %p with contents: %x\n", l2pte, l1pte, *l1pte);

	}

	l2pgtable = (uint32_t *) __va(*l1pte & TTB_L1_PAGE_ADDR_MASK);

	l2pte = l2pte_offset(l1pte, addr);

	do {

		*l2pte = pfn << PAGE_SHIFT;

		set_l2_pte_dcache(l2pte, (nocache ? L2_DCACHE_OFF : L2_DCACHE_WRITEALLOC));

		DBG("Setting l2pte %p with contents: %x\n", l2pte, *l2pte);

		pfn++;

	} while (l2pte++, addr += PAGE_SIZE, addr != end);

	mmu_page_table_flush((uint32_t) l2pgtable, (uint32_t) (l2pgtable + TTB_L2_ENTRIES));
}

/*
 * Allocate a section (only L1 PTE) or page table (L1 & L2 page tables)
 * @nocache indicates if the page can be cache or not (true means no support for cached page)
 */
static void alloc_init_section(uint32_t *l1pte, addr_t addr, addr_t end, addr_t phys, bool nocache)
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
 *
 * @l1pgtable refers to the level 1 page table - if NULL, the system page table is used
 * @virt_base is the virtual address considered for this mapping
 * @phys_base is the physical address to be mapped
 * @size is the number of bytes to be mapped
 * @nocache is true if no cache (TLB) must be used (typically for I/O)
 */
void create_mapping(void *l1pgtable, addr_t virt_base, addr_t phys_base, uint32_t size, bool nocache) {

	addr_t addr, end, length, next;
	uint32_t *l1pte;

	/* If l1pgtable is NULL, we consider the system page table */
	if (l1pgtable == NULL)
		l1pgtable = __sys_root_pgtable;

	addr = virt_base & PAGE_MASK;
	length = ALIGN_UP(size + (virt_base & ~PAGE_MASK), PAGE_SIZE);

	l1pte = l1pte_offset((uint32_t *) l1pgtable, addr);

	end = addr + length;

	do {
		next = l1sect_addr_end(addr, end);

		alloc_init_section(l1pte, addr, next, phys_base, nocache);

		phys_base += next - addr;
		addr = next;

	} while (l1pte++, addr != end);

	/* Invalidate TLBs whenever the mapping is applied on the current page table.
	 * In other cases, the memory context switch will invalidate anyway.
	 */
	if (l1pgtable == __sys_root_pgtable)
		v7_inval_tlb();
}

/*
 * Allocate a new L1 page table. Return NULL if it fails.
 * The page table must be 16-KB aligned.
 */
void *new_root_pgtable(void) {
	uint32_t *pgtable;

	pgtable = memalign(4 * TTB_L1_ENTRIES, SZ_16K);
	if (!pgtable) {
		printk("%s: heap overflow...\n", __func__);
		kernel_panic();
	}

	/* Empty the page table */
	memset(pgtable, 0, 4 * TTB_L1_ENTRIES);

	return pgtable;
}

/**
 * Replace the current page table with a new one. This is used
 * typically during the initialization to have a better granulated
 * memory mapping.
 *
 * @param pgtable
 */
void replace_current_pgtable_with(void *pgtable) {

	/*
	 * Switch to the temporary page table in order to re-configure the original system page table
	 * Warning !! After the switch, we do not have any mapped I/O until the driver core gets initialized.
	 */

	mmu_switch((void *) __pa(pgtable));

	/* Re-configuring the original system page table */
	memcpy((void *) __sys_root_pgtable, (unsigned char *) pgtable, TTB_L1_SIZE);

	/* Finally, switch back to the original location of the system page table */
	mmu_switch((void *) __pa(__sys_root_pgtable));
}

/* Empty the corresponding l2 entries */
static void free_l2_mapping(uint32_t *l1pte, addr_t addr, addr_t end) {
	uint32_t *l2pte, *pgtable;
	int i;
	bool found;

	pgtable = l2pte_first(l1pte);

	l2pte = l2pte_offset(l1pte, addr);

	do {
		DBG("Re-setting l2pte to 0: %p\n", l2pte);

		*l2pte = 0; /* Free this entry */

	} while (l2pte++, addr += PAGE_SIZE, addr != end);

	mmu_page_table_flush((uint32_t) pgtable, (uint32_t) (pgtable + TTB_L2_ENTRIES));

	for (i = 0, found = false, l2pte = l2pte_first(l1pte); !found && (i < TTB_L2_ENTRIES); i++)
		found = (*(l2pte + i) != 0);

	if (!found) {

		free(l2pte); /* Remove the L2 page table since all no entry is mapped */

		*l1pte = 0; /* Free the L1 entry as well */

		flush_pte_entry(l1pte);
	}

}

/* Empty the corresponding l1 entries */
static void free_l1_mapping(uint32_t *l1pte, addr_t addr, addr_t end) {
	uint32_t *__l1pte = l1pte;

	/*
	 * Try a section mapping - end, addr and phys must all be aligned
	 * to a section boundary.
	 */
	if (((addr | end) & ~TTB_SECT_MASK) == 0) {

		do {
			DBG("Re-setting l1pte: %p to 0\n", l1pte);

			*l1pte = 0; /* Free this entry */

		} while (l1pte++, addr += TTB_SECT_SIZE, addr != end);

		mmu_page_table_flush((uint32_t) __l1pte, (uint32_t) l1pte);

	} else {
		/*
		 * No need to loop; L2 pte's aren't interested in the
		 * individual L1 entries.
		 */
		free_l2_mapping(l1pte, addr, end);
	}
}

/*
 * Release an existing mapping
 */
void release_mapping(void *pgtable, addr_t virt_base, uint32_t size) {
	addr_t addr, end, length, next;
	uint32_t *l1pte;

	/* If pgtable is NULL, we consider the system page table */
	if (pgtable == NULL)
		pgtable = __sys_root_pgtable;

	addr = virt_base & PAGE_MASK;
	length = ALIGN_UP(size + (virt_base & ~PAGE_MASK), PAGE_SIZE);

	l1pte = l1pte_offset((uint32_t *) pgtable, addr);

	end = addr + length;

	do {
		next = l1sect_addr_end(addr, end);

		free_l1_mapping(l1pte, addr, next);

		addr = next;

	} while (l1pte++, addr != end);
}

/*
 * Initial configuration of system page table
 * MMU is off
 */
void mmu_configure(addr_t l1pgtable, addr_t fdt_addr) {
	unsigned int i;
	addr_t vaddr, paddr;

	uint32_t *__pgtable = (uint32_t *) l1pgtable;

	icache_disable();
	dcache_disable();

	/* The initial page table is only set by CPU #0 (AGENCY_CPU).
	 * The secondary CPUs use the same page table.
	 */

	if (smp_processor_id() == AGENCY_CPU) {

		/* Empty the page table */

		for (i = 0; i < 4096; i++)
			__pgtable[i] = 0;

		/*
		 * The kernel mapping has to be done with "normal memory" attribute, i.e. using cacheable mappings.
		 * This is required for the use of ldrex/strex instructions in recent core such as Cortex-A72 (or armv8 in general).
		 * Otherwise, strex has weird behaviour -> updated memory resulting with the value of 1 in the destination register (failure).
		 */

		/* Create an identity mapping of 1 MB on running kernel so that the kernel code can go ahead right after the MMU on */
		__pgtable[l1pte_index(CONFIG_RAM_BASE)] = CONFIG_RAM_BASE;
		set_l1_pte_sect_dcache(&__pgtable[l1pte_index(CONFIG_RAM_BASE)], L1_SECT_DCACHE_WRITEALLOC);

		/* Now, create a virtual mapping in the kernel space */

		for (vaddr = AGENCY_VOFFSET, paddr = CONFIG_RAM_BASE; ((vaddr < AGENCY_VOFFSET + CONFIG_RAM_SIZE) && (vaddr < CONFIG_HYPERVISOR_VADDR));
				vaddr += TTB_SECT_SIZE, paddr += TTB_SECT_SIZE)
		{
			*l1pte_offset(__pgtable, vaddr) = paddr;
			set_l1_pte_sect_dcache(l1pte_offset(__pgtable, vaddr), L1_SECT_DCACHE_WRITEALLOC);

		}

		/* Create the mapping of the hypervisor code area. */
		for (vaddr = CONFIG_HYPERVISOR_VADDR, paddr = CONFIG_RAM_BASE; vaddr < CONFIG_HYPERVISOR_VADDR + CONFIG_HYPERVISOR_SIZE;
				vaddr += TTB_SECT_SIZE, paddr += TTB_SECT_SIZE)
		{
			*l1pte_offset(__pgtable, vaddr) = paddr;
			set_l1_pte_sect_dcache(l1pte_offset(__pgtable, vaddr), L1_SECT_DCACHE_WRITEALLOC);
		}

		/* Early mapping I/O for UART */
		__pgtable[l1pte_index(UART_BASE)] = UART_BASE;
		set_l1_pte_sect_dcache(&__pgtable[l1pte_index(UART_BASE)], L1_SECT_DCACHE_OFF);
	}

	mmu_setup((void *) __pgtable);
	
	dcache_enable();
	icache_enable();
	
	if (smp_processor_id() == AGENCY_CPU) {
		/* The device tree is visible in the L_PAGE_OFFSET area */
		fdt_vaddr = (addr_t *) __lva(fdt_addr);

		__sys_root_pgtable = (addr_t *) (CONFIG_HYPERVISOR_VADDR + TTB_L1_SYS_OFFSET);
	}

}

/*
 * Clear the L1 PTE used for mapping of a specific virtual address.
 */
void clear_l1pte(void *l1pgtable, addr_t vaddr) {
	uint32_t *l1pte;

	/* If l1pgtable is NULL, we consider the system page table */
	if (l1pgtable == NULL)
		l1pgtable = (uint32_t *) __sys_root_pgtable;

	l1pte = l1pte_offset(l1pgtable, vaddr);

	*l1pte = 0;

	flush_pte_entry(l1pte);
}

/*
 * Switch the MMU to a L1 page table
 */
void mmu_switch(void *pgtable) {

	flush_dcache_all();

	__mmu_switch(pgtable);
	
	invalidate_icache_all();
	v7_inval_tlb();
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

