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

#include <avz/config.h>
#include <avz/irq.h>
#include <avz/sched.h>
#include <avz/delay.h>
#include <avz/spinlock.h>
#include <avz/softirq.h>
#include <avz/domain.h>
#include <avz/init.h>
#include <avz/event.h>

#include <avz/errno.h>

#include <asm/current.h>
#include <asm/smp.h>
#include <asm/cacheflush.h>
#include <asm/vfp.h>
#include <asm/hardware/gic.h>
#include <asm/hardware/arm_timer.h>

extern void *cpu1_stack, *cpu2_stack, *cpu3_stack;

static volatile int booted[NR_CPUS] = {0};

DEFINE_PER_CPU(spinlock_t, softint_lock);

void (*smp_cross_call)(const struct cpumask *, unsigned int);

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
	dmb();
	sync_cache_w(&pen_release);
}

int read_pen_release(void) {
	dmb();
	sync_cache_r(&pen_release);
	return pen_release;
}


void __init set_smp_cross_call(void (*fn)(const struct cpumask *, unsigned int))
{
	smp_cross_call = fn;
}

void smp_send_event_check_mask(const cpumask_t *mask)
{
	cpumask_t msk;
	int cpu = smp_processor_id();

	cpumask_copy(&msk, mask);
	cpu_clear(smp_processor_id(), msk);

	spin_lock(&per_cpu(softint_lock, cpu));

	/* We keep forcing a send of IPI since the other CPU could be in WFI in an idle loop */

	if (cpus_weight(msk))
		smp_cross_call(&msk, IPI_EVENT_CHECK);

	spin_unlock(&per_cpu(softint_lock, cpu));
}

int smp_call_function(void (*func) (void *info), void *info, int wait)
{
	/* not used */
	return 0;
}

int on_selected_cpus(const cpumask_t *selected, void (*func)(void *info), void *info, int wait)
{
	int cpu;

	for_each_cpu_mask(cpu, *selected) {
		if (cpu_isset(cpu, *selected))
			func(info);
	}

	/* not used */
	return 0;
}

/***************************/

struct secondary_data secondary_data;

static struct smp_operations smp_ops;

void __init smp_set_ops(struct smp_operations *ops)
{
	if (ops)
		smp_ops = *ops;
};


/* platform specific SMP operations */
int __init boot_secondary(unsigned int cpu)
{
	if (smp_ops.smp_boot_secondary)
		return smp_ops.smp_boot_secondary(cpu);

	return 0;
}

void __init smp_init_cpus(void)
{
	if (smp_ops.smp_init_cpus)
		smp_ops.smp_init_cpus();
}

void __init platform_smp_prepare_cpus(unsigned int max_cpus)
{
	if (smp_ops.smp_prepare_cpus)
		smp_ops.smp_prepare_cpus(max_cpus);
}

void __cpuinit platform_secondary_init(unsigned int cpu)
{
	if (smp_ops.smp_secondary_init)
		smp_ops.smp_secondary_init(cpu);
}

static inline void ipi_flush_tlb_all(void *ignored)
{
	local_flush_tlb_all();
}

void flush_tlb_all(void)
{
	on_each_cpu(ipi_flush_tlb_all, NULL, 1);
}

extern void __cpuinit platform_secondary_init(unsigned int cpu);

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
void __cpuinit secondary_start_kernel(void)
{
	unsigned int cpu = smp_processor_id();

	cpu_init();

	printk("CPU%u: Booted secondary processor\n", cpu);

	/*
	 * Give the platform a chance to do its own initialisation.
	 */

	platform_secondary_init(cpu);

	/*
	 * The RT and non-RT ME CPUs need their private timer (oneshot for RT, periodic for non-RT).
	 * They share the same system_timer_clockevent structure, so we need to initialize once only.
	 */
	arm_timer_init(cpu);

	/*
	 * OK, now it's safe to let the boot CPU continue.  Wait for
	 * the CPU migration code to notice that the CPU is online
	 * before we continue - which happens after __cpu_up returns.
	 */
	cpumask_set_cpu(cpu, &cpu_online_map);
	dmb();

	booted[cpu] = 1;

	printk("CPU%d booted. Waiting unpause\n", cpu);

	init_idle_domain();

	idle_domain[smp_processor_id()]->vcpu[0]->arch.guest_table = mk_pagetable(__pa(swapper_pg_dir));

	/* Enabling VFP module on this CPU */
	vfp_enable();

	printk("%s: entering idle loop...\n", __func__);

	/* Prepare an idle domain and starts the idle loop */
	startup_cpu_idle_loop();

	/* Never returned at this point ... */

}
extern void vcpu_periodic_timer_start(struct vcpu *v);

int __cpuinit __cpu_up(unsigned int cpu)
{
	int ret;

	/*
	 * We need to tell the secondary core where to find
	 * its stack and the page tables.
	 */

	switch (cpu) {
	case 1:
		secondary_data.stack = &cpu1_stack;
		break;
	case 2:
		secondary_data.stack = &cpu2_stack;
		break;
	case 3:
		secondary_data.stack = &cpu3_stack;
		break;
	}

	secondary_data.pgdir = virt_to_phys(swapper_pg_dir);

	sync_cache_w(&secondary_data);

	/*
	 * Now bring the CPU into our world.
	 */
	ret = boot_secondary(cpu);
	if (ret == 0) {
		/*
		 * CPU was successfully started, wait for it
		 * to come online or time out.
		 */
		smp_send_event_check_cpu(cpu);

		printk("Now waiting CPU %d to be up and running ...\n", cpu);
		while (!booted[cpu]) ;

		printk("%s finished waiting...\n", __func__);

		if (!cpu_online(cpu)) {
			printk("CPU%u: failed to come online\n", cpu);
			ret = -EIO;
		}
	} else {
		printk("CPU%u: failed to boot: %d\n", cpu, ret);
	}

	secondary_data.stack = NULL;
	secondary_data.pgdir = 0;

	return ret;
}

/******************************************************************************/
/* From linux kernel/smp.c */

/* Called by boot processor to activate the rest. */
void __init smp_init(void)
{
	unsigned int cpu;

	printk(KERN_INFO "CPU #%d is the second CPU reserved for Agency realtime activity.\n", AGENCY_RT_CPU);

	/*
	 * We put this CPU as online to avoid a set up done in avz.
	 * This CPU is fully managed by Linux.
	 */
	cpumask_set_cpu(AGENCY_RT_CPU, &cpu_online_map);

	for_each_present_cpu(cpu) {
		if (num_online_cpus() >= NR_CPUS)
			break;

		spin_lock_init(&per_cpu(softint_lock, cpu));

		if (!cpu_online(cpu))
			cpu_up(cpu);
	}

	/* Any cleanup work */
	printk(KERN_INFO "Brought up %ld CPUs\n", (long) num_online_cpus());
}


