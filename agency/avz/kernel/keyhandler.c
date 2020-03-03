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

#include <asm/processor.h>
#include <avz/keyhandler.h> 

#include <avz/init.h>
#include <avz/event.h>
#include <avz/console.h>
#include <avz/serial.h>
#include <avz/sched.h>
#include <avz/softirq.h>
#include <avz/domain.h>
#include <avz/ctype.h>
#include <asm/debugger.h>
#include <asm/div64.h>

static struct keyhandler *key_table[256];

char keyhandler_scratch[1024];


void handle_keypress(unsigned char key, struct cpu_user_regs *regs)
{
	struct keyhandler *h;

	if ( (h = key_table[key]) == NULL )
		return;

	h->irq_callback ? (*h->u.irq_fn)(key, regs) : (*h->u.fn)(key);

}

void register_keyhandler(unsigned char key, struct keyhandler *handler)
{
    ASSERT(key_table[key] == NULL);
    key_table[key] = handler;
}

static void show_handlers(unsigned char key)
{
    int i;
    printk("'%c' pressed -> showing installed handlers\n", key);
    for ( i = 0; i < ARRAY_SIZE(key_table); i++ ) 
        if ( key_table[i] != NULL ) 
            printk(" key '%c' (ascii '%02x') => %s\n", 
                   isprint(i) ? i : ' ', i, key_table[i]->desc);
}

static struct keyhandler show_handlers_keyhandler = {
    .u.fn = show_handlers,
    .desc = "show this message"
};

static void __dump_execstate(void *unused)
{
    dump_execution_state();
    printk("*** Dumping CPU%d guest state: ***\n", smp_processor_id());
    if ( is_idle_vcpu(current) )
        printk("No guest context (CPU is idle).\n");
    else
        show_execution_state(guest_cpu_user_regs());
}

static void dump_registers(unsigned char key, struct cpu_user_regs *regs)
{
    unsigned int cpu;


    printk("'%c' pressed -> dumping registers\n", key);

    /* Get local execution state out immediately, in case we get stuck. */
    printk("\n*** Dumping CPU%d host state: ***\n", smp_processor_id());
    __dump_execstate(NULL);

    for_each_online_cpu ( cpu )
    {
        if ( cpu == smp_processor_id() )
            continue;
        printk("\n*** Dumping CPU%d host state: ***\n", cpu);
        on_selected_cpus(cpumask_of(cpu), __dump_execstate, NULL, 1);
    }

    printk("\n");
}

static struct keyhandler dump_registers_keyhandler = {
    .irq_callback = 1,
    .diagnostic = 1,
    .u.irq_fn = dump_registers,
    .desc = "dump registers"
};

extern void vcpu_show_execution_state(struct vcpu *v);
static void dump_agency_registers(unsigned char key)
{
	if (agency == NULL)
		return;

	printk("'%c' pressed -> dumping agency's registers\n", key);


	vcpu_show_execution_state(agency->vcpu[0]);
}

static struct keyhandler dump_agency_registers_keyhandler = {
		.diagnostic = 1,
		.u.fn = dump_agency_registers,
		.desc = "dump agency registers"
};

static void dump_domains(unsigned char key)
{
	struct domain *d;
	struct vcpu   *v;
	u64    now = NOW();
	int i;

#define tmpstr keyhandler_scratch

	printk("'%c' pressed -> dumping domain info (now=0x%X:%08X)\n", key,
			(u32)(now>>32), (u32)now);

	for (i = 0; i < MAX_DOMAINS; i++) {
		d = domains[i];

		printk("General information for domain %u:\n", d->domain_id);

		printk("    refcnt=%d dying=%d nr_pages=%d max_pages=%u\n",
				atomic_read(&d->refcnt), d->is_dying,
				d->tot_pages, d->max_pages);

		v = d->vcpu[0];

		printk("VCPU information and callbacks for domain %u:\n", d->domain_id);

		printk("    CPU%d [has=%c] flags=%lx "
				"upcall_pend = %02x",
				v->processor,
				v->is_running ? 'T':'F',
						v->pause_flags,
						vcpu_info(v, evtchn_upcall_pending));

		printk("    %s\n", tmpstr);

	}

#undef tmpstr
}

static struct keyhandler dump_domains_keyhandler = {
		.diagnostic = 1,
		.u.fn = dump_domains,
		.desc = "dump domain (and guest debug) info"
};

static cpumask_t read_clocks_cpumask = CPU_MASK_NONE;
static u64 read_clocks_time[NR_CPUS];
static u64 read_cycles_time[NR_CPUS];

static void read_clocks_slave(void *unused)
{
    unsigned int cpu = smp_processor_id();
    local_irq_disable();
    while ( !cpu_isset(cpu, read_clocks_cpumask) )
        cpu_relax();
    read_clocks_time[cpu] = NOW();
    read_cycles_time[cpu] = get_cycles();
    cpu_clear(cpu, read_clocks_cpumask);
    local_irq_enable();
}

static void read_clocks(unsigned char key)
{
	unsigned int cpu = smp_processor_id(), min_stime_cpu, max_stime_cpu;
	unsigned int min_cycles_cpu, max_cycles_cpu;
	u64 min_stime, max_stime, dif_stime;
	u64 min_cycles, max_cycles, dif_cycles;
	static u64 sumdif_stime = 0, maxdif_stime = 0;
	static u64 sumdif_cycles = 0, maxdif_cycles = 0;
	static u32 count = 0;
	static DEFINE_SPINLOCK(lock);

	spin_lock(&lock);

	smp_call_function(read_clocks_slave, NULL, 0);

	local_irq_disable();
	read_clocks_cpumask = cpu_online_map;
	read_clocks_time[cpu] = NOW();
	read_cycles_time[cpu] = get_cycles();
	cpu_clear(cpu, read_clocks_cpumask);
	local_irq_enable();

	while ( !cpus_empty(read_clocks_cpumask) )
		cpu_relax();

	min_stime_cpu = max_stime_cpu = min_cycles_cpu = max_cycles_cpu = cpu;
	for_each_online_cpu ( cpu )
	{
		if (read_clocks_time[cpu] < read_clocks_time[min_stime_cpu])
			min_stime_cpu = cpu;
		if (read_clocks_time[cpu] > read_clocks_time[max_stime_cpu])
			max_stime_cpu = cpu;
		if (read_cycles_time[cpu] < read_cycles_time[min_cycles_cpu])
			min_cycles_cpu = cpu;
		if (read_cycles_time[cpu] > read_cycles_time[max_cycles_cpu])
			max_cycles_cpu = cpu;
	}

	min_stime = read_clocks_time[min_stime_cpu];
	max_stime = read_clocks_time[max_stime_cpu];
	min_cycles = read_cycles_time[min_cycles_cpu];
	max_cycles = read_cycles_time[max_cycles_cpu];

	spin_unlock(&lock);

	dif_stime = max_stime - min_stime;
	if (dif_stime > maxdif_stime)
		maxdif_stime = dif_stime;

	sumdif_stime += dif_stime;
	dif_cycles = max_cycles - min_cycles;

	if (dif_cycles > maxdif_cycles)
		maxdif_cycles = dif_cycles;

	sumdif_cycles += dif_cycles;

	count++;

	printk("Synced stime skew: max=%llu ns avg=%llu ns samples=%u current=%llu ns\n", maxdif_stime, sumdif_stime/count, count, dif_stime);
	printk("Synced cycles skew: max=%llu avg=%llu samples=%u current=%llu\n", maxdif_cycles, sumdif_cycles/count, count, dif_cycles);
}

static struct keyhandler read_clocks_keyhandler = {
		.diagnostic = 1,
		.u.fn = read_clocks,
		.desc = "display multi-cpu clock info"
};

void __init initialize_keytable(void)
{
    register_keyhandler('d', &dump_registers_keyhandler);
    register_keyhandler('h', &show_handlers_keyhandler);
    register_keyhandler('q', &dump_domains_keyhandler);

    register_keyhandler('t', &read_clocks_keyhandler);
    register_keyhandler('0', &dump_agency_registers_keyhandler);

}
