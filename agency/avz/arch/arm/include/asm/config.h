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

#ifndef __ARM_CONFIG_H__
#define __ARM_CONFIG_H__

#include <mach/memory.h>
#include <generated/autoconf.h>

#define AGENCY_CPU		0
#define AGENCY_RT_CPU     	1

#define	NR_CPUS			4

#define ME_STANDARD_CPU 	2
#define ME_RT_CPU		3


/* (DRE) Define the top of usable memory by the hypervisor and guest... */
#define	TOP_PHYS_MEMORY		(CONFIG_RAM_BASE + CONFIG_RAM_SIZE - 1)

/*
 * Allow for constants defined here to be used from assembly code
 * by prepending the UL suffix only with actual C code compilation.
 */
#ifndef __ASSEMBLY__
#define UL(x) (x##UL)
#else
#define UL(x) (x)
#endif

/* align addr on a size boundary - adjust address up/down if needed */
#define ALIGN_UP(addr,size) (((addr)+((size)-1))&(~((size)-1)))
#define ALIGN_DOWN(addr,size) ((addr)&(~((size)-1)))

/* We keep the STACK_SIZE to 8192 in order to have a similar stack_size as guest OS in SVC mode */
#define STACK_ORDER 1
#define STACK_SIZE  (PAGE_SIZE << STACK_ORDER)

#define HEAP_MAX_SIZE_MB 	(2)

/* Hypervisor owns top 64MB of virtual address space. */
#define HYPERVISOR_VIRT_START   0xFF000000

#define HYPERVISOR_SIZE	0x00c00000  /* 12 MB */
#define HYPERVISOR_PHYS_START CONFIG_RAM_BASE

#endif /* __ARM_CONFIG_H__ */
