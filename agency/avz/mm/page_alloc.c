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

#include <config.h>
#include <types.h>
#include <lib.h>
#include <sched.h>
#include <spinlock.h>
#include <keyhandler.h>
#include <memory.h>

#include <asm/mmu.h>

#define round_pgdown(_p)  ((_p)&PAGE_MASK)
#define round_pgup(_p)    (((_p)+(PAGE_SIZE-1))&PAGE_MASK)

/*
 * Boot-time allocator
 */

static unsigned long first_valid_mfn = ~0UL;

static struct bootmem_region {
	unsigned long s, e; /* MFNs @s through @e-1 inclusive are free */
} *bootmem_region_list;

static unsigned int nr_bootmem_regions = 0;

static void boot_bug(int line)
{
	panic("Boot BUG at %s:%d\n", __FILE__, line);
}
#define BOOT_BUG_ON(p) if ( p ) boot_bug(__LINE__);

static void bootmem_region_add(unsigned long s, unsigned long e)
{
	unsigned int i;

	if ( (bootmem_region_list == NULL) && (s < e) )
		bootmem_region_list = pfn_to_virt(s++);

	if ( s >= e )
		return;

	for ( i = 0; i < nr_bootmem_regions; i++ )
		if ( s < bootmem_region_list[i].e )
			break;

	BOOT_BUG_ON((i < nr_bootmem_regions) && (e > bootmem_region_list[i].s));
	BOOT_BUG_ON(nr_bootmem_regions == (PAGE_SIZE / sizeof(struct bootmem_region)));

	memmove(&bootmem_region_list[i+1], &bootmem_region_list[i], (nr_bootmem_regions - i) * sizeof(*bootmem_region_list));

	bootmem_region_list[i] = (struct bootmem_region) { s, e };

	nr_bootmem_regions++;
}

void init_boot_pages(paddr_t ps, paddr_t pe)
{
	ps = round_pgup(ps);
	pe = round_pgdown(pe);
	if (pe <= ps)
		return;

	first_valid_mfn = min_t(unsigned long, ps >> PAGE_SHIFT, first_valid_mfn);

	bootmem_region_add(ps >> PAGE_SHIFT, pe >> PAGE_SHIFT);
}

unsigned long alloc_boot_pages(unsigned long nr_pfns, unsigned long pfn_align)
{
	unsigned long pg, _e;
	int i;

	for (i = nr_bootmem_regions - 1; i >= 0; i--)
	{
		struct bootmem_region *r = &bootmem_region_list[i];

		pg = (r->e - nr_pfns) & ~(pfn_align - 1);
		if ( pg < r->s )
			continue;

		_e = r->e;
		r->e = pg;
		bootmem_region_add(pg, _e);

		return pg;
	}

	BOOT_BUG_ON(1);
	return 0;
}

/*************************
 * BINARY BUDDY ALLOCATOR
 */

typedef struct page_list_head heap_by_order_t[MAX_ORDER + 1];
static heap_by_order_t *_heap;
#define heap(order) ((*_heap)[order])

static unsigned long *avail = NULL;
static long total_avail_pages;

static DEFINE_SPINLOCK(heap_lock);

static unsigned long init_node_heap(unsigned long mfn, unsigned long nr, bool_t *use_tail)
{
	static heap_by_order_t _heap_static;
	static unsigned long avail_static;
	static int initialised = 0;

	unsigned long needed = (sizeof(*_heap) + sizeof(*avail) + PAGE_SIZE - 1) >> PAGE_SHIFT;
	int j;

	if (!initialised) {

		_heap = &_heap_static;
		avail = &avail_static;
		initialised = 1;
		needed = 0;

	} else if (get_order_from_bytes(sizeof(*_heap)) == get_order_from_pages(needed)) {

		_heap = alloc_heap_pages(get_order_from_pages(needed), 0);
		BUG_ON(!_heap);
		avail = (void *)_heap + (needed << PAGE_SHIFT) - sizeof(*avail);
		needed = 0;

	} else {
		_heap = xmalloc(heap_by_order_t);
		avail = xmalloc_array(unsigned long, 1);
		BUG_ON(!_heap || !avail);
		needed = 0;
	}

	memset(avail, 0, sizeof(long));

	for (j = 0; j <= MAX_ORDER; j++)
		INIT_PAGE_LIST_HEAD(&(*_heap)[j]);

	return needed;
}

/* Allocate 2^@order contiguous pages. */
static struct page_info *alloc_pages(unsigned int order, unsigned int memflags)
{
	unsigned int i, j;
	unsigned long request = 1UL << order;
	struct page_info *pg;

	if (unlikely(order > MAX_ORDER))
		return NULL;

	spin_lock(&heap_lock);

	/*
	 * Start with requested node, but exhaust all node memory in requested
	 * zone before failing.
	 */

	/* Find smallest order which can satisfy the request. */
	for (j = order; j <= MAX_ORDER; j++)
		if ((pg = page_list_remove_head(&heap(j))))
			goto found;


	/* No suitable memory blocks. Fail the request. */

	spin_unlock(&heap_lock);
	return NULL;

	found:
	/* We may have to halve the chunk a number of times. */
	while (j != order)
	{
		PFN_ORDER(pg) = --j;
		page_list_add_tail(pg, &heap(j));
		pg += 1 << j;
	}

	ASSERT(*avail >= request);
	*avail -= request;
	total_avail_pages -= request;
	ASSERT(total_avail_pages >= 0);

	for (i = 0; i < (1 << order); i++)
	{
		/* Reference count must continuously be zero for free pages. */
		BUG_ON(pg[i].count_info != PGC_state_free);

		pg[i].count_info = PGC_state_inuse;

		/* Initialise fields which have other uses for free pages. */
		pg[i].u.inuse.type_info = 0;

	}

	spin_unlock(&heap_lock);

	return pg;
}


/* Free 2^@order set of pages. */
static void free_pages(struct page_info *pg, unsigned int order)
{
	unsigned long mask;
	unsigned int i;

	ASSERT(order <= MAX_ORDER);

	spin_lock(&heap_lock);

	for (i = 0; i < (1 << order); i++)
		pg[i].count_info = PGC_state_free;

	*avail += 1 << order;
	total_avail_pages += 1 << order;

	/* Merge chunks as far as possible. */
	while (order < MAX_ORDER)
	{
		mask = 1UL << order;

		if ((phys_to_pfn(page_to_phys(pg)) & mask))
		{
			/* Merge with predecessor block? */
			if (!pfn_valid(page_to_pfn(pg-mask)) || !page_state_is(pg-mask, free) || (PFN_ORDER(pg-mask) != order))
				break;

			pg -= mask;
			page_list_del(pg, &heap(node, order));
		}
		else
		{
			/* Merge with successor block? */
			if (!pfn_valid(page_to_pfn(pg+mask)) || !page_state_is(pg+mask, free) || (PFN_ORDER(pg+mask) != order))
				break;

			page_list_del(pg + mask, &heap(node, order));
		}

		order++;

	}

	PFN_ORDER(pg) = order;
	page_list_add_tail(pg, &heap(order));

	spin_unlock(&heap_lock);
}

/*
 * Hand the specified arbitrary page range to the specified heap zone
 * checking the node_id of the previous page.  If they differ and the
 * latter is not on a MAX_ORDER boundary, then we reserve the page by
 * not freeing it to the buddy allocator.
 */
static void __init_heap_pages(struct page_info *pg, unsigned long nr_pages)
{
	unsigned long i;

	for ( i = 0; i < nr_pages; i++ )
	{
		if (unlikely(!avail))
		{
			unsigned long s = page_to_pfn(pg + i);
			unsigned long e = page_to_pfn(pg + nr_pages - 1) + 1;
			bool_t use_tail = !(s & ((1UL << MAX_ORDER) - 1)) && (find_first_set_bit(e) <= find_first_set_bit(s));
			unsigned long n;

			n = init_node_heap(page_to_pfn(pg+i), nr_pages - i, &use_tail);
			BUG_ON(i + n > nr_pages);
			if (n && !use_tail)
			{
				i += n - 1;
				continue;
			}
			if (i + n == nr_pages)
				break;

			nr_pages -= n;
		}

		free_pages(pg+i, 0);
	}
}

static unsigned long avail_heap_pages(void)
{
	unsigned long free_pages;

	free_pages = *avail;

	return free_pages;
}

unsigned long total_free_pages(void)
{
	return total_avail_pages;
}

void init_heap_pages(paddr_t ps, paddr_t pe)
{
	ps = round_pgup(ps);
	pe = round_pgdown(pe);
	if (pe <= ps)
		return;

	/*
	 * Ensure there is a one-page buffer between AVZ and other zones, to
	 * prevent merging of power-of-two blocks across the zone boundary.
	 */
	if (ps && !is_heap_mfn(phys_to_pfn(ps)-1))
		ps += PAGE_SIZE;
	if (!is_heap_mfn(phys_to_pfn(pe)))
		pe -= PAGE_SIZE;

	__init_heap_pages(phys_to_page(ps), (pe - ps) >> PAGE_SHIFT);
}


void *alloc_heap_pages(unsigned int order, unsigned int memflags)
{
	struct page_info *pg;

	pg = alloc_pages(order, memflags);
	if (unlikely(pg == NULL))
		return NULL;

	return page_to_virt(pg);
}


void free_heap_pages(void *v, unsigned int order)
{
	if (v == NULL)
		return;

	free_pages(virt_to_page(v), order);
}


static void pagealloc_info(unsigned char key)
{
	printk("Physical memory information:\n");
	printk("    avz heap: %lukB free\n", avail_heap_pages() << (PAGE_SHIFT-10));

}

static struct keyhandler pagealloc_info_keyhandler = {
		.diagnostic = 1,
		.u.fn = pagealloc_info,
		.desc = "memory info"
};

static void pagealloc_keyhandler_init(void)
{
	register_keyhandler('m', &pagealloc_info_keyhandler);
}


static void dump_heap(unsigned char key)
{
	u64 now = NOW();

	printk("'%c' pressed -> dumping heap info (now-0x%X:%08X)\n", key, (u32)(now>>32), (u32)now);

	printk("heap -> %lu pages\n", *avail);
}

static struct keyhandler dump_heap_keyhandler = {
	.diagnostic = 1,
	.u.fn = dump_heap,
	.desc = "dump heap info"
};

void register_heap_trigger(void)
{
	register_keyhandler('H', &dump_heap_keyhandler);
}

void pagealloc_init(void) {

	pagealloc_keyhandler_init();
	register_heap_trigger();

}
