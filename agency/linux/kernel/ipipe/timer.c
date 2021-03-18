/* -*- linux-c -*-
 * linux/kernel/ipipe/timer.c
 *
 * Copyright (C) 2012 Gilles Chanteperdrix
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
 *
 * I-pipe timer request interface.
 */
#include <linux/ipipe.h>
#include <linux/percpu.h>
#include <linux/irqdesc.h>
#include <linux/cpumask.h>
#include <linux/spinlock.h>
#include <linux/ipipe_tickdev.h>
#include <linux/interrupt.h>
#include <linux/export.h>

#include <asm/arch_timer.h>

#include <cobalt/kernel/arith.h>
#include <xenomai/cobalt/kernel/clock.h>

#include <soo/uapi/soo.h>

#include <soo/uapi/console.h>

unsigned long __ipipe_hrtimer_freq;

struct ipipe_timer *__xntimer_rt;

extern int __ipipe_timer_handler(unsigned int irq, void *cookie);

static DEFINE_SPINLOCK(lock);

static DEFINE_PER_CPU(struct ipipe_timer *, percpu_timer);

xnsticks_t xnclock_core_ns_to_ticks(xnsticks_t ns) {
	return ((xnsticks_t) (ns * __xntimer_rt->host_timer->mult) >> __xntimer_rt->host_timer->shift);
}

xnsticks_t xnclock_core_ticks_to_ns(xnsticks_t ticks) {
	return ((xnsticks_t) ticks * __xntimer_rt->host_timer->mult) >> __xntimer_rt->host_timer->shift;
}

/*
 * Default request method: switch to oneshot mode if supported.
 */
static void ipipe_timer_default_request(struct ipipe_timer *timer, int steal)
{
	struct clock_event_device *evtdev = timer->host_timer;

	if (!(evtdev->features & CLOCK_EVT_FEAT_ONESHOT))
		return;

	if (evtdev->state_use_accessors != CLOCK_EVT_STATE_ONESHOT) {
		evtdev->set_state_oneshot(evtdev);
		evtdev->set_next_event(timer->freq / HZ, evtdev);
	}
}

/*
 * Default release method: return the timer to the mode it had when
 * starting.
 */
static void ipipe_timer_default_release(struct ipipe_timer *timer)
{
	struct clock_event_device *evtdev = timer->host_timer;

	switch (evtdev->state_use_accessors) {
	case CLOCK_EVT_STATE_ONESHOT:
		evtdev->set_state_oneshot(evtdev);
		break;
	case CLOCK_EVT_STATE_PERIODIC:
		evtdev->set_state_periodic(evtdev);
		break;
	default:
		lprintk("%s: clock state (%x) not supported ...\n", evtdev->state_use_accessors);
		BUG();
	}

	if (evtdev->state_use_accessors == CLOCK_EVT_STATE_ONESHOT)
		evtdev->set_next_event(timer->freq / HZ, evtdev);
}

void ipipe_host_timer_register(struct clock_event_device *evtdev)
{
	struct ipipe_timer *timer = evtdev->ipipe_timer;

	if (timer == NULL)
		return;

	if (timer->request == NULL)
		timer->request = ipipe_timer_default_request;

	/*
	 * By default, use the same method as linux timer, on ARM at
	 * least, most set_next_event methods are safe to be called
	 * from Xenomai domain anyway.
	 */
	if (timer->set == NULL) {
		timer->timer_set = evtdev;
		timer->set = (typeof(timer->set))evtdev->set_next_event;
	}

	if (timer->release == NULL)
		timer->release = ipipe_timer_default_release;

	if (timer->name == NULL)
		timer->name = evtdev->name;

	if (timer->rating == 0)
		timer->rating = evtdev->rating;

	timer->freq = (1000000000ULL * evtdev->mult) >> evtdev->shift;

	if (timer->min_delay_ticks == 0)
		timer->min_delay_ticks =
			(evtdev->min_delta_ns * evtdev->mult) >> evtdev->shift;

	if (timer->cpumask == NULL)
		timer->cpumask = evtdev->cpumask;

	timer->host_timer = evtdev;

	timer->cycle_last = 0ull;

	ipipe_timer_register(timer);

	if (evtdev->set_state_oneshot)
		evtdev->set_state_oneshot(evtdev);
}

/*
 * register a timer: maintain them in a list sorted by rating
 */
void ipipe_timer_register(struct ipipe_timer *timer)
{
	if (timer->timer_set == NULL)
		timer->timer_set = timer;

	if (timer->cpumask == NULL)
		timer->cpumask = cpumask_of(smp_processor_id());

	__xntimer_rt = timer;

}


/* Set up a timer as per-cpu timer for ipipe */
static void install_pcpu_timer(unsigned cpu, unsigned hrclock_freq, struct ipipe_timer *t) {

	if (__ipipe_hrtimer_freq == 0)
		__ipipe_hrtimer_freq = t->freq;

	per_cpu(ipipe_percpu.hrtimer_irq, cpu) = t->irq;
	per_cpu(percpu_timer, cpu) = t;
}

unsigned long get_xntimer_period(void) {
	return __xntimer_rt->freq;
}
/*
 * Choose per-cpu timers with the highest rating by traversing the
 * rating-sorted list for each CPU.
 */
int ipipe_select_timers(const struct cpumask *mask)
{
	unsigned long flags;

	spin_lock_irqsave(&lock, flags);

	__ipipe_hrclock_freq = __xntimer_rt->freq;

	nkclock.resolution = (xnticks_t) ((u64)__xntimer_rt->host_timer->mult >> __xntimer_rt->host_timer->shift);
	/* Avoid 0 ns */
	nkclock.resolution++;

	/* add-on */
	install_pcpu_timer(AGENCY_RT_CPU, __ipipe_hrclock_freq, __xntimer_rt);

	spin_unlock_irqrestore(&lock, flags);

	return 0;
}

static void ipipe_timer_release_sync(void)
{
	struct ipipe_timer *timer = __ipipe_raw_cpu_read(percpu_timer);

	if (timer)
		timer->release(timer);
}

void ipipe_timers_release(void)
{
	unsigned long flags;
	unsigned cpu;

	local_irq_save(flags);
	ipipe_timer_release_sync();
	local_irq_restore(flags);

	for_each_online_cpu(cpu) {
		per_cpu(ipipe_percpu.hrtimer_irq, cpu) = -1;
		per_cpu(percpu_timer, cpu) = NULL;
		__ipipe_hrtimer_freq = 0;
	}
}

void ipipe_timer_start(void (*tick_handler)(void), set_state_fn_t set_periodic, set_state_fn_t set_oneshot,
				set_state_fn_t set_oneshot_stopped,
				set_state_fn_t set_shutdown,
				int (*emutick)(unsigned long evt,
				struct clock_event_device *cdev),
				unsigned cpu)
{
	struct clock_event_device *evtdev;
	struct ipipe_timer *timer;
#ifndef CONFIG_X86
	unsigned long flags;
#endif
	timer = per_cpu(percpu_timer, cpu);
	evtdev = timer->host_timer;

#ifndef CONFIG_X86
	local_irq_save(flags);
	ipipe_request_irq(timer->irq, __ipipe_timer_handler, NULL);
	local_irq_restore(flags);
#endif
}

void ipipe_timer_stop(unsigned cpu)
{
	unsigned long __maybe_unused flags;
	struct clock_event_device *evtdev;
	struct ipipe_timer *timer;

	timer = per_cpu(percpu_timer, cpu);
	evtdev = timer->host_timer;
}

/*
 * May be called by the timer IPI as well (at the beginning only).
 */
void xntimer_raise(void) {
	xnintr_core_clock_handler();
}

/*
 * ipipe_timer_set will program the hardware timer according to the deadline.
 */
void ipipe_timer_set(u64 deadline)
{
	u64 clc;
	int64_t delta;

	delta = deadline - xnclock_read();

	if (delta <= 0)
		xntimer_raise();
	else {

		delta = min((u64) delta, __xntimer_rt->host_timer->max_delta_ns);
		delta = max((u64) delta, __xntimer_rt->host_timer->min_delta_ns);

		clc = (delta * (u64) __xntimer_rt->host_timer->mult) >> __xntimer_rt->host_timer->shift;

		__xntimer_rt->host_timer->set_next_event((unsigned long) clc, (struct clock_event_device *) __xntimer_rt->host_timer);
	}
}

const char *ipipe_timer_name(void)
{
	return per_cpu(percpu_timer, 0)->name;
}
EXPORT_SYMBOL_GPL(ipipe_timer_name);

