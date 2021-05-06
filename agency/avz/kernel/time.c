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

#define CONFIG_GENERIC_CLOCKEVENTS

#include <config.h>
#include <smp.h>
#include <sched.h>
#include <config.h>
#include <errno.h>
#include <event.h>
#include <sched.h>
#include <config.h>
#include <time.h>
#include <timer.h>
#include <smp.h>
#include <softirq.h>
#include <limits.h>
#include <div64.h>

#include <device/timer.h>

#include <soo/soo.h>

#include <asm/bitops.h>
#include <asm/setup.h>
#include <asm/processor.h>

unsigned long loops_per_jiffy = (1 << 20);

extern struct clocksource *system_timer_clocksource;
extern struct clock_event_device *system_timer_clockevent;
struct sys_timer *system_timer = NULL;

extern spinlock_t hw_lock;

/*
 * Clockevent management - mainly taken from Linux
 */

/**
 * clockevents_set_mode - set the operating mode of a clock event device
 * @dev:	device to modify
 * @mode:	new mode
 *
 * Must be called with interrupts disabled !
 */
void clockevents_set_mode(struct clock_event_device *dev, enum clock_event_mode mode) {
	if (dev->mode != mode) {
		dev->set_mode(mode, dev);
		dev->mode = mode;
	}
}

static u64 cev_delta2ns(unsigned long latch, struct clock_event_device *evt,
		bool ismax) {
	u64 clc = (u64) latch << evt->shift;
	u64 rnd;

	if (unlikely(!evt->mult)) {
		evt->mult = 1;
		BUG();
	}
	rnd = (u64) evt->mult - 1;

	/*
	 * Upper bound sanity check. If the backwards conversion is
	 * not equal latch, we know that the above shift overflowed.
	 */
	if ((clc >> evt->shift) != (u64) latch)
		clc = ~0ULL;

	/*
	 * Scaled math oddities:
	 *
	 * For mult <= (1 << shift) we can safely add mult - 1 to
	 * prevent integer rounding loss. So the backwards conversion
	 * from nsec to device ticks will be correct.
	 *
	 * For mult > (1 << shift), i.e. device frequency is > 1GHz we
	 * need to be careful. Adding mult - 1 will result in a value
	 * which when converted back to device ticks can be larger
	 * than latch by up to (mult - 1) >> shift. For the min_delta
	 * calculation we still want to apply this in order to stay
	 * above the minimum device ticks limit. For the upper limit
	 * we would end up with a latch value larger than the upper
	 * limit of the device, so we omit the add to stay below the
	 * device upper boundary.
	 *
	 * Also omit the add if it would overflow the u64 boundary.
	 */
	if ((~0ULL - clc > rnd)
			&& (!ismax || evt->mult <= (1ULL << evt->shift)))
		clc += rnd;

	do_div(clc, evt->mult);

	/* Deltas less than 1usec are pointless noise */
	return clc > 1000 ? clc : 1000;
}

void clockevents_config(struct clock_event_device *dev, u32 freq, unsigned long min_delta, unsigned long max_delta) {
	u64 sec;

	dev->min_delta_ticks = min_delta;
	dev->max_delta_ticks = max_delta;

	/*
	 * Calculate the maximum number of seconds we can sleep. Limit
	 * to 10 minutes for hardware which can program more than
	 * 32bit ticks so we still get reasonable conversion values.
	 */
	sec = dev->max_delta_ticks;
	do_div(sec, freq);
	if (!sec)
		sec = 1;
	else if ((sec > 600) && (dev->max_delta_ticks > UINT_MAX))
		sec = 600;

	clockevents_calc_mult_shift(dev, freq, sec);

	dev->min_delta_ns = cev_delta2ns(dev->min_delta_ticks, dev, false);
	dev->max_delta_ns = cev_delta2ns(dev->max_delta_ticks, dev, true);
}

/**************/

/*
 * repogram_timer - May be used from various CPUs
 *
 * deadline is the expiring time expressed in ns.
 *
 */
void reprogram_timer(u64 deadline) {
	u64 clc;
	int64_t delta;
	struct clock_event_device *dev;

	/* Only the oneshot timer will be possibly reprogrammed */

	dev = system_timer_clockevent;

	if (dev->mode == CLOCK_EVT_MODE_SHUTDOWN)
		return;

	delta = deadline - NOW();
 
	if (delta <= 0)
		raise_softirq(TIMER_SOFTIRQ);
	else {
		delta = min((u64) delta, dev->max_delta_ns);
		delta = max((u64) delta, dev->min_delta_ns);

		clc = (delta * dev->mult) >> dev->shift;

		dev->set_next_event((unsigned long) clc, (struct clock_event_device *) dev);
	}
}

/**
 * clocks_calc_mult_shift - calculate mult/shift factors for scaled math of clocks
 * @mult:	pointer to mult variable
 * @shift:	pointer to shift variable
 * @from:	frequency to convert from
 * @to:		frequency to convert to
 * @maxsec:	guaranteed runtime conversion range in seconds
 *
 * The function evaluates the shift/mult pair for the scaled math
 * operations of clocksources and clockevents.
 *
 * @to and @from are frequency values in HZ. For clock sources @to is
 * NSEC_PER_SEC == 1GHz and @from is the counter frequency. For clock
 * event @to is the counter frequency and @from is NSEC_PER_SEC.
 *
 * The @maxsec conversion range argument controls the time frame in
 * seconds which must be covered by the runtime conversion with the
 * calculated mult and shift factors. This guarantees that no 64bit
 * overflow happens when the input value of the conversion is
 * multiplied with the calculated mult factor. Larger ranges may
 * reduce the conversion accuracy by chosing smaller mult and shift
 * factors.
 */
void clocks_calc_mult_shift(u32 *mult, u32 *shift, u32 from, u32 to, u32 maxsec) {
	u64 tmp;
	u32 sft, sftacc = 32;

	/*
	 * Calculate the shift factor which is limiting the conversion
	 * range:
	 */
	tmp = ((u64) maxsec * from) >> 32;
	while (tmp) {
		tmp >>= 1;
		sftacc--;
	}

	/*
	 * Find the conversion shift/mult pair which has the best
	 * accuracy and fits the maxsec conversion range:
	 */
	for (sft = 32; sft > 0; sft--) {
		tmp = (u64) to << sft;
		tmp += from / 2;
		do_div(tmp, from);
		if ((tmp >> sftacc) == 0)
			break;
	}
	*mult = tmp;
	*shift = sft;
}

cycle_t cs_read_dummy(void) {
	return (cycle_t) 0L;
}

struct clocksource cs_dummy = { .read = cs_read_dummy, };

int ce_set_next_event_dummy(unsigned long cycles, struct clock_event_device *evt) {
	return 0;
}

struct clock_event_device ce_default = {
	.set_next_event = ce_set_next_event_dummy,
};


/* Late init function (after all CPUs are booted). */
int init_time(void) {

	sys_time = 0ull;

	system_timer_clocksource = &cs_dummy;
	system_timer_clockevent = &ce_default;

	return 0;
}

void send_timer_event(struct domain *d) {
	send_guest_virq(d, VIRQ_TIMER);
}

