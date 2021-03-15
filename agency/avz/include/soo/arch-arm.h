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

#include <types.h>
#include <config.h>

#include <asm/processor.h>

/*
 * Virtual addresses beyond this are not modifiable by guest OSes. The
 * machine->physical mapping table starts at this address, read-only.
 */

/* Actually, the following constants are not relevant since machine_to_phys_mapping is calculated in hypervisor/memory.c */

#ifndef __ASSEMBLY__


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
