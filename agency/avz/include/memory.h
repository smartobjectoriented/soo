/*
 * Copyright (C) 2020 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef MEMORY_H
#define MEMORY_H

#include <types.h>
#include <list.h>

#include <asm/mmu.h>
#include <asm/bitops.h>

#define VECTORS_BASE	0xffff0000

struct domain;
struct page_info;
struct vcpu;

#define page_to_phys(page)  ((uint32_t)((page + min_page) - frame_table) << PAGE_SHIFT)
#define page_to_pfn(_page)  ((unsigned long)((_page + min_page) - frame_table))
#define page_to_virt(_page) phys_to_virt(page_to_phys(_page))

#define pfn_to_page(_pfn)   (frame_table + (_pfn - min_page))
#define phys_to_page(kaddr) (frame_table + (((kaddr) >> PAGE_SHIFT) - min_page))
#define virt_to_page(kaddr) (frame_table + (((__pa(kaddr) >> PAGE_SHIFT)) - min_page))

#define pfn_valid(_pfn)     (((_pfn) >= min_page) && ((_pfn) <= max_page))

/* Boot-time allocator. Turns into generic allocator after bootstrap. */
void init_boot_pages(paddr_t ps, paddr_t pe);
unsigned long alloc_boot_pages(unsigned long nr_pfns, unsigned long pfn_align);
void end_boot_allocator(void);

void init_heap(void *heap_vaddr_start, void *heap_vaddr_end);
void *malloc(size_t size);
void free(void *ptr);

/* AVZ suballocator. These functions are interrupt-safe. */
void init_heap_pages(paddr_t ps, paddr_t pe);
void *alloc_heap_pages(unsigned int order, unsigned int memflags);
void free_heap_pages(void *v, unsigned int order);
#define alloc_heap_page() (alloc_heap_pages(0,0))
#define free_heap_page(v) (free_heap_pages(v,0))

void pagealloc_init(void);

unsigned long total_free_pages(void);

#define _MEMF_bits        24
#define  MEMF_bits(n)     ((n)<<_MEMF_bits)

#define MAX_ORDER 20 /* 2^20 contiguous pages */

#define page_list_head                  list_head
#define PAGE_LIST_HEAD_INIT             LIST_HEAD_INIT
#define PAGE_LIST_HEAD                  LIST_HEAD
#define INIT_PAGE_LIST_HEAD             INIT_LIST_HEAD
#define INIT_PAGE_LIST_ENTRY            INIT_LIST_HEAD
#define page_list_empty                 list_empty
#define page_list_first(hd)             list_entry((hd)->next, \
                                                    struct page_info, list)
#define page_list_next(pg, hd)          list_entry((pg)->list.next, \
                                                    struct page_info, list)
#define page_list_add(pg, hd)           list_add(&(pg)->list, hd)
#define page_list_add_tail(pg, hd)      list_add_tail(&(pg)->list, hd)
#define page_list_del(pg, hd)           list_del(&(pg)->list)
#define page_list_del2(pg, hd1, hd2)    list_del(&(pg)->list)
#define page_list_remove_head(hd)       (!page_list_empty(hd) ? \
    ({ \
        struct page_info *__pg = page_list_first(hd); \
        list_del(&__pg->list); \
        __pg; \
    }) : NULL)

extern int __irq_safe[];

extern unsigned long heap_phys_end;

/*
 * Per-page-frame information.
 *
 * Every architecture must ensure the following:
 *  1. 'struct page_info' contains a 'struct list_head list'.
 *  2. Provide a PFN_ORDER() macro for accessing the order of a free page.
 */
#define PFN_ORDER(_pfn) ((_pfn)->v.free.order)


/* XXX copy-and-paste job; re-examine me */
struct page_info
{

    union {
        /* Each frame can be threaded onto a doubly-linked list.
         *
         * For unused shadow pages, a list of pages of this order; for
         * pinnable shadows, if pinned, a list of other pinned shadows
         * (see sh_type_is_pinnable() below for the definition of
         * "pinnable" shadow types).
         */
        struct list_head list;
        /* For non-pinnable shadows, a higher entry that points at us. */
        paddr_t up;

    };

    /* Reference count and various PGC_xxx flags and fields. */
    unsigned long count_info;

    /* Context-dependent fields follow... */
    union {

        /* Page is in use: ((count_info & PGC_count_mask) != 0). */
        struct {
            /* Type reference count and various PGT_xxx flags and fields. */
            unsigned long type_info;
        } inuse;

        /* Page is in use as a shadow: count_info == 0. */
        struct {
            unsigned long type:5;   /* What kind of shadow is this? */
            unsigned long pinned:1; /* Is the shadow pinned? */
            unsigned long count:26; /* Reference count */
        } sh;
    } u;

    union {
        /* Page is on a free list (including shadow code free lists). */
        struct {
            /* Order-size of the free chunk this page is the head of. */
            unsigned int order;
        } free;

    } v;
};

#define is_heap_mfn(mfn) ({                         \
    unsigned long _mfn = (mfn);                         \
    (_mfn < phys_to_pfn(heap_phys_end));            \
})

extern struct page_info *frame_table;

extern unsigned long min_page;
extern unsigned long max_page;
extern unsigned long total_pages;

void init_frametable(void);

extern void __iomem *ioremap_pages(unsigned long phys_addr, unsigned int size, unsigned int mtype);

#define __arm_ioremap(p, s, m) ioremap_pages(p, s, m)


#define PG_shift(idx)   (BITS_PER_LONG - (idx))
#define PG_mask(x, idx) (x ## UL << PG_shift(idx))


/* Mutually-exclusive page states: { inuse, free }. */
#define PGC_state         PG_mask(3, 9)
#define PGC_state_inuse   PG_mask(0, 9)
#define PGC_state_free    PG_mask(3, 9)
#define page_state_is(pg, st) (((pg)->count_info&PGC_state) == PGC_state_##st)

extern struct domain *idle_domain[];

void dump_page(unsigned int pfn);

int get_ME_free_slot(unsigned int size);
int put_ME_slot(unsigned int ME_slotID);

void early_memory_init(void);
void memory_init(void);

void get_current_addrspace(addrspace_t *addrspace);
bool is_addrspace_equal(addrspace_t *addrspace1, addrspace_t *addrspace2);
void switch_mm(struct domain *d, addrspace_t *next_addrspace);

void *ioremap(unsigned long phys_addr, unsigned int size);

static inline int get_order_from_bytes(paddr_t size)
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

void clear_bss(void);

#endif /* MEMORY_H */
