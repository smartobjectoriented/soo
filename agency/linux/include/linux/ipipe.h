/* -*- linux-c -*-
 * include/linux/ipipe.h
 *
 * Copyright (C) 2002-2014 Philippe Gerum.
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

#ifndef __LINUX_IPIPE_H
#define __LINUX_IPIPE_H

#include <linux/spinlock.h>
#include <linux/cache.h>
#include <linux/percpu.h>
#include <linux/irq.h>
#include <linux/thread_info.h>
#include <linux/ipipe_base.h>
#include <linux/ipipe_debug.h>
#include <asm/ptrace.h>
#include <asm/ipipe.h>

#include <linux/ipipe_domain.h>

void __ipipe_set_irq_pending(struct ipipe_domain *ipd, unsigned int irq);

#define prepare_arch_switch(next)			\
	do {						\
		local_irq_disable();		\
	} while(0)


void ipipe_request_irq(unsigned int irq, ipipe_irq_handler_t handler, void *cookie);

void ipipe_free_irq(struct ipipe_domain *ipd,
		    unsigned int irq);

void ipipe_raise_irq(unsigned int irq);

void ipipe_set_hooks(struct ipipe_domain *ipd,
		     int enables);

unsigned int ipipe_alloc_virq(void);

void ipipe_free_virq(unsigned int virq);

void ipipe_set_irq_affinity(unsigned int irq, cpumask_t cpumask);

void ipipe_enable_irq(unsigned int irq);

static inline void ipipe_disable_irq(unsigned int irq)
{
	struct irq_desc *desc;
	struct irq_chip *chip;

	desc = irq_to_desc(irq);
	if (desc == NULL)
		return;

	chip = irq_desc_get_chip(desc);

	if (WARN_ON_ONCE(chip->irq_disable == NULL && chip->irq_mask == NULL))
		return;

	if (chip->irq_disable)
		chip->irq_disable(&desc->irq_data);
	else
		chip->irq_mask(&desc->irq_data);
}

static inline void ipipe_handle_demuxed_irq(unsigned int cascade_irq)
{
	__ipipe_grab_irq(cascade_irq, false);
}

void ipipe_set_gic_enable(unsigned int irq);


ipipe_irqdesc_t *ipipe_irq_to_desc(unsigned int irq);

struct irq_chip *ipipe_irq_desc_get_chip(ipipe_irqdesc_t *desc);

void xnintr_core_clock_handler(void);

#endif	/* !__LINUX_IPIPE_H */
