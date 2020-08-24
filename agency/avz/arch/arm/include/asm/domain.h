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

#ifndef __ASM_PROC_DOMAIN_H
#define __ASM_PROC_DOMAIN_H

/*
 * Domain numbers
 *
 *  DOMAIN_IO     - domain 2 includes all IO only
 *  DOMAIN_USER   - domain 1 includes all user memory only
 *  DOMAIN_KERNEL - domain 0 includes all kernel memory only
 *
 * The domain numbering depends on whether we support 36 physical
 * address for I/O or not.  Addresses above the 32 bit boundary can
 * only be mapped using supersections and supersections can only
 * be set for domain 0.  We could just default to DOMAIN_IO as zero,
 * but there may be systems with supersection support and no 36-bit
 * addressing.  In such cases, we want to map system memory with
 * supersections to reduce TLB misses and footprint.
 *
 * 36-bit addressing and supersections are only available on
 * CPUs based on ARMv6+ or the Intel XSC3 core.
 *
 * We cannot use domain 0 for the kernel on QSD8x50 since the kernel domain
 * is set to manager mode when set_fs(KERNEL_DS) is called. Setting domain 0
 * to manager mode will disable the workaround for a cpu bug that can cause an
 * invalid fault status and/or tlb corruption (CONFIG_VERIFY_PERMISSION_FAULT).
 */

#define DOMAIN_KERNEL	0
#define DOMAIN_TABLE	0
#define DOMAIN_USER	1
#define DOMAIN_IO	2

#include <asm/mmu.h>

#ifndef __ASSEMBLY__
#include <soo/uapi/avz.h>
#endif

/*
 * Domain types
 */
#define DOMAIN_NOACCESS 0
#define DOMAIN_CLIENT   1
#define DOMAIN_MANAGER  1

#define domain_val(dom,type)    ((type) << (2*(dom)))

#define DOMAIN_SUPERVISOR_VALUE
#define DOMAIN_IO_VALUE

#ifndef __ASSEMBLY__

#define set_domain(x)					\
	do {						\
	__asm__ __volatile__(				\
	"mcr	p15, 0, %0, c3, c0	@ set domain"	\
	  : : "r" (x));					\
	isb();						\
	} while (0)
#endif

#ifndef __ASSEMBLY__

#include <config.h>
#include <memory.h>
#include <cache.h>
#endif

#ifndef __ASSEMBLY__

#define set_domain(x)					\
	do {						\
	__asm__ __volatile__(				\
	"mcr	p15, 0, %0, c3, c0	@ set domain"	\
	  : : "r" (x));					\
	isb();						\
	} while (0)

#endif  /* !__ASSEMBLY__ */


#endif
