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

#include <config.h>
#include <sched.h>
#include <spinlock.h>
#include <softirq.h>
#include <domain.h>
#include <event.h>
#include <smp.h>
#include <sizes.h>

#include <device/irq.h>
#include <device/timer.h>

#include <device/arch/arm_timer.h>
#include <device/arch/gic.h>

#include <asm/setup.h>
#include <asm/cacheflush.h>
#include <asm/vfp.h>

static volatile int booted[NR_CPUS] = {0};

DEFINE_PER_CPU(spinlock_t, softint_lock);

extern void startup_cpu_idle_loop(void);
extern void init_idle_domain(void);

/*
 * control for which core is the next to come out of the secondary
 * boot "holding pen"
 */
volatile int pen_release = -1;

/*
 * Write pen_release in a way that is guaranteed to be visible to all
 * observers, irrespective of whether they're taking part in coherency
 * or not.  This is necessary for the hotplug code to work reliably.
 */
void write_pen_release(int val)
{
	pen_release = val;
	smp_mb();

	flush_dcache_all();
}

int read_pen_release(void) {
	smp_mb();

	flush_dcache_all();

	return pen_release;
}

void smp_trigger_event(int target_cpu)
{
	int cpu = smp_processor_id();

	spin_lock(&per_cpu(softint_lock, cpu));

	/* We keep forcing a send of IPI since the other CPU could be in WFI in an idle loop */

	smp_cross_call(target_cpu, IPI_EVENT_CHECK);

	spin_unlock(&per_cpu(softint_lock, cpu));
}

/***************************/

struct secondary_data secondary_data;

/*
 * handle_IPI() must check if the IPI is known or it has to be propagated
 * to a guest (case of realtime IPI on Agency CPU #1).
 */
void handle_IPI(int ipinr)
{
	switch (ipinr)
	{
	case IPI_WAKEUP:
		/* nothing to do */
		break;

	case IPI_EVENT_CHECK:

		/* Nothing to do, will check for events on return path */
		break;

	default:
		/* Forward the IPI to the guest */

		asm_do_IRQ(ipinr);
		break;
	}

}

/*
 * This is the secondary CPU boot entry.  We're using this CPUs
 * idle thread stack, but a set of temporary page tables.
 */
void secondary_start_kernel(void)
{
#ifndef CONFIG_ARCH_ARM64
	unsigned int cpu = smp_processor_id();

	cpu_init();

	printk("CPU%u: Booted secondary processor\n", cpu);

	/*
	 * Give the platform a chance to do its own initialisation.
	 */

#ifndef CONFIG_PSCI
	smp_secondary_init(cpu);
#endif

	init_timer(cpu);

	smp_mb();

	booted[cpu] = 1;

	printk("CPU%d booted. Waiting unpause\n", cpu);

	init_idle_domain();

	/* Enabling VFP module on this CPU */
	vfp_enable();

	printk("%s: entering idle loop...\n", __func__);

	/* Prepare an idle domain and starts the idle loop */
	startup_cpu_idle_loop();

	/* Never returned at this point ... */
#endif
}

#ifndef CONFIG_ARCH_ARM64

extern void vcpu_periodic_timer_start(struct vcpu *v);

void cpu_up(unsigned int cpu)
{

	/* We re-create a small identity mapping to allow the hypervisor
	 * to bootstrap correctly on other CPUs.
	 */
	create_mapping(NULL, CONFIG_RAM_BASE, CONFIG_RAM_BASE, SZ_1M, false);

	/*
	 * We need to tell the secondary core where to find
	 * its stack and the page tables.
	 */

	switch (cpu) {

	case ME_CPU:
		secondary_data.stack = &cpu3_stack;
		break;

	default:
		printk("%s - CPU %d not supported.\n", __func__, cpu);
		BUG();
	}

	secondary_data.pgdir = virt_to_phys(__sys_l1pgtable);

	flush_dcache_all();

	/*
	 * Now bring the CPU into our world.
	 */

#ifdef CONFIG_PSCI
	psci_smp_boot_secondary(cpu);
#else
	smp_boot_secondary(cpu);
#endif

	/*
	 * CPU was successfully started, wait for it
	 * to come online or time out.
	 */
	smp_trigger_event(cpu);

	printk("Now waiting CPU %d to be up and running ...\n", cpu);

	while (!booted[cpu]) ;

	printk("%s finished waiting...\n", __func__);

	secondary_data.stack = NULL;
	secondary_data.pgdir = 0;
}

#endif /* CONFIG_ARCH_ARM64 */

/******************************************************************************/
/* From linux kernel/smp.c */

/* Called by boot processor to activate the rest. */
void smp_init(void)
{
	printk("CPU #%d is the second CPU reserved for Agency realtime activity.\n", AGENCY_RT_CPU);

	/* Since the RT domain is never scheduled, we set the current domain bound to
	 * CPU #1 to this unique domain.
	 */
	per_cpu(current_domain, AGENCY_RT_CPU) = domains[DOMID_AGENCY_RT];

/* CONFIG_ARM64 not yet supported at this point. */
#ifndef CONFIG_ARCH_ARM64

#ifndef CONFIG_PSCI
	smp_prepare_cpus(NR_CPUS);
#endif

	cpu_up(ME_CPU);

	printk("Brought secondary CPUs for AVZ (CPU #2 and CPU #3)\n");

#endif /* CONFIG_ARCH_ARM64 */

}



