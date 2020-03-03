/*
 * Copyright (C) 2016,2017 Daniel Rossier <daniel.rossier@soo.tech>
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

#ifndef _ASM_TIME_H_
#define _ASM_TIME_H_

#include <avz/types.h>
#include <asm/clock.h>

#define HZ		CONFIG_HZ

extern unsigned long loops_per_jiffy;

/* Parameters used to convert the timespec values */
#ifndef USEC_PER_SEC
#define USEC_PER_SEC (1000000L)
#endif

#ifndef NSEC_PER_SEC
#define NSEC_PER_SEC (1000000000L)
#endif

#ifndef NSEC_PER_USEC
#define NSEC_PER_USEC (1000L)
#endif

void timer_interrupt(bool periodic);

typedef u64 cycles_t;
extern struct clocksource *system_timer_clocksource;
static inline cycles_t get_cycles(void)
{
    return system_timer_clocksource->read();
}

extern void clocks_calc_mult_shift(u32 *mult, u32 *shift, u32 from, u32 to, u32 maxsec);

void clockevents_config(struct clock_event_device *dev, u32 freq, unsigned long min_delta, unsigned long max_delta);
static inline void clockevents_calc_mult_shift(struct clock_event_device *ce, u32 freq, u32 minsec)
{
  return clocks_calc_mult_shift(&ce->mult, &ce->shift, NSEC_PER_SEC, freq, minsec);
}

/* Pointer to platform main clock source and timer */
extern struct clocksource *system_timer_clocksource;
extern struct clock_event_device *system_timer_clockevent;

#endif
