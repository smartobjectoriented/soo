/*
 * Copyright (C) 2016-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef EVTCHN_H
#define EVTCHN_H

#include <asm/processor.h>

#include <device/irq.h>

#include <soo/hypervisor.h>
#include <soo/event_channel.h>

#include <soo/soo.h>

#include <soo/console.h>
#include <soo/vbstore.h>

extern unsigned int type_from_irq(int irq);

extern unsigned int evtchn_from_irq(int irq);

/* Binding types. */
enum {
	IRQT_UNBOUND, IRQT_PIRQ, IRQT_VIRQ, IRQT_IPI, IRQT_EVTCHN
};

static inline void transaction_clear_bit(int evtchn, volatile unsigned long *p) {
	unsigned long tmp;
	unsigned long b, old_b;
	unsigned long mask = ~(1UL << (evtchn & 31));
	unsigned long x, ret, mask2;
	unsigned long flags;

	p += evtchn >> 5;

	flags = local_irq_save();

	__asm__ __volatile__("@transaction_clear_bit\n"
			"	ldr	%5, [%7] \n"
			"	and	%1, %5, %6 \n"
			"50:	mov %4, %1\n"
			"51:ldrex   %0, [%7]\n"
			"   strex   %3, %4, [%7]\n"
			"   teq     %3, #0\n"
			"   bne     51b\n"
			"	cmp	%0, %5\n"
			"	eorne	%2,%0, %5\n"
			"	mvnne	%3, %2\n"
			"	andne	%1,%1,%3\n"
			"	andne	%3, %0, %2\n"
			"	orrne	%1,%1, %3\n"
			"	movne %5, %4\n"
			"	bne		50b\n"
			: "=&r" (x), "=&r" (ret), "=&r" (mask2),"=&r" (tmp) , "=&r" (b),"=&r" (old_b)
			  :"r" (mask), "r" (p)
			   : "cc", "memory");

	local_irq_restore(flags);

}

static inline void transaction_set_bit(int evtchn, volatile unsigned long *p) {
	unsigned long tmp;
	unsigned long b, old_b;
	unsigned long mask = 1UL << (evtchn & 31);
	unsigned long x, ret, mask2;
	unsigned long flags;

	p += evtchn >> 5; /* p = p/32 since we have 32 bits per int */

	flags = local_irq_save();

	__asm__ __volatile__("@transaction_set_bit\n"
			"	ldr	%5, [%7] \n"
			"	orr	%1, %5, %6 \n"
			"50:	mov	%4, %1\n"
			"51:ldrex   %0, [%7]\n"
			"   strex   %3, %4, [%7]\n"
			"   teq     %3, #0\n"
			"   bne     51b\n"
			"	cmp	%0, %5\n"
			"	eorne	%2,%0, %5\n"
			"	mvnne	%3, %2\n"
			"	andne	%1,%1,%3\n"
			"	andne	%3, %0, %2\n"
			"	orrne	%1,%1, %3\n"
			"	movne %5, %4\n"
			"	bne		50b\n"
			: "=&r" (x), "=&r" (ret), "=&r" (mask2),"=&r" (tmp) , "=&r" (b),"=&r" (old_b)
			  :"r" (mask), "r" (p)
			   : "cc", "memory");

	local_irq_restore(flags);
}

/* Inline Functions */
/* Constructor for packed IRQ information. */
static inline u32 mk_irq_info(u32 type, u32 index, u32 evtchn) {
	return ((type << 24) | (index << 16) | evtchn);
}

static inline void clear_evtchn(u32 evtchn) {
	volatile shared_info_t *s = avz_shared_info;

	transaction_clear_bit(evtchn, &s->evtchn_pending[0]);

	dmb();

}
static inline int notify_remote_via_evtchn(uint32_t evtchn) {
	evtchn_send_t op;
	op.evtchn = evtchn;

	return hypercall_trampoline(__HYPERVISOR_event_channel_op, EVTCHNOP_send, (long) &op, 0, 0);
}

/* Entry point for notifications into Linux subsystems. */
void evtchn_do_upcall(cpu_regs_t *regs);

/*
 * LOW-LEVEL DEFINITIONS
 */

/*
 * Dynamically bind an event source to an IRQ-like callback handler.
 * On some platforms this may not be implemented via the Linux IRQ subsystem.
 * The IRQ argument passed to the callback handler is the same as returned
 * from the bind call. It may not correspond to a Linux IRQ number.
 * Returns IRQ or negative errno.
 * UNBIND: Takes IRQ to unbind from; automatically closes the event channel.
 */
extern int bind_evtchn_to_irq_handler(unsigned int evtchn, irq_handler_t handler, irq_handler_t thread_fn, void *data);
extern int bind_interdomain_evtchn_to_irqhandler(unsigned int remote_domain, unsigned int remote_evtchn, irq_handler_t handler, irq_handler_t thread_fn, void *data);
extern void bind_virq_to_irqhandler(unsigned int virq, irq_handler_t handler, irq_handler_t thread_fn, void *data);

extern int bind_existing_interdomain_evtchn(unsigned int local_channel, unsigned int remote_domain, unsigned int remote_evtchn);


extern unsigned int virq_to_irq(unsigned int virq);
extern int bind_virq_to_irq(unsigned int virq);

/*
 * Common unbind function for all event sources. Takes IRQ to unbind from.
 * Automatically closes the underlying event channel (even for bindings
 * made with bind_evtchn_to_irqhandler()).
 */
extern void unbind_from_irqhandler(unsigned int irq);
extern int unbind_domain_evtchn(unsigned int domID, unsigned int evtchn);

extern void mask_evtchn(int );
extern void unmask_evtchn(int );

extern unsigned int type_from_irq(int irq);
/*
 * Unlike notify_remote_via_evtchn(), this is safe to use across
 * save/restore. Notifications on a broken connection are silently dropped.
 */
extern void notify_remote_via_irq(int irq);

void virq_init(void);

#endif /* EVTCHN_H */
