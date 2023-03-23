/*
 * Copyright (C) 2001-2013 Philippe Gerum <rpm@xenomai.org>.
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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/ipipe_tickdev.h>
#include <xenomai/version.h>
#include <cobalt/kernel/sched.h>
#include <cobalt/kernel/clock.h>
#include <cobalt/kernel/timer.h>
#include <cobalt/kernel/heap.h>
#include <cobalt/kernel/intr.h>
#include <cobalt/kernel/apc.h>
#include <cobalt/kernel/ppd.h>
#include <cobalt/kernel/pipe.h>
#include <cobalt/kernel/select.h>
#include <cobalt/kernel/vdso.h>
#include <rtdm/fd.h>
#include "rtdm/internal.h"

#include <asm/smp.h>

#include <soo/uapi/soo.h>

#include <soo/hypervisor.h>
#include <soo/uapi/console.h>

#include <linux/delay.h>


/**
 * @defgroup cobalt Cobalt
 *
 * Cobalt supplements the native Linux kernel in dual kernel
 * configurations. It deals with all time-critical activities, such as
 * handling interrupts, and scheduling real-time threads. The Cobalt
 * kernel has higher priority over all the native kernel activities.
 *
 * Cobalt provides an implementation of the POSIX and RTDM interfaces
 * based on a set of generic RTOS building blocks.
 */


static unsigned long sysheap_size_arg;

static char init_state_arg[16] = "enabled";

static BLOCKING_NOTIFIER_HEAD(state_notifier_list);

volatile bool __cobalt_ready = false;
volatile bool __rt_wakeup = false;

atomic_t cobalt_runstate = ATOMIC_INIT(COBALT_STATE_WARMUP);
EXPORT_SYMBOL_GPL(cobalt_runstate);

#define boot_debug_notice ""
#define boot_lat_trace_notice ""
#define boot_evt_trace_notice ""

#define boot_state_notice						\
	({								\
		realtime_core_state() == COBALT_STATE_STOPPED ?		\
			"[STOPPED]" : "";				\
	})

void cobalt_add_state_chain(struct notifier_block *nb)
{
	blocking_notifier_chain_register(&state_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(cobalt_add_state_chain);

void cobalt_remove_state_chain(struct notifier_block *nb)
{
	blocking_notifier_chain_unregister(&state_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(cobalt_remove_state_chain);

void cobalt_call_state_chain(enum cobalt_run_states newstate)
{
	blocking_notifier_call_chain(&state_notifier_list, newstate, NULL);
}
EXPORT_SYMBOL_GPL(cobalt_call_state_chain);

static int mach_setup(void)
{
	int ret;

	__ipipe_init_post();

	ret = ipipe_select_timers(&xnsched_realtime_cpus);
	if (ret < 0)
		return ret;

	/* Now, we are ready to initialize the IPIs for CPU #1 (head) */

	ret = xnclock_init();
	if (ret)
		BUG();

	return 0;
}

static struct {
	const char *label;
	enum cobalt_run_states state;
} init_states[] = {
	{ "disabled", COBALT_STATE_DISABLED },
	{ "stopped", COBALT_STATE_STOPPED },
	{ "enabled", COBALT_STATE_WARMUP },
};

static void setup_init_state(void)
{
	static char warn_bad_state[] = XENO_WARNING "invalid init state '%s'\n";
	int n;

	for (n = 0; n < ARRAY_SIZE(init_states); n++)
		if (strcmp(init_states[n].label, init_state_arg) == 0) {
			set_realtime_core_state(init_states[n].state);
			return;
		}

	lprintk(warn_bad_state, init_state_arg);
}

static int sys_init(void)
{
	struct xnsched *sched;
	void *heapaddr;
	int ret;

	if (sysheap_size_arg == 0)
		sysheap_size_arg = CONFIG_XENO_OPT_SYS_HEAPSZ;

	heapaddr = xnheap_vmalloc(sysheap_size_arg * 1024);

	if (heapaddr == NULL || xnheap_init(&cobalt_heap, heapaddr, sysheap_size_arg * 1024))
		return -ENOMEM;

	xnheap_set_name(&cobalt_heap, "system heap");

	sched = &per_cpu(nksched, AGENCY_RT_CPU);
	xnsched_init(sched, AGENCY_RT_CPU);

	xnregistry_init();

	/*
	 * If starting in stopped mode, do all initializations, but do
	 * not enable the core timer.
	 */
	if (realtime_core_state() == COBALT_STATE_WARMUP) {
		ret = xntimer_grab_hardware();
		if (ret)
			BUG();

		set_realtime_core_state(COBALT_STATE_RUNNING);
	}

	return 0;
}

void xenomai_init(void)
{
	int ret, __maybe_unused cpu;

	setup_init_state();

	cpumask_clear(&xnsched_realtime_cpus);

	lprintk("CPU%d: Agency RT CPU is CPU #1\n", smp_processor_id());

	cpumask_set_cpu(AGENCY_RT_CPU, &xnsched_realtime_cpus);

	if (cpumask_empty(&xnsched_realtime_cpus)) {
		lprintk("disabled via empty real-time CPU mask\n");
		set_realtime_core_state(COBALT_STATE_DISABLED);
		return ;
	}
	cobalt_cpu_affinity = xnsched_realtime_cpus;

	xnsched_register_classes();

	ret = mach_setup();
	if (ret)
		BUG();

	ret = xnpipe_mount();
	if (ret)
		BUG();

	ret = sys_init();
	if (ret)
		BUG();

	__cobalt_ready = true;

	printk("Cobalt v%s (%s) %s%s%s%s\n",
	       XENO_VERSION_STRING,
	       XENO_VERSION_NAME,
	       boot_debug_notice,
	       boot_lat_trace_notice,
	       boot_evt_trace_notice,
	       boot_state_notice);
}

static int xenomai_pre_init(void) {

	printk("%s: waiting on CPU %d for Xenomai/Cobalt fully initialized...\n", __func__, smp_processor_id());

	/* Ask CPU #1 to wake up. */
	__rt_wakeup = true;

	while (!__cobalt_ready)
		schedule();

	printk("%s: OK got the event from the RT CPU.\n", __func__);

	return 0;
}

postcore_initcall(xenomai_pre_init);
