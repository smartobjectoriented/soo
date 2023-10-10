/*   -*- linux-c -*-
 *   include/linux/ipipe_domain.h
 *
 *   Copyright (C) 2007-2012 Philippe Gerum.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
 *   USA; either version 2 of the License, or (at your option) any later
 *   version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __LINUX_IPIPE_DOMAIN_H
#define __LINUX_IPIPE_DOMAIN_H

#ifdef CONFIG_IPIPE

#include <linux/mutex.h>
#include <linux/percpu.h>
#include <linux/ipipe_base.h>

#include <asm/irqflags.h>
#include <asm/ptrace.h>

struct task_struct;
struct mm_struct;
struct irq_desc;
struct ipipe_vm_notifier;

#define __IPIPE_SYSCALL_P  0
#define __IPIPE_TRAP_P     1
#define __IPIPE_KEVENT_P   2
#define __IPIPE_SYSCALL_E (1 << __IPIPE_SYSCALL_P)
#define __IPIPE_TRAP_E	  (1 << __IPIPE_TRAP_P)
#define __IPIPE_KEVENT_E  (1 << __IPIPE_KEVENT_P)
#define __IPIPE_ALL_E	   0x7
#define __IPIPE_SYSCALL_R (8 << __IPIPE_SYSCALL_P)
#define __IPIPE_TRAP_R	  (8 << __IPIPE_TRAP_P)
#define __IPIPE_KEVENT_R  (8 << __IPIPE_KEVENT_P)
#define __IPIPE_SHIFT_R	   3
#define __IPIPE_ALL_R	  (__IPIPE_ALL_E << __IPIPE_SHIFT_R)

typedef void (*ipipe_irq_ackfn_t)(unsigned int irq, struct irq_desc *desc);

struct ipipe_percpu_data {
	struct pt_regs tick_regs;
	int hrtimer_irq;
};

/*
 * CAREFUL: all accessors based on __ipipe_raw_cpu_ptr() you may find
 * in this file should be used only while hw interrupts are off, to
 * prevent from CPU migration regardless of the running domain.
 */
DECLARE_PER_CPU(struct ipipe_percpu_data, ipipe_percpu);

#endif /* CONFIG_IPIPE */

#endif	/* !__LINUX_IPIPE_DOMAIN_H */
