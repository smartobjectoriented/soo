/*
 * Copyright (C) 2001-2013 Philippe Gerum <rpm@xenomai.org>.
 * Copyright (C) 2004-2006 Gilles Chanteperdrix <gilles.chanteperdrix@xenomai.org>.
 *
 * Xenomai is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Xenomai is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Xenomai; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */
#ifndef _COBALT_X86_ASM_THREAD_H
#define _COBALT_X86_ASM_THREAD_H

#include <linux/sched.h>

#include <asm/ptrace.h>
#include <asm/traps.h>

typedef union fpregs_state x86_fpustate;
#define x86_fpustate_ptr(t) ((t)->fpu.state)

struct xnthread;
struct ipipe_trap_data;
struct xnarchtcb;

struct archinfo {
	struct xnthread *thread;  /* Refer to the parent xnthread (used by current) */
};

struct xnarchtcb {
	struct task_struct *task;
	unsigned long	start_pc;

	struct fpu *kfpu;

	unsigned int root_kfpu: 1;
	struct {
		unsigned long ip;
		unsigned long ax;
	} mayday;
};


void xnarch_save_fpu(struct xnthread *thread);

#define xnarch_fpu_ptr(tcb)     ((tcb)->fpup)

#define xnarch_fault_regs(d)	((d)->regs)
#define xnarch_fault_trap(d)	((d)->exception)
#define xnarch_fault_code(d)	((d)->regs->orig_ax)
#define xnarch_fault_pc(d)	((d)->regs->ip)
#define xnarch_fault_fpu_p(d)	((d)->exception == X86_TRAP_NM)
#define xnarch_fault_pf_p(d)	((d)->exception == X86_TRAP_PF)
#define xnarch_fault_bp_p(d)	((current->ptrace & PT_PTRACED) &&	\
				 ((d)->exception == X86_TRAP_DB || (d)->exception == X86_TRAP_BP))
#define xnarch_fault_notify(d)	(!xnarch_fault_bp_p(d))

void xnarch_switch_fpu(struct xnthread *from, struct xnthread *to);

int xnarch_handle_fpu_fault(struct xnthread *from, 
			struct xnthread *to, struct ipipe_trap_data *d);

void xnarch_cleanup_thread(struct xnthread *thread);
void xnarch_init_thread(struct xnthread *thread);

void xnarch_switch_to(struct xnthread *out, struct xnthread *in);

static inline void xnarch_enter_root(struct xnthread *root) { }

void mach_x86_thread_init(void);
void mach_x86_thread_cleanup(void);

register unsigned long current_stack_pointer asm ("rsp");

/* 16 KB RT task stack size */
#define XNTHREAD_STACK_SIZE 	(THREAD_SIZE)

#define XNTHREAD_START_SP  	(XNTHREAD_STACK_SIZE - 8)

#endif /* !_COBALT_X86_ASM_THREAD_H */
