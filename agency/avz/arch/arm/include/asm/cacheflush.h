/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef CACHEFLUSH_H
#define CACHEFLUSH_H

#include <asm/processor.h>

void cache_clean_flush(void);
void dcache_flush(uint32_t *pte);

void flush_all(void);

void flush_icache_range(uint32_t start, uint32_t end);
void flush_kern_dcache_area(uint32_t p, uint32_t size);

/*
 * Ensure preceding writes to *p by this CPU are visible to
 * subsequent reads by other CPUs:
 */
static inline void __sync_cache_range_w(volatile void *p, size_t size)
{
	char *_p = (char *)p;

	flush_kern_dcache_area((uint32_t) _p, size);

}

/*
 * Ensure preceding writes to *p by other CPUs are visible to
 * subsequent reads by this CPU.  We must be careful not to
 * discard data simultaneously written by another CPU, hence the
 * usage of flush rather than invalidate operations.
 */
static inline void __sync_cache_range_r(volatile void *p, size_t size)
{
	char *_p = (char *)p;

	/* ... and inner cache: */
	flush_kern_dcache_area((uint32_t) _p, size);
}


#define sync_cache_w(ptr) __sync_cache_range_w(ptr, sizeof *(ptr))
#define sync_cache_r(ptr) __sync_cache_range_r(ptr, sizeof *(ptr))

#define __tlb_op(f, insnarg, arg)					\
	do {								\
		asm("mcr " insnarg : : "r" (arg) : "cc");			\
	} while (0)

#define tlb_op(f, regs, arg)	__tlb_op(f, "p15, 0, %0, " regs, arg)

/*
 *	flush_pte_entry
 *
 *	Flush a PTE entry (word aligned, or double-word aligned) to
 *	RAM if the TLB for the CPU we are running on requires this.
 *	This is typically used when we are creating or removing PTE entries.
 *
 */
static inline void flush_pte_entry(void *pte)
{

	tlb_op(TLB_DCLEAN, "c7, c10, 1	@ flush pte", pte);

	dsb();
}


#endif /* CACHEFLUSH_H */
