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

#include <soo/arch-arm.h>

#include <soo/uapi/event_channel.h>
#include <soo/uapi/soo.h>
#include <soo/uapi/debug.h>


void handle_bad_irq(unsigned int irq, struct irqdesc *desc)
{
	BUG();
}

/*
 * NOP functions
 */
static void noop(unsigned int irq)
{
}

struct irqchip dummy_irq_chip = {
		.name           = "dummy",
		.startup        = noop,
		.enable         = noop,
		.disable        = noop,
		.ack            = noop,
		.mask           = noop,
		.unmask         = noop,
};

struct irqchip no_irq_chip = {
		.name           = "none",
		.startup        = noop,
		.enable         = noop,
		.disable        = noop,
		.ack            = noop,
};

/* Global interrupt descriptor table */
struct irqdesc irq_desc[NR_IRQS];

/*
 * default enable function
 */
static void default_enable(unsigned int irq)
{
	struct irqdesc *desc = get_irq_descriptor(irq);

	desc->chip->unmask(irq);
	desc->status &= ~IRQ_MASKED;
}

/*
 * default disable function
 */
static void default_disable(unsigned int irq)
{
}

/*
 * default startup function
 */
static void default_startup(unsigned int irq)
{
	struct irqdesc *desc = get_irq_descriptor(irq);

	desc->chip->enable(irq);
}

/*
 * Fixup enable/disable function pointers
 */
void irq_chip_set_defaults(struct irqchip *chip)
{
	if (!chip->enable)
		chip->enable = default_enable;
	if (!chip->disable)
		chip->disable = default_disable;
	if (!chip->startup)
		chip->startup = default_startup;

}

/**
 *	set_irq_data - set irq type data for an irq
 *	@irq:	Interrupt number
 *	@data:	Pointer to interrupt specific data
 *
 *	Set the hardware irq controller data for an irq
 */
int set_irq_chip_data(unsigned int irq, void *data)
{
	struct irqdesc *desc;
	unsigned long flags;

	if (irq >= NR_IRQS) {
		printk(KERN_ERR "Trying to install controller data for IRQ%d\n", irq);
		return -EINVAL;
	}

	desc = irq_desc + irq;
	spin_lock_irqsave(&desc->lock, flags);
	desc->data = data;
	spin_unlock_irqrestore(&desc->lock, flags);
	return 0;
}

void set_irq_base(unsigned int irq, unsigned int irq_base) {
	struct irqdesc *desc;
	unsigned long flags;

	if (irq >= NR_IRQS) {
		printk(KERN_ERR "Trying to install controller data for IRQ%d\n", irq);
	}

	desc = irq_desc + irq;

	if (desc->chip == NULL) {
		printk(KERN_ERR "Trying to set up chip data with no associated chip yet for IRQ: %d\n", irq);
	}

	spin_lock_irqsave(&desc->lock, flags);
	desc->irq_base = irq_base;
	spin_unlock_irqrestore(&desc->lock, flags);
}

void set_irq_reg_base(unsigned int irq, void *reg_base) {
	struct irqdesc *desc;
	unsigned long flags;

	if (irq >= NR_IRQS) {
		printk(KERN_ERR "Trying to install controller data for IRQ%d\n", irq);
	}

	desc = irq_desc + irq;

	if (desc->chip == NULL) {
		printk(KERN_ERR "Trying to set up chip data with no associated chip yet for IRQ: %d\n", irq);
	}

	spin_lock_irqsave(&desc->lock, flags);
	desc->reg_base = reg_base;
	spin_unlock_irqrestore(&desc->lock, flags);
}

asmlinkage void ll_handle_irq(void) {

	/* arch-dependant low-level processing */
	ll_entry_irq();

}

int setup_irq(unsigned int irq, struct irqaction *new)
{
	struct irqaction **p;
	unsigned long flags;

	struct irqdesc *desc = get_irq_descriptor(irq);

	if (!desc)
		BUG();

	/*
	 * The following block of code has to be executed atomically
	 */
	spin_lock_irqsave(&desc->lock, flags);

	/* There might be already an association with an action (like CP15 timer used by all CPUs for example).
	 * In this case, it is overriden with the new call.
	 */
	p = &desc->action;

	irq_chip_set_defaults(desc->chip);

	desc->status &= ~(IRQ_AUTODETECT | IRQ_WAITING | IRQ_INPROGRESS);

	if (!(desc->status & IRQ_NOAUTOEN)) {
		desc->status &= ~IRQ_DISABLED;
		desc->chip->startup(irq);
	}

	*p = new;

	/* Reset broken irq detection when installing new handler */
	desc->irq_count = 0;

	spin_unlock_irqrestore(&desc->lock, flags);

	new->irq = irq;

	return 0;

}

void set_irq_chip(unsigned int irq, struct irqchip *chip)
{
	struct irqdesc *desc;
	unsigned long flags;

	if (irq >= NR_IRQS) {
		printk("Trying to install chip for IRQ%d\n", irq);
		return;
	}

	if (chip == NULL) {
		printk("BAD CHIP BUG!!!\n");
		while(1);
	}

	desc = get_irq_descriptor(irq);

	spin_lock_irqsave(&desc->lock, flags);

	desc->chip = chip;

	spin_unlock_irqrestore(&desc->lock, flags);
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

	spin_lock_irqsave(&desc->lock, flags);

	desc->handler = handler;

	spin_unlock_irqrestore(&desc->lock, flags);
}

void set_irq_flags(unsigned int irq, unsigned int iflags)
{
	unsigned long flags;
	struct irqdesc *desc;

	if (irq >= NR_IRQS) {
		printk("Trying to set irq flags for IRQ%d\n", irq);
		while(1);
	}

	desc = get_irq_descriptor(irq);

	spin_lock_irqsave(&desc->lock, flags);

	desc->flags = iflags;

	spin_unlock_irqrestore(&desc->lock, flags);
}



irqreturn_t handle_event(unsigned int irq, struct irqaction *action)
{
	irqreturn_t ret;

	ASSERT(action != NULL);

	DBG("action->handler=%08x\n", action->handler);

	ret = action->handler(irq, action->dev_id);

	ASSERT(local_irq_is_disabled());

	return ret;
}

/**
 *	handle_fasteoi_irq - irq handler for transparent controllers
 *	@irq:	the interrupt number
 *	@desc:	the interrupt description structure for this irq
 *
 *	Only a single callback will be issued to the chip: an ->eoi()
 *	call when the interrupt has been serviced. This enables support
 *	for modern forms of interrupt handlers, which handle the flow
 *	details in hardware, transparently.
 */
void handle_fasteoi_irq(unsigned int irq, struct irqdesc *desc)
{
	irqaction_t *action;

	spin_lock(&desc->lock);

	if (unlikely(desc->status & IRQ_INPROGRESS))
		goto out_unlock;

	desc->status &= ~(IRQ_REPLAY | IRQ_WAITING);

	action = desc->action;

	/*
	 * If its disabled or no action available
	 * then mask it and get out of here:
	 */

	if (unlikely(!action || (desc->status & IRQ_DISABLED))) {
		desc->status |= IRQ_PENDING;
		desc->chip->mask(irq);
		goto out_unlock;
	}


	desc->chip->mask(irq);

	spin_unlock(&desc->lock);

	handle_event(irq, action);

	desc->chip->unmask(irq);

	if (desc->chip->eoi)
		desc->chip->eoi(irq);

	return ;

out_unlock:
	spin_unlock(&desc->lock);
}

/*
 * asm_do_IRQ() is the primary IRQ handler.
 */
asmlinkage void asm_do_IRQ(unsigned int irq)
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

	for (irq = 0; irq < NR_IRQS; irq++) {
		irq_desc[irq].status |= IRQ_NO_REQUEST | IRQ_NO_PROBE;

		INIT_LIST_HEAD(&irq_desc[irq].bound_domains);

		spin_lock_init(&irq_desc[irq].lock);
	}

	init_gic();
}

