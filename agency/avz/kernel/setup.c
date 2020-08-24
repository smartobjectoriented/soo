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
#include <lib.h>
#include <sched.h>
#include <softirq.h>
#include <console.h>
#include <domain.h>
#include <smp.h>
#include <libelf.h>
#include <memslot.h>
#include <keyhandler.h>
#include <event.h>

#include <device/device.h>

#include <asm/processor.h>
#include <asm/percpu.h>
#include <asm/io.h>
#include <asm/div64.h>
#include <asm/vfp.h>

#include <soo/soo.h>

#include <soo/uapi/logbool.h>

#define DEBUG

extern void startup_cpu_idle_loop(void);

struct domain *idle_domain[NR_CPUS];

long do_set_callbacks(unsigned long event, unsigned long domcall)
{
	struct vcpu *v = (struct vcpu *) current;

	v->arch.guest_context.event_callback              = event;
	v->arch.guest_context.domcall                     = domcall;

	if (v->domain->domain_id == DOMID_AGENCY) {
		/*
		 * Do the same thing for the realtime subdomain.
		 */
		domains[DOMID_AGENCY_RT]->vcpu[0]->arch.guest_context.event_callback = v->arch.guest_context.event_callback;
		domains[DOMID_AGENCY_RT]->vcpu[0]->arch.guest_context.domcall = v->arch.guest_context.domcall;
	}

	return 0;
}

void dump_backtrace_entry(unsigned long where, unsigned long from)
{
	printk("Function entered at [<%08lx>] from [<%08lx>]\n", where, from);
}

void init_idle_domain(void)
{
  int cpu = smp_processor_id();

	/* Domain creation requires that scheduler structures are initialised. */
	idle_domain[cpu] = domain_create(DOMID_IDLE, false);

	if (idle_domain[cpu] == NULL)
		BUG();

	set_current(idle_domain[cpu]->vcpu[0]);

	this_cpu(curr_vcpu) = current;
}

extern void setup_arch(char **);

void kernel_start(void)
{
	char *command_line;
	int i;

	local_irq_disable();

	early_memory_init();

	initialize_keytable();

	loadAgency();

	percpu_init_areas();

	/* At this time... */
	set_current(NULL);

	/* We initialize the console device(s) very early so we can get debugging. */
	console_init();

	pagealloc_init();

	memory_init();
printk("##0\n");
	softirq_init();
	printk("##1\n");
	/* allocate pages for per-cpu areas */
	for (i = 0; i < NR_CPUS; i++)
		init_percpu_area(i);

	/* Initialization of the machine. */
	setup_arch(&command_line);
	printk("##2\n");
	/* Prepare to adapt the serial virtual address at a better location in the I/O space. */
	console_init_post();
	printk("##3\n");
	logbool_init();

	printk("Init devices...\n");
	devices_init();

	printk("Init scheduler...\n");
	scheduler_init();

	printk("Initializing avz timer...\n");
	/* get the time base kicked */

	timer_init();
	init_time();

	/* create idle domain */
	init_idle_domain();

	/* for further create_mapping use... */
	current->arch.guest_ptable = (void *) __pa(swapper_pg_dir);

	event_channel_init();

	soo_activity_init();

	/* Deal with secondary processors.  */
	printk("spinning up at most %d total processors ...\n", NR_CPUS);

	local_irq_enable();

	smp_init();

	/* Create initial domain 0. */
	domains[DOMID_AGENCY] = domain_create(DOMID_AGENCY, false);
	agency = domains[DOMID_AGENCY];

	if (agency == NULL)
		panic("Error creating primary Agency domain\n");

	/*
	 * We need to create a sub-domain associated to the realtime CPU so that
	 * hypercalls and upcalls will be processed correctly.
	 */

	domains[DOMID_AGENCY_RT] = domain_create(DOMID_AGENCY_RT, false);

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

