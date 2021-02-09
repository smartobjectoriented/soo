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

#include <linux/sched.h>
#include <linux/ipipe.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <cobalt/kernel/thread.h>
#include <asm/mmu_context.h>
#include <asm/processor.h>
#include <asm/fpu/internal.h>

static struct kmem_cache *xstate_cache;

#define fpu_kernel_xstate_size sizeof(struct fpu)

#define cpu_has_xmm boot_cpu_has(X86_FEATURE_XMM)
#define cpu_has_fxsr boot_cpu_has(X86_FEATURE_FXSR)
#define cpu_has_xsave boot_cpu_has(X86_FEATURE_XSAVE)

#define x86_xstate_alignment		__alignof__(union fpregs_state)

#include <asm/switch_to.h>

extern void __kernel_fpu_disable(void);

void xnarch_switch_to(struct xnthread *out, struct xnthread *in)
{
	struct xnarchtcb *out_tcb = &out->tcb, *in_tcb = &in->tcb;
	struct task_struct *prev, *next, *last;

	prev = out_tcb->task;
	next = in_tcb->task;

	switch_to(prev, next, last);

}

int xnarch_handle_fpu_fault(struct xnthread *from,
			struct xnthread *to, struct ipipe_trap_data *d)
{
	/* in eager mode there are no such faults */
	BUG_ON(1);
}

#define current_task_used_kfpu() kernel_fpu_disabled()
#define tcb_used_kfpu(t) ((t)->root_kfpu)

void xnarch_switch_fpu(struct xnthread *from, struct xnthread *to)
{
	struct xnarchtcb *const to_tcb = xnthread_archtcb(to);

	if (!tcb_used_kfpu(to_tcb))
		return;

	copy_kernel_to_fpregs(&to_tcb->kfpu->state);
	__kernel_fpu_disable();
}

/*
 * fork_frame is at the very top of the stack.
 */
void xnarch_init_thread(struct xnthread *thread)
{
	struct xnarchtcb *tcb = xnthread_archtcb(thread);
	struct task_struct *p;
	void *thread_stack;

	/* Init the xnthread stack within the VMALLOC area of Linux. */
	thread_stack = __vmalloc_node_range(THREAD_SIZE, THREAD_SIZE,
			VMALLOC_START, VMALLOC_END,
			THREADINFO_GFP,
			PAGE_KERNEL,
			0, 0, __builtin_return_address(0));

	memset(thread_stack, 0, THREAD_SIZE);

	/* Put the address of the xnthread at the bottom of the stack. */
	*((unsigned long *) thread_stack) = (unsigned long) thread;

	/* Underlying Linux task struct for various purposes like current() in stack_strace function */
	thread->tcb.task = kmalloc(sizeof(struct task_struct), GFP_ATOMIC);
	BUG_ON(!thread->tcb.task);

	/* shortcut */
	p = thread->tcb.task;

	memset(p, 0, sizeof(struct task_struct));

	p->stack = (void *) thread_stack;

	/* Initialize a new thread following Linux conventions so that the x86 operations
	 * will be simpler to handle.
	 */

	start_thread(&thread->tcb.ai->regs, thread->tcb.start_pc, thread->tcb.ai->sp);

	tcb->kfpu = kmem_cache_zalloc(xstate_cache, GFP_ATOMIC);
	tcb->root_kfpu = 0;

	/* XNFPU is always set */
	xnthread_set_state(thread, XNFPU);

	fpu__initialize(&p->thread.fpu);

}

void xnarch_cleanup_thread(struct xnthread *thread)
{
	struct xnarchtcb *tcb = xnthread_archtcb(thread);

	kfree(tcb->kfpu);

}

int mach_x86_thread_init(void)
{
	xstate_cache = kmem_cache_create("cobalt_x86_xstate",
					 fpu_kernel_xstate_size,
					 x86_xstate_alignment,
					 0,
					 NULL);
	if (xstate_cache == NULL)
		return -ENOMEM;

	return 0;
}

void mach_x86_thread_cleanup(void)
{
	kmem_cache_destroy(xstate_cache);
}
