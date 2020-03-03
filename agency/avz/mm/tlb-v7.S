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

#include <asm/linkage.h>
#include <avz/init.h>
#include <generated/asm-offsets.h>
#include <asm/tlbflush.h>
#include <asm/cpu-ops.h>
#include <asm/page.h>
#include "proc-macros.S"

/*
 *	v7wbi_flush_kern_tlb_range(start,end)
 *
 *	Invalidate a range of kernel TLB entries
 *
 *	- start - start address (may not be aligned)
 *	- end   - end address (exclusive, may not be aligned)
 */
ENTRY(v7wbi_flush_kern_tlb_range)
	dsb
	mov	r0, r0, lsr #PAGE_SHIFT		@ align address
	mov	r1, r1, lsr #PAGE_SHIFT
	mov	r0, r0, lsl #PAGE_SHIFT
	mov	r1, r1, lsl #PAGE_SHIFT
1:

	mcr	p15, 0, r0, c8, c3, 1		@ TLB invalidate U MVA (shareable)

	add	r0, r0, #PAGE_SZ
	cmp	r0, r1
	blo	1b
	mov	r2, #0
	mcr	p15, 0, r2, c7, c5, 6		@ flush BTAC/BTB
	dsb
	isb
	mov	pc, lr

	__INITDATA

	.type	v7wbi_tlb_fns, #object
ENTRY(v7wbi_tlb_fns)
	.long	0
	.long	v7wbi_flush_kern_tlb_range
	.long	0
	.size	v7wbi_tlb_fns, . - v7wbi_tlb_fns
