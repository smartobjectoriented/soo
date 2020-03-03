/*
 * Copyright (C) 2016,2017 Daniel Rossier <daniel.rossier@soo.tech>
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

#ifndef __IRQ_H__
#define __IRQ_H__

#include <avz/config.h>
#include <avz/cpumask.h>
#include <avz/spinlock.h>
#include <avz/time.h>

#if !defined(__arm__)

struct irqaction {
    void (*handler)(int, void *);
    const char *name;
    void *dev_id;
    bool_t free_on_release;
};

/*
 * IRQ line status.
 */
#define IRQ_INPROGRESS	1	/* IRQ handler active - do not enter! */
#define IRQ_DISABLED	2	/* IRQ disabled - do not enter! */
#define IRQ_PENDING	4	/* IRQ pending - replay on enable */
#define IRQ_REPLAY	8	/* IRQ has been replayed but not acked yet */
#define IRQ_GUEST       16      /* IRQ is handled by guest OS(es) */
#define IRQ_GUEST_EOI_PENDING 32 /* IRQ was disabled, pending a guest EOI */
#define IRQ_MOVE_PENDING      64  /* IRQ is migrating to another CPUs */
#define IRQ_PER_CPU     256     /* IRQ is per CPU */


/*
 * Interrupt controller descriptor. This is all we need
 * to describe about the low-level hardware.
 */
struct hw_interrupt_type {
    const char *typename;
    unsigned int (*startup)(unsigned int irq);
    void (*shutdown)(unsigned int irq);
    void (*enable)(unsigned int irq);
    void (*disable)(unsigned int irq);
    void (*ack)(unsigned int irq);
    void (*end)(unsigned int irq);
    void (*set_affinity)(unsigned int irq, cpumask_t mask);
};

typedef const struct hw_interrupt_type hw_irq_controller;

#endif /* !defined(__arm__) */

#include <asm/irq.h>

#define	NR_VIRQS	2


void init_IRQ(void);
void trap_init(void);

#endif /* __IRQ_H__ */
