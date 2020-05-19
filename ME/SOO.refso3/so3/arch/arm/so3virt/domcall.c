/*
 * Copyright (C) 2016-2018 Daniel Rossier <daniel.rossier@soo.tech>
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

#include <memory.h>

#include <asm/mmu.h>
#include <asm/cacheflush.h>

#include <mach/domcall.h>
#include <mach/uart.h>

#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/console.h>

/* Keep track of already updated base address entries.
 * A base address is encoded on 22 bits, since we have to keep track of such addr (coarse page table)
 */

/* We are able to keep track of base addresses for 2 MB RAM with 4 KB pages (1 bit per page) */

/* 128 KB */
#define BASEADDR_BITMAP_BYTES 	1 << 6

static unsigned char baseaddr_2nd_bitmap[BASEADDR_BITMAP_BYTES];

/* Init the bitmap */
static void init_baseaddr_2nd_bitmap(void) {
	int i;

	for (i = 0; i < BASEADDR_BITMAP_BYTES; i++)
		baseaddr_2nd_bitmap[i] = 0;
}

/*
 * Page tables can start at 1 KB-aligned address
 */
static void set_baseaddr_2nd_bitmap(unsigned int baseaddr) {
	unsigned int pos, mod;

	baseaddr = (baseaddr - CONFIG_RAM_BASE) >> 10;

	pos = baseaddr >> 3;
	mod = baseaddr % 8;

	baseaddr_2nd_bitmap[pos] |= (1 << (7 - mod));

}

static unsigned int is_set_baseaddr_2nd_bitmap(unsigned int baseaddr) {
	unsigned int pos, mod;

	baseaddr = (baseaddr - CONFIG_RAM_BASE) >> 10;

	pos = baseaddr >> 3;
	mod = baseaddr % 8;

	return (baseaddr_2nd_bitmap[pos] & (1 << (7 - mod)));

}


/****************************************************/

/* Page walking */

static void set_l2pte(uint32_t *l2pte, struct DOMCALL_fix_page_tables_args *args) {
	unsigned int base = 0;

	base = *l2pte & PAGE_MASK;
	base += pfn_to_phys(args->pfn_offset);

	/*
	 * Check if the physical address is really within the RAM (subject to be adujsted).
	 * I/O addresses must not be touched.
	 */
	if ((phys_to_pfn(base) >= args->min_pfn) && (phys_to_pfn(base) < args->min_pfn + args->nr_pages)) {

		*l2pte = (*l2pte & ~PAGE_MASK) | base;

		asm volatile("mcr p15, 0, %0, c7, c10, 1" : : "r" (l2pte));

		isb();
	}
}

/* Process all PTEs of a 2nd-level page table */
static void process_pte_pgtable(uint32_t *l1pte, struct DOMCALL_fix_page_tables_args *args) {

	uint32_t *l2pte;
	int i;

	l2pte = (uint32_t *) __va(*l1pte & L1DESC_L2PT_BASE_ADDR_MASK);

	/* Walk through the L2 PTEs. */

	for (i = 0; i < L2_PAGETABLE_ENTRIES; i++, l2pte++)
		if (*l2pte)
			set_l2pte(l2pte, args);
}

static void set_l1pte(uint32_t *l1pte, struct DOMCALL_fix_page_tables_args *args) {
	volatile unsigned int base;

	if ((*l1pte & L1DESC_TYPE_MASK) == L1DESC_TYPE_SECT)
		base = *l1pte & L1_SECT_MASK;
	else {
		ASSERT((*l1pte & L1DESC_TYPE_MASK) == L1DESC_TYPE_PT);
		base = (*l1pte & L1DESC_L2PT_BASE_ADDR_MASK);
	}

	base += pfn_to_phys(args->pfn_offset);

	/*
	 * Check if the physical address is really within the RAM (subject to be adjusted).
	 * I/O addresses must not be touched.
	 */
	if ((phys_to_pfn(base) >= args->min_pfn) && (phys_to_pfn(base) < args->min_pfn + args->nr_pages)) {

		if ((*l1pte & L1DESC_TYPE_MASK) == L1DESC_TYPE_SECT)
			*l1pte = (*l1pte & ~L1_SECT_MASK) | base;
		else
			*l1pte = (*l1pte & ~L1DESC_L2PT_BASE_ADDR_MASK) | base;


		asm volatile("mcr p15, 0, %0, c7, c10, 1" : : "r" (l1pte));

		isb();
	}

}

int adjust_l1_page_tables(unsigned long addr, unsigned long end, uint32_t *pgtable, struct DOMCALL_fix_page_tables_args *args)
{
	uint32_t *l1pte;
	unsigned int sect_addr;

	l1pte = l1pte_offset(pgtable, addr);

	sect_addr = (addr >> 20) - 1;

	do {
		sect_addr++;

		if (*l1pte)
			set_l1pte(l1pte, args);

		l1pte++;

	} while (sect_addr != ((end-1) >> 20));

	return 0;
}

int adjust_l2_page_tables(unsigned long addr, unsigned long end, uint32_t *pgtable, struct DOMCALL_fix_page_tables_args *args)
{
	uint32_t *l1pte;
	unsigned int base;
	unsigned int sect_addr;

	l1pte = l1pte_offset(pgtable, addr);
	sect_addr = (addr >> 20) - 1;

	do {
		sect_addr++;

		if ((*l1pte & L1DESC_TYPE_MASK) == L1DESC_TYPE_PT) {
			base = *l1pte & L1DESC_L2PT_BASE_ADDR_MASK;

			if (!is_set_baseaddr_2nd_bitmap(base)) {

				process_pte_pgtable(l1pte, args);

				/* We keep track of one pfn only, since they are always processed simultaneously */
				set_baseaddr_2nd_bitmap(base);
			}
		}

		l1pte++;

	} while (sect_addr != ((end-1) >> 20));

	return 0;
}

/****************************************************/

static int do_fix_kernel_page_table(struct DOMCALL_fix_page_tables_args *args)
{
	set_pfn_offset(args->pfn_offset);

	init_baseaddr_2nd_bitmap();

	/* IO mapping regions have to be adjusted as well. */

	adjust_l1_page_tables(IO_MAPPING_BASE, POST_MIGRATION_REMAPPING_MAX, __sys_l1pgtable, args);
	adjust_l2_page_tables(IO_MAPPING_BASE, POST_MIGRATION_REMAPPING_MAX, __sys_l1pgtable, args);

	/* We remap everything from the location where page sharing is expected until the very last page. */

	adjust_l1_page_tables(HYPERVISOR_VIRT_ADDR + HYPERVISOR_VIRT_SIZE, 0xffffffff, __sys_l1pgtable, args);
	adjust_l2_page_tables(HYPERVISOR_VIRT_ADDR + HYPERVISOR_VIRT_SIZE, 0xffffffff, __sys_l1pgtable, args);

	/* Flush all cache */
	flush_all();

	/* Set as processed */
#if 0 /* not sure.... */
	set_baseaddr_2nd_bitmap((pfn_to_phys(virt_to_pfn((unsigned int) __sys_l1pgtable[0]))));
#endif

	return 0;
}

static int do_fix_other_page_tables(struct DOMCALL_fix_page_tables_args *args) {

	/* All page tables used by processes must be adapted. */
	/* Not yet supported. */

	return 0;
}

/* Main callback function used by AVZ */
int domcall(int cmd, void *arg)
{
	int rc = 0;

	switch (cmd) {

	case DOMCALL_presetup_adjust_variables:
		rc = do_presetup_adjust_variables(arg);
		break;

	case DOMCALL_postsetup_adjust_variables:
		rc = do_postsetup_adjust_variables(arg);
		break;

	case DOMCALL_fix_kernel_page_table:
		rc = do_fix_kernel_page_table((struct DOMCALL_fix_page_tables_args *) arg);
		break;

	case DOMCALL_fix_other_page_tables:
		rc = do_fix_other_page_tables((struct DOMCALL_fix_page_tables_args *) arg);
		break;

	case DOMCALL_sync_domain_interactions:
		rc = do_sync_domain_interactions(arg);
		break;

	case DOMCALL_sync_directcomm:
		rc = do_sync_directcomm(arg);
		break;

	case DOMCALL_soo:
		rc = do_soo_activity(arg);
		break;

	default:
		printk("Unknowmn cmd %#x\n", cmd);
		rc = -1;
		break;
	}

	return rc;
}
