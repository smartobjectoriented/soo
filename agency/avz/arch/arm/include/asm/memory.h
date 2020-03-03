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

#ifndef __ASM_ARM_MEMORY_H
#define __ASM_ARM_MEMORY_H

/*
 * Allow for constants defined here to be used from assembly code
 * by prepending the UL suffix only with actual C code compilation.
 */
#ifndef __ASSEMBLY__

#define UL(x) (x##UL)
void flush_all(void);

#else
#define UL(x) (x)
#endif


#include <avz/compiler.h>
#include <mach/memory.h>
#include <asm/sizes.h>


/*
 * Page offset: 3GB
 */
/* (DRE) Crucial !! */

#define PAGE_OFFSET		UL(0xff000000)
#define L_PAGE_OFFSET	UL(0xc0000000)

/*
 * Maximum size of Linux kernel linear mapping (within the 1 GB region)
 */
#define KERNEL_LINEAR_MAX_SIZE (PAGE_OFFSET - L_PAGE_OFFSET)

#define VECTORS_BASE UL(0xffff0000)

/*
 * Allow 16MB-aligned ioremap pages
 */
#define IOREMAP_MAX_ORDER	24


/*
 * Convert a physical address to a Page Frame Number and back
 */
#define	__phys_to_pfn(paddr)	((paddr) >> PAGE_SHIFT)
#define	__pfn_to_phys(pfn)	((pfn) << PAGE_SHIFT)

#ifndef __ASSEMBLY__
void make_heap_noncacheable(void);
#endif

#endif
