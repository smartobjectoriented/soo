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

#ifndef __ASM_EVTCHN_H__
#define __ASM_EVTCHN_H__

#include <linux/interrupt.h>
#include <asm/ptrace.h>
#include <linux/smp.h>
#include <linux/irq.h>

#include <soo/hypervisor.h>
#include <soo/avz.h>

#include <soo/uapi/event_channel.h>
#include <soo/uapi/soo.h>
#include <soo/uapi/console.h>

#include <soo/vbstore.h>

#include <asm/ipipe_hwirq.h>

extern unsigned int evtchn_from_virq(int virq);
extern unsigned int evtchn_from_irq_data(struct irq_data *irq_data);

/* Binding types. */
enum {
	IRQT_UNBOUND, IRQT_PIRQ, IRQT_VIRQ, IRQT_IPI, IRQT_EVTCHN
};

static inline void clear_evtchn(u32 evtchn) {
	AVZ_shared->evtchn_pending[evtchn] = false;
}

static inline void notify_remote_via_evtchn(uint32_t evtchn)
{
	evtchn_send_t op;
	op.evtchn = evtchn;

	hypercall_trampoline(__HYPERVISOR_event_channel_op, EVTCHNOP_send, (long) &op, 0, 0);
}

/* Entry point for notifications into Linux subsystems. */
asmlinkage void evtchn_do_upcall(struct pt_regs *regs);

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
extern int bind_evtchn_to_virq_handler(unsigned int evtchn, irq_handler_t handler, irq_handler_t thread_fn, unsigned long irqflags, const char *devname, void *dev_id);
extern int bind_interdomain_evtchn_to_virqhandler(unsigned int remote_domain, unsigned int remote_evtchn, irq_handler_t handler, irq_handler_t thread_fn, unsigned long irqflags, const char *devname, void *dev_id);
extern int bind_existing_interdomain_evtchn(unsigned int local_channel, unsigned int remote_domain, unsigned int remote_evtchn);
extern int bind_virq_to_virqhandler(unsigned int virq, irq_handler_t handler, unsigned long irqflags, const char *devname, void *dev_id);

extern void virtshare_mask_irq(struct irq_data *irq_data);
extern void virtshare_unmask_irq(struct irq_data *irq_data);

int rtdm_bind_evtchn_to_virq_handler(rtdm_irq_t *irq_handle, unsigned int evtchn, rtdm_irq_handler_t handler, unsigned long irqflags, const char *devname, void *dev_id);
int rtdm_bind_interdomain_evtchn_to_virqhandler(rtdm_irq_t *irq_handle, unsigned int remote_domain, unsigned int remote_evtchn, rtdm_irq_handler_t handler, unsigned long irqflags, const char *devname, void *dev_id);
void rtdm_unbind_from_virqhandler(rtdm_irq_t *irq_handle);

int rtdm_bind_virq_to_ivrqhandler(rtdm_irq_t *irq_handle, unsigned int virq, rtdm_irq_handler_t handler, unsigned long irqflags, const char *devname, void *dev_id);

void __unmask_irq(unsigned int irq, struct irq_desc *desc);
void __ack_irq(unsigned int virq, struct irq_desc *desc);

/*
 * Common unbind function for all event sources. Takes IRQ to unbind from.
 * Automatically closes the underlying event channel (even for bindings
 * made with bind_evtchn_to_irqhandler()).
 */
extern void unbind_from_virqhandler(unsigned int virq, void *dev_id);
extern void unbind_domain_evtchn(unsigned int domID, unsigned int evtchn);

extern void mask_evtchn(int );
extern void unmask_evtchn(int );

/*
 * Unlike notify_remote_via_evtchn(), this is safe to use across
 * save/restore. Notifications on a broken connection are silently dropped.
 */
extern void notify_remote_via_virq(int virq);

void virq_init(void);

#endif
