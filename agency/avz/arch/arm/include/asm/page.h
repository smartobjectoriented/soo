/*
 * Copyright (C) 2016,2017 Daniel Rossier <daniel.rossier@soo.tech>
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

#ifndef _ASMARM_PAGE_H
#define _ASMARM_PAGE_H

/* PAGE_SHIFT determines the page size */
#define PAGE_SHIFT		12
#define PAGE_SIZE		(1 << PAGE_SHIFT)
#define PAGE_MASK		(~(PAGE_SIZE-1))

/* to align the pointer to the (next) page boundary */
#define PAGE_ALIGN(addr)	(((addr)+PAGE_SIZE-1)&PAGE_MASK)


#ifndef _ASM_PAGE_H
#define _ASM_PAGE_H

#ifndef __ASSEMBLY__

#include <asm/config.h>
#include <asm/types.h>

#include <avz/lib.h>

typedef u32 intpte_t;
typedef u32 intpde_t;

#define PRIpte "lx"


#define PADDR_BITS              32
#define PADDR_MASK              (~0UL)

#define PFN_DOWN(x)   ((x) >> PAGE_SHIFT)
#define PFN_UP(x)     (((x) + PAGE_SIZE-1) >> PAGE_SHIFT)

#define SECTION_UP(x) (((x) + SZ_1M-1) >> PGD_SHIFT)

typedef struct { unsigned long pte; } l1_pgentry_t;
typedef struct { intpte_t l2;  } l2_pgentry_t;



typedef struct { u32 pfn; } pagetable_t;
#define pagetable_get_paddr(x) ((physaddr_t)(x).pfn << PAGE_SHIFT)
#define pagetable_get_pfn(x)   ((x).pfn)
#define mk_pagetable(pa)       \
		({ pagetable_t __p; __p.pfn = (pa) >> PAGE_SHIFT; __p; })

typedef struct { unsigned long pte; } pte_t;
typedef struct { unsigned long pmd; } pmd_t;
typedef struct { unsigned long pgd[2]; } pgd_t;
typedef struct { unsigned long pgprot; } pgprot_t;

#define __pte(x)        ((pte_t) { (x) } )
#define __pmd(x)        ((pmd_t) { (x) } )
#define __pgprot(x)     ((pgprot_t) { (x) } )

#define clear_page(page)	memzero((void *)(page), PAGE_SIZE)

typedef l2_pgentry_t pde_t;

/* page-table type */

#define pte_val(x)      ((x).pte)
#define pmd_val(x)      ((x).pmd)
#define pgd_val(x)      ((x).pgd[0])
#define pgprot_val(x)   ((x).pgprot)
/*
 * NB. We don't currently track I/O holes in the physical RAM space.
 */

/*
 * We add two functions for retrieving virt and phys address relative to
 * Linux offset according to the memory map (used to access guest mem)
 */
#define __lpa(vaddr) ((vaddr) - L_PAGE_OFFSET + PHYS_OFFSET)
#define __lva(paddr) ((paddr) - PHYS_OFFSET + L_PAGE_OFFSET)

/* (LCD) virt_to_maddr & maddr_to_virt do not use the memory map as they are
         only used to compute hypervisor addresses */
#define virt_to_maddr(va)   ((unsigned long)(va) - PAGE_OFFSET + PHYS_OFFSET)
#define maddr_to_virt(ma)   ((void *)((unsigned long)(ma) + PAGE_OFFSET - PHYS_OFFSET))

/* Shorthand versions of the above functions. */
#define __pa(x)             ((virt_to_maddr(x)))
#define __va(x)             ((maddr_to_virt(x)))

#define virt_to_phys(x)     (__pa(x))
#define phys_to_virt(x)     (__va(x))

#define pfn_to_page(_pfn)   (frame_table + (_pfn - min_page))
#define phys_to_page(kaddr) (frame_table + (((kaddr) >> PAGE_SHIFT) - min_page) )
#define virt_to_page(kaddr) (frame_table + (((__pa(kaddr) >> PAGE_SHIFT)) - min_page) )

#define pfn_valid(_pfn)     (((_pfn) >= min_page) && ((_pfn) <= max_page))

#define pfn_to_phys(pfn)    ((physaddr_t)(pfn) << PAGE_SHIFT)
#define phys_to_pfn(pa)     ((unsigned long)((pa) >> PAGE_SHIFT))

/* Convert between frame number and address formats.  */
#define __pfn_to_paddr(pfn) ((paddr_t)(pfn) << PAGE_SHIFT)
#define __paddr_to_pfn(pa)  ((unsigned long)((pa) >> PAGE_SHIFT))


#define paddr_to_pfn(pa)    __paddr_to_pfn(pa)
#define pfn_to_paddr(pfn)   __pfn_to_paddr(pfn)

#define mfn_to_page(_mfn)   pfn_to_page(_mfn)
#define mfn_valid(_mfn)     pfn_valid(_mfn)

#define virt_to_mfn(va)     (virt_to_maddr(va) >> PAGE_SHIFT)
#define mfn_to_virt(mfn)    (maddr_to_virt((mfn) << PAGE_SHIFT))

/* Convert between machine frame numbers and page-info structures. */

#define page_to_phys(page)  ((physaddr_t)((page + min_page) - frame_table) << PAGE_SHIFT)
#define page_to_pfn(_page)  ((unsigned long)((_page + min_page) - frame_table))
#define page_to_virt(_page) phys_to_virt(page_to_phys(_page))
#define page_to_mfn(_page)  page_to_pfn(_page)


/* Convert between machine addresses and page-info structures. */
#define maddr_to_page(ma)   phys_to_page(ma)
#define page_to_maddr(pg)   page_to_phys(pg)


static inline int get_order_from_bytes(physaddr_t size)
{
	int order;

	size = (size - 1) >> PAGE_SHIFT;
	for (order = 0; size; order++)
		size >>= 1;

	return order;
}


static inline int get_order_from_pages(unsigned long nr_pages)
{
	int order;
	nr_pages--;
	for (order = 0; nr_pages; order++)
		nr_pages >>= 1;
	return order;
}
#include <asm/pgtable.h>

#define _PAGE_PRESENT  0x001UL
#define _PAGE_RW       0x002UL
#define _PAGE_USER     0x004UL
#define _PAGE_PWT      0x008UL
#define _PAGE_PCD      0x010UL
#define _PAGE_ACCESSED 0x020UL
#define _PAGE_DIRTY    0x040UL
#define _PAGE_PAT      0x080UL
#define _PAGE_PSE      0x080UL
#define _PAGE_GLOBAL   0x100UL



#define _L2_PAGE_SECTION        0x002U
#define _L2_PAGE_COARSE_PT      0x001U
#define _L2_PAGE_PRESENT        0x003U
#define _L2_PAGE_BUFFERABLE 0x004U
#define _L2_PAGE_CACHEABLE      0x008U

#define _L1_PAGE_SMALL_PG       0x002U
#define _L1_PAGE_PRESENT        0x002U
#define _L1_PAGE_BUFFERABLE 0x004U
#define _L1_PAGE_CACHEABLE      0x008U
#define _L1_PAGE_AP_MANAGER     0xFF0U
#define _L1_PAGE_RW_USER        PTE_SMALL_AP_URW_SRW
#define _L1_PAGE_RO_USER        PTE_SMALL_AP_URO_SRW

/*
 * Debug option: Ensure that granted mappings are not implicitly unmapped.
 * WARNING: This will need to be disabled to run OSes that use the spare PTE
 * bits themselves (e.g., *BSD).
 */


/* Get direct integer representation of a pte's contents (intpte_t). */
#define l1e_get_intpte(x)          ((x).pte)
#define l2e_get_intpte(x)          ((x).l2)


/* Get pfn mapped by pte (unsigned long). */
#define l1e_get_pfn(x)             \
		((unsigned long)(((x).pte & (PADDR_MASK&PAGE_MASK)) >> PAGE_SHIFT))
#define l2e_get_pfn(x)             \
		((unsigned long)(((x).l2 & (PADDR_MASK&PAGE_MASK)) >> PAGE_SHIFT))

/* Get physical address of page mapped by pte (physaddr_t). */
#define l1e_get_paddr(x)           \
		((physaddr_t)(((x).pte & (PADDR_MASK&PAGE_MASK))))
#define l2e_get_paddr(x)           \
		((physaddr_t)(((x).l2 & (PADDR_MASK&PAGE_MASK))))

/* for ARM section entry and coarse page table */
#define l2e_section_get_paddr(x) l2e_get_paddr(x)
#define l2e_coarse_pt_get_paddr(x) \
		((physaddr_t)(((x).l2 & (PADDR_MASK & 0xFFFFFC00))))

/* Get pointer to info structure of page mapped by pte (struct page_info *). */
#define l1e_get_page(x)           (pfn_to_page(l1e_get_pfn(x)))
#define l2e_get_page(x)           (pfn_to_page(l2e_get_pfn(x)))

/* Get pte access flags (unsigned int). */
#define l1e_get_flags(x)           (get_pte_flags((x).pte))
#define l2e_get_flags(x)           (get_pte_flags((x).l2))

/* Construct an empty pte. */
#define l1e_empty()                ((pte_t) { 0 })
#define l2e_empty()                ((pde_t) { 0 })

/* Extract flags into 12-bit integer, or turn 12-bit flags into a pte mask. */
#define get_pte_flags(x) ((int)(x) & 0xFFF)
#define put_pte_flags(x) ((intpte_t)((x) & 0xFFF))

// TODO: SBZ (should be zero) fileds
/* Construct a pte from a pfn and access flags. */
#define l1e_from_pfn(pfn, flags)   \
		((pte_t) { ((intpte_t)(pfn) << PAGE_SHIFT) | put_pte_flags(flags) })
#define l2e_from_pfn(pfn, flags)   \
		((pde_t) { ((intpte_t)(pfn) << PAGE_SHIFT) | put_pte_flags(flags) })

static inline pte_t l1e_from_paddr(physaddr_t pa, unsigned int flags)
{
	ASSERT((pa & ~(PADDR_MASK & PAGE_MASK)) == 0);
	return (pte_t) { pa | put_pte_flags(flags) };
}
static inline pde_t l2e_from_paddr(physaddr_t pa, unsigned int flags)
{
	ASSERT((pa & ~(PADDR_MASK & PAGE_MASK)) == 0);
	return (pde_t) { pa | put_pte_flags(flags) };
}

/* Construct a pte from its direct integer representation. */
#define l1e_from_intpte(intpte)    ((pte_t) { (intpte_t)(intpte) })
#define l2e_from_intpte(intpte)    ((pde_t) { (intpte_t)(intpte) })

/* Get pte access flags (unsigned int). */
#define l1e_get_flags(x)           (get_pte_flags((x).pte))
#define l2e_get_flags(x)           (get_pte_flags((x).l2))

/* Construct a pte from a page pointer and access flags. */
#define l1e_from_page(page, flags) (l1e_from_pfn(page_to_pfn(page),(flags)))
#define l2e_from_page(page, flags) (l2e_from_pfn(page_to_pfn(page),(flags)))

#define l2e_to_l1e(x)              ((pte_t *)__va(l2e_get_paddr(x)))

#endif  /* ! __ASSEMBLY__ */
#endif

#endif
