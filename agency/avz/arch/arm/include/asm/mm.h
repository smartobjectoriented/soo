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

#ifndef ASM_MM_H
#define ASM_MM_H

#include <avz/types.h>
#include <avz/list.h>
#include <avz/cpumask.h>
#include <avz/mm.h>

#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/bitops.h>

extern int __irq_safe[];

extern unsigned long heap_phys_end;
extern int boot_of_mem_avail(int pos, ulong *start, ulong *end);

/* Used to characterize memory type used for create_mapping() typically */
struct mem_type {
  unsigned int prot_pte;
  unsigned int prot_pte_ext;
  unsigned int prot_l1;
  unsigned int prot_sect;
  unsigned int domain;
};


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
    (_mfn < paddr_to_pfn(heap_phys_end));            \
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

void switch_domain_address_space(struct domain *from, struct domain *to);

void write_ptbase(struct vcpu *v);
void save_ptbase(struct vcpu *v);
void dump_page(unsigned int pfn);

int get_ME_free_slot(unsigned int size);
int put_ME_slot(unsigned int ME_slotID);

void early_memory_init(void);
void memory_init(void);

#endif /* ASM_MM_H */

