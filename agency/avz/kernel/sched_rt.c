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

#include <avz/lib.h>
#include <avz/sched.h>
#include <avz/sched-if.h>
#include <avz/timer.h>
#include <avz/softirq.h>
#include <avz/time.h>
#include <avz/errno.h>

#include <soo/soo.h>

#include <asm/config.h>

#include <soo/uapi/debug.h>
#include <soo/uapi/domctl.h>

DEFINE_SPINLOCK(sched_rt_lock);

struct scheduler sched_rt;

static struct vcpu *domains_runnable[MAX_DOMAINS];

/*
 * Main scheduling policy function.
 * The policy is based on a sched-fifo like policy, i.e. priority-based FIFO.
 */
struct task_slice rt_do_schedule(void)
{
	struct task_slice ret;
	unsigned int loopmax = MAX_DOMAINS;
	struct schedule_data *sd;

	ret.task = NULL;
	ret.time = 0;  /* Never used for realtime ME */

	sd = &sched_rt.sched_data;

	do {
		sd->current_dom = (sd->current_dom + 1) % MAX_DOMAINS;
		ret.task = domains_runnable[sd->current_dom];

		loopmax--;
	} while ((ret.task == NULL) && loopmax);

	if (ret.task == NULL)
		ret.task = idle_domain[smp_processor_id()]->vcpu[0];

#if 0
	printk("%s: on cpu %d picking now: %lx \n", __func__, smp_processor_id(), ret.task->domain->domain_id);
#endif


	return ret;
}

/*
 * schedule_lock is acquired.
 */
static void rt_sleep(struct vcpu *v)
{
	DBG("rt_sleep was called, domain-id %i\n", v->domain->domain_id);

	if (is_idle_vcpu(v))
		return;

	domains_runnable[v->domain->domain_id] = NULL;

	if (sched_rt.sched_data.current_dom == v->domain->domain_id)
		cpu_raise_softirq(v->processor, SCHEDULE_SOFTIRQ);

}


static void rt_wake(struct vcpu *v)
{
	DBG("rt_wake was called, domain-id %i.%i\n",v->processor, v->domain->domain_id);

	domains_runnable[v->domain->domain_id] = v;

	cpu_raise_softirq(v->processor, SCHEDULE_SOFTIRQ);

	/* We do not manage domain priorities, so we do not invoke schedule() at this time */

	/* We raise a timer IRQ to give the RT domain the change to go ahead with its execution. */
	send_timer_rt_event(v);

}

void sched_rt_init(void) {

	int i;

	for (i = 0; i < MAX_DOMAINS; i++)
		domains_runnable[i] = NULL;

	sched_rt.sched_data.current_dom = 0;

	spin_lock_init(&sched_rt.sched_data.schedule_lock);
}


struct scheduler sched_rt = {
		.name     = "SOO AVZ flip scheduler",

		.init = sched_rt_init,

		.do_schedule    = rt_do_schedule,
		
		.sleep          = rt_sleep,
		.wake           = rt_wake,
};
