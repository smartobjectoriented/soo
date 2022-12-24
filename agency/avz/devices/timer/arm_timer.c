/*
 * Copyright (C) 2014-2022 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <time.h>
#include <types.h>
#include <sched.h>
#include <io.h>

#include <device/irq.h>
#include <device/arch/arm_timer.h>

#include <asm/arm_timer.h>

#include <soo/uapi/physdev.h>

static unsigned long clkevt_reload;

#undef CNTV_TVAL
#undef CNTV_CTL
#undef CNTFRQ
#undef CNTP_TVAL
#undef CNTP_CTL

#define CNTVCT_LO	0x08
#define CNTVCT_HI	0x0c
#define CNTFRQ		0x10
#define CNTP_TVAL	0x28
#define CNTP_CTL	0x2c
#define CNTV_TVAL	0x38
#define CNTV_CTL	0x3c

#define ARCH_TIMER_CTRL_ENABLE		(1 << 0)
#define ARCH_TIMER_CTRL_IT_MASK		(1 << 1)
#define ARCH_TIMER_CTRL_IT_STAT		(1 << 2)

struct clocksource *system_timer_clocksource;
struct clock_event_device *system_timer_clockevent;

static void timer_handler(unsigned int irq, struct irqdesc *irqdesc)
{
	unsigned long ctrl;

#ifdef CONFIG_ARM64VT
	ctrl = arch_timer_reg_read_el2(ARCH_TIMER_REG_CTRL);
#else
	ctrl = arch_timer_reg_read_cp15(ARCH_TIMER_VIRT_ACCESS, ARCH_TIMER_REG_CTRL);
#endif

	if (ctrl & ARCH_TIMER_CTRL_IT_STAT) {

		ctrl |= ARCH_TIMER_CTRL_IT_MASK;
#ifdef CONFIG_ARM64VT
		arch_timer_reg_write_el2(ARCH_TIMER_REG_CTRL, ctrl);
#else
		arch_timer_reg_write_cp15(ARCH_TIMER_VIRT_ACCESS, ARCH_TIMER_REG_CTRL, ctrl);
#endif
		if (smp_processor_id() == ME_CPU) {

			/* Periodic timer */
			system_timer_clockevent->set_next_event(clkevt_reload, system_timer_clockevent);
			timer_interrupt(true);

		} else
			timer_interrupt(false);
	}
}

static inline void timer_set_mode(int mode, struct clock_event_device *clk)
{
	unsigned long ctrl;

	switch (mode) {

	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
#ifdef CONFIG_ARM64VT
	ctrl = arch_timer_reg_read_el2(ARCH_TIMER_REG_CTRL);
	ctrl &= ~ARCH_TIMER_CTRL_ENABLE;
	arch_timer_reg_write_el2(ARCH_TIMER_REG_CTRL, ctrl);
#else
		ctrl = arch_timer_reg_read_cp15(ARCH_TIMER_VIRT_ACCESS, ARCH_TIMER_REG_CTRL);
		ctrl &= ~ARCH_TIMER_CTRL_ENABLE;
		arch_timer_reg_write_cp15(ARCH_TIMER_VIRT_ACCESS, ARCH_TIMER_REG_CTRL, ctrl);
#endif
		break;
	default:
		break;
	}
}

static void arch_timer_set_mode_virt(enum clock_event_mode mode, struct clock_event_device *clk)
{
	timer_set_mode(mode, clk);
}

static inline void set_next_event(unsigned long evt, struct clock_event_device *clk)
{
	unsigned long ctrl;
#ifdef CONFIG_ARM64VT
	ctrl = arch_timer_reg_read_el2(ARCH_TIMER_REG_CTRL);
#else
	ctrl = arch_timer_reg_read_cp15(ARCH_TIMER_VIRT_ACCESS, ARCH_TIMER_REG_CTRL);
#endif
	ctrl |= ARCH_TIMER_CTRL_ENABLE;
	ctrl &= ~ARCH_TIMER_CTRL_IT_MASK;

#ifdef CONFIG_ARM64VT
	arch_timer_reg_write_el2(ARCH_TIMER_REG_TVAL, evt);
	arch_timer_reg_write_el2(ARCH_TIMER_REG_CTRL, ctrl);
#else
	arch_timer_reg_write_cp15(ARCH_TIMER_VIRT_ACCESS, ARCH_TIMER_REG_TVAL, evt);
	arch_timer_reg_write_cp15(ARCH_TIMER_VIRT_ACCESS, ARCH_TIMER_REG_CTRL, ctrl);
#endif
}

static int arch_timer_set_next_event_virt(unsigned long evt, struct clock_event_device *clk)
{
	set_next_event(evt, clk);

	return 0;
}

/******* clockevent ********/

static struct clock_event_device arm_timer_clockevent = {
		.features       = CLOCK_EVT_FEAT_ONESHOT,
		.set_mode	= arch_timer_set_mode_virt,
		.set_next_event	= arch_timer_set_next_event_virt,
		.handler = timer_handler,
};


/******* clocksource *******/

static struct clocksource arm_clocksource = {
		.name		= "sys_clocksource",
		.read		= arch_counter_get_cntvct,
		.mask		= CLOCKSOURCE_MASK(56),
		.flags		= CLOCK_SOURCE_IS_CONTINUOUS | CLOCK_SOURCE_SUSPEND_NONSTOP,
};

/**
 * Initialize and start the CPU system timer.
 *
 * @param cpu
 */
void init_timer(int cpu)
{
	BUG_ON((cpu == AGENCY_CPU) || (cpu == AGENCY_RT_CPU));

	/* Low-leve init, disabled, interrupt off */
#ifdef CONFIG_ARM64VT
	arch_timer_reg_write_el2(ARCH_TIMER_REG_CTRL, 0);
#else
	arch_timer_reg_write_cp15(ARCH_TIMER_VIRT_ACCESS, ARCH_TIMER_REG_CTRL, 0);
#endif

	/* System clocksource */
	system_timer_clocksource = &arm_clocksource;

	system_timer_clocksource->rate = arch_timer_get_cntfrq();

	printk("%s: detected frequency of clocksource on CPU %d: %u\n", __func__, cpu, system_timer_clocksource->rate);

	/* Currently, should be the same for all CPUs (should be placed in a per_cpu variable. */
	clocks_calc_mult_shift(&system_timer_clocksource->mult, &system_timer_clocksource->shift, system_timer_clocksource->rate, NSEC_PER_SEC, 3600);

	/*
	 * System clockevent (periodic)
	 */
	system_timer_clockevent = &arm_timer_clockevent;

	system_timer_clockevent->rate = system_timer_clocksource->rate;
	system_timer_clockevent->prescale = 0;

	clkevt_reload = DIV_ROUND_CLOSEST(system_timer_clockevent->rate, CONFIG_HZ);

#ifdef CONFIG_ARM64VT
	setup_irq(IRQ_ARCH_ARM_TIMER_EL2, system_timer_clockevent->handler);
#else
	setup_irq(IRQ_ARCH_ARM_TIMER_EL1, system_timer_clockevent->handler);
#endif

	/* Compute the various parameters for this clockevent */
	clockevents_config(system_timer_clockevent, system_timer_clockevent->rate, 0xf, 0x7fffffff);

	/* Enable the realtime clockevent timer */
	clockevents_set_mode(system_timer_clockevent, CLOCK_EVT_MODE_ONESHOT);

	/* First event */
	system_timer_clockevent->set_next_event(clkevt_reload, system_timer_clockevent);
}

