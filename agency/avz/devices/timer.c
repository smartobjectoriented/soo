
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

#include <spinlock.h>
#include <softirq.h>
#include <timer.h>
#include <domain.h>
#include <vcpu.h>
#include <sched.h>

#include <device/arch/arm_timer.h>

static DEFINE_SPINLOCK(timer_access);

u64 sys_time;

void timer_interrupt(bool periodic) {
	int i;

	if (periodic) {

		/* Now check for ticking the non-realtime domains which need periodic ticks. */
		for (i = 2; i < MAX_DOMAINS; i++) {
			/*
			 * We have to check if the domain exists and its VCPU has been created. If not,
			 * there is no need to propagate the timer event.
			 */
			if ((domains[i] != NULL) && !domains[i]->is_dying) {
				if ((domains[i]->runstate == RUNSTATE_running) || (domains[i]->runstate == RUNSTATE_runnable)) {
					if ((domains[i]->need_periodic_timer) && (domains[i]->shared_info != NULL))

						/* Forward to the guest */
						send_timer_event(domains[i]);
				}
			}
		}
	}

	 /* Raise a softirq on the CPU which is processing the interrupt. */
	raise_softirq(TIMER_SOFTIRQ);
}


/***************************************************************************
 * System Time
 ***************************************************************************/
/*
 * Return the time in ns from the monotonic clocksource.
 * May be called simultaneously by the agency CPU  and the ME CPU
 */
u64 get_s_time(void) {
	u64 cycle_now, cycle_delta;
	struct clocksource *clock = system_timer_clocksource;

	/* Protect against concurrent access from different CPUs */

	spin_lock(&timer_access);

	cycle_now = clock->read();

	if (clock->cycle_last > cycle_now)
		cycle_delta = (clock->mask - clock->cycle_last + cycle_now) & clock->mask;
	else
		cycle_delta = (cycle_now - clock->cycle_last) & clock->mask;

	clock->cycle_last = cycle_now;

	sys_time += cyc2ns(clock, cycle_delta);

	spin_unlock(&timer_access);

	return sys_time;
}

