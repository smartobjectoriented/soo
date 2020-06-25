/*
 * Real-Time Driver Model for Xenomai, driver library
 *
 * Copyright (C) 2005-2007 Jan Kiszka <jan.kiszka@web.de>
 * Copyright (C) 2005 Joerg Langenberg <joerg.langenberg@gmx.net>
 * Copyright (C) 2008 Gilles Chanteperdrix <gilles.chanteperdrix@xenomai.org>
 * Copyright (C) 2014 Philippe Gerum <rpm@xenomai.org>
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
 * along with Xenomai; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/mman.h>
#include <linux/highmem.h>
#include <linux/err.h>
#include <linux/anon_inodes.h>

#include <rtdm/driver.h>
#include "internal.h"

#include <trace/events/cobalt-rtdm.h>

#include <asm/page.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/smp.h>

#include <soo/uapi/soo.h>

#include <soo/evtchn.h>

/* Used to synchronize non-RT and RT agency during task creation */
DEFINE_SPINLOCK(nonrt_task_create_lock);

/**
 * @ingroup rtdm_driver_interface
 * @defgroup rtdm_clock Clock Services
 * @{
 */

#ifdef DOXYGEN_CPP /* Only used for doxygen doc generation */

/**
 * @brief Get monotonic time
 *
 * @return The monotonic time in nanoseconds is returned
 *
 * @note The resolution of this service depends on the system timer. In
 * particular, if the system timer is running in periodic mode, the return
 * value will be limited to multiples of the timer tick period.
 *
 * @note The system timer may have to be started to obtain valid results.
 * Whether this happens automatically (as on Xenomai) or is controlled by the
 * application depends on the RTDM host environment.
 *
 * @coretags{unrestricted}
 */
nanosecs_abs_t rtdm_clock_read(void);
#endif /* DOXYGEN_CPP */
/** @} */

/**
 * @ingroup rtdm_driver_interface
 * @defgroup rtdm_task Task Services
 * @{
 */

struct args_task_create {
	rtdm_task_t * volatile task;
	const char * volatile name;
	rtdm_task_proc_t task_proc;
	void * volatile arg;
	int priority;
	nanosecs_rel_t period;
};

static volatile struct args_task_create args_task_create;

/**
 * @brief Initialise and start a real-time task
 *
 * After initialising a task, the task handle remains valid and can be
 * passed to RTDM services until either rtdm_task_destroy() or
 * rtdm_task_join() was invoked.
 *
 * @param[in,out] task Task handle
 * @param[in] name Optional task name
 * @param[in] task_proc Procedure to be executed by the task
 * @param[in] arg Custom argument passed to @c task_proc() on entry
 * @param[in] priority Priority of the task, see also
 * @ref rtdmtaskprio "Task Priority Range"
 * @param[in] period Period in nanoseconds of a cyclic task, 0 for non-cyclic
 * mode. Waiting for the first and subsequent periodic events is
 * done using rtdm_task_wait_period().
 *
 * @return 0 on success, otherwise negative error code
 *
 * @coretags{secondary-only, might-switch}
 */
int rtdm_task_init(rtdm_task_t *task, const char *name,
		   rtdm_task_proc_t task_proc, void *arg,
		   int priority, nanosecs_rel_t period)
{
	union xnsched_policy_param param;
	struct xnthread_start_attr sattr;
	struct xnthread_init_attr iattr;
	int err;

	memset(&iattr, 0, sizeof(struct xnthread_init_attr));


	/* Now we take care where is the task created from (non-RT agency or RT agency) */
	if (smp_processor_id() != AGENCY_RT_CPU) {

		spin_lock(&nonrt_task_create_lock);

		args_task_create.task = task;
		args_task_create.name = name;
		args_task_create.task_proc = task_proc;
		args_task_create.arg = arg;
		args_task_create.priority = priority;
		args_task_create.period = period;

		smp_kick_rt_agency_for_task_create();

		return 0;
	}

	iattr.name = name;
	iattr.flags = 0;
	iattr.personality = &xenomai_personality;
	iattr.affinity = CPU_MASK_NONE;

	cpumask_set_cpu(AGENCY_RT_CPU, &iattr.affinity);

	param.rt.prio = priority;

	sattr.mode = 0;
	sattr.entry = task_proc;
	sattr.cookie = arg;

	err = xnthread_init(task, &iattr, &xnsched_class_rt, &param, period, &sattr, false);
	if (err)
		BUG();

	return 0;
}

EXPORT_SYMBOL_GPL(rtdm_task_init);

/*
 * Called asynchronously via IPI schedule call from the non-RT agency.
 */
void xnthread_do_task_create(void) {
	int err;
	struct args_task_create __args;
	union xnsched_policy_param param;
	struct xnthread_start_attr sattr;
	struct xnthread_init_attr iattr;

	__args = args_task_create;

	__args.task = args_task_create.task;
	__args.name = args_task_create.name;
	__args.task_proc = args_task_create.task_proc;
	__args.arg = args_task_create.arg;
	__args.priority = args_task_create.priority;
	__args.period = args_task_create.period;

	iattr.name = __args.name;
	iattr.flags = 0;
	iattr.personality = &xenomai_personality;
	iattr.affinity = CPU_MASK_NONE;
	cpumask_set_cpu(AGENCY_RT_CPU, &iattr.affinity);

	param.rt.prio = __args.priority;

	sattr.mode = 0;
	sattr.entry = __args.task_proc;
	sattr.cookie = __args.arg;

	spin_unlock(&nonrt_task_create_lock);

	err = xnthread_init(__args.task, &iattr, &xnsched_class_rt, &param, __args.period, &sattr, true);

	if (err)
		BUG();

}

#ifdef DOXYGEN_CPP /* Only used for doxygen doc generation */
/**
 * @brief Destroy a real-time task
 *
 * This call sends a termination request to @a task, then waits for it
 * to exit. All RTDM task should check for pending termination
 * requests by calling rtdm_task_should_stop() from their work loop.
 *
 * If @a task is current, rtdm_task_destroy() terminates the current
 * context, and does not return to the caller.
 *
 * @param[in,out] task Task handle as returned by rtdm_task_init()
 *
 * @note Passing the same task handle to RTDM services after the completion of
 * this function is not allowed.
 *
 * @coretags{secondary-only, might-switch}
 */
void rtdm_task_destroy(rtdm_task_t *task);

/**
 * @brief Check for pending termination request
 *
 * Check whether a termination request was received by the current
 * RTDM task. Termination requests are sent by calling
 * rtdm_task_destroy().
 *
 * @return Non-zero indicates that a termination request is pending,
 * in which case the caller should wrap up and exit.
 *
 * @coretags{rtdm-task, might-switch}
 */
int rtdm_task_should_stop(void);

/**
 * @brief Adjust real-time task priority
 *
 * @param[in,out] task Task handle as returned by rtdm_task_init()
 * @param[in] priority New priority of the task, see also
 * @ref rtdmtaskprio "Task Priority Range"
 *
 * @coretags{task-unrestricted, might-switch}
 */
void rtdm_task_set_priority(rtdm_task_t *task, int priority);

/**
 * @brief Adjust real-time task period
 *
 * @param[in,out] task Task handle as returned by rtdm_task_init(), or
 * NULL for referring to the current RTDM task or Cobalt thread.
 *
 * @param[in] start_date The initial (absolute) date of the first
 * release point, expressed in nanoseconds.  @a task will be delayed
 * by the first call to rtdm_task_wait_period() until this point is
 * reached. If @a start_date is zero, the first release point is set
 * to @a period nanoseconds after the current date.

 * @param[in] period New period in nanoseconds of a cyclic task, zero
 * to disable cyclic mode for @a task.
 *
 * @coretags{task-unrestricted}
 */
int rtdm_task_set_period(rtdm_task_t *task, nanosecs_abs_t start_date,
			 nanosecs_rel_t period);

/**
 * @brief Wait on next real-time task period
 *
 * @param[in] overruns_r Address of a long word receiving the count of
 * overruns if -ETIMEDOUT is returned, or NULL if the caller don't
 * need that information.
 *
 * @return 0 on success, otherwise:
 *
 * - -EINVAL is returned if calling task is not in periodic mode.
 *
 * - -ETIMEDOUT is returned if a timer overrun occurred, which indicates
 * that a previous release point has been missed by the calling task.
 *
 * @coretags{primary-only, might-switch}
 */
int rtdm_task_wait_period(unsigned long *overruns_r);

/**
 * @brief Activate a blocked real-time task
 *
 * @return Non-zero is returned if the task was actually unblocked from a
 * pending wait state, 0 otherwise.
 *
 * @coretags{unrestricted, might-switch}
 */
int rtdm_task_unblock(rtdm_task_t *task);

/**
 * @brief Get current real-time task
 *
 * @return Pointer to task handle
 *
 * @coretags{mode-unrestricted}
 */
rtdm_task_t *rtdm_task_current(void);

/**
 * @brief Sleep a specified amount of time
 *
 * @param[in] delay Delay in nanoseconds, see @ref RTDM_TIMEOUT_xxx for
 * special values.
 *
 * @return 0 on success, otherwise:
 *
 * - -EINTR is returned if calling task has been unblock by a signal or
 * explicitly via rtdm_task_unblock().
 *
 * - -EPERM @e may be returned if an illegal invocation environment is
 * detected.
 *
 * @coretags{primary-only, might-switch}
 */
int rtdm_task_sleep(nanosecs_rel_t delay);

/**
 * @brief Sleep until a specified absolute time
 *
 * @deprecated Use rtdm_task_sleep_abs instead!
 *
 * @param[in] wakeup_time Absolute timeout in nanoseconds
 *
 * @return 0 on success, otherwise:
 *
 * - -EINTR is returned if calling task has been unblock by a signal or
 * explicitly via rtdm_task_unblock().
 *
 * - -EPERM @e may be returned if an illegal invocation environment is
 * detected.
 *
 * @coretags{primary-only, might-switch}
 */
int rtdm_task_sleep_until(nanosecs_abs_t wakeup_time);

/**
 * @brief Sleep until a specified absolute time
 *
 * @param[in] wakeup_time Absolute timeout in nanoseconds
 * @param[in] mode Selects the timer mode, see RTDM_TIMERMODE_xxx for details
 *
 * @return 0 on success, otherwise:
 *
 * - -EINTR is returned if calling task has been unblock by a signal or
 * explicitly via rtdm_task_unblock().
 *
 * - -EPERM @e may be returned if an illegal invocation environment is
 * detected.
 *
 * - -EINVAL is returned if an invalid parameter was passed.
 *
 * @coretags{primary-only, might-switch}
 */
int rtdm_task_sleep_abs(nanosecs_abs_t wakeup_time, enum rtdm_timer_mode mode);

/**
 * @brief Safe busy waiting
 *
 * This service alternates active spinning and sleeping within a wait
 * loop, until a condition is satisfied. While sleeping, a task is
 * scheduled out and does not consume any CPU time.
 *
 * rtdm_task_busy_wait() is particularly useful for waiting for a
 * state change reading an I/O register, which usually happens shortly
 * after the wait starts, without incurring the adverse effects of
 * long busy waiting if it doesn't.
 *
 * @param[in] condition The C expression to be tested for detecting
 * completion.
 * @param[in] spin_ns The time to spin on @a condition before
 * sleeping, expressed as a count of nanoseconds.
 * @param[in] sleep_ns The time to sleep for before spinning again,
 * expressed as a count of nanoseconds.
 *
 * @return 0 on success if @a condition is satisfied, otherwise:
 *
 * - -EINTR is returned if the calling task has been unblocked by a
 * Linux signal or explicitly via rtdm_task_unblock().
 *
 * - -EPERM may be returned if an illegal invocation environment is
 * detected.
 *
 * @coretags{primary-only, might-switch}
 */
int rtdm_task_busy_wait(bool condition, nanosecs_rel_t spin_ns,
			nanosecs_rel_t sleep_ns);

#endif /* DOXYGEN_CPP */

int __rtdm_task_sleep(xnticks_t timeout, xntmode_t mode)
{
	struct xnthread *thread;

	if (!XENO_ASSERT(COBALT, !xnsched_unblockable_p()))
		return -EPERM;

	thread = xnthread_current();
	xnthread_suspend(thread, XNDELAY, timeout, mode, NULL);

	return xnthread_test_info(thread, XNBREAK) ? -EINTR : 0;
}

EXPORT_SYMBOL_GPL(__rtdm_task_sleep);

/**
 * @brief Wait on a real-time task to terminate
 *
 * @param[in,out] task Task handle as returned by rtdm_task_init()
 *
 * @note Passing the same task handle to RTDM services after the
 * completion of this function is not allowed.
 *
 * @note This service does not trigger the termination of the targeted
 * task.  The user has to take of this, otherwise rtdm_task_join()
 * will never return.
 *
 * @coretags{mode-unrestricted}
 */
void rtdm_task_join(rtdm_task_t *task)
{
	trace_cobalt_driver_task_join(task);

	xnthread_join(task, true);
}

EXPORT_SYMBOL_GPL(rtdm_task_join);

/**
 * @brief Busy-wait a specified amount of time
 *
 * This service does not schedule out the caller, but rather spins in
 * a tight loop, burning CPU cycles until the timeout elapses.
 *
 * @param[in] delay Delay in nanoseconds. Note that a zero delay does @b not
 * have the meaning of @c RTDM_TIMEOUT_INFINITE here.
 *
 * @note The caller must not be migratable to different CPUs while executing
 * this service. Otherwise, the actual delay will be undefined.
 *
 * @coretags{unrestricted}
 */
void rtdm_task_busy_sleep(nanosecs_rel_t delay)
{
	xnticks_t wakeup;

	wakeup = xnclock_read() + delay;

	while ((xnsticks_t)(xnclock_read() - wakeup) < 0)
		cpu_relax();
}

EXPORT_SYMBOL_GPL(rtdm_task_busy_sleep);
/** @} */

/**
 * @ingroup rtdm_driver_interface
 * @defgroup rtdm_timer Timer Services
 * @{
 */

#ifdef DOXYGEN_CPP /* Only used for doxygen doc generation */
/**
 * @brief Initialise a timer
 *
 * @param[in,out] timer Timer handle
 * @param[in] handler Handler to be called on timer expiry
 * @param[in] name Optional timer name
 *
 * @return 0 on success, otherwise negative error code
 *
 * @coretags{task-unrestricted}
 */
int rtdm_timer_init(rtdm_timer_t *timer, rtdm_timer_handler_t handler,
		    const char *name);
#endif /* DOXYGEN_CPP */

/**
 * @brief Destroy a timer
 *
 * @param[in,out] timer Timer handle as returned by rtdm_timer_init()
 *
 * @coretags{task-unrestricted}
 */
void rtdm_timer_destroy(rtdm_timer_t *timer)
{
	unsigned long s;

	xnlock_get_irqsave(&nklock, s);
	xntimer_destroy(timer);
	xnlock_put_irqrestore(&nklock, s);
}

EXPORT_SYMBOL_GPL(rtdm_timer_destroy);

/**
 * @brief Start a timer
 *
 * @param[in,out] timer Timer handle as returned by rtdm_timer_init()
 * @param[in] expiry Firing time of the timer, @c mode defines if relative or
 * absolute
 * @param[in] interval Relative reload value, > 0 if the timer shall work in
 * periodic mode with the specific interval, 0 for one-shot timers
 * @param[in] mode Defines the operation mode, see @ref RTDM_TIMERMODE_xxx for
 * possible values
 *
 * @return 0 on success, otherwise:
 *
 * - -ETIMEDOUT is returned if @c expiry describes an absolute date in
 * the past. In such an event, the timer is nevertheless armed for the
 * next shot in the timeline if @a interval is non-zero.
 *
 * @coretags{unrestricted}
 */
int rtdm_timer_start(rtdm_timer_t *timer, nanosecs_abs_t expiry,
		     nanosecs_rel_t interval, enum rtdm_timer_mode mode)
{
	unsigned long s;
	int err;

	xnlock_get_irqsave(&nklock, s);
	err = xntimer_start(timer, expiry, interval, (xntmode_t)mode);
	xnlock_put_irqrestore(&nklock, s);

	return err;
}

EXPORT_SYMBOL_GPL(rtdm_timer_start);

/**
 * @brief Stop a timer
 *
 * @param[in,out] timer Timer handle as returned by rtdm_timer_init()
 *
 * @coretags{unrestricted}
 */
void rtdm_timer_stop(rtdm_timer_t *timer)
{
	unsigned long s;

	xnlock_get_irqsave(&nklock, s);
	xntimer_stop(timer);
	xnlock_put_irqrestore(&nklock, s);
}

EXPORT_SYMBOL_GPL(rtdm_timer_stop);

#ifdef DOXYGEN_CPP /* Only used for doxygen doc generation */
/**
 * @brief Start a timer from inside a timer handler
 *
 * @param[in,out] timer Timer handle as returned by rtdm_timer_init()
 * @param[in] expiry Firing time of the timer, @c mode defines if relative or
 * absolute
 * @param[in] interval Relative reload value, > 0 if the timer shall work in
 * periodic mode with the specific interval, 0 for one-shot timers
 * @param[in] mode Defines the operation mode, see @ref RTDM_TIMERMODE_xxx for
 * possible values
 *
 * @return 0 on success, otherwise:
 *
 * - -ETIMEDOUT is returned if @c expiry describes an absolute date in the
 * past.
 *
 * @coretags{coreirq-only}
 */
int rtdm_timer_start_in_handler(rtdm_timer_t *timer, nanosecs_abs_t expiry,
				nanosecs_rel_t interval,
				enum rtdm_timer_mode mode);

/**
 * @brief Stop a timer from inside a timer handler
 *
 * @param[in,out] timer Timer handle as returned by rtdm_timer_init()
 *
 * @coretags{coreirq-only}
 */
void rtdm_timer_stop_in_handler(rtdm_timer_t *timer);
#endif /* DOXYGEN_CPP */
/** @} */

/* --- IPC cleanup helper --- */

#define RTDM_SYNCH_DELETED          XNSYNCH_SPARE0

void __rtdm_synch_flush(struct xnsynch *synch, unsigned long reason)
{
	unsigned long s;

	xnlock_get_irqsave(&nklock, s);

	if (reason == XNRMID)
		xnsynch_set_status(synch, RTDM_SYNCH_DELETED);

	if (likely(xnsynch_flush(synch, reason) == XNSYNCH_RESCHED))
		xnsched_run();

	xnlock_put_irqrestore(&nklock, s);
}

EXPORT_SYMBOL_GPL(__rtdm_synch_flush);

/**
 * @ingroup rtdm_driver_interface
 * @defgroup rtdm_sync Synchronisation Services
 * @{
 */

/*!
 * @name Timeout Sequence Management
 * @{
 */

/**
 * @brief Initialise a timeout sequence
 *
 * This service initialises a timeout sequence handle according to the given
 * timeout value. Timeout sequences allow to maintain a continuous @a timeout
 * across multiple calls of blocking synchronisation services. A typical
 * application scenario is given below.
 *
 * @param[in,out] timeout_seq Timeout sequence handle
 * @param[in] timeout Relative timeout in nanoseconds, see
 * @ref RTDM_TIMEOUT_xxx for special values
 *
 * Application Scenario:
 * @code
int device_service_routine(...)
{
	rtdm_toseq_t timeout_seq;
	...

	rtdm_toseq_init(&timeout_seq, timeout);
	...
	while (received < requested) {
		ret = rtdm_event_timedwait(&data_available, timeout, &timeout_seq);
		if (ret < 0) // including -ETIMEDOUT
			break;

		// receive some data
		...
	}
	...
}
 * @endcode
 * Using a timeout sequence in such a scenario avoids that the user-provided
 * relative @c timeout is restarted on every call to rtdm_event_timedwait(),
 * potentially causing an overall delay that is larger than specified by
 * @c timeout. Moreover, all functions supporting timeout sequences also
 * interpret special timeout values (infinite and non-blocking),
 * disburdening the driver developer from handling them separately.
 *
 * @coretags{task-unrestricted}
 */
void rtdm_toseq_init(rtdm_toseq_t *timeout_seq, nanosecs_rel_t timeout)
{
	XENO_WARN_ON(COBALT, xnsched_unblockable_p()); /* only warn here */

	*timeout_seq = xnclock_read() + timeout;
}

EXPORT_SYMBOL_GPL(rtdm_toseq_init);

/** @} */

/**
 * @ingroup rtdm_sync
 * @defgroup rtdm_sync_event Event Services
 * @{
 */

/**
 * @brief Initialise an event
 *
 * @param[in,out] event Event handle
 * @param[in] pending Non-zero if event shall be initialised as set, 0 otherwise
 *
 * @coretags{task-unrestricted}
 */
void rtdm_event_init(rtdm_event_t *event, unsigned long pending)
{
	unsigned long s;

	trace_cobalt_driver_event_init(event, pending);

	/* Make atomic for re-initialisation support */
	xnlock_get_irqsave(&nklock, s);

	xnsynch_init(&event->synch_base, XNSYNCH_PRIO, NULL);

	/* SOO.tech */
	if (pending) {
		xnsynch_set_status(&event->synch_base, RTDM_EVENT_PENDING);
		event->synch_base.occurrence = pending;
	} else
		event->synch_base.occurrence = 0;

	xnselect_init(&event->select_block);

	xnlock_put_irqrestore(&nklock, s);
}

EXPORT_SYMBOL_GPL(rtdm_event_init);

/**
 * @brief Destroy an event
 *
 * @param[in,out] event Event handle as returned by rtdm_event_init()
 *
 * @coretags{task-unrestricted, might-switch}
 */
void rtdm_event_destroy(rtdm_event_t *event)
{
	trace_cobalt_driver_event_destroy(event);
	__rtdm_synch_flush(&event->synch_base, XNRMID);
	xnselect_destroy(&event->select_block);
}
EXPORT_SYMBOL_GPL(rtdm_event_destroy);

/**
 * @brief Signal an event occurrence to currently listening waiters
 *
 * This function wakes up all current waiters of the given event, but it does
 * not change the event state. Subsequently callers of rtdm_event_wait() or
 * rtdm_event_timedwait() will therefore be blocked first.
 *
 * @param[in,out] event Event handle as returned by rtdm_event_init()
 *
 * @coretags{unrestricted, might-switch}
 */
void rtdm_event_pulse(rtdm_event_t *event)
{
	trace_cobalt_driver_event_pulse(event);
	__rtdm_synch_flush(&event->synch_base, 0);
}
EXPORT_SYMBOL_GPL(rtdm_event_pulse);

/**
 * @brief Signal an event occurrence
 *
 * This function sets the given event and wakes up all current waiters. If no
 * waiter is presently registered, the next call to rtdm_event_wait() or
 * rtdm_event_timedwait() will return immediately.
 *
 * @param[in,out] event Event handle as returned by rtdm_event_init()
 *
 * @coretags{unrestricted, might-switch}
 */

/* SOO.tech */

/*
 * We added the notion of event occurrence which can be multiple.
 * In case of multiple occurrences, each call to rtdm_event_wait() will
 * decrement one occurrence at a time. In that sense, it is pretty
 * similar to a realtime completion structure.
 */
void rtdm_event_signal(rtdm_event_t *event)
{
	int resched = 0;
	unsigned long s;

	trace_cobalt_driver_event_signal(event);

	xnlock_get_irqsave(&nklock, s);

	xnsynch_set_status(&event->synch_base, RTDM_EVENT_PENDING);

	/* SOO.tech */
	event->synch_base.occurrence++;

	if (xnsynch_flush(&event->synch_base, 0))
		resched = 1;
	if (xnselect_signal(&event->select_block, 1))
		resched = 1;
	if (resched)
		xnsched_run();

	xnlock_put_irqrestore(&nklock, s);
}

EXPORT_SYMBOL_GPL(rtdm_event_signal);

/**
 * @brief Wait on event occurrence
 *
 * This is the light-weight version of rtdm_event_timedwait(), implying an
 * infinite timeout.
 *
 * @param[in,out] event Event handle as returned by rtdm_event_init()
 *
 * @return 0 on success, otherwise:
 *
 * - -EINTR is returned if calling task has been unblock by a signal or
 * explicitly via rtdm_task_unblock().
 *
 * - -EIDRM is returned if @a event has been destroyed.
 *
 * - -EPERM @e may be returned if an illegal invocation environment is
 * detected.
 *
 * @coretags{primary-only, might-switch}
 */
int rtdm_event_wait(rtdm_event_t *event)
{
	return rtdm_event_timedwait(event, 0, NULL);
}

EXPORT_SYMBOL_GPL(rtdm_event_wait);

/**
 * @brief Wait on event occurrence with timeout
 *
 * This function waits or tests for the occurence of the given event, taking
 * the provided timeout into account. On successful return, the event is
 * reset.
 *
 * @param[in,out] event Event handle as returned by rtdm_event_init()
 * @param[in] timeout Relative timeout in nanoseconds, see
 * @ref RTDM_TIMEOUT_xxx for special values
 * @param[in,out] timeout_seq Handle of a timeout sequence as returned by
 * rtdm_toseq_init() or NULL
 *
 * @return 0 on success, otherwise:
 *
 * - -ETIMEDOUT is returned if the if the request has not been satisfied
 * within the specified amount of time.
 *
 * - -EINTR is returned if calling task has been unblock by a signal or
 * explicitly via rtdm_task_unblock().
 *
 * - -EIDRM is returned if @a event has been destroyed.
 *
 * - -EPERM @e may be returned if an illegal invocation environment is
 * detected.
 *
 * - -EWOULDBLOCK is returned if a negative @a timeout (i.e., non-blocking
 * operation) has been specified.
 *
 * @coretags{primary-only, might-switch}
 */
int rtdm_event_timedwait(rtdm_event_t *event, nanosecs_rel_t timeout,
			 rtdm_toseq_t *timeout_seq)
{
	struct xnthread *thread;
	int err = 0, ret;
	unsigned long s;

	if (!XENO_ASSERT(COBALT, !xnsched_unblockable_p()))
		return -EPERM;

	trace_cobalt_driver_event_wait(event, xnthread_current());

	xnlock_get_irqsave(&nklock, s);

	if (unlikely(event->synch_base.status & RTDM_SYNCH_DELETED))
		err = -EIDRM;
	else if (likely(event->synch_base.status & RTDM_EVENT_PENDING)) {

		/* SOO.tech */
		event->synch_base.occurrence--;

		if (event->synch_base.occurrence == 0) {
			xnsynch_clear_status(&event->synch_base, RTDM_EVENT_PENDING);
			xnselect_signal(&event->select_block, 0);
		}

	} else {
		/* non-blocking mode */
		if (timeout < 0) {
			err = -EWOULDBLOCK;
			goto unlock_out;
		}

		thread = xnthread_current();

		if (timeout_seq && (timeout > 0))
			/* timeout sequence */
			ret = xnsynch_sleep_on(&event->synch_base, *timeout_seq,
					       XN_ABSOLUTE);
		else
			/* infinite or relative timeout */
			ret = xnsynch_sleep_on(&event->synch_base, timeout, XN_RELATIVE);

		if (likely(ret == 0)) {

			/* SOO.tech */
			event->synch_base.occurrence--;

			if (event->synch_base.occurrence == 0) {
				xnsynch_clear_status(&event->synch_base,
						RTDM_EVENT_PENDING);
				xnselect_signal(&event->select_block, 0);
			}

		} else if (ret & XNTIMEO)
			err = -ETIMEDOUT;
		else if (ret & XNRMID)
			err = -EIDRM;
		else /* XNBREAK */
			err = -EINTR;
	}

unlock_out:
	xnlock_put_irqrestore(&nklock, s);

	return err;
}

EXPORT_SYMBOL_GPL(rtdm_event_timedwait);

/**
 * @brief Clear event state
 *
 * @param[in,out] event Event handle as returned by rtdm_event_init()
 *
 * @coretags{unrestricted}
 */
void rtdm_event_clear(rtdm_event_t *event)
{
	unsigned long s;

	trace_cobalt_driver_event_clear(event);

	xnlock_get_irqsave(&nklock, s);

	/* SOO.tech */
	/* We clear the number of occurrence of this event. */

	event->synch_base.occurrence = 0;

	xnsynch_clear_status(&event->synch_base, RTDM_EVENT_PENDING);
	xnselect_signal(&event->select_block, 0);

	xnlock_put_irqrestore(&nklock, s);
}

EXPORT_SYMBOL_GPL(rtdm_event_clear);

/**
 * @brief Bind a selector to an event
 *
 * This functions binds the given selector to an event so that the former is
 * notified when the event state changes. Typically the select binding handler
 * will invoke this service.
 *
 * @param[in,out] event Event handle as returned by rtdm_event_init()
 * @param[in,out] selector Selector as passed to the select binding handler
 * @param[in] type Type of the bound event as passed to the select binding handler
 * @param[in] fd_index File descriptor index as passed to the select binding
 * handler
 *
 * @return 0 on success, otherwise:
 *
 * - -ENOMEM is returned if there is insufficient memory to establish the
 * dynamic binding.
 *
 * - -EINVAL is returned if @a type or @a fd_index are invalid.
 *
 * @coretags{task-unrestricted}
 */
int rtdm_event_select(rtdm_event_t *event, rtdm_selector_t *selector,
		      enum rtdm_selecttype type, unsigned int fd_index)
{
	struct xnselect_binding *binding;
	int err;
	unsigned long s;

	binding = xnmalloc(sizeof(*binding));
	if (!binding)
		return -ENOMEM;

	xnlock_get_irqsave(&nklock, s);
	err = xnselect_bind(&event->select_block,
			    binding, selector, type, fd_index,
			    event->synch_base.status & (RTDM_SYNCH_DELETED |
						       RTDM_EVENT_PENDING));
	xnlock_put_irqrestore(&nklock, s);

	if (err)
		xnfree(binding);

	return err;
}
EXPORT_SYMBOL_GPL(rtdm_event_select);

/** @} */

/**
 * @ingroup rtdm_sync
 * @defgroup rtdm_sync_sem Semaphore Services
 * @{
 */

/**
 * @brief Initialise a semaphore
 *
 * @param[in,out] sem Semaphore handle
 * @param[in] value Initial value of the semaphore
 *
 * @coretags{task-unrestricted}
 */
void rtdm_sem_init(rtdm_sem_t *sem, unsigned long value)
{
	unsigned long s;

	trace_cobalt_driver_sem_init(sem, value);

	/* Make atomic for re-initialisation support */
	xnlock_get_irqsave(&nklock, s);

	sem->value = value;
	xnsynch_init(&sem->synch_base, XNSYNCH_PRIO, NULL);
	xnselect_init(&sem->select_block);

	xnlock_put_irqrestore(&nklock, s);
}

EXPORT_SYMBOL_GPL(rtdm_sem_init);

/**
 * @brief Destroy a semaphore
 *
 * @param[in,out] sem Semaphore handle as returned by rtdm_sem_init()
 *
 * @coretags{task-unrestricted, might-switch}
 */
void rtdm_sem_destroy(rtdm_sem_t *sem)
{
	trace_cobalt_driver_sem_destroy(sem);
	__rtdm_synch_flush(&sem->synch_base, XNRMID);
	xnselect_destroy(&sem->select_block);
}
EXPORT_SYMBOL_GPL(rtdm_sem_destroy);

/**
 * @brief Decrement a semaphore
 *
 * This is the light-weight version of rtdm_sem_timeddown(), implying an
 * infinite timeout.
 *
 * @param[in,out] sem Semaphore handle as returned by rtdm_sem_init()
 *
 * @return 0 on success, otherwise:
 *
 * - -EINTR is returned if calling task has been unblock by a signal or
 * explicitly via rtdm_task_unblock().
 *
 * - -EIDRM is returned if @a sem has been destroyed.
 *
 * - -EPERM @e may be returned if an illegal invocation environment is
 * detected.
 *
 * @coretags{primary-only, might-switch}
 */
int rtdm_sem_down(rtdm_sem_t *sem)
{
	return rtdm_sem_timeddown(sem, 0, NULL);
}

EXPORT_SYMBOL_GPL(rtdm_sem_down);

/**
 * @brief Decrement a semaphore with timeout
 *
 * This function tries to decrement the given semphore's value if it is
 * positive on entry. If not, the caller is blocked unless non-blocking
 * operation was selected.
 *
 * @param[in,out] sem Semaphore handle as returned by rtdm_sem_init()
 * @param[in] timeout Relative timeout in nanoseconds, see
 * @ref RTDM_TIMEOUT_xxx for special values
 * @param[in,out] timeout_seq Handle of a timeout sequence as returned by
 * rtdm_toseq_init() or NULL
 *
 * @return 0 on success, otherwise:
 *
 * - -ETIMEDOUT is returned if the if the request has not been satisfied
 * within the specified amount of time.
 *
 * - -EWOULDBLOCK is returned if @a timeout is negative and the semaphore
 * value is currently not positive.
 *
 * - -EINTR is returned if calling task has been unblock by a signal or
 * explicitly via rtdm_task_unblock().
 *
 * - -EIDRM is returned if @a sem has been destroyed.
 *
 * - -EPERM @e may be returned if an illegal invocation environment is
 * detected.
 *
 * @coretags{primary-only, might-switch}
 */
int rtdm_sem_timeddown(rtdm_sem_t *sem, nanosecs_rel_t timeout,
		       rtdm_toseq_t *timeout_seq)
{
	struct xnthread *thread;
	int err = 0, ret;
	unsigned long s;

	if (!XENO_ASSERT(COBALT, !xnsched_unblockable_p()))
		return -EPERM;

	trace_cobalt_driver_sem_wait(sem, xnthread_current());

	xnlock_get_irqsave(&nklock, s);

	if (unlikely(sem->synch_base.status & RTDM_SYNCH_DELETED))
		err = -EIDRM;
	else if (sem->value > 0) {
		if(!--sem->value)
			xnselect_signal(&sem->select_block, 0);
	} else if (timeout < 0) /* non-blocking mode */
		err = -EWOULDBLOCK;
	else {
		thread = xnthread_current();

		if (timeout_seq && timeout > 0)
			/* timeout sequence */
			ret = xnsynch_sleep_on(&sem->synch_base, *timeout_seq,
					       XN_ABSOLUTE);
		else
			/* infinite or relative timeout */
			ret = xnsynch_sleep_on(&sem->synch_base, timeout, XN_RELATIVE);

		if (ret) {
			if (ret & XNTIMEO)
				err = -ETIMEDOUT;
			else if (ret & XNRMID)
				err = -EIDRM;
			else /* XNBREAK */
				err = -EINTR;
		}
	}

	xnlock_put_irqrestore(&nklock, s);

	return err;
}

EXPORT_SYMBOL_GPL(rtdm_sem_timeddown);

/**
 * @brief Increment a semaphore
 *
 * This function increments the given semphore's value, waking up a potential
 * waiter which was blocked upon rtdm_sem_down().
 *
 * @param[in,out] sem Semaphore handle as returned by rtdm_sem_init()
 *
 * @coretags{unrestricted, might-switch}
 */
void rtdm_sem_up(rtdm_sem_t *sem)
{
	unsigned long s;

	trace_cobalt_driver_sem_up(sem);

	xnlock_get_irqsave(&nklock, s);

	if (xnsynch_wakeup_one_sleeper(&sem->synch_base))
		xnsched_run();
	else
		if (sem->value++ == 0
		    && xnselect_signal(&sem->select_block, 1))
			xnsched_run();

	xnlock_put_irqrestore(&nklock, s);
}

EXPORT_SYMBOL_GPL(rtdm_sem_up);

/**
 * @brief Bind a selector to a semaphore
 *
 * This functions binds the given selector to the semaphore so that the former
 * is notified when the semaphore state changes. Typically the select binding
 * handler will invoke this service.
 *
 * @param[in,out] sem Semaphore handle as returned by rtdm_sem_init()
 * @param[in,out] selector Selector as passed to the select binding handler
 * @param[in] type Type of the bound event as passed to the select binding handler
 * @param[in] fd_index File descriptor index as passed to the select binding
 * handler
 *
 * @return 0 on success, otherwise:
 *
 * - -ENOMEM is returned if there is insufficient memory to establish the
 * dynamic binding.
 *
 * - -EINVAL is returned if @a type or @a fd_index are invalid.
 *
 * @coretags{task-unrestricted}
 */
int rtdm_sem_select(rtdm_sem_t *sem, rtdm_selector_t *selector,
		    enum rtdm_selecttype type, unsigned int fd_index)
{
	struct xnselect_binding *binding;
	int err;
	unsigned long s;

	binding = xnmalloc(sizeof(*binding));
	if (!binding)
		return -ENOMEM;

	xnlock_get_irqsave(&nklock, s);
	err = xnselect_bind(&sem->select_block, binding, selector,
			    type, fd_index,
			    (sem->value > 0) ||
			    sem->synch_base.status & RTDM_SYNCH_DELETED);
	xnlock_put_irqrestore(&nklock, s);

	if (err)
		xnfree(binding);

	return err;
}
EXPORT_SYMBOL_GPL(rtdm_sem_select);

/** @} */

/**
 * @ingroup rtdm_sync
 * @defgroup rtdm_sync_mutex Mutex services
 * @{
 */

/**
 * @brief Initialise a mutex
 *
 * This function initalises a basic mutex with priority inversion protection.
 * "Basic", as it does not allow a mutex owner to recursively lock the same
 * mutex again.
 *
 * @param[in,out] mutex Mutex handle
 *
 * @coretags{task-unrestricted}
 */
void rtdm_mutex_init(rtdm_mutex_t *mutex)
{
	unsigned long s;

	/* Make atomic for re-initialisation support */
	xnlock_get_irqsave(&nklock, s);
	xnsynch_init(&mutex->synch_base, XNSYNCH_PIP, &mutex->fastlock);
	xnlock_put_irqrestore(&nklock, s);
}
EXPORT_SYMBOL_GPL(rtdm_mutex_init);

/**
 * @brief Destroy a mutex
 *
 * @param[in,out] mutex Mutex handle as returned by rtdm_mutex_init()
 *
 * @coretags{task-unrestricted, might-switch}
 */
void rtdm_mutex_destroy(rtdm_mutex_t *mutex)
{
	trace_cobalt_driver_mutex_destroy(mutex);

	__rtdm_synch_flush(&mutex->synch_base, XNRMID);
}
EXPORT_SYMBOL_GPL(rtdm_mutex_destroy);

/**
 * @brief Release a mutex
 *
 * This function releases the given mutex, waking up a potential waiter which
 * was blocked upon rtdm_mutex_lock() or rtdm_mutex_timedlock().
 *
 * @param[in,out] mutex Mutex handle as returned by rtdm_mutex_init()
 *
 * @coretags{primary-only, might-switch}
 */
void rtdm_mutex_unlock(rtdm_mutex_t *mutex)
{
	if (!XENO_ASSERT(COBALT, !xnsched_interrupt_p()))
		return;

	trace_cobalt_driver_mutex_release(mutex);

	if (unlikely(xnsynch_release(&mutex->synch_base,
				     xnsched_current_thread()) != NULL))
		xnsched_run();
}
EXPORT_SYMBOL_GPL(rtdm_mutex_unlock);

/**
 * @brief Request a mutex
 *
 * This is the light-weight version of rtdm_mutex_timedlock(), implying an
 * infinite timeout.
 *
 * @param[in,out] mutex Mutex handle as returned by rtdm_mutex_init()
 *
 * @return 0 on success, otherwise:
 *
 * - -EIDRM is returned if @a mutex has been destroyed.
 *
 * - -EPERM @e may be returned if an illegal invocation environment is
 * detected.
 *
 * @coretags{primary-only, might-switch}
 */
int rtdm_mutex_lock(rtdm_mutex_t *mutex)
{
	return rtdm_mutex_timedlock(mutex, 0, NULL);
}

EXPORT_SYMBOL_GPL(rtdm_mutex_lock);

/**
 * @brief Request a mutex with timeout
 *
 * This function tries to acquire the given mutex. If it is not available, the
 * caller is blocked unless non-blocking operation was selected.
 *
 * @param[in,out] mutex Mutex handle as returned by rtdm_mutex_init()
 * @param[in] timeout Relative timeout in nanoseconds, see
 * @ref RTDM_TIMEOUT_xxx for special values
 * @param[in,out] timeout_seq Handle of a timeout sequence as returned by
 * rtdm_toseq_init() or NULL
 *
 * @return 0 on success, otherwise:
 *
 * - -ETIMEDOUT is returned if the if the request has not been satisfied
 * within the specified amount of time.
 *
 * - -EWOULDBLOCK is returned if @a timeout is negative and the semaphore
 * value is currently not positive.
 *
 * - -EIDRM is returned if @a mutex has been destroyed.
 *
 * - -EPERM @e may be returned if an illegal invocation environment is
 * detected.
 *
 * @coretags{primary-only, might-switch}
 */
int rtdm_mutex_timedlock(rtdm_mutex_t *mutex, nanosecs_rel_t timeout,
			 rtdm_toseq_t *timeout_seq)
{
	struct xnthread *curr;
	int ret;
	unsigned long s;

	if (!XENO_ASSERT(COBALT, !xnsched_unblockable_p()))
		return -EPERM;

	curr = xnthread_current();
	trace_cobalt_driver_mutex_wait(mutex, curr);

	xnlock_get_irqsave(&nklock, s);

	if (unlikely(mutex->synch_base.status & RTDM_SYNCH_DELETED)) {
		ret = -EIDRM;
		goto out;
	}

	ret = xnsynch_try_acquire(&mutex->synch_base);
	if (ret != -EBUSY)
		goto out;

	if (timeout < 0) {
		ret = -EWOULDBLOCK;
		goto out;
	}

	for (;;) {
		if (timeout_seq && timeout > 0) /* timeout sequence */
			ret = xnsynch_acquire(&mutex->synch_base, *timeout_seq,
					      XN_ABSOLUTE);
		else		/* infinite or relative timeout */
			ret = xnsynch_acquire(&mutex->synch_base, timeout,
					      XN_RELATIVE);
		if (ret == 0)
			break;
		if (ret & XNBREAK)
			continue;
		ret = ret & XNTIMEO ? -ETIMEDOUT : -EIDRM;
		break;
	}
out:
	xnlock_put_irqrestore(&nklock, s);

	return ret;
}

EXPORT_SYMBOL_GPL(rtdm_mutex_timedlock);
/** @} */

/** @} Synchronisation services */

/**
 * @ingroup rtdm_driver_interface
 * @defgroup rtdm_irq Interrupt Management Services
 * @{
 */



/**
 * @brief Register an interrupt handler
 *
 * This function registers the provided handler with an IRQ line and enables
 * the line.
 *
 * @param[in,out] irq_handle IRQ handle
 * @param[in] irq_no Line number of the addressed IRQ
 * @param[in] handler Interrupt handler
 * @param[in] flags Registration flags, see @ref RTDM_IRQTYPE_xxx for details
 * @param[in] device_name Device name to show up in real-time IRQ lists
 * @param[in] arg Pointer to be passed to the interrupt handler on invocation
 *
 * @return 0 on success, otherwise:
 *
 * - -EINVAL is returned if an invalid parameter was passed.
 *
 * - -EBUSY is returned if the specified IRQ line is already in use.
 *
 * @coretags{secondary-only}
 */
int rtdm_irq_request(rtdm_irq_t *irq_handle, unsigned int irq_no,
		     rtdm_irq_handler_t handler, unsigned long flags,
		     const char *device_name, void *arg)
{
	int err;
	xniack_t ack_fn = NULL;

	BUG_ON(smp_processor_id() != AGENCY_RT_CPU);

	err = xnintr_init(irq_handle, device_name, irq_no, handler, ack_fn, flags);
	if (err)
		return err;

	xnintr_attach(irq_handle, arg);
	if (err)
		BUG();

	xnintr_enable(irq_handle);

	return 0;
}

EXPORT_SYMBOL_GPL(rtdm_irq_request);

#ifdef DOXYGEN_CPP /* Only used for doxygen doc generation */
/**
 * @brief Release an interrupt handler
 *
 * @param[in,out] irq_handle IRQ handle as returned by rtdm_irq_request()
 *
 * @return 0 on success, otherwise negative error code
 *
 * @note The caller is responsible for shutting down the IRQ source at device
 * level before invoking this service. In turn, rtdm_irq_free ensures that any
 * pending event on the given IRQ line is fully processed on return from this
 * service.
 *
 * @coretags{secondary-only}
 */
int rtdm_irq_free(rtdm_irq_t *irq_handle);

/**
 * @brief Enable interrupt line
 *
 * @param[in,out] irq_handle IRQ handle as returned by rtdm_irq_request()
 *
 * @return 0 on success, otherwise negative error code
 *
 * @note This service is for exceptional use only. Drivers should
 * always prefer interrupt masking at device level (via corresponding
 * control registers etc.)  over masking at line level. Keep in mind
 * that the latter is incompatible with IRQ line sharing and can also
 * be more costly as interrupt controller access requires broader
 * synchronization. Also, such service is solely available from
 * secondary mode. The caller is responsible for excluding such
 * conflicts.
 *
 * @coretags{secondary-only}
 */
int rtdm_irq_enable(rtdm_irq_t *irq_handle);

/**
 * @brief Disable interrupt line
 *
 * @param[in,out] irq_handle IRQ handle as returned by rtdm_irq_request()
 *
 * @return 0 on success, otherwise negative error code
 *
 * @note This service is for exceptional use only. Drivers should
 * always prefer interrupt masking at device level (via corresponding
 * control registers etc.)  over masking at line level. Keep in mind
 * that the latter is incompatible with IRQ line sharing and can also
 * be more costly as interrupt controller access requires broader
 * synchronization.  Also, such service is solely available from
 * secondary mode.  The caller is responsible for excluding such
 * conflicts.
 *
 * @coretags{secondary-only}
 */
int rtdm_irq_disable(rtdm_irq_t *irq_handle);
#endif /* DOXYGEN_CPP */

/** @} Interrupt Management Services */

/**
 * @ingroup rtdm_driver_interface
 * @defgroup rtdm_nrtsignal Non-Real-Time Signalling Services
 *
 * These services provide a mechanism to request the execution of a specified
 * handler in non-real-time context. The triggering can safely be performed in
 * real-time context without suffering from unknown delays. The handler
 * execution will be deferred until the next time the real-time subsystem
 * releases the CPU to the non-real-time part.
 * @{
 */

#ifdef DOXYGEN_CPP /* Only used for doxygen doc generation */

/**
 * @brief Register a non-real-time signal handler
 *
 * @param[in,out] nrt_sig Signal handle
 * @param[in] handler Non-real-time signal handler
 * @param[in] arg Custom argument passed to @c handler() on each invocation
 *
 * @return 0 on success, otherwise:
 *
 * - -EAGAIN is returned if no free signal slot is available.
 *
 * @coretags{task-unrestricted}
 */
int rtdm_nrtsig_init(rtdm_nrtsig_t *nrt_sig, rtdm_nrtsig_handler_t handler,
		     void *arg);

/**
 * @brief Release a non-realtime signal handler
 *
 * @param[in,out] nrt_sig Signal handle
 *
 * @coretags{task-unrestricted}
 */
void rtdm_nrtsig_destroy(rtdm_nrtsig_t *nrt_sig);
#endif /* DOXYGEN_CPP */


/**
 * @brief Enforces a rate limit
 *
 * This function enforces a rate limit: not more than @a rs->burst callbacks
 * in every @a rs->interval.
 *
 * @param[in,out] rs rtdm_ratelimit_state data
 * @param[in] func name of calling function
 *
 * @return 0 means callback will be suppressed and 1 means go ahead and do it
 *
 * @coretags{unrestricted}
 */
int rtdm_ratelimit(struct rtdm_ratelimit_state *rs, const char *func)
{
	rtdm_lockctx_t lock_ctx;
	int ret;

	if (!rs->interval)
		return 1;

	rtdm_lock_get_irqsave(&rs->lock, lock_ctx);

	if (!rs->begin)
		rs->begin = rtdm_clock_read();
	if (rtdm_clock_read() >= rs->begin + rs->interval) {
		if (rs->missed)
			printk(KERN_WARNING "%s: %d callbacks suppressed\n",
			       func, rs->missed);
		rs->begin   = 0;
		rs->printed = 0;
		rs->missed  = 0;
	}
	if (rs->burst && rs->burst > rs->printed) {
		rs->printed++;
		ret = 1;
	} else {
		rs->missed++;
		ret = 0;
	}
	rtdm_lock_put_irqrestore(&rs->lock, lock_ctx);

	return ret;
}
EXPORT_SYMBOL_GPL(rtdm_ratelimit);

#ifdef DOXYGEN_CPP /* Only used for doxygen doc generation */

/**
 * Real-time safe rate-limited message printing on kernel console
 *
 * @param[in] format Format string (conforming standard @c printf())
 * @param ... Arguments referred by @a format
 *
 * @return On success, this service returns the number of characters printed.
 * Otherwise, a negative error code is returned.
 *
 * @coretags{unrestricted}
 */
void rtdm_printk_ratelimited(const char *format, ...);

/**
 * Real-time safe message printing on kernel console
 *
 * @param[in] format Format string (conforming standard @c printf())
 * @param ... Arguments referred by @a format
 *
 * @return On success, this service returns the number of characters printed.
 * Otherwise, a negative error code is returned.
 *
 * @coretags{unrestricted}
 */
void rtdm_printk(const char *format, ...);

/**
 * Allocate memory block
 *
 * @param[in] size Requested size of the memory block
 *
 * @return The pointer to the allocated block is returned on success, NULL
 * otherwise.
 *
 * @coretags{unrestricted}
 */
void *rtdm_malloc(size_t size);

/**
 * Release real-time memory block
 *
 * @param[in] ptr Pointer to memory block as returned by rtdm_malloc()
 *
 * @coretags{unrestricted}
 */
void rtdm_free(void *ptr);

/**
 * Check if read access to user-space memory block is safe
 *
 * @param[in] fd RTDM file descriptor as passed to the invoked
 * device operation handler
 * @param[in] ptr Address of the user-provided memory block
 * @param[in] size Size of the memory block
 *
 * @return Non-zero is return when it is safe to read from the specified
 * memory block, 0 otherwise.
 *
 * @coretags{task-unrestricted}
 */
int rtdm_read_user_ok(struct rtdm_fd *fd, const void __user *ptr,
		      size_t size);

/**
 * Check if read/write access to user-space memory block is safe
 *
 * @param[in] fd RTDM file descriptor as passed to the invoked
 * device operation handler
 * @param[in] ptr Address of the user-provided memory block
 * @param[in] size Size of the memory block
 *
 * @return Non-zero is return when it is safe to read from or write to the
 * specified memory block, 0 otherwise.
 *
 * @coretags{task-unrestricted}
 */
int rtdm_rw_user_ok(struct rtdm_fd *fd, const void __user *ptr,
		    size_t size);

/**
 * Copy user-space memory block to specified buffer
 *
 * @param[in] fd RTDM file descriptor as passed to the invoked
 * device operation handler
 * @param[in] dst Destination buffer address
 * @param[in] src Address of the user-space memory block
 * @param[in] size Size of the memory block
 *
 * @return 0 on success, otherwise:
 *
 * - -EFAULT is returned if an invalid memory area was accessed.
 *
 * @note Before invoking this service, verify via rtdm_read_user_ok() that the
 * provided user-space address can securely be accessed.
 *
 * @coretags{task-unrestricted}
 */
int rtdm_copy_from_user(struct rtdm_fd *fd, void *dst,
			const void __user *src, size_t size);

/**
 * Check if read access to user-space memory block and copy it to specified
 * buffer
 *
 * @param[in] fd RTDM file descriptor as passed to the invoked
 * device operation handler
 * @param[in] dst Destination buffer address
 * @param[in] src Address of the user-space memory block
 * @param[in] size Size of the memory block
 *
 * @return 0 on success, otherwise:
 *
 * - -EFAULT is returned if an invalid memory area was accessed.
 *
 * @note This service is a combination of rtdm_read_user_ok and
 * rtdm_copy_from_user.
 *
 * @coretags{task-unrestricted}
 */
int rtdm_safe_copy_from_user(struct rtdm_fd *fd, void *dst,
			     const void __user *src, size_t size);

/**
 * Copy specified buffer to user-space memory block
 *
 * @param[in] fd RTDM file descriptor as passed to the invoked
 * device operation handler
 * @param[in] dst Address of the user-space memory block
 * @param[in] src Source buffer address
 * @param[in] size Size of the memory block
 *
 * @return 0 on success, otherwise:
 *
 * - -EFAULT is returned if an invalid memory area was accessed.
 *
 * @note Before invoking this service, verify via rtdm_rw_user_ok() that the
 * provided user-space address can securely be accessed.
 *
 * @coretags{task-unrestricted}
 */
int rtdm_copy_to_user(struct rtdm_fd *fd, void __user *dst,
		      const void *src, size_t size);

/**
 * Check if read/write access to user-space memory block is safe and copy
 * specified buffer to it
 *
 * @param[in] fd RTDM file descriptor as passed to the invoked
 * device operation handler
 * @param[in] dst Address of the user-space memory block
 * @param[in] src Source buffer address
 * @param[in] size Size of the memory block
 *
 * @return 0 on success, otherwise:
 *
 * - -EFAULT is returned if an invalid memory area was accessed.
 *
 * @note This service is a combination of rtdm_rw_user_ok and
 * rtdm_copy_to_user.
 *
 * @coretags{task-unrestricted}
 */
int rtdm_safe_copy_to_user(struct rtdm_fd *fd, void __user *dst,
			   const void *src, size_t size);

/**
 * Copy user-space string to specified buffer
 *
 * @param[in] fd RTDM file descriptor as passed to the invoked
 * device operation handler
 * @param[in] dst Destination buffer address
 * @param[in] src Address of the user-space string
 * @param[in] count Maximum number of bytes to copy, including the trailing
 * '0'
 *
 * @return Length of the string on success (not including the trailing '0'),
 * otherwise:
 *
 * - -EFAULT is returned if an invalid memory area was accessed.
 *
 * @note This services already includes a check of the source address,
 * calling rtdm_read_user_ok() for @a src explicitly is not required.
 *
 * @coretags{task-unrestricted}
 */
int rtdm_strncpy_from_user(struct rtdm_fd *fd, char *dst,
			   const char __user *src, size_t count);

/**
 * Test if running in a real-time task
 *
 * @return Non-zero is returned if the caller resides in real-time context, 0
 * otherwise.
 *
 * @coretags{unrestricted}
 */
int rtdm_in_rt_context(void);

/**
 * Test if the caller is capable of running in real-time context
 *
 * @param[in] fd RTDM file descriptor as passed to the invoked
 * device operation handler
 *
 * @return Non-zero is returned if the caller is able to execute in real-time
 * context (independent of its current execution mode), 0 otherwise.
 *
 * @note This function can be used by drivers that provide different
 * implementations for the same service depending on the execution mode of
 * the caller. If a caller requests such a service in non-real-time context
 * but is capable of running in real-time as well, it might be appropriate
 * for the driver to reject the request via -ENOSYS so that RTDM can switch
 * the caller and restart the request in real-time context.
 *
 * @coretags{unrestricted}
 */
int rtdm_rt_capable(struct rtdm_fd *fd);

#endif /* DOXYGEN_CPP */

/** @} Utility Services */
