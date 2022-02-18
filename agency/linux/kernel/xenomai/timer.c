/*
 * Copyright (C) 2001,2002,2003,2007,2012 Philippe Gerum <rpm@xenomai.org>.
 * Copyright (C) 2004 Gilles Chanteperdrix <gilles.chanteperdrix@xenomai.org>
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
#include <linux/ipipe.h>
#include <linux/ipipe_tickdev.h>
#include <linux/sched.h>
#include <cobalt/kernel/sched.h>
#include <cobalt/kernel/thread.h>
#include <cobalt/kernel/timer.h>
#include <cobalt/kernel/intr.h>
#include <cobalt/kernel/clock.h>
#include <cobalt/kernel/trace.h>
#include <cobalt/kernel/arith.h>

#include <trace/events/cobalt-core.h>

#include <soo/uapi/soo.h>
#include <soo/uapi/console.h>

/* Main list of xntimers */
struct list_head __xntimers;

/**
 * @ingroup cobalt_core
 * @defgroup cobalt_core_timer Timer services
 *
 * The Xenomai timer facility depends on a clock source (xnclock) for
 * scheduling the next activation times.
 *
 * The core provides and depends on a monotonic clock source (nkclock)
 * with nanosecond resolution, driving the platform timer hardware
 * exposed by the interrupt pipeline.
 *
 * @{
 */

int xntimer_heading_p(struct xntimer *timer)
{
	struct xnsched *sched = timer->sched;
	struct xntimer *h;

	h = xntimerq_head();
	if (h == timer)
		return 1;

	if (sched->lflags & XNHDEFER) {
		h = xntimerq_second();
		if (h == timer)
			return 1;
	}

	return 0;
}

void xntimer_enqueue_and_program(struct xntimer *timer)
{
	xntimer_enqueue(timer);
	if (xntimer_heading_p(timer)) {

		struct xnsched *sched = xntimer_sched(timer);

		if (smp_processor_id() != AGENCY_RT_CPU)
			xnclock_remote_shot(sched); /* Occurs during (periodic) thread initialization on CPU #0 */
		else
			xnclock_program_shot(sched);
	}
}

/**
 * Arm a timer.
 *
 * Activates a timer so that the associated timeout handler will be
 * fired after each expiration time. A timer can be either periodic or
 * one-shot, depending on the reload value passed to this routine. The
 * given timer must have been previously initialized.
 *
 * A timer is attached to the clock specified in xntimer_init().
 *
 * @param timer The address of a valid timer descriptor.
 *
 * @param value The date of the initial timer shot, expressed in
 * nanoseconds.
 *
 * @param interval The reload value of the timer. It is a periodic
 * interval value to be used for reprogramming the next timer shot,
 * expressed in nanoseconds. If @a interval is equal to XN_INFINITE,
 * the timer will not be reloaded after it has expired.
 *
 * @param mode The timer mode. It can be XN_RELATIVE if @a value shall
 * be interpreted as a relative date, XN_ABSOLUTE for an absolute date
 * based on the monotonic clock of the related time base (as returned
 * my xnclock_read_monotonic()), or XN_REALTIME if the absolute date
 * is based on the adjustable real-time date for the relevant clock
 * (obtained from xnclock_read_realtime()).
 *
 * @return 0 is returned upon success, or -ETIMEDOUT if an absolute
 * date in the past has been given. In such an event, the timer is
 * nevertheless armed for the next shot in the timeline if @a interval
 * is different from XN_INFINITE.
 *
 * @coretags{unrestricted, atomic-entry}
 */
int xntimer_start(struct xntimer *timer,
		  xnticks_t value, xnticks_t interval,
		  xntmode_t mode)
{
	xnticks_t date, now, delay, period;
	int ret = 0;

	trace_cobalt_timer_start(timer, value, interval, mode);

	if ((timer->status & XNTIMER_DEQUEUED) == 0)
		xntimer_dequeue(timer);

	now = xnclock_read();

	timer->status &= ~(XNTIMER_REALTIME | XNTIMER_FIRED | XNTIMER_PERIODIC);
	switch (mode) {
	case XN_RELATIVE:
		if ((xnsticks_t)value < 0)
			return -ETIMEDOUT;

		date = value + now;
		break;
	case XN_REALTIME:
		timer->status |= XNTIMER_REALTIME;
		value -= xnclock_read();
		/* fall through */
	default: /* XN_ABSOLUTE || XN_REALTIME */

		date = value;
		if ((xnsticks_t)(date - now) <= 0) {
			if (interval == XN_INFINITE)
				return -ETIMEDOUT;
			/*
			 * We are late on arrival for the first
			 * delivery, wait for the next shot on the
			 * periodic time line.
			 */
			delay = now - date;

			period = interval;
			date += period * (xnarch_div64(delay, period) + 1);
		}
		break;
	}

	timer->interval_ns = XN_INFINITE;
	timer->interval = XN_INFINITE;
	if (interval != XN_INFINITE) {
		timer->interval_ns = interval;
		timer->interval = interval;
		timer->periodic_ticks = 0;
		timer->start_date = date;
		timer->pexpect_ticks = 0;
		timer->status |= XNTIMER_PERIODIC;
	}
	timer->date = date;

	timer->status |= XNTIMER_RUNNING;

	xntimer_enqueue_and_program(timer);

	if (timer->status & XNTIMER_FIRED)
		ret = -ETIMEDOUT;

	return ret;
}
EXPORT_SYMBOL_GPL(xntimer_start);

/**
 * @fn int xntimer_stop(struct xntimer *timer)
 *
 * @brief Disarm a timer.
 *
 * This service deactivates a timer previously armed using
 * xntimer_start(). Once disarmed, the timer can be subsequently
 * re-armed using the latter service.
 *
 * @param timer The address of a valid timer descriptor.
 *
 * @coretags{unrestricted, atomic-entry}
 */
void __xntimer_stop(struct xntimer *timer)
{
	struct xnsched *sched;
	int heading = 0;

	trace_cobalt_timer_stop(timer);

	if ((timer->status & XNTIMER_DEQUEUED) == 0) {
		heading = xntimer_heading_p(timer);
		xntimer_dequeue(timer);
	}
	timer->status &= ~(XNTIMER_FIRED|XNTIMER_RUNNING);
	sched = xntimer_sched(timer);

	/*
	 * If we removed the heading timer, reprogram the next shot if
	 * any. If the timer was running on another CPU, let it tick.
	 */
	if (heading && sched == xnsched_current())
		xnclock_program_shot(sched);
}
EXPORT_SYMBOL_GPL(__xntimer_stop);

/**
 * @fn xnticks_t xntimer_get_date(struct xntimer *timer)
 *
 * @brief Return the absolute expiration date.
 *
 * Return the next expiration date of a timer as an absolute count of
 * nanoseconds.
 *
 * @param timer The address of a valid timer descriptor.
 *
 * @return The expiration date in nanoseconds. The special value
 * XN_INFINITE is returned if @a timer is currently disabled.
 *
 * @coretags{unrestricted, atomic-entry}
 */
xnticks_t xntimer_get_date(struct xntimer *timer)
{
	if (!xntimer_running_p(timer))
		return XN_INFINITE;

	return xntimer_expiry(timer);
}
EXPORT_SYMBOL_GPL(xntimer_get_date);

/**
 * @fn xnticks_t xntimer_get_timeout(struct xntimer *timer)
 *
 * @brief Return the relative expiration date.
 *
 * This call returns the count of nanoseconds remaining until the
 * timer expires.
 *
 * @param timer The address of a valid timer descriptor.
 *
 * @return The count of nanoseconds until expiry. The special value
 * XN_INFINITE is returned if @a timer is currently disabled.  It
 * might happen that the timer expires when this service runs (even if
 * the associated handler has not been fired yet); in such a case, 1
 * is returned.
 *
 * @coretags{unrestricted, atomic-entry}
 */
xnticks_t xntimer_get_timeout(struct xntimer *timer)
{
	xnticks_t expiry, now;

	if (!xntimer_running_p(timer))
		return XN_INFINITE;

	now = xnclock_read();
	expiry = xntimer_expiry(timer);
	if (expiry < now)
		return 1;  /* Will elapse shortly. */

	return expiry - now;
}
EXPORT_SYMBOL_GPL(xntimer_get_timeout);

/**
 * @fn void xntimer_init(struct xntimer *timer,struct xnclock *clock,void (*handler)(struct xntimer *timer), struct xnsched *sched, int flags)
 * @brief Initialize a timer object.
 *
 * Creates a timer. When created, a timer is left disarmed; it must be
 * started using xntimer_start() in order to be activated.
 *
 * @param timer The address of a timer descriptor the nucleus will use
 * to store the object-specific data.  This descriptor must always be
 * valid while the object is active therefore it must be allocated in
 * permanent memory.
 *
 * @param clock The clock the timer relates to. Xenomai defines a
 * monotonic system clock, with nanosecond resolution, named
 * nkclock. In addition, external clocks driven by other tick sources
 * may be created dynamically if CONFIG_XENO_OPT_EXTCLOCK is defined.
 *
 * @param handler The routine to call upon expiration of the timer.
 *
 * @param sched An optional pointer to the per-CPU scheduler slot the
 * new timer is affine to. If non-NULL, the timer will fire on the CPU
 * @a sched is bound to, otherwise it will fire either on the current
 * CPU if real-time, or on the first real-time CPU.
 *
 * @param flags A set of flags describing the timer. The valid flags are:
 *
 * - XNTIMER_NOBLCK, the timer won't be frozen while GDB takes over
 * control of the application.
 *
 * There is no limitation on the number of timers which can be
 * created/active concurrently.
 *
 * @coretags{unrestricted}
 */
#ifdef DOXYGEN_CPP
void xntimer_init(struct xntimer *timer, struct xnclock *clock,
		  void (*handler)(struct xntimer *timer),
		  struct xnsched *sched,
		  int flags);
#endif

void __xntimer_init(struct xntimer *timer,
		    struct xnclock *clock,
		    void (*handler)(struct xntimer *timer),
		    struct xnsched *sched,
		    int flags)
{
	unsigned long s __maybe_unused;
	int cpu;


	timer->date = XN_INFINITE;
	xntimer_set_priority(timer, XNTIMER_STDPRIO);
	timer->status = (XNTIMER_DEQUEUED|(flags & XNTIMER_INIT_MASK));
	timer->handler = handler;
	timer->interval_ns = 0;
	/*
	 * Timers are affine to a real-time CPU. If no affinity was
	 * specified, assign the timer to the first possible CPU which
	 * can receive interrupt events from the clock device attached
	 * to the reference clock for this timer.
	 */
	if (sched) {
		/*
		 * Complain loudly if no tick is expected from the
		 * clock device on the CPU served by the specified
		 * scheduler slot. This reveals a CPU affinity
		 * mismatch between the clock hardware and the client
		 * code initializing the timer. This check excludes
		 * core timers which may have their own reason to bind
		 * to a passive CPU (e.g. host timer).
		 */
		XENO_WARN_ON_SMP(COBALT, !(flags & __XNTIMER_CORE) && !cpumask_test_cpu(xnsched_cpu(sched), &clock->affinity));
		timer->sched = sched;
	} else {
		cpu = xnclock_get_default_cpu(clock, 0);
		timer->sched = xnsched_struct(cpu);
	}
}
EXPORT_SYMBOL_GPL(__xntimer_init);

/**
 * @fn void xntimer_destroy(struct xntimer *timer)
 *
 * @brief Release a timer object.
 *
 * Destroys a timer. After it has been destroyed, all resources
 * associated with the timer have been released. The timer is
 * automatically deactivated before deletion if active on entry.
 *
 * @param timer The address of a valid timer descriptor.
 *
 * @coretags{unrestricted}
 */
void xntimer_destroy(struct xntimer *timer)
{
	unsigned long s;

	xnlock_get_irqsave(&nklock, s);
	xntimer_stop(timer);
	timer->status |= XNTIMER_KILLED;
	timer->sched = NULL;

#ifdef CONFIG_XENO_OPT_STATS
	list_del(&timer->next_stat);
	clock->nrtimers--;
	xnvfile_touch(&clock->timer_vfile);
#endif /* CONFIG_XENO_OPT_STATS */

	xnlock_put_irqrestore(&nklock, s);
}
EXPORT_SYMBOL_GPL(xntimer_destroy);


/**
 * Get the count of overruns for the last tick.
 *
 * This service returns the count of pending overruns for the last
 * tick of a given timer, as measured by the difference between the
 * expected expiry date of the timer and the date @a now passed as
 * argument.
 *
 * @param timer The address of a valid timer descriptor.
 *
 * @param now current date (as
 * xnclock_read_raw(xntimer_clock(timer)))
 *
 * @return the number of overruns of @a timer at date @a now
 *
 * @coretags{unrestricted, atomic-entry}
 */
unsigned long long xntimer_get_overruns(struct xntimer *timer, xnticks_t now)
{
	xnticks_t period = timer->interval;
	xnsticks_t delta;
	unsigned long long overruns = 0;

	delta = now - xntimer_pexpect(timer);
	if (unlikely(delta >= (xnsticks_t) period)) {

		period = timer->interval_ns;
		overruns = xnarch_div64(delta, period);
		timer->pexpect_ticks += overruns;

		if (xntimer_running_p(timer)) {
			xntimer_dequeue(timer);
			while (timer->date < now) {
				timer->periodic_ticks++;
				xntimer_update_date(timer);
			}
			xntimer_enqueue_and_program(timer);
		}
	}

	timer->pexpect_ticks++;
	return overruns;
}
EXPORT_SYMBOL_GPL(xntimer_get_overruns);

/**
 * @internal
 * @fn static int program_htick_shot(unsigned long delay, struct clock_event_device *cdev)
 *
 * @brief Program next host tick as a Xenomai timer event.
 *
 * Program the next shot for the host tick on the current CPU.
 * Emulation is done using a nucleus timer attached to the master
 * timebase.
 *
 * @param delay The time delta from the current date to the next tick,
 * expressed as a count of nanoseconds.
 *
 * @param cdev An pointer to the clock device which notifies us.
 *
 * @coretags{unrestricted}
 */
static int program_htick_shot(unsigned long delay,
			      struct clock_event_device *cdev)
{
	struct xnsched *sched;
	int ret;
	unsigned long s;

	xnlock_get_irqsave(&nklock, s);
	sched = xnsched_current();
	ret = xntimer_start(&sched->htimer, delay, XN_INFINITE, XN_RELATIVE);
	xnlock_put_irqrestore(&nklock, s);

	return ret ? -ETIME : 0;
}

/**
 * @internal
 * @fn void switch_htick_mode(enum clock_event_mode mode, struct clock_event_device *cdev)
 *
 * @brief Tick mode switch emulation callback.
 *
 * Changes the host tick mode for the tick device of the current CPU.
 *
 * @param mode The new mode to switch to. The possible values are:
 *
 * - CLOCK_EVT_STATE_ONESHOT, for a switch to oneshot mode.
 *
 * - CLOCK_EVT_STATE_PERIODIC, for a switch to periodic mode. The current
 * implementation for the generic clockevent layer Linux exhibits
 * should never downgrade from a oneshot to a periodic tick mode, so
 * this mode should not be encountered. This said, the associated code
 * is provided, basically for illustration purposes.
 *
 * - CLOCK_EVT_STATE_SHUTDOWN, indicates the removal of the current
 * tick device. Normally, the nucleus only interposes on tick devices
 * which should never be shut down, so this mode should not be
 * encountered.
 *
 * @param cdev An opaque pointer to the clock device which notifies us.
 *
 * @coretags{unrestricted}
 *
 * @note GENERIC_CLOCKEVENTS is required from the host kernel.
 */

int htick_set_state_periodic(struct clock_event_device *dev) {
	xnticks_t tickval;
	struct xnsched *sched;
	unsigned long s;

	xnlock_get_irqsave(&nklock, s);

	sched = xnsched_current();
	tickval = 1000000000UL / HZ;

	xntimer_start(&sched->htimer, tickval, tickval, XN_RELATIVE);

	xnlock_put_irqrestore(&nklock, s);

	return 0;
}

int htick_set_state_oneshot(struct clock_event_device *dev) {
	return 0;
}

int htick_set_state_oneshot_stopped(struct clock_event_device *dev) {
	return 0;
}

int htick_set_state_shutdown(struct clock_event_device *dev) {
	struct xnsched *sched;
	unsigned long s;

	xnlock_get_irqsave(&nklock, s);

	sched = xnsched_current();

	xntimer_stop(&sched->htimer);

	xnlock_put_irqrestore(&nklock, s);

	return 0;
}

int xntimer_grab_hardware(void)
{
	struct xnsched *sched;
	unsigned long s;

	ipipe_timer_start(xnintr_core_clock_handler,
				htick_set_state_periodic, htick_set_state_oneshot, htick_set_state_oneshot_stopped,
				htick_set_state_shutdown, program_htick_shot, AGENCY_RT_CPU);

	xnlock_get_irqsave(&nklock, s);

	/*
	 * If the current tick device for the target CPU is
	 * periodic, we won't be called back for host tick
	 * emulation. Therefore, we need to start a periodic
	 * nucleus timer which will emulate the ticking for
	 * that CPU, since we are going to hijack the hw clock
	 * chip for managing our own system timer.
	 *
	 * CAUTION:
	 *
	 * - nucleus timers may be started only _after_ the hw
	 * timer has been set up for the target CPU through a
	 * call to xntimer_grab_hardware().
	 *
	 * - we don't compensate for the elapsed portion of
	 * the current host tick, since we cannot get this
	 * information easily for all CPUs except the current
	 * one, and also because of the declining relevance of
	 * the jiffies clocksource anyway.
	 *
	 * - we must not hold the nklock across calls to
	 * xntimer_grab_hardware().
	 */

	sched = xnsched_struct(AGENCY_RT_CPU);

	xnlock_put_irqrestore(&nklock, s);

	return 0;

}
EXPORT_SYMBOL_GPL(xntimer_grab_hardware);

/**
 * @fn void xntimer_release_hardware(void)
 * @brief Release hardware timers.
 *
 * Releases hardware timers previously grabbed by a call to
 * xntimer_grab_hardware().
 *
 * @coretags{secondary-only}
 */
void xntimer_release_hardware(void)
{
	int cpu;

	/*
	 * We must not hold the nklock while stopping the hardware
	 * timer, since this could cause deadlock situations to arise
	 * on SMP systems.
	 */
	for_each_realtime_cpu(cpu)
		ipipe_timer_stop(cpu);

#ifdef CONFIG_XENO_OPT_STATS
	xnintr_destroy(&nktimer);
#endif /* CONFIG_XENO_OPT_STATS */
}

void dump_xntimers(void) {
	struct list_head *p;
	struct xntimer *cur;

	lprintk("---------------- Dump of xntimers on CPU %d --------------\n", smp_processor_id());
	list_for_each(p, &__xntimers)
	{
		cur = list_entry(p, struct xntimer, link);
		lprintk("    timer vaddr: %p  handler vaddr: %p   date: %llu ** __xntimers: %p p->prev: %p p->next: %p \n", cur, cur->handler, cur->date, &__xntimers, p->prev, p->next);

	}
}
