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
#include <asm/types.h>

#ifndef __ASSEMBLY__

#include <asm/memory.h>

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

#define HYPERVISOR_VIRT_START 0xFF000000

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

typedef struct trap_info {
	__u32	vector;
	__u32	flags;
	__u32	address;
} trap_info_t;

typedef struct cpu_ext_regs {
	__u64	wr0;
	__u64	wr1;
	__u64	wr2;
	__u64	wr3;
	__u64	wr4;
	__u64	wr5;
	__u64	wr6;
	__u64	wr7;
	__u64	wr8;
	__u64	wr9;
	__u64	wr10;
	__u64	wr11;
	__u64	wr12;
	__u64	wr13;
	__u64	wr14;
	__u64	wr15;
	__u32	wcssf;
	__u32	wcasf;
	__u32	wcgr0;
	__u32	wcgr1;
	__u32	wcgr2;
	__u32	wcgr3;
	__u32	wcid;
	__u32	wcon;
} cpu_ext_regs_t;

typedef struct cpu_sys_regs {
        __u32   vpsr;
        __u32   vksp;
        __u32   vusp;
        __u32   vdacr;
        __u32   guest_dacr;
        __u32	guest_tls;
        __u32   guest_tls_rw;
        __u32   guest_context_id;
        __u32   guest_per_cpu;
        __u32	guest_ttbr0;
        __u32   guest_ttbr1;
        __u32   guest_ttbcr;
        __u32   vfar;
        __u32   vfsr;
        __u32   vcp0;
        __u32   vcp1;
} cpu_sys_regs_t;

/* ONLY used to communicate with dom0! See also struct exec_domain. */
struct vcpu_guest_context {
    cpu_user_regs_t user_regs;         /* User-level CPU registers     */

    cpu_ext_regs_t	ext_regs;
    cpu_sys_regs_t	sys_regs;
    __u32		event_callback;
    __u32		domcall;
    __u32		prep_switch_domain_callback;	/* Address of prepare_switch callback */
};



typedef struct vcpu_guest_context vcpu_guest_context_t;

struct arch_vcpu_info {
};

#endif

#endif /* __ARCH_ARM__ */
