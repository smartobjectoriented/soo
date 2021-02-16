/*
 * Copyright (C) 2015-2017 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef MMU_H
#define MMU_H

#ifndef __ASSEMBLY__
#include <config.h>
#include <types.h>
#endif

#include <sizes.h>

/* PAGE_SHIFT determines the page size */
#define PAGE_SHIFT	12
#define PAGE_SIZE       (1 << PAGE_SHIFT)
#define PAGE_MASK       (~(PAGE_SIZE-1))

/*
 * Page offset: 3GB
 */
/* (DRE) Crucial !! */

#define PAGE_OFFSET	UL(0xff000000)
#define L_PAGE_OFFSET	UL(0xc0000000)

#define TTB_L1_SYS_OFFSET	0x4000

/* Define the number of entries in each page table */

#define TTB_L1_ORDER      12
#define TTB_L2_ORDER      8

#define TTB_I1_SHIFT	  (32 - TTB_L1_ORDER)
#define TTB_I1_MASK	  (~((1 << TTB_I1_SHIFT)-1))

#define TTB_I2_SHIFT	  PAGE_SHIFT
#define TTB_I2_MASK	  (~((TTB_I2_SHIFT)-1))

#define TTB_L1_ENTRIES    (1 << TTB_L1_ORDER)
#define TTB_L2_ENTRIES    (1 << TTB_L2_ORDER)

/* Size of the L1 page table */
#define TTB_L1_SIZE    	  (4 << TTB_L1_ORDER)

/* To get the address of the L2 page table from a L1 descriptor */
#define TTB_L1_SECT_ADDR_SHIFT	20
#define TTB_L1_SECT_ADDR_OFFSET	(1 << TTB_L1_SECT_ADDR_SHIFT)
#define TTB_L1_SECT_ADDR_MASK	(~(TTB_L1_SECT_ADDR_OFFSET - 1))

#define TTB_L1_PAGE_ADDR_SHIFT	10
#define TTB_L1_PAGE_ADDR_OFFSET	(1 << TTB_L1_PAGE_ADDR_SHIFT)
#define TTB_L1_PAGE_ADDR_MASK	(~(TTB_L1_PAGE_ADDR_OFFSET - 1))

#define TTB_SECT_SIZE	(0x100000)
#define TTB_SECT_MASK   (~(TTB_SECT_SIZE - 1))

#define TTB_L2_ADDR_MASK	(~(PAGE_SIZE-1))

/* Given a virtual address, get an entry offset into a page table. */
#define l1pte_index(a) (((uint32_t) a) >> (32 - TTB_L1_ORDER))
#define l2pte_index(a) ((((uint32_t) a) >> PAGE_SHIFT) & (TTB_L2_ENTRIES - 1))

#define pte_index_to_vaddr(i1, i2) ((i1 << TTB_I1_SHIFT) | (i2 << TTB_I2_SHIFT))

#define l1pte_offset(pgtable, addr)     (pgtable + l1pte_index(addr))
#define l2pte_offset(l1pte, addr) 	((uint32_t *) __va(*l1pte & TTB_L1_PAGE_ADDR_MASK) + l2pte_index(addr))
#define l2pte_first(l1pte)		((uint32_t *) __va(*l1pte & TTB_L1_PAGE_ADDR_MASK))

#define l1sect_addr_end(addr, end)                                         \
 ({      unsigned long __boundary = ((addr) + TTB_SECT_SIZE) & TTB_SECT_MASK;  \
         (__boundary - 1 < (end) - 1) ? __boundary: (end);                \
 })

/* Short-Descriptor Translation Table Level 1 Bits */

#define TTB_L1_RES0		(1 << 4)
#define TTB_L1_IMPDEF		(1 << 9)
#define TTB_DOMAIN(x)		((x & 0xf) << 5)

/* Page Table related bits */
#define TTB_L1_PAGE_PXN		(1 << 2)
#define TTB_L1_PAGE_NS		(1 << 3)
#define TTB_L1_PAGE		(1 << 0)

/* Section related bits */
#define TTB_L1_NG		(1 << 17)
#define TTB_L1_S		(1 << 16)
#define TTB_L1_SECT_NS		(1 << 19)
#define TTB_L1_TEX(x)		((x & 0x7) << 12)
#define TTB_L1_SECT_PXN		(1 << 0)
#define TTB_L1_XN		(1 << 4)
#define TTB_L1_C		(1 << 3)
#define TTB_L1_B		(1 << 2)
#define TTB_L1_SECT		(2 << 0)
#define TTB_L1_L2		(1 << 0)

/* R/W in kernel and user mode */
#define TTB_L1_AP		(3 << 10)

/* Short-Descriptor Translation Table Level 2 Bits */

#define TTB_L2_NG		(1 << 11)
#define TTB_L2_S		(1 << 10)

#define TTB_L2_TEX(x)		((x & 0x7) << 6)
#define TTB_L2_XN		(1 << 0)
#define TTB_L2_C		(1 << 3)
#define TTB_L2_B		(1 << 2)
#define TTB_L2_PAGE		(2 << 0)

/* R/W in kernel and user mode */
#define TTB_L2_AP		(3 << 4)

 /* TTBR0 bits */
 #define TTBR0_BASE_ADDR_MASK	0xFFFFC000
 #define TTBR0_RGN_NC		(0 << 3)
 #define TTBR0_RGN_WBWA		(1 << 3)
 #define TTBR0_RGN_WT		(2 << 3)
 #define TTBR0_RGN_WB		(3 << 3)

 /* TTBR0[6] is IRGN[0] and TTBR[0] is IRGN[1] */
 #define TTBR0_IRGN_NC		(0 << 0 | 0 << 6)
 #define TTBR0_IRGN_WBWA	(0 << 0 | 1 << 6)
 #define TTBR0_IRGN_WT		(1 << 0 | 0 << 6)
 #define TTBR0_IRGN_WB		(1 << 0 | 1 << 6)

#define clear_page(page)	memset((void *)(page), 0, PAGE_SIZE)

#define SECTION_UP(x) (((x) + SZ_1M-1) >> TTB_L1_SECT_ADDR_SHIFT)

#define PFN_DOWN(x)   ((x) >> PAGE_SHIFT)
#define PFN_UP(x)     (((x) + PAGE_SIZE-1) >> PAGE_SHIFT)


/*
 * We add two functions for retrieving virt and phys address relative to
 * Linux offset according to the memory map (used to access guest mem)
 */
#define __lpa(vaddr) ((vaddr) - L_PAGE_OFFSET + CONFIG_RAM_BASE)
#define __lva(paddr) ((paddr) - CONFIG_RAM_BASE + L_PAGE_OFFSET)

#define __pa(vaddr)             (((uint32_t) vaddr) - PAGE_OFFSET + ((uint32_t) CONFIG_RAM_BASE))
#define __va(paddr)             (((uint32_t) paddr) - ((uint32_t) CONFIG_RAM_BASE) + PAGE_OFFSET)

#define virt_to_phys(x)     (__pa(x))
#define phys_to_virt(x)     (__va(x))

#define pfn_to_phys(pfn) ((pfn) << PAGE_SHIFT)
#define phys_to_pfn(phys) (((uint32_t) phys) >> PAGE_SHIFT)
#define virt_to_pfn(virt) (phys_to_pfn(__va((uint32_t) virt)))
#define pfn_to_virt(pfn) (phys_to_virt(pfn_to_phys(pfn)))

#ifndef __ASSEMBLY__

static inline bool l1pte_is_sect(uint32_t l1pte) {

	/* Check if the L1 pte is for mapping of section or not */
	return (l1pte & 0x2);
}


/* Options available for data cache related to section */
enum ttb_l1_sect_dcache_option {
	L1_SECT_DCACHE_OFF = TTB_DOMAIN(0) | TTB_L1_SECT,
	L1_SECT_DCACHE_WRITETHROUGH = L1_SECT_DCACHE_OFF | TTB_L1_C,
	L1_SECT_DCACHE_WRITEBACK = L1_SECT_DCACHE_WRITETHROUGH | TTB_L1_B,
	L1_SECT_DCACHE_WRITEALLOC = L1_SECT_DCACHE_WRITEBACK | TTB_L1_TEX(1),
};

enum ttb_l1_page_dcache_option {
	L1_PAGE_DCACHE_OFF = TTB_DOMAIN(0) | TTB_L1_PAGE | TTB_L1_RES0,
	L1_PAGE_DCACHE_WRITETHROUGH = L1_PAGE_DCACHE_OFF,
	L1_PAGE_DCACHE_WRITEBACK = L1_PAGE_DCACHE_WRITETHROUGH,
	L1_PAGE_DCACHE_WRITEALLOC = L1_PAGE_DCACHE_WRITEBACK,
};

/* Options available for data cache related to a page */
enum ttb_l2_dcache_option {
	L2_DCACHE_OFF = TTB_L2_PAGE,
	L2_DCACHE_WRITETHROUGH = L2_DCACHE_OFF | TTB_L2_C,
	L2_DCACHE_WRITEBACK = L2_DCACHE_WRITETHROUGH | TTB_L2_B,
	L2_DCACHE_WRITEALLOC = L2_DCACHE_WRITEBACK | TTB_L2_TEX(1),
};

/*
 * This structure holds internal fields required to
 * manage the MMU configuration regarding address space.
 */
typedef struct {
	uint32_t ttbr0[NR_CPUS];
	uint32_t pgtable_paddr;
	uint32_t pgtable_vaddr;
} addrspace_t;

#define cpu_get_l1pgtable()	\
({						\
	unsigned long ttbr;			\
	__asm__("mrc	p15, 0, %0, c2, c0, 0"	\
		 : "=r" (ttbr) : : "cc");	\
	ttbr &= TTBR0_BASE_ADDR_MASK;		\
})

#define cpu_get_ttbr0() \
({						\
	unsigned long ttbr;			\
	__asm__("mrc	p15, 0, %0, c2, c0, 0"	\
		 : "=r" (ttbr) : : "cc");		\
	ttbr;					\
})

extern uint32_t *__sys_l1pgtable;
extern uint32_t *l2pt_current_base;
extern unsigned long l2pt_phys_start;

extern void __mmu_switch(uint32_t l1pgtable_phys);

void pgtable_copy_kernel_area(uint32_t *l1pgtable);

void create_mapping(uint32_t *l1pgtable, uint32_t virt_base, uint32_t phys_base, uint32_t size, bool nocache);

uint32_t *new_l1pgtable(void);
void reset_l1pgtable(uint32_t *l1pgtable, bool remove);

void clear_l1pte(uint32_t *l1pgtable, uint32_t vaddr);

void mmu_switch(addrspace_t *addrspace);
void dump_pgtable(uint32_t *l1pgtable);

void dump_current_pgtable(void);

void set_l1_pte_sect_dcache(uint32_t *l1pte, enum ttb_l1_sect_dcache_option option);
void set_l1_pte_page_dcache(uint32_t *l1pte, enum ttb_l1_page_dcache_option option);
void set_l2_pte_dcache(uint32_t *l2pte, enum ttb_l2_dcache_option option);

void mmu_setup(uint32_t *pgtable);

void vectors_init(void);

#endif


#endif /* MMU_H */

