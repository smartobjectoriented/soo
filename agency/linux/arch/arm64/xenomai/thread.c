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

struct static_key __xeno_vfp_key = STATIC_KEY_INIT_TRUE;

asmlinkage void __asm_thread_switch(struct task_struct *out, struct task_struct *in);

static DEFINE_MUTEX(vfp_check_lock);

asmlinkage void __asm_vfp_save(union vfp_state *vfp, unsigned int fpexc);

asmlinkage void __asm_vfp_load(union vfp_state *vfp, unsigned int cpu);

#define do_vfp_fmrx(_vfp_)						\
	({								\
		u32 __v;						\
		asm volatile("mrc p10, 7, %0, " __stringify(_vfp_)	\
			     ", cr0, 0 @ fmrx %0, " #_vfp_:		\
			     "=r" (__v));				\
		__v;							\
	})

#define do_vfp_fmxr(_vfp_,_var_)				\
	asm volatile("mcr p10, 7, %0, " __stringify(_vfp_)	\
		     ", cr0, 0 @ fmxr " #_vfp_ ", %0":		\
		     /* */ : "r" (_var_))

extern union vfp_state *vfp_current_hw_state[NR_CPUS];

static inline union vfp_state *get_fpu_owner(void)
{
	union vfp_state *vfp_owner;
	unsigned int cpu;

	unsigned int fpexc;
#if 0
	fpexc = do_vfp_fmrx(FPEXC);
	if (!(fpexc & FPEXC_EN))
		return NULL;

	cpu = ipipe_processor_id();
	vfp_owner = vfp_current_hw_state[cpu];
	if (!vfp_owner)
		return NULL;

	if (vfp_owner->hard.cpu != cpu)
		return NULL;
#endif
	return vfp_owner;
}
#if 0
#define do_disable_vfp(fpexc)					\
	do_vfp_fmxr(FPEXC, fpexc & ~FPEXC_EN)

#define XNARCH_VFP_ANY_EXC						\
	(FPEXC_EX|FPEXC_DEX|FPEXC_FP2V|FPEXC_VV|FPEXC_TRAP_MASK)

#define do_enable_vfp()							\
	({								\
		unsigned _fpexc = do_vfp_fmrx(FPEXC) | FPEXC_EN;	\
		do_vfp_fmxr(FPEXC, _fpexc & ~XNARCH_VFP_ANY_EXC);	\
		_fpexc;							\
	})

int xnarch_fault_fpu_p(struct ipipe_trap_data *d)
{
	/* This function does the same thing to decode the faulting instruct as
	   "call_fpe" in arch/arm/entry-armv.S */
	static unsigned copro_to_exc[16] = {
		IPIPE_TRAP_UNDEFINSTR,
		/* FPE */
		IPIPE_TRAP_FPU, IPIPE_TRAP_FPU,
		IPIPE_TRAP_UNDEFINSTR,
#ifdef CONFIG_CRUNCH
		IPIPE_TRAP_FPU, IPIPE_TRAP_FPU, IPIPE_TRAP_FPU,
#else /* !CONFIG_CRUNCH */
		IPIPE_TRAP_UNDEFINSTR, IPIPE_TRAP_UNDEFINSTR, IPIPE_TRAP_UNDEFINSTR,
#endif /* !CONFIG_CRUNCH */
		IPIPE_TRAP_UNDEFINSTR, IPIPE_TRAP_UNDEFINSTR, IPIPE_TRAP_UNDEFINSTR,
#ifdef CONFIG_VFP
		IPIPE_TRAP_VFP, IPIPE_TRAP_VFP,
#else /* !CONFIG_VFP */
		IPIPE_TRAP_UNDEFINSTR, IPIPE_TRAP_UNDEFINSTR,
#endif /* !CONFIG_VFP */
		IPIPE_TRAP_UNDEFINSTR, IPIPE_TRAP_UNDEFINSTR,
		IPIPE_TRAP_UNDEFINSTR, IPIPE_TRAP_UNDEFINSTR,
	};
	unsigned instr, exc, cp;
	char *pc;

	if (d->exception == IPIPE_TRAP_FPU)
		return 1;

	if (d->exception == IPIPE_TRAP_VFP)
		goto trap_vfp;

	if (d->exception != IPIPE_TRAP_UNDEFINSTR)
		return 0;

	pc = (char *) xnarch_fault_pc(d);
	if (unlikely(thumb_mode(d->regs))) {
		unsigned short thumbh, thumbl;

			return 0;

		thumbh = *(unsigned short *) pc;
		thumbl = *((unsigned short *) pc + 1);

		if ((thumbh & 0x0000f800) < 0x0000e800)
			return 0;
		instr = (thumbh << 16) | thumbl;

#ifdef CONFIG_NEON
		if ((instr & 0xef000000) == 0xef000000
		    || (instr & 0xff100000) == 0xf9000000)
			goto trap_vfp;
#endif
	} else {
		instr = *(unsigned *) pc;

#ifdef CONFIG_NEON
		if ((instr & 0xfe000000) == 0xf2000000
		    || (instr & 0xff100000) == 0xf4000000)
			goto trap_vfp;
#endif
	}

	if ((instr & 0x0c000000) != 0x0c000000)
		return 0;

	cp = (instr & 0x00000f00) >> 8;
#ifdef CONFIG_IWMMXT
	/* We need something equivalent to _TIF_USING_IWMMXT for Xenomai kernel
	   threads */
	if (cp <= 1) {
		d->exception = IPIPE_TRAP_FPU;
		return 1;
	}
#endif

	exc = copro_to_exc[cp];
	if (exc == IPIPE_TRAP_VFP) {
	  trap_vfp:
		/* If an exception is pending, the VFP fault is not really an
		   "FPU unavailable" fault, so we return undefinstr in that
		   case, the nucleus will let linux handle the fault. */
		exc = do_vfp_fmrx(FPEXC);
		if (exc & (FPEXC_EX|FPEXC_DEX)
		    || ((exc & FPEXC_EN) && do_vfp_fmrx(FPSCR) & FPSCR_IXE))
			exc = IPIPE_TRAP_UNDEFINSTR;
		else
			exc = IPIPE_TRAP_VFP;
	}

	d->exception = exc;
	return exc != IPIPE_TRAP_UNDEFINSTR;
}

void xnarch_save_fpu(struct xnthread *thread)
{
	struct xnarchtcb *tcb = &thread->tcb;
	if (tcb->fpup)
		__asm_vfp_save(tcb->fpup, do_enable_vfp());
}

void xnarch_switch_fpu(struct xnthread *from, struct xnthread *to)
{
	union vfp_state *const from_fpup = from ? from->tcb.fpup : NULL;
	unsigned cpu = ipipe_processor_id();


	union vfp_state *const to_fpup = to->tcb.fpup;
	unsigned fpexc = do_enable_vfp();

	if (from_fpup == to_fpup)
		return;

	if (from_fpup)
		__asm_vfp_save(from_fpup, fpexc);

	__asm_vfp_load(to_fpup, cpu);

}

int xnarch_handle_fpu_fault(struct xnthread *from, 
			struct xnthread *to, struct ipipe_trap_data *d)
{
	if (xnthread_test_state(to, XNFPU))
		/* FPU is already enabled, probably an exception */
               return 0;

	if (!static_key_true(&__xeno_vfp_key))
		/* VFP instruction emitted, on a cpu without VFP, this
		   is an error */
		return 0;

	xnlock_get(&nklock);
	xnthread_set_state(to, XNFPU);
	xnlock_put(&nklock);

	xnarch_switch_fpu(from, to);

	/* Retry faulting instruction */
	d->regs->ARM_pc = xnarch_fault_pc(d);
	return 1;
}
#endif

/* To be completed later on */
void xnarch_init_thread(struct xnthread *thread) {
	void *thread_stack;
	struct task_struct *p;
	struct pt_regs *childregs;
lprintk("######### XNARCH_INIT 0\n");
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
	lprintk("######### XNARCH_INIT 1\n");
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

//	fpsimd_thread_switch(in->tcb.task);
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
lprintk("### SWITCH 0\n");
	cpu_switch_to(out->tcb.task, in->tcb.task);
lprintk("### SWITCH 1\n");
}
