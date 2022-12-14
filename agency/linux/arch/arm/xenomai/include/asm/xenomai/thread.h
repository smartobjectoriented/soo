/*
 * Copyright (C) 2005 Stelian Pop
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
#ifndef _COBALT_ARM_ASM_THREAD_H
#define _COBALT_ARM_ASM_THREAD_H

#ifdef CONFIG_XENO_ARCH_FPU
#ifdef CONFIG_VFP
#include <asm/vfp.h>
#endif /* CONFIG_VFP */
#endif /* !CONFIG_XENO_ARCH_FPU */

struct xnthread;
struct ipipe_trap_data;
struct xnarchtcb;
struct thread_info;

struct xnarchtcb {

	/* Need to be here at first since used by __asm_thread_switch */
	unsigned long   r4;
	unsigned long   r5;
	unsigned long   r6;
	unsigned long   r7;
	unsigned long   r8;
	unsigned long   r9;
	unsigned long   sl;
	unsigned long   fp;
	unsigned long   sp;
	unsigned long   pc;

	struct task_struct *task;
	struct thread_info *ti;

	unsigned long	start_pc;

	union vfp_state *fpup;

#define xnarch_fpu_ptr(tcb)     ((tcb)->fpup)

	struct {
		unsigned long pc;
		unsigned long r0;
#ifdef __ARM_EABI__
		unsigned long r7;
#endif
#ifdef CONFIG_ARM_THUMB
		unsigned long psr;
#endif
	} mayday;
};

#define xnarch_fault_regs(d)	((d)->regs)
#define xnarch_fault_trap(d)	((d)->exception)
#define xnarch_fault_code(d)	(0)
#define xnarch_fault_pc(d)	((d)->regs->ARM_pc - (thumb_mode((d)->regs) ? 2 : 4)) /* XXX ? */

#define xnarch_fault_pf_p(d)	((d)->exception == IPIPE_TRAP_ACCESS)
#define xnarch_fault_bp_p(d)	((current->ptrace & PT_PTRACED) &&	\
				 ((d)->exception == IPIPE_TRAP_BREAK ||	\
				  (d)->exception == IPIPE_TRAP_UNDEFINSTR))

#define xnarch_fault_notify(d) (!xnarch_fault_bp_p(d))

void xnarch_switch_to(struct xnthread *out, struct xnthread *in);

#if defined(CONFIG_XENO_ARCH_FPU) && defined(CONFIG_VFP)


void xnarch_init_shadow_tcb(struct xnthread *thread);

int xnarch_fault_fpu_p(struct ipipe_trap_data *d);

void xnarch_save_fpu(struct xnthread *thread);

void xnarch_switch_fpu(struct xnthread *from, struct xnthread *thread);

int xnarch_handle_fpu_fault(struct xnthread *from, 
			struct xnthread *to, struct ipipe_trap_data *d);

#else /* !CONFIG_XENO_ARCH_FPU || !CONFIG_VFP */

/*
 * Userland may raise FPU faults with FPU-enabled kernels, regardless
 * of whether real-time threads actually use FPU, so we simply ignore
 * these faults.
 */
static inline int xnarch_fault_fpu_p(struct ipipe_trap_data *d)
{
	return 0;
}


static inline void xnarch_save_fpu(struct xnthread *thread) { }

static inline void xnarch_switch_fpu(struct xnthread *f, struct xnthread *t) { }

static inline int xnarch_handle_fpu_fault(struct xnthread *from, 
					struct xnthread *to, struct ipipe_trap_data *d)
{
	return 0;
}
#endif /*  !CONFIG_XENO_ARCH_FPU || !CONFIG_VFP */

static inline void xnarch_enable_kfpu(void) { }

static inline void xnarch_disable_kfpu(void) { }

register unsigned long current_stack_pointer asm ("sp");

/* Standard Linux RT task stack size */
#define XNTHREAD_STACK_SIZE 	(THREAD_SIZE)

void xnarch_cleanup_thread(struct xnthread *thread);
void xnarch_init_thread(struct xnthread *thread);

#endif /* !_COBALT_ARM_ASM_THREAD_H */
