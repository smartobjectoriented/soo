/* -*- linux-c -*-
 * arch/arm/include/asm/ipipe_base.h
 *
 * Copyright (C) 2007 Gilles Chanteperdrix.
 * Copyright (C) 2010 Philippe Gerum (SMP port).
 * Copyright (C) 2016-2019 Daniel Rossier (daniel.rossier@heig-vd.ch)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
 * USA; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __ASM_ARM_IPIPE_BASE_H
#define __ASM_ARM_IPIPE_BASE_H

#include <asm/irq.h>		/* For NR_IRQS */

#define ipipe_processor_id() raw_smp_processor_id()

#define IPIPE_ARCH_HAVE_VIRQ_IPI

/* ARM traps */
#define IPIPE_TRAP_ACCESS	 0	/* Data or instruction access exception */
#define IPIPE_TRAP_SECTION	 1	/* Section fault */
#define IPIPE_TRAP_DABT		 2	/* Generic data abort */
#define IPIPE_TRAP_UNKNOWN	 3	/* Unknown exception */
#define IPIPE_TRAP_BREAK	 4	/* Instruction breakpoint */
#define IPIPE_TRAP_FPU		 5	/* Floating point exception */
#define IPIPE_TRAP_VFP		 6	/* VFP floating point exception */
#define IPIPE_TRAP_UNDEFINSTR	 7	/* Undefined instruction */
#define IPIPE_TRAP_ALIGNMENT	 8	/* Unaligned access exception */
#define IPIPE_TRAP_MAYDAY        9	/* Internal recovery trap */
#define IPIPE_NR_FAULTS         10

#ifndef __ASSEMBLY__

#endif /* !__ASSEMBLY__ */

#endif /* __ASM_ARM_IPIPE_BASE_H */
