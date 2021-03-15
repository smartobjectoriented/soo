/*
 * Copyright (C) 2006,2007 Philippe Gerum <rpm@xenomai.org>.
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>.
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
#ifndef _COBALT_KERNEL_CLOCK_H
#define _COBALT_KERNEL_CLOCK_H

#include <linux/ipipe.h>
#include <clocksource/arm_arch_timer.h>

#include <cobalt/kernel/list.h>
#include <cobalt/uapi/kernel/types.h>

/**
 * @addtogroup cobalt_core_clock
 * @{
 */

struct xnsched;
struct xntimerdata;
u64 get_s_time(void);

struct xnclock {

	/** (ns) */
	xnticks_t resolution;

	/** Clock name. */
	const char *name;

	/* Private section. */
	struct xntimerdata *timerdata;
	int id;

	/** Possible CPU affinity of clock beat. */
	cpumask_t affinity;
};

extern struct xnclock nkclock;

extern unsigned int nkclock_lock;

int xnclock_register(struct xnclock *clock,
		     const cpumask_t *affinity);

void xnclock_tick(void);

void xnclock_adjust(struct xnclock *clock,
		    xnsticks_t delta);

void xnclock_core_local_shot(struct xnsched *sched);

void xnclock_core_remote_shot(struct xnsched *sched);

xnsticks_t xnclock_core_ns_to_ticks(xnsticks_t ns);

xnsticks_t xnclock_core_ticks_to_ns(xnsticks_t ticks);

xnticks_t xnclock_core_read_monotonic(void);

static inline xnticks_t xnclock_core_read_raw(void)
{
#ifndef CONFIG_X86
	return arch_timer_get_kvm_info()->timecounter.cc->read(arch_timer_get_kvm_info()->timecounter.cc);
#else /* x86 */
	return rdtsc();
#endif
}

static inline void xnclock_program_shot(struct xnsched *sched)
{
	xnclock_core_local_shot(sched);
}

static inline void xnclock_remote_shot(struct xnsched *sched)
{
	xnclock_core_remote_shot(sched);
}


extern u64 native_sched_clock_from_tsc(u64 tsc);

static inline u64 cyc2ns(u64 cycles)
{
#ifndef CONFIG_X86
	return ((u64) cycles * arch_timer_get_kvm_info()->timecounter.cc->mult) >> arch_timer_get_kvm_info()->timecounter.cc->shift;
#else
	return native_sched_clock_from_tsc(cycles);
#endif
}

static inline xnticks_t xnclock_read(void)
{
	return (xnticks_t) ktime_get();
}

static inline int xnclock_set_time(struct xnclock *clock,
				   const struct timespec *ts)
{
	/*
	 * There is no way to change the core clock's idea of time.
	 */
	return -EINVAL;
}

int xnclock_get_default_cpu(struct xnclock *clock, int cpu);


static inline xnticks_t xnclock_get_resolution(void)
{
	return nkclock.resolution; /* ns */
}

static inline void xnclock_set_resolution(struct xnclock *clock,
					  xnticks_t resolution)
{
	clock->resolution = resolution; /* ns */
}

unsigned long long xnclock_divrem_billion(unsigned long long value,
					  unsigned long *rem);

xnticks_t xnclock_get_host_time(void);

int xnclock_init(void);

/** @} */

#endif /* !_COBALT_KERNEL_CLOCK_H */
