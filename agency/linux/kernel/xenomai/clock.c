/*
 * Copyright (C) 2006-2011 Philippe Gerum <rpm@xenomai.org>.
 * Copyright (C) 2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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
#include <linux/percpu.h>
#include <linux/errno.h>
#include <linux/ipipe_tickdev.h>
#include <cobalt/kernel/sched.h>
#include <cobalt/kernel/timer.h>
#include <cobalt/kernel/clock.h>
#include <cobalt/kernel/arith.h>

#include <soo/uapi/soo.h>

#include <cobalt/uapi/time.h>
#include <asm/xenomai/calibration.h>
#include <trace/events/cobalt-core.h>

void xnclock_core_local_shot(struct xnsched *sched)
{
	struct xntimer *timer;

	/*
	 * Here we try to defer the host tick heading the timer queue,
	 * so that it does not preempt a real-time activity uselessly,
	 * in two cases:
	 *
	 * 1) a rescheduling is pending for the current CPU. We may
	 * assume that a real-time thread is about to resume, so we
	 * want to move the host tick out of the way until the host
	 * kernel resumes, unless there is no other outstanding
	 * timers.
	 *
	 * 2) the current thread is running in primary mode, in which
	 * case we may also defer the host tick until the host kernel
	 * resumes.
	 *
	 * The host tick deferral is cleared whenever Xenomai is about
	 * to yield control to the host kernel (see ___xnsched_run()),
	 * or a timer with an earlier timeout date is scheduled,
	 * whichever comes first.
	 */

	sched->lflags &= ~XNHDEFER;
	timer = xntimerq_head();
	if (timer != NULL) {

		if (unlikely(timer == &sched->htimer)) {

			if (xnsched_resched_p(sched) || !xnthread_test_state(sched->curr, XNROOT)) {
				timer = xntimerq_second();
				if (timer)
					sched->lflags |= XNHDEFER;
			}
		}

		ipipe_timer_set(timer->date);
	}
}

/*
 * This function is called to send a timer IPI to CPU #1 for periodic task (first interrupt)
 */
void xnclock_core_remote_shot(struct xnsched *sched)
{
	smp_trigger_tick();
}

#ifdef CONFIG_SMP

int xnclock_get_default_cpu(struct xnclock *clock, int cpu)
{
	cpumask_t set;
	/*
	 * Check a CPU number against the possible set of CPUs
	 * receiving events from the underlying clock device. If the
	 * suggested CPU does not receive events from this device,
	 * return the first one which does.  We also account for the
	 * dynamic set of real-time CPUs.
	 */
	cpumask_and(&set, &clock->affinity, &cobalt_cpu_affinity);
	if (!cpumask_empty(&set) && !cpumask_test_cpu(cpu, &set))
		cpu = cpumask_first(&set);

	return cpu;
}
EXPORT_SYMBOL_GPL(xnclock_get_default_cpu);

#endif /* !CONFIG_SMP */

/**
 * @brief Register a Xenomai clock.
 *
 * This service installs a new clock which may be used to drive
 * Xenomai timers.
 *
 * @param clock The new clock to register.
 *
 * @param affinity The set of CPUs we may expect the backing clock
 * device to tick on.
 *
 * @coretags{secondary-only}
 */
int xnclock_register(struct xnclock *clock, const cpumask_t *affinity)
{
	BUG_ON(smp_processor_id() != AGENCY_RT_CPU);

	/*
	 * A CPU affinity set is defined for each clock, enumerating
	 * the CPUs which can receive ticks from the backing clock
	 * device.  This set must be a subset of the real-time CPU
	 * set.
	 */
	cpumask_and(&clock->affinity, affinity, &xnsched_realtime_cpus);
	if (cpumask_empty(&clock->affinity))
		return -EINVAL;

	/*
	 * POLA: init all timer slots for the new clock, although some
	 * of them might remain unused depending on the CPU affinity
	 * of the event source(s).
	 */

	xntimerq_init();

	return 0;
}
EXPORT_SYMBOL_GPL(xnclock_register);

/**
 * @fn void xnclock_tick(struct xnclock *clock)
 * @brief Process a clock tick.
 *
 * This routine processes an incoming @a clock event, firing elapsed
 * timers as appropriate.
 *
 * @param clock The clock for which a new event was received.
 *
 * @coretags{coreirq-only, atomic-entry}
 *
 * @note The current CPU must be part of the real-time affinity set,
 * otherwise weird things may happen.
 */

void xnclock_tick(void)
{
	struct xnsched *sched = xnsched_current();
	struct xntimer *timer;
	xnsticks_t delta;
	xnticks_t now;
	static bool __tick_in_progress = false;

	/*
	 * Optimisation: any local timer reprogramming triggered by
	 * invoked timer handlers can wait until we leave the tick
	 * handler. Use this status flag as hint to xntimer_start().
	 */

	if (__tick_in_progress)
		return ;

	__tick_in_progress = true;

	now = xnclock_read();

	while ((timer = xntimerq_head()) != NULL) {

		delta = (xnsticks_t)(timer->date - now);

		if (delta > 0)
			break;

		trace_cobalt_timer_expire(timer);

		xntimer_dequeue(timer);

		/*
		 * By postponing the propagation of the low-priority
		 * host tick to the interrupt epilogue (see
		 * xnintr_irq_handler()), we save some I-cache, which
		 * translates into precious microsecs on low-end hw.
		 */

		if (unlikely(timer == &sched->htimer)) {
			sched->lflags |= XNHTICK;
			sched->lflags &= ~XNHDEFER;
			if (timer->status & XNTIMER_PERIODIC)
				goto advance;
			continue;
		}
		timer->handler(timer);
		now = xnclock_read();

		timer->status |= XNTIMER_FIRED;
		/*
		 * Only requeue periodic timers which have not been
		 * requeued, stopped or killed.
		 */
		if ((timer->status &
		     (XNTIMER_PERIODIC|XNTIMER_DEQUEUED|XNTIMER_KILLED|XNTIMER_RUNNING)) !=
		    (XNTIMER_PERIODIC|XNTIMER_DEQUEUED|XNTIMER_RUNNING))
			continue;
	advance:
		do {
			timer->periodic_ticks++;
			xntimer_update_date(timer);
		} while (timer->date < now);


 		xntimer_enqueue(timer);
	}

	__tick_in_progress = false;

	xnclock_program_shot(sched);
}
EXPORT_SYMBOL_GPL(xnclock_tick);

struct xnclock nkclock = {
	.name = "coreclk",
	.resolution = 1,	/* nanosecond. */
	.id = -1,
};
EXPORT_SYMBOL_GPL(nkclock);


int xnclock_init(void)
{

	xnclock_register(&nkclock, &xnsched_realtime_cpus);

	return 0;
}


