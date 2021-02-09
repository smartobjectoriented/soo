/*
 * Copyright (C) 2017 Baptiste Delporte <bonel@bonel.net>
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

#include <linux/version.h>
#include <linux/types.h>

#include <soo/debug/meminfo.h>

/* To get the address of the L2 page table from a L1 descriptor */
#define L1DESC_L2PT_BASE_ADDR_SHIFT	10
#define L1DESC_L2PT_BASE_ADDR_OFFSET	(1 << L1DESC_L2PT_BASE_ADDR_SHIFT)
#define L1DESC_L2PT_BASE_ADDR_MASK	(~(L1DESC_L2PT_BASE_ADDR_OFFSET - 1))

#define L1_PAGETABLE_ORDER      	12
#define L2_PAGETABLE_ORDER      	8

#define L1_PAGETABLE_ENTRIES    	(1 << L1_PAGETABLE_ORDER)
#define L2_PAGETABLE_ENTRIES    	(1 << L2_PAGETABLE_ORDER)

#define L1_PAGETABLE_SHIFT      	20
#define L2_PAGETABLE_SHIFT      	12

#define L1DESC_TYPE_MASK		0x3
#define L1DESC_TYPE_SECT 		0x2
#define L1DESC_TYPE_PT 	        	0x1

/* Dumping page tables */

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
				if ((i < 0xf80) || (i > 0xffc))
					for (j = 0; j < 256; j++) {
						l2pte = ((uint32_t *) __va(*l1pte & L1DESC_L2PT_BASE_ADDR_MASK)) + j;
						if (*l2pte)
							lprintk("      - L2 pte@%p (i2=%x) mapping %x  content: %x\n", l2pte, j, (i << 20) | (j << 12), *l2pte);
					}
			}
		}
	}
}


#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,0,0))

int debug_meminfo_proc_show(void)
{
	struct sysinfo i;
	struct vmalloc_info vmi;
	long cached;
	long available;
	unsigned long pagecache;
	unsigned long wmark_low = 0;
	unsigned long pages[NR_LRU_LISTS];
	struct zone *zone;
	int lru;

/*
 * display in kilobytes.
 */
#define K(x) ((x) << (PAGE_SHIFT - 10))
	si_meminfo(&i);
	si_swapinfo(&i);
	//committed = percpu_counter_read_positive(&vm_committed_as);

	cached = global_page_state(NR_FILE_PAGES) -
			total_swapcache_pages() - i.bufferram;
	if (cached < 0)
		cached = 0;

	get_vmalloc_info(&vmi);

	for (lru = LRU_BASE; lru < NR_LRU_LISTS; lru++)
		pages[lru] = global_page_state(NR_LRU_BASE + lru);

	for_each_zone(zone)
		wmark_low += zone->watermark[WMARK_LOW];

	/*
	 * Estimate the amount of memory available for userspace allocations,
	 * without causing swapping.
	 *
	 * Free memory cannot be taken below the low watermark, before the
	 * system starts swapping.
	 */
	available = i.freeram - wmark_low;

	/*
	 * Not all the page cache can be freed, otherwise the system will
	 * start swapping. Assume at least half of the page cache, or the
	 * low watermark worth of cache, needs to stay.
	 */
	pagecache = pages[LRU_ACTIVE_FILE] + pages[LRU_INACTIVE_FILE];
	pagecache -= min(pagecache / 2, wmark_low);
	available += pagecache;

	/*
	 * Part of the reclaimable slab consists of items that are in use,
	 * and cannot be freed. Cap this estimate at the low watermark.
	 */
	available += global_page_state(NR_SLAB_RECLAIMABLE) -
		     min(global_page_state(NR_SLAB_RECLAIMABLE) / 2, wmark_low);

	if (available < 0)
		available = 0;

	/*
	 * Tagged format, for easy grepping and expansion.
	 */
	lprintk("(ME) MemFree:        %8lu kB\n"
		"(ME) MemAvailable:   %8lu kB\n",
		K(i.freeram),
		K(available));

	return 0;
#undef K
}

#endif

