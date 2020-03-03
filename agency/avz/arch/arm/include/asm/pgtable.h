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

#ifndef _ASMARM_PGTABLE_H
#define _ASMARM_PGTABLE_H


#include <asm/proc-fns.h>

#include <asm/memory.h>
#include <mach/vmalloc.h>
#include <asm/mmu.h>

#include <asm/tlbflush.h>

#define PTRS_PER_PTE		512
#define PTRS_PER_PMD		1
#define PTRS_PER_PGD		2048

#define PMD_SHIFT		20
#define PGDIR_SHIFT		20

#define PMD_SIZE		(1UL << PMD_SHIFT)
#define PMD_MASK		(~(PMD_SIZE-1))

#define PGDIR_SIZE		(1UL << PGDIR_SHIFT)
#define PGDIR_MASK		(~(PGDIR_SIZE-1))
/*
 * "Linux" PTE definitions.
 *
 * We keep two sets of PTEs - the hardware and the linux version.
 * This allows greater flexibility in the way we map the Linux bits
 * onto the hardware tables, and allows us to have YOUNG and DIRTY
 * bits.
 *
 * The PTE table pointer refers to the hardware entries; the "Linux"
 * entries are stored 1024 bytes below.
 */
#define L_PTE_PRESENT		(1 << 0)
#define L_PTE_YOUNG		(1 << 1)
#define L_PTE_FILE		(1 << 2)	/* only when !PRESENT */
#define L_PTE_DIRTY		(1 << 6)
#define L_PTE_RDONLY		(1 << 7)
#define L_PTE_USER		(1 << 8)
#define L_PTE_XN		(1 << 9)
#define L_PTE_SHARED		(1 << 10)	/* shared(v6), coherent(xsc3) */

/* compatibility with v6 */
#define L_PTE_EXTENDED          (1 << 11)        /* CB bits mapped to extended memory type */
/*
 * These are the memory types, defined to be compatible with
 * pre-ARMv6 CPUs cacheable and bufferable bits:   XXCB
 */
#define L_PTE_MT_UNCACHED	(0x00 << 2)	/* 0000 */
#define L_PTE_MT_BUFFERABLE	(0x01 << 2)	/* 0001 */
#define L_PTE_MT_WRITETHROUGH	(0x02 << 2)	/* 0010 */
#define L_PTE_MT_WRITEBACK	(0x03 << 2)	/* 0011 */
#define L_PTE_MT_MINICACHE	(0x06 << 2)	/* 0110 (sa1100, xscale) */
#define L_PTE_MT_WRITEALLOC	(0x07 << 2)	/* 0111 */
#define L_PTE_MT_DEV_SHARED	(0x04 << 2)	/* 0100 */
#define L_PTE_MT_DEV_NONSHARED	(0x0c << 2)	/* 1100 */
#define L_PTE_MT_DEV_WC		(0x09 << 2)	/* 1001 */
#define L_PTE_MT_DEV_CACHED	(0x0b << 2)	/* 1011 */
#define L_PTE_MT_VECTORS        (0x0f << 2)      /* 1111 */
#define L_PTE_MT_MASK		(0x0f << 2)

#ifndef __ASSEMBLY__

/*
 * The following macros handle the cache and bufferable bits...
 */
#define _L_PTE_DEFAULT	L_PTE_PRESENT | L_PTE_YOUNG

extern pgprot_t		pgprot_user;
extern pgprot_t		pgprot_kernel;

#define _MOD_PROT(p, b)	__pgprot(pgprot_val(p) | (b))

#define PAGE_NONE		_MOD_PROT(pgprot_user, L_PTE_XN | L_PTE_RDONLY)
#define PAGE_SHARED		_MOD_PROT(pgprot_user, L_PTE_USER | L_PTE_XN)
#define PAGE_SHARED_EXEC	_MOD_PROT(pgprot_user, L_PTE_USER)
#define PAGE_COPY		_MOD_PROT(pgprot_user, L_PTE_USER | L_PTE_RDONLY | L_PTE_XN)
#define PAGE_COPY_EXEC		_MOD_PROT(pgprot_user, L_PTE_USER | L_PTE_RDONLY)
#define PAGE_READONLY		_MOD_PROT(pgprot_user, L_PTE_USER | L_PTE_RDONLY | L_PTE_XN)
#define PAGE_READONLY_EXEC	_MOD_PROT(pgprot_user, L_PTE_USER | L_PTE_RDONLY)
#define PAGE_KERNEL		_MOD_PROT(pgprot_kernel, L_PTE_XN)
#define PAGE_KERNEL_EXEC	pgprot_kernel

#define __PAGE_NONE		__pgprot(_L_PTE_DEFAULT | L_PTE_RDONLY | L_PTE_XN)
#define __PAGE_SHARED		__pgprot(_L_PTE_DEFAULT | L_PTE_USER | L_PTE_XN)
#define __PAGE_SHARED_EXEC	__pgprot(_L_PTE_DEFAULT | L_PTE_USER)
#define __PAGE_COPY		__pgprot(_L_PTE_DEFAULT | L_PTE_USER | L_PTE_RDONLY | L_PTE_XN)
#define __PAGE_COPY_EXEC	__pgprot(_L_PTE_DEFAULT | L_PTE_USER | L_PTE_RDONLY)
#define __PAGE_READONLY		__pgprot(_L_PTE_DEFAULT | L_PTE_USER | L_PTE_RDONLY | L_PTE_XN)
#define __PAGE_READONLY_EXEC	__pgprot(_L_PTE_DEFAULT | L_PTE_USER | L_PTE_RDONLY)

#endif /* __ASSEMBLY__ */
/*
 * The table below defines the page protection levels that we insert into our
 * Linux page table version.  These get translated into the best that the
 * architecture can perform.  Note that on most ARM hardware:
 *  1) We cannot do execute protection
 *  2) If we could do execute protection, then read is implied
 *  3) write implies read permissions
 */
#define __P000  __PAGE_NONE
#define __P001  __PAGE_READONLY
#define __P010  __PAGE_COPY
#define __P011  __PAGE_COPY
#define __P100  __PAGE_READONLY_EXEC
#define __P101  __PAGE_READONLY_EXEC
#define __P110  __PAGE_COPY_EXEC
#define __P111  __PAGE_COPY_EXEC

#define __S000  __PAGE_NONE
#define __S001  __PAGE_READONLY
#define __S010  __PAGE_SHARED
#define __S011  __PAGE_SHARED
#define __S100  __PAGE_READONLY_EXEC
#define __S101  __PAGE_READONLY_EXEC
#define __S110  __PAGE_SHARED_EXEC
#define __S111  __PAGE_SHARED_EXEC

/* Added (DRE) */
#define PGD_ALIGN(x)		((x + (0x4000 - 1)) & ~(0x4000 - 1))
#define PGT_ALIGN(x)		((x + (0x1000 - 1)) & ~(0x1000 - 1))
/*
 * ZERO_PAGE is a global shared page that is always zero: used
 * for zero-mapped memory areas etc..
 */


#define pte_pfn(pte)		(pte_val(pte) >> PAGE_SHIFT)
#define pfn_pte(pfn,prot)	(__pte(((pfn) << PAGE_SHIFT) | pgprot_val(prot)))

#define pte_none(pte)		(!pte_val(pte))
#define pte_clear(mm,addr,ptep)	set_pte_at((mm),(addr),(ptep), __pte(0))
#define pte_page(pte)		(pfn_to_page(pte_pfn(pte)))

#define pte_unmap(pte)		do { } while (0)
#define pte_unmap_nested(pte)	do { } while (0)
/*#define set_pte(ptep, pte)	cpu_set_pte(ptep,pte)*/
#define set_pte_ext(ptep,pte,ext) cpu_set_pte_ext(ptep,pte,ext)
/*#define set_pte_at(mm,addr,ptep,pteval) set_pte(ptep,pteval)*/
#define set_pte_at(mm,addr,ptep,pteval) do { \
	set_pte_ext(ptep, pteval, (addr) >= TASK_SIZE ? 0 : PTE_EXT_NG); \
 } while (0)

/*
 * The following only work if pte_present() is true.
 * Undefined behaviour if not..
 */
#define pte_present(pte)	(pte_val(pte) & L_PTE_PRESENT)
#define pte_read(pte)		(pte_val(pte) & L_PTE_USER)
#define pte_write(pte)		(pte_val(pte) & L_PTE_WRITE)
#define pte_exec(pte)		(pte_val(pte) & L_PTE_EXEC)
#define pte_dirty(pte)		(pte_val(pte) & L_PTE_DIRTY)
#define pte_young(pte)		(pte_val(pte) & L_PTE_YOUNG)

/*
 * The following only works if pte_present() is not true.
 */
#define pte_file(pte)		(pte_val(pte) & L_PTE_FILE)
#define pte_to_pgoff(x)		(pte_val(x) >> 2)
#define pgoff_to_pte(x)		__pte(((x) << 2) | L_PTE_FILE)

#include <asm/pgtable-hwdef.h>


#define PGD_SHIFT               (20)
#define PGT_SHIFT               (12)

#define SECTION_SIZE            (0x100000)
#define SECTION_MASK		(~(SECTION_SIZE - 1))


#ifndef __ASSEMBLY__

#define PTE_FILE_MAX_BITS	30

#define PTE_BIT_FUNC(fn,op) \
static inline pte_t pte_##fn(pte_t pte) { pte_val(pte) op; return pte; }


#define PDE_AP_SRW_UNO          (0x01 << 10)
#define PDE_AP_SRW_URO          (0x02 << 10)
#define PDE_AP_SRW_URW          (0x03 << 10)

#define PDE_BUFFERABLE		(0x04)
#define PDE_CACHEABLE		(0x08)

typedef u32 pteval_t;


#define GRANT_PTE_FLAGS		(PTE_TYPE_SMALL | PTE_BUFFERABLE | PTE_CACHEABLE | PTE_AP_URW_SRW)

#define cpu_get_pgd_phys()	\
	({						\
		unsigned long pg;			\
		__asm__("mrc	p15, 0, %0, c2, c0, 0"	\
			 : "=r" (pg) : : "cc");		\
		pg &= ~0x3fff;				\
	})
/*
 * Mark the prot value as uncacheable and unbufferable.
 */
#define pgprot_noncached(prot)	__pgprot(pgprot_val(prot) & ~(L_PTE_CACHEABLE | L_PTE_BUFFERABLE))
#define pgprot_writecombine(prot) __pgprot(pgprot_val(prot) & ~L_PTE_CACHEABLE)

#define pmd_none(pmd)		(!pmd_val(pmd))


#define pmd_clear(pdep)			\
	do {				\
		pdep->l2 = 0;	\
		clean_pmd_entry((pde_t *)pdep); \
	} while (0)

#define pmd_page(pmd) phys_to_page((pmd_val(pmd))

#define __L1_PAGETABLE_ORDER      8
#define __L2_PAGETABLE_ORDER      12

#define __L1_PAGETABLE_ENTRIES    (1<<__L1_PAGETABLE_ORDER)
#define __L2_PAGETABLE_ENTRIES    (1<<__L2_PAGETABLE_ORDER)

#define __L1_PAGETABLE_SHIFT      12
#define __L2_PAGETABLE_SHIFT      20      /* REMARK: not 22 (x86) */
#define __L2_PAGE_TABLE_SIZE		(PAGE_SIZE << 2)

extern pde_t swapper_pg_dir[__L2_PAGETABLE_ENTRIES];

#define idle_pg_table_l2 swapper_pg_dir


/* Given a virtual address, get an entry offset into a page table. */
#define pte_index(a)         \
    (((a) >> __L1_PAGETABLE_SHIFT) & (__L1_PAGETABLE_ENTRIES - 1))
#define pde_index(a)         \
    (((a) >> __L2_PAGETABLE_SHIFT) & (__L2_PAGETABLE_ENTRIES - 1))


/* to find an entry in a page-table-directory */

#define pde_offset(v, addr)		(((pde_t *)(((pde_t *)(v)->arch.guest_vtable)))+pde_index(addr))

/* to find an entry in a page-table-directory */
#define pgd_offset(addr)	(swapper_pg_dir+pde_index(addr))
#define pgd_offset_priv(pgd, addr) (pgd+pde_index(addr))

#define pte_offset_kernel(pmd, addr)		(((pte_t *) l2e_to_l1e(*pmd)+pte_index(addr)))

/* to find an entry in a kernel page-table-directory */
#define pde_offset_k(addr)	pgd_offset(addr)

static inline void __pmd_populate(pmd_t *pmdp, unsigned long pmdval)
{
        *pmdp = __pmd(pmdval);
        //*(pmdp+1)   = __pmd(pmdval + 256 * sizeof(pte_t));
        flush_pmd_entry((pde_t *) pmdp);
}


/* Find an entry in the second-level page table.. */
#define pmd_offset(dir)	((pde_t *)(dir))

/* Find an entry in the third-level page table.. */
#define __pte_index(addr)	(((addr) >> PAGE_SHIFT) & (PTRS_PER_PTE - 1))

/*
 * remap a physical page `pfn' of size `size' with page protection `prot'
 * into virtual address `from'
 */
#define io_remap_pfn_range(vma,from,pfn,size,prot) \
		remap_pfn_range(vma, from, pfn, size, prot)


#define pgd_addr_end(addr, end)						\
({	unsigned long __boundary = ((addr) + PGDIR_SIZE) & PGDIR_MASK;	\
	(__boundary - 1 < (end) - 1)? __boundary: (end);		\
})

void *get_l2_pgtable(void);

#endif /* !__ASSEMBLY__ */

#endif /* _ASMARM_PGTABLE_H */
