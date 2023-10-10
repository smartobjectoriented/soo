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

#ifdef CONFIG_ARM

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

void dump_pgtable(unsigned long *l1pgtable) {

	int i, j;
	unsigned long *l1pte, *l2pte;

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
						l2pte = ((unsigned long *) __va(*l1pte & L1DESC_L2PT_BASE_ADDR_MASK)) + j;
						if (*l2pte)
							lprintk("      - L2 pte@%p (i2=%x) mapping %x  content: %x\n", l2pte, j, (i << 20) | (j << 12), *l2pte);
					}
			}
		}
	}
}
#endif

#ifdef CONFIG_ARM64

#define TTB_L0_ORDER      9
#define TTB_L1_ORDER      9
#define TTB_L2_ORDER      9
#define TTB_L3_ORDER      9

#define TTB_I0_SHIFT	  39
#define TTB_I0_MASK	  (~((1 << TTB_I0_SHIFT)-1))

#define TTB_I1_SHIFT	  30
#define TTB_I1_MASK	  (~((1 << TTB_I1_SHIFT)-1))

#define TTB_I2_SHIFT	  21
#define TTB_I2_MASK	  (~((1 << TTB_I2_SHIFT)-1))

#define TTB_I3_SHIFT	  12
#define TTB_I3_MASK	  (~((1 << TTB_I3_SHIFT)-1))

#define TTB_L0_ENTRIES    (1 << TTB_L0_ORDER)
#define TTB_L1_ENTRIES    (1 << TTB_L1_ORDER)
#define TTB_L2_ENTRIES    (1 << TTB_L2_ORDER)
#define TTB_L3_ENTRIES    (1 << TTB_L3_ORDER)

/* Block related */
#define TTB_L1_BLOCK_ADDR_SHIFT		30
#define TTB_L1_BLOCK_ADDR_OFFSET	(1 << TTB_L1_BLOCK_ADDR_SHIFT)
#define TTB_L1_BLOCK_ADDR_MASK		((~(TTB_L1_BLOCK_ADDR_OFFSET - 1)) & ((1UL << 48) - 1))

#define TTB_L2_BLOCK_ADDR_SHIFT		21
#define TTB_L2_BLOCK_ADDR_OFFSET	(1 << TTB_L2_BLOCK_ADDR_SHIFT)
#define TTB_L2_BLOCK_ADDR_MASK		((~(TTB_L2_BLOCK_ADDR_OFFSET - 1)) & ((1UL << 48) - 1))

/* Table related */
#define TTB_L0_TABLE_ADDR_SHIFT		12
#define TTB_L0_TABLE_ADDR_OFFSET	(1 << TTB_L0_TABLE_ADDR_SHIFT)
#define TTB_L0_TABLE_ADDR_MASK		((~(TTB_L0_TABLE_ADDR_OFFSET - 1)) & ((1UL << 48) - 1))

#define TTB_L1_TABLE_ADDR_SHIFT		TTB_L0_TABLE_ADDR_SHIFT
#define TTB_L1_TABLE_ADDR_OFFSET	TTB_L0_TABLE_ADDR_OFFSET
#define TTB_L1_TABLE_ADDR_MASK		TTB_L0_TABLE_ADDR_MASK

#define TTB_L2_TABLE_ADDR_SHIFT		TTB_L0_TABLE_ADDR_SHIFT
#define TTB_L2_TABLE_ADDR_OFFSET	TTB_L0_TABLE_ADDR_OFFSET
#define TTB_L2_TABLE_ADDR_MASK		TTB_L0_TABLE_ADDR_MASK

#define TTB_L3_PAGE_ADDR_SHIFT		12
#define TTB_L3_PAGE_ADDR_OFFSET		(1 << TTB_L3_PAGE_ADDR_SHIFT)
#define TTB_L3_PAGE_ADDR_MASK		((~(TTB_L3_PAGE_ADDR_OFFSET - 1)) & ((1UL << 48) - 1))

#define PTE_TYPE_FAULT		(0 << 0)
#define PTE_TYPE_TABLE		(3 << 0)
#define PTE_TYPE_BLOCK		(1 << 0)
#define PTE_TYPE_VALID		(1 << 0)


static inline int pte_type(unsigned long *pte)
{
	return *pte & PTE_TYPE_MASK;
}


void dump_pgtable(unsigned long *l0pgtable) {

	unsigned long i, j, k, l;
	unsigned long *l0pte, *l1pte, *l2pte, *l3pte;

	lprintk("           ***** Page table dump *****\n");

	for (i = 0; i < TTB_L0_ENTRIES; i++) {
		l0pte = l0pgtable + i;
		if (*l0pte) {

			lprintk("  - L0 pte@%lx (idx %x) mapping %lx content: %lx\n", l0pgtable+i, i, i << TTB_I0_SHIFT, *l0pte);
			BUG_ON(pte_type(l0pte) != PTE_TYPE_TABLE);

			/* Walking through the blocks/table entries */
			for (j = 0; j < TTB_L1_ENTRIES; j++) {
				l1pte = ((unsigned long *) __va(*l0pte & TTB_L0_TABLE_ADDR_MASK)) + j;
				if (*l1pte) {
					if (pte_type(l1pte) == PTE_TYPE_TABLE) {
						lprintk("    (TABLE) L1 pte@%lx (idx %x) mapping %lx content: %lx\n", l1pte, j,
								(i << TTB_I0_SHIFT) + (j << TTB_I1_SHIFT), *l1pte);

						for (k = 0; k < TTB_L2_ENTRIES; k++) {
							l2pte = ((unsigned long *) __va(*l1pte & TTB_L1_TABLE_ADDR_MASK)) + k;
							if (*l2pte) {
								if (pte_type(l2pte) == PTE_TYPE_TABLE) {
									lprintk("    (TABLE) L2 pte@%lx (idx %x) mapping %lx content: %lx\n", l2pte, k,
											(i << TTB_I0_SHIFT) + (j << TTB_I1_SHIFT) + (k << TTB_I2_SHIFT), *l2pte);

									for (l = 0; l < TTB_L3_ENTRIES; l++) {
										l3pte = ((unsigned long *) __va(*l2pte & TTB_L2_TABLE_ADDR_MASK)) + l;
										if (*l3pte)
											lprintk("      (PAGE) L3 pte@%lx (idx %x) mapping %lx content: %lx\n", l3pte, l,
													(i << TTB_I0_SHIFT) + (j << TTB_I1_SHIFT) + (k << TTB_I2_SHIFT) + (l << TTB_I3_SHIFT), *l3pte);
									}
								} else {
									/* Necessary of BLOCK type */
									BUG_ON(pte_type(l2pte) != PTE_TYPE_BLOCK);
									lprintk("      (PAGE) L2 pte@%lx (idx %x) mapping %lx content: %lx\n", l2pte, k,
											(i << TTB_I0_SHIFT) + (j << TTB_I1_SHIFT) + (k << TTB_I2_SHIFT), *l2pte);
								}
							}
						}
					} else {
						/* Necessary of BLOCK type */
						BUG_ON(pte_type(l1pte) != PTE_TYPE_BLOCK);

						lprintk("      (PAGE) L1 pte@%lx (idx %x) mapping %lx content: %lx\n", l1pte, j, (i << TTB_I0_SHIFT) + (j << TTB_I1_SHIFT), *l1pte);
					}
				}
			}

		}
	}
}

#endif

