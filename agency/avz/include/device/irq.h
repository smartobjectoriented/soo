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
#define IRQ_TYPE_EDGE_BOTH (IRQ_TYPE_EDGE_FALLING | IRQ_TYPE_EDGE_RISING)
#define IRQ_TYPE_LEVEL_HIGH	0x00000004	/* Level high type */
#define IRQ_TYPE_LEVEL_LOW	0x00000008	/* Level low type */
#define IRQ_TYPE_SENSE_MASK	0x0000000f	/* Mask of the above */
#define IRQ_TYPE_PROBE		0x00000010	/* Probing in progress */

#define IRQ_ARCH_ARM_TIMER	27

#define NR_IRQS	256

#define NR_VECTORS	NR_IRQS

#define IRQF_VALID			0x00000010
#define IRQF_SHARABLE		0x00000040
#define IRQF_ONESHOT        0x00002000
#define IRQF_NOAUTOEN   	(1 << 2)
#define IRQF_PROBE          (1 << 1)

#define IRQ_INPROGRESS			0x00000100

/* Internal flags */
#define IRQ_DISABLED            0x00000200      /* IRQ disabled - do not enter! */
#define IRQ_PENDING             0x00000400      /* IRQ pending - replay on enable */
#define IRQ_REPLAY              0x00000800      /* IRQ has been replayed but not acked yet */
#define IRQ_AUTODETECT          0x00001000      /* IRQ is being autodetected */
#define IRQ_WAITING             0x00002000      /* IRQ not yet seen - for autodetection */
#define IRQ_LEVEL               0x00004000      /* IRQ level triggered */
#define IRQ_MASKED              0x00008000      /* IRQ masked - shouldn't be seen again */
#define IRQ_PER_CPU             0x00010000      /* IRQ is per CPU */
#define IRQ_NO_PROBE            0x00020000      /* IRQ is not valid for probing */
#define IRQ_NO_REQUEST          0x00040000      /* IRQ cannot be requested */
#define IRQ_NOAUTOEN            0x00080000      /* IRQ will not be enabled on request irq */
#define IRQ_WAKEUP              0x00100000      /* IRQ triggers system wakeup */
#define IRQ_MOVE_PENDING        0x00200000      /* need to re-target IRQ destination */
#define IRQ_NO_BALANCING        0x00400000      /* IRQ is excluded from balancing */
#define IRQ_MOVE_PCNTXT         0x01000000      /* IRQ migration from process context */
#define IRQ_AFFINITY_SET        0x02000000      /* IRQ affinity was set from userspace*/
#define IRQ_GUEST_EOI_PENDING   0x08000000      /* IRQ was disabled, pending a guest EOI */

#define IRQ_NONE        (0)
#define IRQ_HANDLED     (1)

#define IRQ_DELAYED_DISABLE	0x10000000	/* IRQ disable (masking) happens delayed. */


/* Borrowed from Linux include/linux/interrupt.h */
/*
 * These flags used only by the kernel as part of the
 * irq handling routines.
 *
 * IRQF_SAMPLE_RANDOM - irq is used to feed the random generator
 * IRQF_SHARED - allow sharing the irq among several devices
 * IRQF_PROBE_SHARED - set by callers when they expect sharing mismatches to occur
 * IRQF_PERCPU - Interrupt is per cpu
 * IRQF_NOBALANCING - Flag to exclude this interrupt from irq balancing
 * IRQF_IRQPOLL - Interrupt is used for polling (only the interrupt that is
 *                registered first in an shared interrupt is considered for
 *                performance reasons)
 */

#define IRQF_SAMPLE_RANDOM	0x00000040
#define IRQF_SHARED		0x00000080
#define IRQF_PROBE_SHARED	0x00000100
#define IRQF_PERCPU		0x00000400
#define IRQF_NOBALANCING	0x00000800
#define IRQF_IRQPOLL		0x00001000


/*
 * Use this value to indicate lack of interrupt
 * capability
 */
#ifndef NO_IRQ
#define NO_IRQ	((unsigned int)(-1))
#endif


#define IRQT_NOEDGE	(0)
#define IRQT_RISING	(__IRQT_RISEDGE)
#define IRQT_FALLING	(__IRQT_FALEDGE)
#define IRQT_BOTHEDGE	(__IRQT_RISEDGE|__IRQT_FALEDGE)
#define IRQT_LOW	(__IRQT_LOWLVL)
#define IRQT_HIGH	(__IRQT_HIGHLVL)
#define IRQT_PROBE	IRQ_TYPE_PROBE

#define	NR_VIRQS	2

void init_IRQ(void);
void trap_init(void);

#undef vector_to_irq
#define vector_to_irq(vec) (vec)
#undef irq_to_vector
#define irq_to_vector(irq) (irq)

#define domain_pirq_to_irq(d, pirq) ((d)->arch.pirq_irq[pirq])
#define domain_irq_to_pirq(d, irq) ((d)->arch.irq_pirq[irq])

#ifndef __ASSEMBLY__
struct irqaction;
extern void migrate_irqs(void);
#endif

struct irqaction;
struct pt_regs;
struct irqdesc;
struct seq_file;
struct cpu_user_regs;

typedef int irqreturn_t;
typedef void (*irq_control_t)(unsigned int);
typedef void (*irq_handler_t)(unsigned int, struct irqdesc *);

typedef struct irqaction {
	irqreturn_t 		(*handler)(int, void *);
	const char 		*name;
	void 			*dev_id;
	int irq;
} irqaction_t;


struct irqchip {
	/*
	 * Acknowledge the IRQ.
	 * If this is a level-based IRQ, then it is expected to mask the IRQ
	 * as well.
	 */
	void (*ack)(unsigned int);
	/*
	 * Mask the IRQ in hardware.
	 */
	void (*mask)(unsigned int);
	/*
	 * Unmask the IRQ in hardware.
	 */
	void (*unmask)(unsigned int);

	const char      *name;
	void    	(*startup)(unsigned int irq);

	void            (*enable)(unsigned int irq);
	void            (*disable)(unsigned int irq);

	void            (*mask_ack)(unsigned int irq);
	void            (*eoi)(unsigned int irq);

};
typedef struct irqchip irqchip_t;

/* Simple list for handling bound domains */
typedef struct {
	struct domain *dom;
	struct list_head list;
} bound_domains_t;

struct irqdesc {
	char			*type;
	irq_handler_t		handler;
	struct irqchip		*chip;
	struct irqaction 	*action;
	unsigned int		flags;
	unsigned int		status;
	spinlock_t		lock;
	void			*chipdata;
	void			*data;
	unsigned int            irq_count;      /* For detecting broken IRQs */

	/* List of bound domain for this IRQ (required for IPIs for example) */
	struct list_head	bound_domains;

	/* Possibly having different irq banks with different irq start and base address */
	unsigned int irq_base;
	void *reg_base;
} ;
typedef struct irqdesc irqdesc_t;

extern struct irqdesc irq_desc[NR_IRQS];

extern struct irqdesc irq_desc[NR_IRQS];

int set_irq_chip_data(unsigned int irq, void *data);
#define set_irq_data(irq, data) set_irq_chip_data(irq, data)

#define get_irq_chip_data(irq)       (irq_desc[irq].data)
#define get_irq_data(irq)            (irq_desc[irq].data)
#define get_irq_chip(irq)            (irq_desc[irq].chip)

void set_irq_chip(unsigned int irq, struct irqchip *);
void set_irq_flags(unsigned int irq, unsigned int flags);
void set_irq_handler(unsigned int irq, irq_handler_t handler);

void set_irq_base(unsigned int irq, unsigned int irq_base);
void set_irq_reg_base(unsigned int irq, void *reg_base);

void disable_irq(unsigned int);
void enable_irq(unsigned int);
int set_irq_type(unsigned int irq, unsigned int type);
int setup_irq(unsigned int, struct irqaction *);

/* IRQ action dispatcher */
void handle_fasteoi_irq(unsigned int irq, struct irqdesc *desc);

/* IRQ action for the ARCH_TIMER PPI */
void handle_arm_arch_timer_irq(unsigned int irq, struct irqdesc *desc);

#define get_irq_descriptor(irq) (irq_desc + irq)
#define irq_to_desc(irq) (irq_desc + irq)

extern void arch_irq_init(void); /* (LSU) */
extern void free_irq(unsigned int irq);

extern void asm_do_IRQ(unsigned int irq);

extern void ll_entry_irq(void);

DECLARE_PER_CPU(spinlock_t, intc_lock);

void dev_irq_init(void);

#endif /* IRQ_H */

