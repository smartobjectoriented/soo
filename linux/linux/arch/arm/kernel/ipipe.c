/* -*- linux-c -*-
 * linux/arch/arm/kernel/ipipe.c
 *
 * Copyright (C) 2002-2005 Philippe Gerum.
 * Copyright (C) 2004 Wolfgang Grandegger (Adeos/arm port over 2.4).
 * Copyright (C) 2005 Heikki Lindholm (PowerPC 970 fixes).
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
 *
 * Architecture-dependent I-PIPE support for ARM.
 */

#include <linux/kernel.h>
#include <linux/smp.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/bitops.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kallsyms.h>
#include <linux/kprobes.h>
#include <linux/ipipe_trace.h>
#include <linux/irq.h>
#include <linux/irqnr.h>
#include <linux/prefetch.h>
#include <linux/cpu.h>
#include <linux/ipipe_domain.h>
#include <linux/ipipe_tickdev.h>

#include <asm/atomic.h>
#include <asm/hardirq.h>
#include <asm/io.h>
#include <asm/unistd.h>
#include <asm/mmu_context.h>

#include <soo/uapi/console.h>
#include <soo/evtchn.h>

void ipipe_set_irq_affinity(unsigned int irq, cpumask_t cpumask)
{
	struct cpumask __cpumask; /* paravirt */
	
	DBG("%s: data_chip for IRQ %d  0x%08X, set_aff = 0x%08X\n", __func__, 
		irq, 
		irqdescs[irq].irq_data.chip, 
		irqdescs[irq].irq_data.chip->irq_set_affinity);

	/* SOO.tech */
	/* VIRQ associated irqchip does not have a specific set_affinity function. */
	if ((irqdescs[irq].irq_data.chip == NULL) ||
	    (irqdescs[irq].irq_data.chip->irq_set_affinity == NULL))
		return ;

	cpumask_clear(&__cpumask);
	cpumask_set_cpu(smp_processor_id(), &__cpumask);

	irqdescs[irq].irq_data.chip->irq_set_affinity(&irqdescs[irq].irq_data, &__cpumask, true);

}
EXPORT_SYMBOL_GPL(ipipe_set_irq_affinity);

void ipipe_set_gic_enable(unsigned int irq) 
{
	irqdescs[irq].irq_data.chip->irq_unmask(&irqdescs[irq].irq_data);
}
EXPORT_SYMBOL_GPL(ipipe_set_gic_enable);

/**
 * IRQs are off
 */
void __ipipe_grab_irq(int irq, bool reset)
{

	ipipe_trace_irq_entry(irq);
	__ipipe_dispatch_irq(irq, reset);
	ipipe_trace_irq_exit(irq);
}

