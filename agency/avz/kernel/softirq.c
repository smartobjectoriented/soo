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
#include <softirq.h>

#include <asm/cacheflush.h>

static volatile bool softirq_stat[NR_CPUS][NR_SOFTIRQS];

static softirq_handler softirq_handlers[NR_SOFTIRQS];

DEFINE_SPINLOCK(softirq_pending_lock);

void do_softirq(void)
{
	unsigned int i, cpu;
	unsigned int loopmax;

	loopmax = 0;

	cpu = smp_processor_id();

	while (true) {

		spin_lock(&softirq_pending_lock);


		for (i = 0; i < NR_SOFTIRQS; i++)
			if (softirq_stat[cpu][i])
				break;

		if (i == NR_SOFTIRQS)
			return;

		if (loopmax > 100)   /* Probably something wrong ;-) */
			printk("%s: Warning trying to process softirq on cpu %d for quite a long time (i = %d)...\n", __func__, cpu, i);

		softirq_stat[cpu][i] = false;

		spin_unlock(&softirq_pending_lock);

		(*softirq_handlers[i])();

		loopmax++;
	}
}

void open_softirq(int nr, softirq_handler handler)
{
	ASSERT(nr < NR_SOFTIRQS);

	softirq_handlers[nr] = handler;
}

/*
 * The softirq_pending mask (irqstat) must be coherent between the agency CPUs and MEs CPU
 * since they do not run in the same OS environment, the hardware cache coherency is not guaranteed.
 */
void cpu_raise_softirq(unsigned int cpu, unsigned int nr)
{
	softirq_stat[cpu][nr] = true;

	smp_trigger_event(cpu);
}

void raise_softirq(unsigned int nr)
{
	softirq_stat[smp_processor_id()][nr] = true;
}

void softirq_init(void)
{
	int i, cpu;

	cpu = smp_processor_id();

	for (i = 0; i < NR_SOFTIRQS; i++)
		softirq_stat[cpu][i] = false;

	spin_lock_init(&softirq_pending_lock);
}
