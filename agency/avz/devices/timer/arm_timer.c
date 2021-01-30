/*
 * Copyright (C) 2016 Daniel Rossier <daniel.rossier@soo.tech>
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

static void arch_timer_reg_write(int access, enum arch_timer_reg reg, u32 val, struct clock_event_device *clk)
{
	if (access == ARCH_TIMER_MEM_PHYS_ACCESS) {

		switch (reg) {
		case ARCH_TIMER_REG_CTRL:
			writel(val, clk->base + CNTP_CTL);
			break;

		case ARCH_TIMER_REG_TVAL:
			writel(val, clk->base + CNTP_TVAL);
			break;
		}
	} else if (access == ARCH_TIMER_MEM_VIRT_ACCESS) {

		switch (reg) {
		case ARCH_TIMER_REG_CTRL:
			writel(val, clk->base + CNTV_CTL);
			break;
		case ARCH_TIMER_REG_TVAL:
			writel(val, clk->base + CNTV_TVAL);
			break;
		}
	} else {
		arch_timer_reg_write_cp15(access, reg, val);
	}
}

static inline u32 arch_timer_reg_read(int access, enum arch_timer_reg reg, struct clock_event_device *clk)
{
	u32 val;

	if (access == ARCH_TIMER_MEM_PHYS_ACCESS) {

		switch (reg) {
		case ARCH_TIMER_REG_CTRL:
			val = readl(clk->base + CNTP_CTL);
			break;
		case ARCH_TIMER_REG_TVAL:
			val = readl(clk->base + CNTP_TVAL);
			break;
		}

	} else if (access == ARCH_TIMER_MEM_VIRT_ACCESS) {

		switch (reg) {
		case ARCH_TIMER_REG_CTRL:
			val = readl(clk->base + CNTV_CTL);
			break;
		case ARCH_TIMER_REG_TVAL:
			val = readl(clk->base + CNTV_TVAL);
			break;
		}
	} else {
		val = arch_timer_reg_read_cp15(access, reg);
	}

	return val;
}

static inline irqreturn_t timer_handler(const int access, struct clock_event_device *evt)
{
	unsigned long ctrl;

	ctrl = arch_timer_reg_read(access, ARCH_TIMER_REG_CTRL, evt);

	if (ctrl & ARCH_TIMER_CTRL_IT_STAT) {
		ctrl |= ARCH_TIMER_CTRL_IT_MASK;
		arch_timer_reg_write(access, ARCH_TIMER_REG_CTRL, ctrl, evt);

		if (smp_processor_id() == ME_CPU) {
			/* Periodic timer */
			system_timer_clockevent->set_next_event(clkevt_reload, system_timer_clockevent);
			timer_interrupt(true);
		} else
			timer_interrupt(false);
	}

	return IRQ_HANDLED;
}


static irqreturn_t arch_timer_handler_virt(int irq, void *dev_id)
{
	struct clock_event_device *evt = dev_id;

	return timer_handler(ARCH_TIMER_VIRT_ACCESS, evt);
}

#if 0
static irqreturn_t arch_timer_handler_phys(int irq, void *dev_id)
{
	struct clock_event_device *evt = dev_id;

	return timer_handler(ARCH_TIMER_PHYS_ACCESS, evt);
}

static irqreturn_t arch_timer_handler_phys_mem(int irq, void *dev_id)
{
	struct clock_event_device *evt = dev_id;

	return timer_handler(ARCH_TIMER_MEM_PHYS_ACCESS, evt);
}

static irqreturn_t arch_timer_handler_virt_mem(int irq, void *dev_id)
{
	struct clock_event_device *evt = dev_id;

	return timer_handler(ARCH_TIMER_MEM_VIRT_ACCESS, evt);
}
#endif /* 0 */

static inline void timer_set_mode(const int access, int mode, struct clock_event_device *clk)
{
	unsigned long ctrl;
	switch (mode) {
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
		ctrl = arch_timer_reg_read(access, ARCH_TIMER_REG_CTRL, clk);
		ctrl &= ~ARCH_TIMER_CTRL_ENABLE;
		arch_timer_reg_write(access, ARCH_TIMER_REG_CTRL, ctrl, clk);
		break;
	default:
		break;
	}
}

static void arch_timer_set_mode_virt(enum clock_event_mode mode,
				     struct clock_event_device *clk)
{
	timer_set_mode(ARCH_TIMER_VIRT_ACCESS, mode, clk);
}

#if 0 /* Not used at the moment */
static void arch_timer_set_mode_phys(enum clock_event_mode mode,
				     struct clock_event_device *clk)
{
	timer_set_mode(ARCH_TIMER_PHYS_ACCESS, mode, clk);
}

static void arch_timer_set_mode_virt_mem(enum clock_event_mode mode,
					 struct clock_event_device *clk)
{
	timer_set_mode(ARCH_TIMER_MEM_VIRT_ACCESS, mode, clk);
}

static void arch_timer_set_mode_phys_mem(enum clock_event_mode mode,
					 struct clock_event_device *clk)
{
	timer_set_mode(ARCH_TIMER_MEM_PHYS_ACCESS, mode, clk);
}
#endif /* 0 */


static inline void set_next_event(const int access, unsigned long evt, struct clock_event_device *clk)
{
	unsigned long ctrl;

	ctrl = arch_timer_reg_read(access, ARCH_TIMER_REG_CTRL, clk);
	ctrl |= ARCH_TIMER_CTRL_ENABLE;
	ctrl &= ~ARCH_TIMER_CTRL_IT_MASK;
	arch_timer_reg_write(access, ARCH_TIMER_REG_TVAL, evt, clk);
	arch_timer_reg_write(access, ARCH_TIMER_REG_CTRL, ctrl, clk);
}

static int arch_timer_set_next_event_virt(unsigned long evt,
					  struct clock_event_device *clk)
{
	set_next_event(ARCH_TIMER_VIRT_ACCESS, evt, clk);

	return 0;
}

#if 0 /* Not used at the moment */

static int arch_timer_set_next_event_phys(unsigned long evt,
					  struct clock_event_device *clk)
{
	set_next_event(ARCH_TIMER_PHYS_ACCESS, evt, clk);
	return 0;
}

static int arch_timer_set_next_event_virt_mem(unsigned long evt,
					      struct clock_event_device *clk)
{
	set_next_event(ARCH_TIMER_MEM_VIRT_ACCESS, evt, clk);
	return 0;
}

static int arch_timer_set_next_event_phys_mem(unsigned long evt,
					      struct clock_event_device *clk)
{
	set_next_event(ARCH_TIMER_MEM_PHYS_ACCESS, evt, clk);
	return 0;
}
#endif /* 0 */

/******* clockevent ********/

static struct clock_event_device arm_timer_clockevent = {
		.features       = CLOCK_EVT_FEAT_ONESHOT,
		.set_mode	= arch_timer_set_mode_virt,
		.set_next_event	= arch_timer_set_next_event_virt,
		.__irqaction = {
				.name = "arm_timer",
				.dev_id = &arm_timer_clockevent,
				.handler = arch_timer_handler_virt
		}
};


/******* clocksource *******/

static struct clocksource arm_clocksource = {
		.name		= "sys_clocksource",
		.read		= arch_counter_get_cntvct,
		.mask		= CLOCKSOURCE_MASK(56),
		.flags		= CLOCK_SOURCE_IS_CONTINUOUS | CLOCK_SOURCE_SUSPEND_NONSTOP,
};

/*
 * According to the CPU, system_timer_clockevent will be commonly used for both non-RT and RT.
 */
void init_timer(int cpu)
{
	/* System clocksource */
	system_timer_clocksource = &arm_clocksource;

	system_timer_clocksource->rate = arch_timer_get_cntfrq();
	printk("%s: detected frequency of clocksource on CPU %d: %u\n", __func__, smp_processor_id(), system_timer_clocksource->rate);

	clocks_calc_mult_shift(&system_timer_clocksource->mult, &system_timer_clocksource->shift, system_timer_clocksource->rate, NSEC_PER_SEC, 3600);

	/*
	 * System clockevent (periodic)
	 */
	system_timer_clockevent = &arm_timer_clockevent;

	system_timer_clockevent->__irqaction.irq = IRQ_ARCH_ARM_TIMER;

	system_timer_clockevent->rate = system_timer_clocksource->rate;
	system_timer_clockevent->prescale = 0;

	clkevt_reload = DIV_ROUND_CLOSEST(system_timer_clockevent->rate, CONFIG_HZ);

	setup_irq(system_timer_clockevent->__irqaction.irq, &system_timer_clockevent->__irqaction);

	/* Compute the various parameters for this clockevent */
	clockevents_config(system_timer_clockevent, system_timer_clockevent->rate, 0xf, 0x7fffffff);

	/* Enable the realtime clockevent timer */
	clockevents_set_mode(system_timer_clockevent, CLOCK_EVT_MODE_ONESHOT);

	/* First event */
	system_timer_clockevent->set_next_event(clkevt_reload, system_timer_clockevent);

}

