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
#include <avz/init.h>
#include <avz/lib.h>
#include <avz/cpumask.h>
#include <avz/sched.h>
#include <avz/softirq.h>
#include <avz/console.h>
#include <avz/mm.h>
#include <avz/domain.h>
#include <avz/irq.h>

#include <avz/keyhandler.h>

#include <asm/processor.h>

#include <asm/cache.h>
#include <asm/debugger.h>
#include <asm/delay.h>
#include <asm/percpu.h>
#include <asm/io.h>
#include <asm/div64.h>
#include <asm/time.h>
#include <mach/irqs.h>
#include <mach/system.h>
#include <asm/vfp.h>

#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/pgtable-hwdef.h>
#include <asm/memslot.h>

#include <asm/cputype.h>

#include <avz/smp.h>
#include <avz/libelf.h>

#include <soo/soo.h>

#include <soo/uapi/logbool.h>

#define DEBUG

extern void startup_cpu_idle_loop(void);

/* maxcpus: maximum number of CPUs to activate. */
static unsigned int max_cpus = NR_CPUS;
integer_param("maxcpus", max_cpus);

uint cpu_hard_id[NR_CPUS] __initdata;
cpumask_t cpu_present_map;

extern char __per_cpu_start[], __per_cpu_data_end[], __per_cpu_end[];

struct domain *idle_domain[NR_CPUS];

extern int __init customize_machine(void);

int __cpu_logical_map[NR_CPUS];

static void percpu_free_unused_areas(void)
{
	unsigned int i;

	for (i = 0; i < NR_CPUS; i++) {
		if (!cpu_online(i)) {
#if 0 /* as long as no effect ... */
			free_percpu_area(i);
#endif /* 0 */
		}
	}
}

void __init smp_setup_processor_id(void)
{
	int i;
	u32 cpu = is_smp() ? read_cpuid_mpidr() & 0xff : 0;

	cpu_logical_map(0) = cpu;
	for (i = 1; i < NR_CPUS; ++i) {
		cpu_logical_map(i) = i == cpu ? 0 : i;
		cpumask_set_cpu(i, &cpu_possible_map);
	}

	printk(KERN_INFO "Booting the Agency Hypervisor AVZ on physical CPU %d\n", cpu);
}


static void __init do_initcalls(void)
{
	initcall_t *call;
	for (call = &__initcall_start; call < &__initcall_end; call++) {
		(*call)();
	}
}

/*
 *	Activate the first processor.
 */

static void __init boot_cpu_init(void)
{
	int cpu = smp_processor_id();
	/* Mark the boot cpu "present", "online" etc for SMP and UP case */
	cpu_set(cpu, cpu_possible_map);
	cpu_set(cpu, cpu_present_map);
	cpu_set(cpu, cpu_online_map);
}

void init_idle_domain(void)
{
  int cpu = smp_processor_id();

	/* Domain creation requires that scheduler structures are initialised. */
	idle_domain[cpu] = domain_create(DOMID_IDLE, (smp_processor_id() == ME_RT_CPU), false);

	if (idle_domain[cpu] == NULL)
		BUG();

	set_current(idle_domain[cpu]->vcpu[0]);

	this_cpu(curr_vcpu) = current;
}


static void __init start_of_day(void)
{

	logbool_init();

	printk("Init IRQ...\n");
	init_IRQ();

	printk("Init machine...\n");
	customize_machine();

	printk("Init scheduler...\n");
	scheduler_init();

	initialize_keytable();

	printk("Initializing timer...\n");
	/* get the time base kicked */

	timer_init();
	init_time();

	/* create idle domain */
	init_idle_domain();

	/* for further create_mapping use... */
	current->arch.guest_table = mk_pagetable(__pa(swapper_pg_dir));

	do_initcalls();

}

extern void setup_arch(char **);
extern struct vcpu *__init alloc_domU_vcpu0(struct domain *d);

void __init __start_avz(void)
{
	char *command_line;
	int i;
	int cpus;

	local_irq_disable();

	smp_clear_cpu_maps();

	early_memory_init();

	loadAgency();

	smp_setup_processor_id();

	percpu_init_areas();

	/* Mark boot CPU (CPU0) possible, present, active and online */
	boot_cpu_init();

	/* At this time... */
	set_current(NULL);

	/* We initialize the console device(s) very early so we can get debugging. */
	console_init();

	memory_init();

	softirq_init();

	/* allocate pages for per-cpu areas */
	for_each_possible_cpu(i)
	{
		/* from the second core */
		if (i != 0)
			init_percpu_area(i);
	}

	/* Initialization of the machine. */
	setup_arch(&command_line);

	trap_init();

	start_of_day();

	/* Deal with secondary processors.  */
	printk("spinning up at most %d total processors ...\n", max_cpus);

	/* This cannot be called before secondary cpus are marked online.  */
	percpu_free_unused_areas();

	local_irq_enable();

	cpus = smp_get_max_cpus();
	smp_prepare_cpus(cpus);

	smp_init();

	/* Create initial domain 0. */
	domains[DOMID_AGENCY] = domain_create(DOMID_AGENCY, false, false);
	agency = domains[DOMID_AGENCY];

	if (agency == NULL)
		panic("Error creating primary Agency domain\n");

	/*
	 * We need to create a sub-domain associated to the realtime CPU so that
	 * hypercalls and upcalls will be processed correctly.
	 */

	domains[DOMID_AGENCY_RT] = domain_create(DOMID_AGENCY_RT, false, false);

	if (domains[DOMID_AGENCY_RT] == NULL)
		panic("Error creating realtime agency subdomain.\n");

	if (construct_agency(domains[DOMID_AGENCY]) != 0)
		panic("Could not set up agency guest OS\n");

	/* Check that we do have a agency at this point, as we need it. */
	if (agency == NULL) {
		printk("No agency found, stopping here...\n");
		while (1);
	}

	/* Enabling VFP module on this CPU */
	vfp_enable();

	domain_unpause_by_systemcontroller(agency);

	set_current(idle_domain[smp_processor_id()]->vcpu[0]);

	startup_cpu_idle_loop();

}

