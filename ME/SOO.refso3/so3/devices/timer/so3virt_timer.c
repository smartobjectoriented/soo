/*
 * Copyright (C) 2016-2018 Daniel Rossier <daniel.rossier@soo.tech>
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

#include <common.h>
#include <softirq.h>
#include <schedule.h>
#include <timer.h>

#include <asm/io.h>

#include <device/timer.h>
#include <device/device.h>
#include <device/driver.h>
#include <device/irq.h>

#include <device/arch/arm_timer.h>

#include <soo/evtchn.h>

static irq_return_t timer_isr(int irq, void *data) {

	jiffies++;

	raise_softirq(TIMER_SOFTIRQ);

	return IRQ_COMPLETED;
}


/* Clocksource */

/*
 * Read the clocksource timer value taking into account a possible existing ref
 * from a previous location (other smart object).
 *
 */
u64 clocksource_read(void) {

	return arch_counter_get_cntvct();
}

/* Called after a migration. */
void postmig_adjust_timer(void) {

	clocksource_timer.cycle_last = clocksource_timer.read();

}

/*
 * Initialize the clocksource timer for free-running timer (used for system time)
 */
static int clocksource_timer_init(dev_t *dev) {

	clocksource_timer.dev = dev;
	clocksource_timer.cycle_last = 0;

	clocksource_timer.read = clocksource_read;
	clocksource_timer.rate = arch_timer_get_cntfrq();
	clocksource_timer.mask = CLOCKSOURCE_MASK(56);

	return 0;
}

void periodic_timer_start(void) {

	periodic_timer.period = NSECS / HZ;

	clocks_calc_mult_shift(&clocksource_timer.mult, &clocksource_timer.shift, clocksource_timer.rate, NSECS, 3600);

	bind_virq_to_irqhandler(VIRQ_TIMER, timer_isr, NULL, NULL);
}

void oneshot_timer_start(void) {

	clocks_calc_mult_shift(&clocksource_timer.mult, &clocksource_timer.shift, clocksource_timer.rate, NSECS, 3600);

	bind_virq_to_irqhandler(VIRQ_TIMER_RT, timer_isr, NULL, NULL);

}

/*
 * Program the oneshot timer with a deadline expressed in ns.
 */
void oneshot_timer_set_delay(uint64_t delay_ns) {
	avz_sched_deadline(delay_ns);
}


/*
 * Initialize the periodic timer used by the kernel.
 */
static int periodic_timer_init(dev_t *dev) {

	periodic_timer.dev = dev;

	/* Initialize Timer */

	periodic_timer.start = periodic_timer_start;

	return 0;
}

#ifdef CONFIG_RTOS
static int oneshot_timer_init(dev_t *dev) {

	oneshot_timer.dev = dev;

	oneshot_timer.set_delay = oneshot_timer_set_delay;
	oneshot_timer.start = oneshot_timer_start;

	return 0;
}
#endif /* CONFIG_RTOS */

REGISTER_DRIVER_CORE("so3virt-timer,periodic-timer", periodic_timer_init);
REGISTER_DRIVER_CORE("so3virt-timer,clocksource-timer", clocksource_timer_init);

#ifdef CONFIG_RTOS
REGISTER_DRIVER(so3virt_oneshot_timer, "so3virt-timer,oneshot-timer", oneshot_timer_init);
#endif /* CONFIG_RTOS */


