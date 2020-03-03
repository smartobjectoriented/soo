/******************************************************************************
 * event.h
 * 
 * A nice interface for passing asynchronous events to guest OSes.
 * 
 * Copyright (c) 2002-2006, K A Fraser
 */

#ifndef __EVENT_H__
#define __EVENT_H__

#include <avz/config.h>
#include <avz/sched.h>
#include <avz/smp.h>
#include <avz/softirq.h>
#include <asm/bitops.h>

/*
 * send_guest_vcpu_virq: Notify guest via a per-VCPU VIRQ.
 *  @v:        VCPU to which virtual IRQ should be sent
 *  @virq:     Virtual IRQ number (VIRQ_*)
 */
void send_guest_vcpu_virq(struct vcpu *v, int virq);

/*
 * send_guest_pirq:
 *  @d:        Domain to which physical IRQ should be sent
 *  @pirq:     Physical IRQ number
 * Returns TRUE if the delivery port was already pending.
 */
int send_guest_pirq(struct domain *d, int pirq);

/* Send a notification from a given domain's event-channel port. */
int evtchn_send(struct domain *d, unsigned int lport);

/* Bind a local event-channel port to the specified VCPU. */
long evtchn_bind_vcpu(unsigned int port, unsigned int vcpu_id);

/* Unmask a local event-channel port. */
int evtchn_unmask(unsigned int port);

bool handle_guest_bound_irq(unsigned int irq);

#endif /* __EVENT_H__ */
