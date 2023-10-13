/*
 * Copyright (C) 2001-2013 Philippe Gerum <rpm@xenomai.org>.
 *
 * Xenomai is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
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
#include <linux/module.h>
#include <linux/signal.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <cobalt/kernel/sched.h>
#include <cobalt/kernel/thread.h>
#include <cobalt/kernel/timer.h>
#include <cobalt/kernel/intr.h>
#include <cobalt/kernel/heap.h>
#include <cobalt/kernel/arith.h>
#include <cobalt/uapi/signal.h>
#define CREATE_TRACE_POINTS
#include <trace/events/cobalt-core.h>

#include <soo/uapi/console.h>

/**
 * @ingroup cobalt_core
 * @defgroup cobalt_core_sched Thread scheduling control
 * @{
 */

cpumask_t xnsched_realtime_cpus;

DEFINE_PER_CPU(struct xnsched, nksched);
EXPORT_PER_CPU_SYMBOL_GPL(nksched);

cpumask_t cobalt_cpu_affinity = CPU_MASK_ALL;
EXPORT_SYMBOL_GPL(cobalt_cpu_affinity);

LIST_HEAD(nkthreadq);
LIST_HEAD(list_threads);

int cobalt_nrthreads;

#ifdef CONFIG_XENO_OPT_VFILE
struct xnvfile_rev_tag nkthreadlist_tag;
#endif

static struct xnsched_class *xnsched_class_highest;

#define for_each_xnsched_class(p) \
   for (p = xnsched_class_highest; p; p = p->next)

static void xnsched_register_class(struct xnsched_class *sched_class)
{
	sched_class->next = xnsched_class_highest;
	xnsched_class_highest = sched_class;

	/*
	 * Classes shall be registered by increasing priority order,
	 * idle first and up.
	 */
	XENO_BUG_ON(COBALT, sched_class->next &&
		   sched_class->next->weight > sched_class->weight);

	printk(XENO_INFO "scheduling class %s registered.\n", sched_class->name);
}

void xnsched_register_classes(void)
{
	xnsched_register_class(&xnsched_class_idle);
	xnsched_register_class(&xnsched_class_rt);
}

static void roundrobin_handler(struct xntimer *timer)
{
	struct xnsched *sched = container_of(timer, struct xnsched, rrbtimer);
	xnsched_tick(sched);
}

void xnsched_init(struct xnsched *sched, int cpu)
{
	char rrbtimer_name[XNOBJECT_NAME_LEN];
	char htimer_name[XNOBJECT_NAME_LEN];
	char root_name[XNOBJECT_NAME_LEN];
	union xnsched_policy_param param;
	struct xnthread_init_attr attr;
	struct xnsched_class *p;

	sched->cpu = cpu;
	ksformat(htimer_name, sizeof(htimer_name), "[host-timer/%u]", cpu);
	ksformat(rrbtimer_name, sizeof(rrbtimer_name), "[rrb-timer/%u]", cpu);
	ksformat(root_name, sizeof(root_name), "ROOT/%u", cpu);
	cpumask_clear(&sched->resched);

	for_each_xnsched_class(p) {
		if (p->sched_init)
			p->sched_init(sched);
	}

	sched->status = 0;
	sched->lflags = 0;
	sched->inesting = 0;
	sched->curr = &sched->rootcb;

	attr.flags = XNROOT | XNFPU;
	attr.name = root_name;
	attr.personality = &xenomai_personality;
	attr.affinity = *cpumask_of(cpu);
	param.idle.prio = XNSCHED_IDLE_PRIO;

	__xnthread_init(&sched->rootcb, &attr, sched, &xnsched_class_idle, &param, 0, NULL, false);
	
	/* Initial current thread */
	__xnthread_current = &sched->rootcb;

	/*
	 * No direct handler here since the host timer processing is
	 * postponed to xnintr_irq_handler(), as part of the interrupt
	 * exit code.
	 */

	xntimer_init(&sched->htimer, &nkclock, NULL, sched, __XNTIMER_CORE);
	xntimer_set_priority(&sched->htimer, XNTIMER_LOPRIO);

	xntimer_init(&sched->rrbtimer, &nkclock, roundrobin_handler, sched, __XNTIMER_CORE);
	xntimer_set_priority(&sched->rrbtimer, XNTIMER_LOPRIO);

	xnstat_exectime_set_current(sched, &sched->rootcb.stat.account);

	sched->fpuholder = &sched->rootcb;

	list_add_tail(&sched->rootcb.glink, &nkthreadq);
	cobalt_nrthreads++;
}

void xnsched_destroy(struct xnsched *sched)
{
	xntimer_destroy(&sched->htimer);
	xntimer_destroy(&sched->rrbtimer);
	xntimer_destroy(&sched->rootcb.ptimer);
	xntimer_destroy(&sched->rootcb.rtimer);

}

static inline void set_thread_running(struct xnsched *sched,
				      struct xnthread *thread)
{
	xnthread_clear_state(thread, XNREADY);
	if (xnthread_test_state(thread, XNRRB))
		xntimer_start(&sched->rrbtimer,
			      thread->rrperiod, XN_INFINITE, XN_RELATIVE);
	else
		xntimer_stop(&sched->rrbtimer);
}

/* Must be called with nklock locked, interrupts off. */
struct xnthread *xnsched_pick_next(struct xnsched *sched)
{
	struct xnsched_class *p __maybe_unused;
	struct xnthread *curr = sched->curr;
	struct xnthread *thread;

	if (!xnthread_test_state(curr, XNTHREAD_BLOCK_BITS | XNZOMBIE)) {
		/*
		 * Do not preempt the current thread if it holds the
		 * scheduler lock.
		 */
		if (curr->lock_count > 0) {
			xnsched_set_self_resched(sched);
			return curr;
		}
		/*
		 * Push the current thread back to the runnable queue
		 * of the scheduling class it belongs to, if not yet
		 * linked to it (XNREADY tells us if it is).
		 */
		/* SOO.tech */
		/* xnsched_requeue() has been replaced by xnsched_enqueue() which will put the thread
		 * at the queue tail so that we give a chance to other threads of the same priority to
		 * be executed. This is useful for managing busy looping on dc_event variable for example.
		 */
		if (!xnthread_test_state(curr, XNREADY)) {
			xnsched_enqueue(curr);
			xnthread_set_state(curr, XNREADY);
		}
	}

	/*
	 * Find the runnable thread having the highest priority among
	 * all scheduling classes, scanned by decreasing priority.
	 */

	thread = xnsched_rt_pick(sched);
	if (unlikely(thread == NULL))
		thread = &sched->rootcb;

	set_thread_running(sched, thread);

	return thread;

}

void xnsched_lock(void)
{
	struct xnsched *sched = xnsched_current();
	struct xnthread *curr = sched->curr;

	/*
	 * CAUTION: The fast xnthread_current() accessor carries the
	 * relevant lock nesting count only if current runs in primary
	 * mode. Otherwise, if the caller is unknown or relaxed
	 * Xenomai-wise, then we fall back to the root thread on the
	 * current scheduler, which must be done with IRQs off.
	 * Either way, we don't need to grab the super lock.
	 */
	if (unlikely(curr == NULL || xnthread_test_state(curr, XNRELAX))) {
		/*
		 * In IRQ: scheduler already locked, and we may have
		 * interrupted xnthread_relax() where the BUG_ON condition is
		 * temporarily false.
		 */
		if (sched->lflags & XNINIRQ)
			return;

		irqoff_only();
		curr = &sched->rootcb;
		XENO_BUG_ON(COBALT, xnsched_current()->curr != curr);
	}

	curr->lock_count++;
}
EXPORT_SYMBOL_GPL(xnsched_lock);

void xnsched_unlock(void)
{
	struct xnsched *sched = xnsched_current();
	struct xnthread *curr = sched->curr;

	if (unlikely(curr == NULL || xnthread_test_state(curr, XNRELAX))) {
		/*
		 * In IRQ
		 */
		if (sched->lflags & XNINIRQ)
			return;

		irqoff_only();
		curr = &xnsched_current()->rootcb;
	}

	if (!XENO_ASSERT(COBALT, curr->lock_count > 0))
		return;

	if (--curr->lock_count == 0) {
		xnthread_clear_localinfo(curr, XNLBALERT);
		xnsched_run();
	}
}
EXPORT_SYMBOL_GPL(xnsched_unlock);

/* Must be called with nklock locked, interrupts off. */
void xnsched_putback(struct xnthread *thread)
{
	if (xnthread_test_state(thread, XNREADY))
		xnsched_dequeue(thread);
	else
		xnthread_set_state(thread, XNREADY);

	xnsched_enqueue(thread);
	xnsched_set_resched(thread->sched);
}

/* Must be called with nklock locked, interrupts off. */
int xnsched_set_policy(struct xnthread *thread,
		       struct xnsched_class *sched_class,
		       const union xnsched_policy_param *p)
{
	int ret;

	/*
	 * Declaring a thread to a new scheduling class may fail, so
	 * we do that early, while the thread is still a member of the
	 * previous class. However, this also means that the
	 * declaration callback shall not do anything that might
	 * affect the previous class (such as touching thread->rlink
	 * for instance).
	 */
	if (sched_class != thread->base_class) {
		ret = xnsched_declare(sched_class, thread, p);
		if (ret)
			return ret;
	}

	/*
	 * As a special case, we may be called from __xnthread_init()
	 * with no previous scheduling class at all.
	 */
	if (likely(thread->base_class != NULL)) {
		if (xnthread_test_state(thread, XNREADY))
			xnsched_dequeue(thread);

		if (sched_class != thread->base_class)
			xnsched_forget(thread);
	}

	thread->sched_class = sched_class;
	thread->base_class = sched_class;
	xnsched_setparam(thread, p);
	thread->bprio = thread->cprio;
	thread->wprio = thread->cprio + sched_class->weight;

	if (xnthread_test_state(thread, XNREADY))
		xnsched_enqueue(thread);

	if (!xnthread_test_state(thread, XNDORMANT))
		xnsched_set_resched(thread->sched);

	return 0;
}
EXPORT_SYMBOL_GPL(xnsched_set_policy);

/* Must be called with nklock locked, interrupts off. */
void xnsched_track_policy(struct xnthread *thread,
			  struct xnthread *target)
{
	union xnsched_policy_param param;

	if (xnthread_test_state(thread, XNREADY))
		xnsched_dequeue(thread);
	/*
	 * Self-targeting means to reset the scheduling policy and
	 * parameters to the base ones. Otherwise, make thread inherit
	 * the scheduling data from target.
	 */
	if (target == thread) {
		thread->sched_class = thread->base_class;
		xnsched_trackprio(thread, NULL);
	} else {
		xnsched_getparam(target, &param);
		thread->sched_class = target->sched_class;
		xnsched_trackprio(thread, &param);
	}

	if (xnthread_test_state(thread, XNREADY))
		xnsched_enqueue(thread);

	xnsched_set_resched(thread->sched);
}

static void migrate_thread(struct xnthread *thread, struct xnsched *sched)
{
	struct xnsched_class *sched_class = thread->sched_class;

	if (xnthread_test_state(thread, XNREADY)) {
		xnsched_dequeue(thread);
		xnthread_clear_state(thread, XNREADY);
	}

	if (sched_class->sched_migrate)
		sched_class->sched_migrate(thread, sched);
	/*
	 * WARNING: the scheduling class may have just changed as a
	 * result of calling the per-class migration hook.
	 */
	thread->sched = sched;
}

/*
 * Must be called with nklock locked, interrupts off. thread must be
 * runnable.
 */
void xnsched_migrate(struct xnthread *thread, struct xnsched *sched)
{
	xnsched_set_resched(thread->sched);
	migrate_thread(thread, sched);

	/* Move thread to the remote runnable queue. */
	xnsched_putback(thread);

}

/*
 * Must be called with nklock locked, interrupts off. Thread may be
 * blocked.
 */
void xnsched_migrate_passive(struct xnthread *thread, struct xnsched *sched)
{
	struct xnsched *last_sched = thread->sched;

	migrate_thread(thread, sched);

	if (!xnthread_test_state(thread, XNTHREAD_BLOCK_BITS)) {
		xnsched_requeue(thread);
		xnthread_set_state(thread, XNREADY);
		xnsched_set_resched(last_sched);
	}
}

struct xnthread *xnsched_findq(struct list_head *q, int prio)
{
	struct xnthread *thread;

	if (list_empty(q))
		return NULL;

	/* Find thread leading a priority group. */
	list_for_each_entry(thread, q, rlink) {
		if (prio == thread->cprio)
			return thread;
	}

	return NULL;
}

static inline void switch_context(struct xnthread *prev, struct xnthread *next)
{

	xnarch_switch_to(prev, next);

}

void __xnsched_run_handler(void) /* hw interrupts off. */
{
	trace_cobalt_schedule_remote(xnsched_current());

	xnsched_run();
}

void __asm_xnsched_run(void) {

	if (smp_processor_id() != 1)
		return ;

	if (likely(__cobalt_ready)) {
		xnsched_set_self_resched(xnsched_current());
		xnsched_run();
	}
}

/*
 * Cleanup all zombie threads.
 */
void clean_zombie(void) {
	struct xnthread *cur, *tmp;

	list_for_each_entry_safe(cur, tmp, &list_threads, main_list) {
		if (xnthread_test_state(cur, XNZOMBIE)) {
			xnarch_cleanup_thread(cur);
			list_del(&cur->main_list);
		}
	}
}

int ___xnsched_run(struct xnsched *sched)
{
	struct xnthread *prev, *next, *curr;
	int switched;
	unsigned long s;

	/* Do not perform any scheduling operation while we are executing the top half processing.
	 * Scheduling will be postponed after irq_exit in entry-armv.S
	 */
	if (unlikely(per_cpu(in_upcall_progress, smp_processor_id()) == true))
		return 0;

	trace_cobalt_schedule(sched);

	xnlock_get_irqsave(&nklock, s);

	curr = sched->curr;

	xntrace_pid(xnthread_current()->pid, xnthread_current_priority(curr));
reschedule:
	switched = 0;

	next = xnsched_pick_next(sched);
	if (next == curr) {
		if (unlikely(xnthread_test_state(next, XNROOT))) {
			if (sched->lflags & XNHDEFER)
				xnclock_program_shot(sched);
		}
		goto out;
	}

	prev = curr;

	trace_cobalt_switch_context(prev, next);

	if (xnthread_test_state(next, XNROOT))
		xnsched_reset_watchdog(sched);

	sched->curr = next;

	if (sched->lflags & XNHDEFER)
		xnclock_program_shot(sched);

	xnstat_exectime_switch(sched, &next->stat.account);
	xnstat_counter_inc(&next->stat.csw);

#if 0
	lprintk("### xenomai changing thread from %s to %s ...\n", prev->name, next->name);
#endif

	switch_context(prev, next);

	/* Check if the previous thread was terminated.
	 * If another thread was joining the prev thread, it has been already woken up and
	 * do not use any further information regarding the prev thread.
	 * In all cases, the prev thread can safely disappear.
	 */
	clean_zombie();

	switched = 1;
	sched = xnsched_finish_unlocked_switch(sched);
	/*
	 * Re-read the currently running thread, this is needed
	 * because of relaxed/hardened transitions.
	 */
	curr = sched->curr;
	xnthread_switch_fpu(sched);
	xntrace_pid(xnthread_current()->pid, xnthread_current_priority(curr));
out:
	if (switched &&
	    xnsched_maybe_resched_after_unlocked_switch(sched))
		goto reschedule;

	xnlock_put_irqrestore(&nklock, s);

	/* Check if this *current* thread has been canceled in-between ... */
	if (xnthread_test_state(curr, XNCANCELD))
		xnthread_cancel(curr);

	return switched;
}
EXPORT_SYMBOL_GPL(___xnsched_run);

/** @} */
