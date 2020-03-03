/*
 * Copyright (C) 2014-2018 Daniel Rossier <daniel.rossier@heig-vd.ch>
 * Copyright (C) 2017 Baptiste Delporte <bonel@bonel.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <avz/config.h>
#include <avz/init.h>
#include <avz/types.h>
#include <avz/errno.h>
#include <avz/sched.h>
#include <avz/lib.h>
#include <avz/smp.h>
#include <avz/time.h>
#include <avz/softirq.h>
#include <avz/timer.h>
#include <avz/keyhandler.h>
#include <avz/percpu.h>
#include <avz/cpumask.h>
#include <avz/xmalloc.h>

#include <asm/system.h>

#include <soo/soo.h>

static void dump_timerq(unsigned char key);

u64 edf_current = STIME_MAX;

struct timers {

	spinlock_t lock;
	bool_t overflow;
	struct timer **heap;
	struct timer *list;
	struct timer *running;

}__cacheline_aligned;

static DEFINE_PER_CPU(struct timers, timers);

/* Tell if a CPU is a host_timer_cpu, i.e. a CPU which is bound to a physcal timer */
bool is_host_timer_cpu(int cpu) {
	return ((cpu == AGENCY_CPU) || (cpu == AGENCY_RT_CPU));
}

static void remove_from_list(struct timer **list, struct timer *t) {
	struct timer *curr, *prev;

	if (*list == t) {
		*list = t->list_next;
		t->list_next = NULL;
	} else {

		curr = *list;
		while (curr != t) {
			prev = curr;
			curr = curr->list_next;
		}
		prev->list_next = curr->list_next;
		curr->list_next = NULL;
	}
}

static void add_to_list(struct timer **list, struct timer *t) {
	struct timer *curr;

	if (*list == NULL) {
		*list = t;
		t->list_next = NULL;
	} else {

		/* Check for an existing timer and update the deadline if so. */

		curr = *list;
		while (curr != NULL) {
			if (curr == t) {
				curr->expires = t->expires;
				return;
			}
			curr = curr->list_next;
		}

		/* Not found, we add the new timer at the list head */
		t->list_next = *list;
		*list = t;

	}
}

/****************************************************************************
 * TIMER OPERATIONS.
 */

static int remove_entry(struct timers *timers, struct timer *t) {
	int rc = 0;

	switch (t->status) {
	case TIMER_STATUS_in_list:
		remove_from_list(&timers->list, t);
		break;

	default:
		rc = 0;
		printk("t->status = %d\n", t->status);
		dump_stack();
		BUG();
	}

	t->status = TIMER_STATUS_inactive;
	return rc;
}

static void add_entry(struct timers *timers, struct timer *t) {

	/* The agency CPU (CPU #0) manages periodic timers only with a fast path propagation to the guest. */
	ASSERT(t->cpu != AGENCY_CPU);

	ASSERT(t->status == TIMER_STATUS_inactive);

	t->status = TIMER_STATUS_in_list;

	add_to_list(&timers->list, t);
}

static inline void add_timer(struct timer *timer) {
	add_entry(&per_cpu(timers, timer->cpu), timer);
}

static inline void timer_lock(struct timer *timer) {
	spin_lock(&per_cpu(timers, timer->cpu).lock);
}

static inline void timer_unlock(struct timer *timer) {
	spin_unlock(&per_cpu(timers, timer->cpu).lock);
}

/*
 * Stop a timer, i.e. remove from the timer list.
 */
void __stop_timer(struct timer *timer) {

	if (active_timer(timer))
		remove_entry(&per_cpu(timers, timer->cpu), timer);
}

void set_timer(struct timer *timer, u64 expires) {

	timer_lock(timer);

	if (active_timer(timer))
		__stop_timer(timer);

	timer->expires = expires;

	if (likely(timer->status != TIMER_STATUS_killed))
		add_timer(timer);

	timer_unlock(timer);
}

void stop_timer(struct timer *timer) {

	timer_lock(timer);
	__stop_timer(timer);
	timer_unlock(timer);
}

void kill_timer(struct timer *timer) {

	BUG_ON(this_cpu(timers).running == timer);

	timer_lock(timer);

	if (active_timer(timer))
		__stop_timer(timer);

	timer->status = TIMER_STATUS_killed;

	timer_unlock(timer);
}

static void execute_timer(struct timers *ts, struct timer *t) {
	void (*fn)(void *) = t->function;
	void *data = t->data;

	ts->running = t;

	spin_unlock(&ts->lock);
	(*fn)(data);
	spin_lock(&ts->lock);

	ts->running = NULL;

}

/*
 * Main timer softirq processing
 */
static void timer_softirq_action(void) {
	struct timer *cur, *t, *start;
	struct timers *ts;

	u64 now;
	u64 end = STIME_MAX;

	ts = &this_cpu(timers);

	spin_lock(&ts->lock);

	now = NOW();

	/* Execute ready list timers. */
	cur = ts->list;

	/* Verify if some timers reached their deadline, remove them and execute them if any. */
	while (cur != NULL) {
		t = cur;
		cur = cur->list_next;

		if (t->expires <= now) {
#if 0
			printk("### %s: NOW: %llu executing timer expires: %llu   ***  delta: %d\n", __func__, now, t->expires, t->expires - now);
#endif
			remove_entry(ts, t);
			execute_timer(ts, t);
		}
	}

	/* Examine the pending timers to get the earliest deadline */
	start = NULL;
	t = ts->list;
	while (t != NULL) {

		if (((start == NULL) && (t->expires < end)) || (t->expires < start->expires))
			start = t;

		t = t->list_next;
	}

	/* Check if it is needed to reprogram the timer, only for ME RT CPU since ME standard CPU has periodic timer */

	if ((smp_processor_id() == ME_RT_CPU) && (start != NULL)) 
		reprogram_timer(start->expires); 

	spin_unlock(&ts->lock);

}

static void dump_timer(struct timer *t, u64 now) {
	/* We convert 1000 to u64 in order to use the well-implemented __aeabi_uldivmod function */
	printk("  expires = %llu, now = %llu, expires - now = %llu ns timer=%p cb=%p(%p) cpu=%d\n",
			t->expires, now, t->expires - now, t, t->function, t->data, t->cpu);
}

static void dump_timerq(unsigned char key) {
	struct timer *t;
	struct timers *ts;
	u64 now = NOW();
	int i, j;

	printk("Dumping timer queues:\n");

	for_each_online_cpu(i)
	{
		ts = &per_cpu(timers, i);

		printk("CPU #%d:\n", i);
		spin_lock(&ts->lock);

		for (t = ts->list, j = 0; t != NULL; t = t->list_next, j++)
			dump_timer(t, now);

		spin_unlock(&ts->lock);
	}
}

static struct keyhandler dump_timerq_keyhandler = { .diagnostic = 1, .u.fn =
		dump_timerq, .desc = "dump timer queues" };

void __init timer_init(void) {
	int i;

	open_softirq(TIMER_SOFTIRQ, timer_softirq_action);

	for_each_possible_cpu(i)
		spin_lock_init(&per_cpu(timers, i).lock);

	register_keyhandler('a', &dump_timerq_keyhandler);
}

