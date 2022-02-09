/* -*- linux-c -*-
 * arch/arm/include/asm/ipipe_hwirq.h
 *
 * Copyright (C) 2002-2005 Philippe Gerum.
 * Copyright (C) 2005 Stelian Pop.
 * Copyright (C) 2006-2008 Gilles Chanteperdrix.
 * Copyright (C) 2010 Philippe Gerum (SMP port).
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

#ifndef _ASM_ARM_IPIPE_HWIRQ_H
#define _ASM_ARM_IPIPE_HWIRQ_H


#define hard_local_irq_save()		arch_local_irq_save()
#define hard_local_irq_restore(x)	arch_local_irq_restore(x)
#define hard_local_irq_enable()		arch_local_irq_enable()
#define hard_local_irq_disable()	arch_local_irq_disable()
#define hard_irqs_disabled()		irqs_disabled()

#define hard_irqs_disabled_flags(flags) arch_irqs_disabled_flags(flags)

#define hard_smp_local_irq_save()		hard_local_irq_save()
#define hard_smp_local_irq_restore(flags)	hard_local_irq_restore(flags)

#endif /* _ASM_ARM_IPIPE_HWIRQ_H */
