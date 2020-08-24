/*
 * Copyright (C) 2014-2018 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <config.h>
#include <lib.h>
#include <sched.h>
#include <domain.h>
#include <event.h>
#include <time.h>
#include <timer.h>
#include <sched-if.h>
#include <softirq.h>
#include <spinlock.h>
#include <errno.h>

#include <device/irq.h>

#include <soo/soo.h>

#include <soo/uapi/schedop.h>

/* Various timer handlers. */

inline void vcpu_runstate_change(struct vcpu *v, int new_state) {

	/*
	 * We might already be in RUNSTATE_blocked before setting to this state; for example,
	 * if a ME has been paused and migrates, and is killed during the cooperation phase,
	 * the call to shutdown() will lead to be here with such a state already.
	 */
	ASSERT((v->runstate == RUNSTATE_blocked) || (v->runstate != new_state));

	v->runstate = new_state;
}

int sched_init_vcpu(struct vcpu *v, unsigned int processor) 
{
	struct domain *d = v->domain;
	unsigned int rc = 0;


	/* Idle VCPUs are scheduled immediately. */
	if (is_idle_domain(d))
		v->is_running = 1;

	return rc;
}

void sched_destroy_vcpu(struct vcpu *v)
{
	kill_timer(&v->oneshot_timer);
}

void vcpu_sleep_nosync(struct vcpu *v)
{
	bool __already_locked = false;

	if (spin_is_locked(&v->sched->sched_data.schedule_lock))
		__already_locked = true;
	else
		spin_lock(&v->sched->sched_data.schedule_lock);

	/*
	 * Stop associated timers.
	 * This code is executed by CPU #0. It means that we have to take care about potential
	 * re-insertion of new timers by CPU #3 activities after the oneshot_timer has been stopped.
	 * For this reason, we first set the VPF_blocked bit to 1, and the sched_deadline/sched_sleep have
	 * to check for this bit.
	 */
	set_bit(_VPF_blocked, &v->pause_flags);

	/* Now, setting the domain to blocked state */
	if (v->runstate != RUNSTATE_running)
		vcpu_runstate_change(v, RUNSTATE_blocked);

	if (active_timer(&v->oneshot_timer))
		stop_timer(&v->oneshot_timer);

	v->sched->sleep(v);

	if (!__already_locked)
		spin_unlock(&v->sched->sched_data.schedule_lock);
}

/*
 * Set a domain sleeping. The domain state is set to blocked.
 */
void vcpu_sleep_sync(struct vcpu *v)
{
	vcpu_sleep_nosync(v);

	while (v->is_running)
		cpu_relax();

}

void vcpu_wake(struct vcpu *v)
{
	bool __already_locked = false;

	if (spin_is_locked(&v->sched->sched_data.schedule_lock))
		__already_locked = true;
	else
		spin_lock(&v->sched->sched_data.schedule_lock);

	if (v->runstate >= RUNSTATE_blocked) {
		vcpu_runstate_change(v, RUNSTATE_runnable);

		v->sched->wake(v);

		clear_bit(_VPF_blocked, &v->pause_flags);
	}

	if (!__already_locked)
		spin_unlock(&v->sched->sched_data.schedule_lock);

}

/*
 * Voluntarily yield the processor to anther domain on this CPU.
 */
static long do_yield(void)
{
	raise_softirq(SCHEDULE_SOFTIRQ);

	return 0;
}

long do_sched_op(int cmd, void *args)
{
	long ret = 0;

	switch (cmd) {

	case SCHEDOP_yield:
		ret = do_yield();
		break;
	}

	return ret;
}

extern void dump_timerq(unsigned char key);

/* 
 * The main function
 * - deschedule the current domain (scheduler independent).
 * - pick a new domain (scheduler dependent).
 */
static void schedule(void)
{
	struct vcpu          *prev = current, *next = NULL;
	struct schedule_data *sd;
	struct task_slice     next_slice;

	ASSERT(local_irq_is_disabled());

	ASSERT(prev->runstate == RUNSTATE_running);

	/* To avoid that another CPU manipulates scheduler data structures */
	/* Maybe the lock is already acquired from do_sleep() for example */
	if (!spin_is_locked(&current->sched->sched_data.schedule_lock))
		spin_lock(&current->sched->sched_data.schedule_lock);

	sd = &current->sched->sched_data;

	stop_timer(&sd->s_timer);

	/* get policy-specific decision on scheduling... */
	next_slice = prev->sched->do_schedule();

	next = next_slice.task;

	if (next_slice.time > 0ull)
		set_timer(&next->sched->sched_data.s_timer, NOW() + MILLISECS(next_slice.time));


	if (unlikely(prev == next))
	{
		spin_unlock(&prev->sched->sched_data.schedule_lock);
		ASSERT(prev->runstate == RUNSTATE_running);

		return ;
	}
	ASSERT(prev->runstate == RUNSTATE_running);

	vcpu_runstate_change(prev, (test_bit(_VPF_blocked, &prev->pause_flags) ? RUNSTATE_blocked : (prev->domain->is_dying ? RUNSTATE_offline : RUNSTATE_runnable)));

	ASSERT(next->runstate != RUNSTATE_running);
	vcpu_runstate_change(next, RUNSTATE_running);

	ASSERT(!next->is_running);
	next->is_running = 1;

#if 0
	printk("### running on cpu: %d prev: %d next: %d\n", smp_processor_id(), prev->domain->domain_id, next->domain->domain_id);
#endif

	/* We do not unlock the schedulder_lock until everything has been processed */

	context_switch(prev, next);

	/* From here, prev and next are those in the current domain; don't forget ;-) */

}

void context_saved(struct vcpu *prev)
{
	prev->is_running = 0;
}

/** Just to bootstrap the agency **/
static struct task_slice agency_schedule(void)
{
	struct task_slice ts;

	ts.task = domains[0]->vcpu[0];
	ts.time = 0;

	return ts;
}

static void agency_wake(struct vcpu *d) {
	raise_softirq(SCHEDULE_SOFTIRQ);
}

struct scheduler sched_agency = {
		.name = "SOO AVZ agency activation",

		.wake = agency_wake,
		.do_schedule = agency_schedule
};


/* Initialise the data structures. */
void  scheduler_init(void)
{
	open_softirq(SCHEDULE_SOFTIRQ, schedule);

	sched_flip.init();
}

