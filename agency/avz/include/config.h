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

#ifndef CONFIG_H
#define CONFIG_H

#define	NR_CPUS			4

/*
 * CPU #0 is the primary (non-RT) Agency CPU.
 * CPU #1 is the hard RT Agency CPU.
 * CPU #2 is the second (SMP) Agency CPU.
 * CPU #3 is the ME CPU.
 */

#define AGENCY_CPU		0
#define AGENCY_RT_CPU     	1

#define ME_CPU		 	3

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

#define HYPERVISOR_SIZE	0x00c00000  /* 12 MB */
#define HYPERVISOR_PHYS_START CONFIG_RAM_BASE

#define EXPORT_SYMBOL(var)
#define EXPORT_SYMBOL_GPL(var)

#define KERN_ERR       "<Error>"
#define KERN_CRIT      "<Critical>"
#define KERN_EMERG     "<Emergency>"
#define KERN_WARNING   "<Warning>"
#define KERN_NOTICE    "<Notice>"
#define KERN_INFO      "<Info>"
#define KERN_DEBUG     "<Debug>"

/* Linux 'checker' project. */
#define __iomem
#define __user
#define __force
#define __bitwise

#ifndef __ASSEMBLY__

int current_domain_id(void);
#define dprintk(_l, _f, _a...)                              \
    printk(_l "%s:%d: " _f, __FILE__ , __LINE__ , ## _a )
#define gdprintk(_l, _f, _a...)                             \
    printk(_l "%s:%d:d%d " _f, __FILE__,       \
           __LINE__, current_domain_id() , ## _a )

#include <compiler.h>

#endif /* !__ASSEMBLY__ */

#define __STR(...) #__VA_ARGS__
#define STR(...) __STR(__VA_ARGS__)

#ifndef __ASSEMBLY__

/* Turn a plain number into a C unsigned long constant. */
#define __mk_unsigned_long(x) x ## UL
#define mk_unsigned_long(x) __mk_unsigned_long(x)
/*
 * Pseudo-usr mode allows the hypervisor to switch back to the right stack (G-stach/H-stack) depending on whether
 * the guest issued a hypercall or if an interrupt occurred during some processing in the hypervisor.
 * 0 means we are in some hypervisor code, 1 means we are in some guest code.
 */
extern int pseudo_usr_mode[];

#else /* __ASSEMBLY__ */

/* In assembly code we cannot use C numeric constant suffixes. */
#define mk_unsigned_long(x) x
#endif /* !__ASSEMBLY__ */

#define fastcall
#define __cpuinitdata
#define __cpuinit


#endif /* CONFIG_H */
