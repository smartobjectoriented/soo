/* -*- linux-c -*-
 * include/linux/ipipe_base.h
 *
 * Copyright (C) 2002-2014 Philippe Gerum.
 *               2007 Jan Kiszka.
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

#ifndef __LINUX_IPIPE_BASE_H
#define __LINUX_IPIPE_BASE_H

#include <linux/irq.h>

#include <asm/irq.h>

struct kvm_vcpu;
struct ipipe_vm_notifier;

typedef int (*ipipe_irq_handler_t)(unsigned int irq, void *cookie);

/* Cobalt IRQ descriptor */
typedef struct {
	unsigned int irq;
	ipipe_irq_handler_t handler;
	struct irq_data irq_data;
	struct irq_common_data common_data;
	void *data;
	void *xnintr;
	raw_spinlock_t lock;
} ipipe_irqdesc_t;

extern ipipe_irqdesc_t irqdescs[NR_PIRQS + NR_VIRQS];

extern void (*ipipe_assign_chip)(ipipe_irqdesc_t *irqdesc);

#include <asm/ipipe_base.h>
#include <linux/compiler.h>
#include <linux/linkage.h>

#ifndef IPIPE_NR_ROOT_IRQS
#define IPIPE_NR_ROOT_IRQS	NR_IRQS
#endif /* !IPIPE_NR_ROOT_IRQS */


/* Interrupt control bits */
#define IPIPE_HANDLE_FLAG	0
#define IPIPE_STICKY_FLAG	1
#define IPIPE_LOCK_FLAG		2

#define IPIPE_HANDLE_MASK	(1 << IPIPE_HANDLE_FLAG)
#define IPIPE_STICKY_MASK	(1 << IPIPE_STICKY_FLAG)
#define IPIPE_LOCK_MASK		(1 << IPIPE_LOCK_FLAG)

struct pt_regs;
struct ipipe_domain;

struct ipipe_trap_data {
	int exception;
	struct pt_regs *regs;
};

#define IPIPE_KEVT_SCHEDULE	0
#define IPIPE_KEVT_SIGWAKE	1
#define IPIPE_KEVT_SETSCHED	2
#define IPIPE_KEVT_SETAFFINITY	3
#define IPIPE_KEVT_EXIT		4
#define IPIPE_KEVT_CLEANUP	5
#define IPIPE_KEVT_HOSTRT	6

struct ipipe_vm_notifier {
	void (*handler)(struct ipipe_vm_notifier *nfy);
};

void __ipipe_init_early(void);
void __ipipe_init_post(void);

void __ipipe_init(void);

void __ipipe_restore_root_nosync(unsigned long x);

#define IPIPE_IRQF_NOACK    0x1
#define IPIPE_IRQF_NOSYNC   0x2

void __ipipe_dispatch_irq(unsigned int irq, bool reset);

void __ipipe_lock_irq(unsigned int irq);

void __ipipe_unlock_irq(unsigned int irq);

void __ipipe_set_RT_IRQ(unsigned int irq);


static inline void __ipipe_init_taskinfo(struct task_struct *p) { }

void __xnintr_irq_handler(unsigned int irq);

#endif	/* !__LINUX_IPIPE_BASE_H */
