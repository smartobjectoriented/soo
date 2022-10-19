/*
 * Copyright (C) 2014-2018 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#if 0
#define DEBUG
#endif

#include <types.h>
#include <errno.h>
#include <spinlock.h>
#include <list.h>
#include <config.h>
#include <softirq.h>
#include <bitmap.h>
#include <sched.h>
#include <event.h>

#include <device/irq.h>

#include <device/arch/gic.h>

#include <soo/uapi/event_channel.h>
#include <soo/uapi/soo.h>
#include <soo/uapi/debug.h>

volatile bool __in_interrupt = false;

irq_ops_t irq_ops;

/* Global interrupt descriptor table */
struct irqdesc irq_desc[NR_IRQS];

int setup_irq(unsigned int irq, irq_handler_t handler)
{
	unsigned long flags;

	struct irqdesc *desc = get_irq_descriptor(irq);

	if (!desc)
		BUG();

	/*
	 * The following block of code has to be executed atomically
	 */
	flags = spin_lock_irqsave(&desc->lock);

	desc->handler = handler;

	irq_ops.irq_enable(irq);

	irq_set_affinity(irq, smp_processor_id());

	/* Reset broken irq detection when installing new handler */
	desc->irq_count = 0;

	spin_unlock_irqrestore(&desc->lock, flags);

	return 0;

}

void set_irq_handler(unsigned int irq, irq_handler_t handler)
{
	struct irqdesc *desc;
	unsigned long flags;

	if (irq >= NR_IRQS) {
		printk("Trying to install handler for IRQ%d\n", irq);
		return;
	}

	if (handler == NULL) {
		printk("Handler is not specificed\n");
		while (1);
	}

	desc = get_irq_descriptor(irq);

	flags = spin_lock_irqsave(&desc->lock);

	desc->handler = handler;

	spin_unlock_irqrestore(&desc->lock, flags);
}

void irq_handle(cpu_regs_t *regs) {

	/* The following boolean indicates we are currently in the interrupt call path.
	 * It will be reset at the end of the softirq processing.
	 */
	__in_interrupt = true;

	irq_ops.irq_handle(regs);

	/* Now perform the softirq processing in any case, i.e. even if the domain IRQs were off,
	 * avz may schedule to another domain along an hypercall.
	 */

	do_softirq();
}


/*
 * asm_do_IRQ() is the primary IRQ handler.
 */
void asm_do_IRQ(unsigned int irq)
{
	struct irqdesc *desc;

	DBG("%s(%d)\n", __func__, irq);

	if (irq >= NR_IRQS) {
		printk("Bad IRQ = %d\n", irq);
		BUG();
	}

	desc = get_irq_descriptor(irq);

	DBG("handler=%08x\n", desc->handler);
	desc->handler(irq, desc);
}

void init_irq(void)
{
	int irq;

	for (irq = 0; irq < NR_IRQS; irq++)
		spin_lock_init(&irq_desc[irq].lock);

	init_gic();
}

