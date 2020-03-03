/*
 *  linux/include/asm-arm/cacheflush.h
 *
 *  Copyright (C) 1999-2002 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _ASMARM_CACHEFLUSH_H
#define _ASMARM_CACHEFLUSH_H

#include <asm/cachetype.h>

#define sync_cache_w(ptr) __sync_cache_range_w(ptr, sizeof *(ptr))
#define sync_cache_r(ptr) __sync_cache_range_r(ptr, sizeof *(ptr))

struct cpu_cache_fns {
	void (*flush_kern_all)(void);
	void (*coherent_kern_range)(unsigned long, unsigned long);
	void (*flush_kern_dcache_area)(void *, size_t size);
};


#define	cpu_flush_cache_all 		flush_cache_all
#define cpu_clean_cache_range		clean_cache_range
#define cpu_clean_cache_entry		flush_cache_page
#define cpu_flush_cache_entry		flush_cache_page

extern struct cpu_cache_fns cpu_cache;

#define __cpuc_flush_kern_all		cpu_cache.flush_kern_all
#define __cpuc_coherent_kern_range	cpu_cache.coherent_kern_range
#define __cpuc_flush_dcache_area 	cpu_cache.flush_kern_dcache_area


/*
 * Convert calls to our calling convention.
 */
#define flush_cache_all()		__cpuc_flush_kern_all()

/*
 * Perform necessary cache operations to ensure that data previously
 * stored within this range of addresses can be executed by the CPU.
 */
#define flush_icache_range(s,e)		__cpuc_coherent_kern_range(s,e)

/*
 * Perform necessary cache operations to ensure that the TLB will
 * see data written in the specified area.
 */
#define clean_dcache_area(start,size)	cpu_dcache_clean_area(start, size)

#define clean_cache_range __cpuc_clean_cache_range

/*
 * There is no __cpuc_clean_dcache_area but we use it anyway for
 * code intent clarity, and alias it to __cpuc_flush_dcache_area.
 */
#define __cpuc_clean_dcache_area __cpuc_flush_dcache_area

/*
 * Ensure preceding writes to *p by this CPU are visible to
 * subsequent reads by other CPUs:
 */
static inline void __sync_cache_range_w(volatile void *p, size_t size)
{
	char *_p = (char *)p;

	__cpuc_clean_dcache_area(_p, size);

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
	__cpuc_flush_dcache_area(_p, size);
}


#endif
