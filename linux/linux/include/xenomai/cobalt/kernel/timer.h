/*
 * Copyright (C) 2001,2002,2003 Philippe Gerum <rpm@xenomai.org>.
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

#ifndef _COBALT_KERNEL_TIMER_H
#define _COBALT_KERNEL_TIMER_H

#include <cobalt/kernel/clock.h>
#include <cobalt/kernel/stat.h>
#include <cobalt/kernel/list.h>
#include <cobalt/kernel/assert.h>
#include <cobalt/kernel/ancillaries.h>

extern struct list_head __xntimers;

/**
 * @addtogroup cobalt_core_timer
 * @{
 */
#define XN_INFINITE   ((xnticks_t)0)
#define XN_NONBLOCK   ((xnticks_t)-1)

/* Timer modes */
typedef enum xntmode {
	XN_RELATIVE,
	XN_ABSOLUTE,
	XN_REALTIME
} xntmode_t;

/* Timer status */
#define XNTIMER_DEQUEUED  0x00000001
#define XNTIMER_KILLED    0x00000002
#define XNTIMER_PERIODIC  0x00000004
#define XNTIMER_REALTIME  0x00000008
#define XNTIMER_FIRED     0x00000010
#define XNTIMER_NOBLCK    0x00000020
#define XNTIMER_RUNNING   0x00000040

#define XNTIMER_INIT_MASK	(XNTIMER_NOBLCK)

#define __XNTIMER_CORE    0x10000000

/* These flags are available to the real-time interfaces */
#define XNTIMER_SPARE0  0x01000000
#define XNTIMER_SPARE1  0x02000000
#define XNTIMER_SPARE2  0x04000000
#define XNTIMER_SPARE3  0x08000000
#define XNTIMER_SPARE4  0x10000000
#define XNTIMER_SPARE5  0x20000000
#define XNTIMER_SPARE6  0x40000000
#define XNTIMER_SPARE7  0x80000000

/* Timer priorities */
#define XNTIMER_LOPRIO  (-999999999)
#define XNTIMER_STDPRIO 0
#define XNTIMER_HIPRIO  999999999

struct xntimer {

	/** Link in timers list. */
	struct list_head link;

	/** Timer status. */
	unsigned long status;
	/** Periodic interval (clock ticks, 0 == one shot). */
	xnticks_t interval;
	/** Periodic interval (nanoseconds, 0 == one shot). */
	xnticks_t interval_ns;
	/** Count of timer ticks in periodic mode. */
	xnticks_t periodic_ticks;
	/** First tick date in periodic mode. */
	xnticks_t start_date;
	/** Date of next periodic release point (timer ticks). */
	xnticks_t pexpect_ticks;
	/** Sched structure to which the timer is attached. */
	struct xnsched *sched;

	/* Current state */
	xnticks_t date;
	int prio;

	/** Timeout handler. */
	void (*handler)(struct xntimer *timer);
};

void dump_xntimers(void);

#define xntlist_init()		INIT_LIST_HEAD(&__xntimers)
#define xntlist_empty()		list_empty(&__xntimers)

static inline struct xntimer *xntlist_head(void)
{
	if (list_empty(&__xntimers))
		return NULL;

	return list_first_entry(&__xntimers, struct xntimer, link);
}

static inline struct xntimer *xntlist_next(struct xntimer *h)
{
	if (list_is_last(&h->link, &__xntimers))
		return NULL;

	return list_entry(h->link.next, struct xntimer, link);
}

static inline struct xntimer *xntlist_second(void)
{
	struct xntimer *h;

	if (list_empty(&__xntimers))
		return NULL;

	h = list_first_entry(&__xntimers, struct xntimer, link);

	return xntlist_next(h);
}

static inline void xntlist_insert(struct xntimer *holder)
{
	struct xntimer *p;

	if (list_empty(&__xntimers)) {
		list_add(&holder->link, &__xntimers);
		return;
	}

	/* Sanity checking */
	list_for_each_entry(p, &__xntimers, link) {
		/* Sanity checking */
		if (p == holder)
			BUG();
	}

	/*
	 * Insert the new timer at the proper place in the single
	 * queue. O(N) here, but this is the price for the increased
	 * flexibility...
	 */
	list_for_each_entry_reverse(p, &__xntimers, link) {

		if ((xnsticks_t) (holder->date - p->date) > 0 || (holder->date == p->date && holder->prio <= p->prio))
			break;
	}

	  list_add(&holder->link, &p->link);
}

static inline void xntlist_remove(struct xntimer *h) { list_del(&h->link); }

#define xntimerq_init()        	xntlist_init()

#define xntimerq_empty()       	xntlist_empty()
#define xntimerq_head()        	xntlist_head()
#define xntimerq_second()      	xntlist_second()
#define xntimerq_insert(h)   	xntlist_insert(h)
#define xntimerq_remove(h)   	xntlist_remove(h)

struct xnsched;

static inline void xntimer_set_clock(struct xntimer *timer,
				     struct xnclock *newclock)
{
	XENO_BUG_ON(COBALT, newclock != &nkclock);
}

static inline struct xnsched *xntimer_sched(struct xntimer *timer)
{
	return timer->sched;
}

#define xntimer_queue() (__xntimers)

static inline void xntimer_update_date(struct xntimer *timer)
{
	timer->date = timer->start_date + timer->periodic_ticks * timer->interval_ns;
}

static inline xnticks_t xntimer_pexpect(struct xntimer *timer)
{
	return timer->start_date + timer->pexpect_ticks * timer->interval_ns;
}

static inline void xntimer_set_priority(struct xntimer *timer, int prio) {
	timer->prio = prio;
}

static inline int xntimer_active_p(struct xntimer *timer)
{
	return timer->sched != NULL;
}

static inline int xntimer_running_p(struct xntimer *timer)
{
	return (timer->status & XNTIMER_RUNNING) != 0;
}

static inline int xntimer_fired_p(struct xntimer *timer)
{
	return (timer->status & XNTIMER_FIRED) != 0;
}

static inline int xntimer_periodic_p(struct xntimer *timer)
{
	return (timer->status & XNTIMER_PERIODIC) != 0;
}

void __xntimer_init(struct xntimer *timer,
		    struct xnclock *clock,
		    void (*handler)(struct xntimer *timer),
		    struct xnsched *sched,
		    int flags);

#define xntimer_init	__xntimer_init

static inline
void xntimer_switch_tracking(struct xntimer *timer,
			     struct xnclock *newclock) { }

void xntimer_destroy(struct xntimer *timer);

/**
 * @fn xnticks_t xntimer_interval(struct xntimer *timer)
 *
 * @brief Return the timer interval value.
 *
 * Return the timer interval value in nanoseconds.
 *
 * @param timer The address of a valid timer descriptor.
 *
 * @return The duration of a period in nanoseconds. The special value
 * XN_INFINITE is returned if @a timer is currently disabled or
 * one shot.
 *
 * @coretags{unrestricted, atomic-entry}
 */
static inline xnticks_t xntimer_interval(struct xntimer *timer)
{
	return timer->interval_ns;
}

static inline xnticks_t xntimer_expiry(struct xntimer *timer)
{
	/* Real expiry date in ticks without anticipation (no gravity) */
	return timer->date;
}

int xntimer_start(struct xntimer *timer,
		xnticks_t value,
		xnticks_t interval,
		xntmode_t mode);

void __xntimer_stop(struct xntimer *timer);

xnticks_t xntimer_get_date(struct xntimer *timer);
xnticks_t xntimer_get_timeout(struct xntimer *timer);
xnticks_t xntimer_get_interval(struct xntimer *timer);

int xntimer_heading_p(struct xntimer *timer);

static inline void xntimer_stop(struct xntimer *timer)
{
	if (timer->status & XNTIMER_RUNNING)
		__xntimer_stop(timer);
}

static inline xnticks_t xntimer_get_timeout_stopped(struct xntimer *timer)
{
	return xntimer_get_timeout(timer);
}

static inline void xntimer_enqueue(struct xntimer *timer)
{

	xntimerq_insert(timer);

	timer->status &= ~XNTIMER_DEQUEUED;
}

static inline void xntimer_dequeue(struct xntimer *timer) {
	xntimerq_remove(timer);

	timer->status |= XNTIMER_DEQUEUED;
}

unsigned long long xntimer_get_overruns(struct xntimer *timer, xnticks_t now);


int xntimer_setup_ipi(void);
void xntimer_release_ipi(void);

char *xntimer_format_time(xnticks_t ns, char *buf, size_t bufsz);

int xntimer_grab_hardware(void);

void xntimer_release_hardware(void);

#endif /* !_COBALT_KERNEL_TIMER_H */
