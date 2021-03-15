/*
 * Copyright (C) 2001,2002,2003,2004 Philippe Gerum <rpm@xenomai.org>.
 *
 * ARM port
 *   Copyright (C) 2005 Stelian Pop
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

#include <linux/sched.h>
#include <linux/ipipe.h>
#include <linux/mm.h>
#include <linux/jump_label.h>
#include <linux/sched/task_stack.h>

#include <asm/ipipe_hwirq.h>

#include <asm/mmu_context.h>

#include <cobalt/kernel/thread.h>

#include <xenomai/cobalt/kernel/lock.h>

#include <soo/uapi/console.h>

void xnarch_save_fpu(struct xnthread *thread)
{

}

void xnarch_switch_fpu(struct xnthread *from, struct xnthread *to)
{

}

/* To be completed later on */
void xnarch_init_thread(struct xnthread *thread) {
	void *thread_stack;
	struct task_struct *p;
	struct pt_regs *childregs;

	/* Underlying Linux task struct for various purposes like current() in stack_strace function */
	thread->tcb.task = kmalloc(sizeof(struct task_struct), GFP_ATOMIC);
	BUG_ON(!thread->tcb.task);
	memset(thread->tcb.task, 0, sizeof(struct task_struct));

	/* Init the xnthread stack within the VMALLOC area of Linux. */
	thread_stack = __vmalloc_node_range(THREAD_SIZE, THREAD_ALIGN,
					     VMALLOC_START, VMALLOC_END,
					     __GFP_ATOMIC,
					     PAGE_KERNEL,
					     0, 0, __builtin_return_address(0));
	BUG_ON(!thread_stack);

	memset(thread_stack, 0, XNTHREAD_STACK_SIZE);

	thread->tcb.pc = thread->tcb.start_pc;

	/* shortcut */
	p = thread->tcb.task;

	memset(p, 0, sizeof(struct task_struct));

	p->stack = (void *) thread_stack;

	/* Initialize a new thread following Linux conventions so that the ARM operations
	 * will be simpler to handle (context switch, FPU, etc.).
	 */

	childregs = task_pt_regs(p);

	childregs->orig_x0 = 0;

	childregs->pc = (unsigned long)thread->tcb.start_pc;
	childregs->sp = (u64) p->stack + XNTHREAD_STACK_SIZE;

	thread->tcb.sp = childregs->sp;

	thread->tcb.task->thread.cpu_context.sp = thread->tcb.sp;
	thread->tcb.task->thread.cpu_context.pc = thread->tcb.pc;

	thread->tcb.task->cpu = AGENCY_RT_CPU;

	/* XNFPU is always set */
	xnthread_set_state(thread, XNFPU);
}

/*
 * Low-level cleaning
 */
void xnarch_cleanup_thread(struct xnthread *thread) {

	struct xnarchtcb *tcb = xnthread_archtcb(thread);

	BUG_ON(xnthread_current() == thread);

	vfree((void *) tcb->task->stack);
	kfree(thread->tcb.task);

}

extern void entry_task_switch(struct task_struct *next);

void xnarch_switch_to(struct xnthread *out, struct xnthread *in)
{
	BUG_ON(!hard_irqs_disabled());


#if 0
	tls_thread_switch(next);
	hw_breakpoint_thread_switch(next);
	contextidr_thread_switch(next);
#endif
	// Do not use the IRQ stack on CPU #1, because there may be scheduling
	// after the top half processing, but before the stack switch of the normal
	// return path.

	//entry_task_switch(in->tcb.task);
#if 0
	uao_thread_switch(next);
	ptrauth_thread_switch(next);
	ssbs_thread_switch(next);
#endif

	/* Update the current thread */
	__xnthread_current = in;

	/*
	 * Complete any pending TLB or cache maintenance on this CPU in case
	 * the thread migrates to a different CPU.
	 * This full barrier is also required by the membarrier system
	 * call.
	 */
	dsb(ish);

	cpu_switch_to(out->tcb.task, in->tcb.task);

}
