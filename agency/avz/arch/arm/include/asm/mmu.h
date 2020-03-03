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

/*
 * This header file comes from SO3. It contains the general MMU-related definition and is intended to
 * replace the defines which are currently used in AVZ in the future.
 */

#ifndef __ARM_MMU_H
#define __ARM_MMU_H

#define L1_SYS_PAGE_TABLE_OFFSET	0x4000

/* Define the number of entries in each page table */


#define L1_PAGETABLE_ORDER      	12
#define L2_PAGETABLE_ORDER      	8

#define L1_PAGETABLE_ENTRIES    	(1 << L1_PAGETABLE_ORDER)
#define L2_PAGETABLE_ENTRIES    	(1 << L2_PAGETABLE_ORDER)

#define L1_PAGETABLE_SHIFT      	20
#define L2_PAGETABLE_SHIFT      	12

#define L1_PAGETABLE_SIZE       	(PAGE_SIZE << 2)

/* To get the address of the L2 page table from a L1 descriptor */
#define L1DESC_L2PT_BASE_ADDR_SHIFT	10
#define L1DESC_L2PT_BASE_ADDR_OFFSET	(1 << L1DESC_L2PT_BASE_ADDR_SHIFT)
#define L1DESC_L2PT_BASE_ADDR_MASK	(~(L1DESC_L2PT_BASE_ADDR_OFFSET - 1))

/* Page table type */

#define L1_SECT_SIZE			(0x100000)
#define L1_SECT_MASK            	(~(L1_SECT_SIZE - 1))

#define L1DESC_SECT_XN             	(1 << 4)

#define L1DESC_SECT_DOMAIN_MASK       	(0xf << 5)
#define L1DESC_PT_DOMAIN_MASK       	(0xf << 5)

#define PTE_DESC_DOMAIN_0		(0x0 << 5)

#define L1DESC_SECT_AP01	     	(1 << 10)
#define L1DESC_SECT_AP2			(0 << 15)

#define L1DESC_TYPE_MASK		0x3
#define L1DESC_TYPE_SECT 		0x2
#define L1DESC_TYPE_PT 	        	0x1

/* L2 page table attributes */
#define L2DESC_SMALL_PAGE_ADDR_MASK	(~(PAGE_SIZE-1))

/* AP[0] is used as Access Flag.
 * Access Flag (AF): ARMv8.0 requires that software manages the Access flag. This means an Access flag fault is generated whenever
 * an attempt is made to read into the TLB a translation table descriptor entry for which the value of the Access flag
 * is 0.
 * AP[1] and AP[2] are set to 1 for read/write at any privilege.
 */
#define L2DESC_SMALL_PAGE_AP01		(1 << 4)
#define L2DESC_SMALL_PAGE_AP2		(0 << 9)
#define L2DESC_PAGE_TYPE_SMALL		0x2

/* Common attributes for L1 and L2 */
#define DESC_BUFFERABLE     	(1 << 2)
#define DESC_CACHEABLE      	(1 << 3)
#define DESC_CACHE		(DESC_BUFFERABLE | DESC_CACHEABLE)


/* Given a virtual address, get an entry offset into a page table. */
#define l1pte_index(a) ((((uint32_t) a) >> L1_PAGETABLE_SHIFT) & (L1_PAGETABLE_ENTRIES - 1))
#define l2pte_index(a) ((((uint32_t) a) >> L2_PAGETABLE_SHIFT) & (L2_PAGETABLE_ENTRIES - 1))

#define l1pte_offset(pgtable, addr)     (pgtable + l1pte_index(addr))
#define l2pte_offset(l1pte, addr) 	(((uint32_t *) (__va(*l1pte) & L1DESC_L2PT_BASE_ADDR_MASK)) + l2pte_index(addr))
#define l2pte_first(l1pte)		(((uint32_t *) (__va(*l1pte) & L1DESC_L2PT_BASE_ADDR_MASK)))


#endif
