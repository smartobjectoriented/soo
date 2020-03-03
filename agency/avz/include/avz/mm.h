/*
 * Copyright (C) 2016 Daniel Rossier <daniel.rossier@soo.tech>
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

#ifndef MM_H
#define MM_H__

#include <avz/config.h>
#include <avz/types.h>
#include <avz/spinlock.h>
#include <avz/list.h>

struct domain;
struct page_info;

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

unsigned long total_free_pages(void);

#define _MEMF_bits        24
#define  MEMF_bits(n)     ((n)<<_MEMF_bits)

#ifdef CONFIG_PAGEALLOC_MAX_ORDER
#define MAX_ORDER CONFIG_PAGEALLOC_MAX_ORDER
#else
#define MAX_ORDER 20 /* 2^20 contiguous pages */
#endif

#define page_list_entry list_head

#include <asm/mm.h>


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



#endif /* MM_H__ */
