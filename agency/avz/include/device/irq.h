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

#ifndef IRQ_H
#define IRQ_H

#include <spinlock.h>
#include <list.h>

#include <soo/uapi/avz.h>
#include <soo/uapi/event_channel.h>

#include <asm/percpu.h>

/*
 * IRQ line status.
 *
 * Bits 0-16 are reserved for the IRQF_* bits in linux/interrupt.h
 *
 * IRQ types
 */
#define IRQ_TYPE_NONE		0x00000000	/* Default, unspecified type */
#define IRQ_TYPE_EDGE_RISING	0x00000001	/* Edge rising type */
#define IRQ_TYPE_EDGE_FALLING	0x00000002	/* Edge falling type */
#define IRQ_TYPE_EDGE_BOTH 	(IRQ_TYPE_EDGE_FALLING | IRQ_TYPE_EDGE_RISING)
#define IRQ_TYPE_LEVEL_HIGH	0x00000004	/* Level high type */
#define IRQ_TYPE_LEVEL_LOW	0x00000008	/* Level low type */
#define IRQ_TYPE_SENSE_MASK	0x0000000f	/* Mask of the above */
#define IRQ_TYPE_PROBE		0x00000010	/* Probing in progress */

/* IRQ ARM Timer is 26 in EL2, 27 in EL1 */
#define IRQ_ARCH_ARM_TIMER_EL2	26

#define NR_IRQS			1020

#define NR_VECTORS		NR_IRQS

#define IRQF_VALID		0x00000010
#define IRQF_SHARABLE		0x00000040
#define IRQF_ONESHOT        	0x00002000
#define IRQF_NOAUTOEN   	(1 << 2)
#define IRQF_PROBE          	(1 << 1)


#define IRQ_NONE        (0)
#define IRQ_HANDLED     (1)

#define	NR_VIRQS	2

extern volatile bool __in_interrupt;

void init_IRQ(void);
void trap_init(void);

#ifndef __ASSEMBLY__
struct irqaction;
extern void migrate_irqs(void);
#endif

struct irqdesc;
struct cpu_user_regs;

typedef int irqreturn_t;

typedef void (*irq_handler_t)(unsigned int, struct irqdesc *);

struct irqdesc {
	irq_handler_t		handler;

	spinlock_t		lock;

	unsigned int            irq_count;      /* For detecting broken IRQs */
} ;
typedef struct irqdesc irqdesc_t;

extern struct irqdesc irq_desc[NR_IRQS];

/* IRQ controller */
typedef struct  {

    void (*irq_enable)(unsigned int irq);
    void (*irq_disable)(unsigned int irq);
    void (*irq_mask)(unsigned int irq);
    void (*irq_unmask)(unsigned int irq);

    void (*irq_handle)(cpu_regs_t *regs);

} irq_ops_t;

extern irq_ops_t irq_ops;

void set_irq_handler(unsigned int irq, irq_handler_t handler);

void disable_irq(unsigned int);
void enable_irq(unsigned int);

int setup_irq(unsigned int, irq_handler_t);

int irq_set_affinity(unsigned int irq, int cpu);

#define get_irq_descriptor(irq) (irq_desc + irq)
#define irq_to_desc(irq) (irq_desc + irq)

extern void asm_do_IRQ(unsigned int irq);

DECLARE_PER_CPU(spinlock_t, intc_lock);

#endif /* IRQ_H */

