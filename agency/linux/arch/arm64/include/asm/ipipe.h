/* -*- linux-c -*-
 * arch/arm/include/asm/ipipe.h
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

#ifndef __ARM_IPIPE_H
#define __ARM_IPIPE_H

#define BROKEN_BUILTIN_RETURN_ADDRESS
#undef __BUILTIN_RETURN_ADDRESS0
#undef __BUILTIN_RETURN_ADDRESS1
#ifdef CONFIG_FRAME_POINTER
#define __BUILTIN_RETURN_ADDRESS0 arm_return_addr(0)
#define __BUILTIN_RETURN_ADDRESS1 arm_return_addr(1)
extern unsigned long arm_return_addr(int level);
#else
#define __BUILTIN_RETURN_ADDRESS0 ((unsigned long)__builtin_return_address(0))
#define __BUILTIN_RETURN_ADDRESS1 (0)
#endif

#include <linux/jump_label.h>
#include <linux/ipipe_trace.h>
#include <linux/ipipe_base.h>

#define IPIPE_CORE_RELEASE	9

/* SOO.tech */
#define NR_IPI_MAX		10

struct ipipe_domain;

extern ipipe_irqdesc_t *ipis_desc;

unsigned long long __ipipe_mach_get_tsc(void);
#define __ipipe_tsc_get() __ipipe_mach_get_tsc()
#ifndef __ipipe_hrclock_freq
extern unsigned long __ipipe_hrtimer_freq;
#define __ipipe_hrclock_freq __ipipe_hrtimer_freq
#endif /* !__ipipe_mach_hrclock_freq */


#define ipipe_read_tsc(t)	do { t = __ipipe_tsc_get(); } while(0)
#define __ipipe_read_timebase()	__ipipe_tsc_get()

#define ipipe_tsc2ns(t) \
({ \
	unsigned long long delta = (t)*1000; \
	do_div(delta, __ipipe_hrclock_freq / 1000000 + 1); \
	(unsigned long)delta; \
})
#define ipipe_tsc2us(t) \
({ \
	unsigned long long delta = (t); \
	do_div(delta, __ipipe_hrclock_freq / 1000000 + 1); \
	(unsigned long)delta; \
})

static inline const char *ipipe_clock_name(void)
{
	return "ipipe_tsc";
}

/* Private interface -- Internal use only */

#define __ipipe_enable_irq(irq)		enable_irq(irq)
#define __ipipe_disable_irq(irq)	disable_irq(irq)

void __ipipe_grab_irq(int irq, bool reset);

void __ipipe_exit_irq(struct pt_regs *regs);


#endif	/* !__ARM_IPIPE_H */
