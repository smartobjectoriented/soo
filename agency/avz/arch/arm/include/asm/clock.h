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


#ifndef CLOCK_H
#define CLOCK_H

/* Clocksource */

#include <avz/types.h>
#include <avz/time.h>

#include <asm/div64.h>
#include <asm/cache.h>
#include <asm/irq.h>


/* clocksource cycle base type */
typedef u64 cycle_t;
struct clocksource;
struct timespec;

/**
 * struct clocksource - hardware abstraction for a free running counter
 *	Provides mostly state-free accessors to the underlying hardware.
 *
 * @name:		ptr to clocksource name
 * @list:		list head for registration
 * @rating:		rating value for selection (higher is better)
 *			To avoid rating inflation the following
 *			list should give you a guide as to how
 *			to assign your clocksource a rating
 *			1-99: Unfit for real use
 *				Only available for bootup and testing purposes.
 *			100-199: Base level usability.
 *				Functional for real use, but not desired.
 *			200-299: Good.
 *				A correct and usable clocksource.
 *			300-399: Desired.
 *				A reasonably fast and accurate clocksource.
 *			400-499: Perfect
 *				The ideal clocksource. A must-use where
 *				available.
 * @read:		returns a cycle value
 * @mask:		bitmask for two's complement
 *			subtraction of non 64 bit counters
 * @mult:		cycle to nanosecond multiplier
 * @mult_orig:          cycle to nanosecond multiplier (unadjusted by NTP)
 * @shift:		cycle to nanosecond divisor (power of two)
 * @flags:		flags describing special properties
 *
 */
struct clocksource {
	/*
	 * First part of structure is read mostly
	 */
	char *name;

	void __iomem *base;  /* virt address to access the counter */
	void __iomem *vaddr;  /* virt address to read the clocksource (not the base) */

	unsigned int rate;

	cycle_t (*read)(void);
	cycle_t mask;
	u32 mult;
	u32 mult_orig;
	u32 shift;
	unsigned long flags;

	/*
	 * Second part is written at each timer interrupt
	 * Keep it in a different cache line to dirty no
	 * more than one cache line.
	 */
	cycle_t cycle_last ____cacheline_aligned_in_smp;

};

/*
 * Clock source flags bits::
 */
#define CLOCK_SOURCE_IS_CONTINUOUS		0x01
#define CLOCK_SOURCE_MUST_VERIFY		  0x02

#define CLOCK_SOURCE_WATCHDOG				  0x10
#define CLOCK_SOURCE_VALID_FOR_HRES		0x20
#define CLOCK_SOURCE_WATCHDOG         0x10
#define CLOCK_SOURCE_UNSTABLE         0x40
#define CLOCK_SOURCE_SUSPEND_NONSTOP  0x80
#define CLOCK_SOURCE_RESELECT         0x100

/* simplify initialization of mask field */
#define CLOCKSOURCE_MASK(bits) (cycle_t)(bits<64 ? ((1ULL<<bits)-1) : -1)



/**
 * clocksource_read: - Access the clocksource's current cycle value
 * @cs:		pointer to clocksource being read
 *
 * Uses the clocksource to return the current cycle_t value
 */
static inline cycle_t clocksource_read(struct clocksource *cs)
{
	return cs->read();
}

/**
 * cyc2ns - converts clocksource cycles to nanoseconds
 * @cs:		Pointer to clocksource
 * @cycles:	Cycles
 *
 * Uses the clocksource and ntp ajdustment to convert cycle_ts to nanoseconds.
 *
 * XXX - This could use some mult_lxl_ll() asm optimization
 */
static inline u64 cyc2ns(struct clocksource *cs, u64 cycles)
{
	return ((u64) cycles * cs->mult) >> cs->shift;
}

/* used to install a new clocksource */
extern int clocksource_register(struct clocksource*);

/* Clock event */

struct clock_event_device;

/* Clock event mode commands */
enum clock_event_mode {
	CLOCK_EVT_MODE_UNUSED = 0,
	CLOCK_EVT_MODE_SHUTDOWN,
	CLOCK_EVT_MODE_PERIODIC,
	CLOCK_EVT_MODE_ONESHOT,
};

/*
 * Clock event features
 */
#define CLOCK_EVT_FEAT_PERIODIC		0x000001
#define CLOCK_EVT_FEAT_ONESHOT		0x000002

/**
 * struct clock_event_device - clock event device descriptor
 * @name:		ptr to clock event name
 * @features:		features

 * @mult:		nanosecond to cycles multiplier
 * @shift:		nanoseconds to cycles divisor (power of two)
 * @rating:		variable to rate clock event devices
 * @irq:		IRQ number (only for non CPU local devices)
 * @cpumask:		cpumask to indicate for which CPUs this device works
 * @set_next_event:	set next event function
 * @set_mode:		set mode function
 * @event_handler:	Assigned by the framework to be called by the low
 *			level handler of the event source
 * @mode:		operating mode assigned by the management code
 * @next_event:		local storage for the next event in oneshot mode
 */

struct clock_event_device {
	const char *name;
	unsigned int features;

	void __iomem *base; /* virt address to access the timer */
	u32 timer_nr;	/* If multiple timer can be accessed from the same base address */
	unsigned int rate;
	unsigned int prescale;

	u32 mult;
	int shift;

	u64 max_delta_ns;
	u64 min_delta_ns;

	unsigned long min_delta_ticks;
	unsigned long max_delta_ticks;

	irqaction_t __irqaction;

	int (*set_next_event)(unsigned long evt, struct clock_event_device *);
	void (*set_mode)(enum clock_event_mode mode, struct clock_event_device *);
	void (*event_handler)(struct clock_event_device *);

	enum clock_event_mode mode;
};

/* Clock event layer functions */

extern void clockevents_set_mode(struct clock_event_device *dev, enum clock_event_mode mode);


#endif /* CLOCK_H */
