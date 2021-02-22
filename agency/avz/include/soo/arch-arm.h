/*
 * Copyright (C) 2016 Daniel Rossier <daniel.rossier@soo.tech>
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

#ifndef __ARCH_ARM_H__
#define __ARCH_ARM_H__

#ifdef __AVZ__
#include <types.h>
#include <config.h>
#else
#include <asm/types.h>
#endif


/*
 * Virtual addresses beyond this are not modifiable by guest OSes. The
 * machine->physical mapping table starts at this address, read-only.
 */

/* Actually, the following constants are not relevant since machine_to_phys_mapping is calculated in hypervisor/memory.c */



#ifndef __ASSEMBLY__
extern unsigned long *machine_phys_mapping;
#define machine_to_phys_mapping  (machine_phys_mapping)
#endif

#ifndef __ASSEMBLY__

/* User-accessible registers */
/* This structure should NOT be modified !! */

typedef struct cpu_user_regs {
	__u32   r0;
	__u32   r1;
	__u32   r2;
	__u32   r3;
	__u32   r4;
	__u32   r5;
	__u32   r6;
	__u32   r7;
	__u32   r8;
	__u32   r9;
	__u32   r10;
	__u32   r11;
	__u32   r12;
	__u32   r13;
	__u32   r14;
	__u32   r15;
	__u32   psr;
	__u32   ctx;
} cpu_user_regs_t;

typedef struct cpu_sys_regs {
	__u32   vpsr;
	__u32   vksp;
	__u32   vusp;
	__u32   vdacr;
} cpu_sys_regs_t;

/* ONLY used to communicate with dom0! See also struct exec_domain. */
struct vcpu_guest_context {
	cpu_user_regs_t user_regs;         /* User-level CPU registers  */
	cpu_sys_regs_t	sys_regs;

	addr_t	event_callback;
	addr_t	domcall;
	addr_t	prep_switch_domain_callback;	/* Address of prepare_switch callback */
};

typedef struct vcpu_guest_context vcpu_guest_context_t;

#endif

#endif /* __ARCH_ARM__ */
