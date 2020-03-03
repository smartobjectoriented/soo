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
#include <avz/mm.h>
#include <avz/sched.h>
#include <avz/softirq.h>

uint32_t softirq_stat[NR_CPUS];

static softirq_handler softirq_handlers[NR_SOFTIRQS];

DEFINE_SPINLOCK(softirq_pending_lock);

static void __do_softirq(unsigned long ignore_mask)
{
	unsigned int i, cpu;
	unsigned long pending;
	unsigned int loopmax;

	loopmax = 0;

	cpu = smp_processor_id();

	for ( ; ; )
	{
		spin_lock(&softirq_pending_lock);

		if ((pending = (softirq_pending(cpu) & ~ignore_mask)) == 0) {
			spin_unlock(&softirq_pending_lock);
			break;
		}

		i = find_first_set_bit(pending);

		if (loopmax > 100)   /* Probably something wrong ;-) */
			printk("%s: Warning trying to process softirq on cpu %d for quite a long time (i = %d)...\n", __func__, cpu, i);

		transaction_clear_bit(i, (unsigned long *) &softirq_pending(cpu));

		spin_unlock(&softirq_pending_lock);

		(*softirq_handlers[i])();

		loopmax++;
	}
}

/*
 * Helper to get a ref to irq_stat
 */
uint32_t get_softirq_stat(void)
{
	return softirq_stat[smp_processor_id()];
}

void do_softirq(void)
{
	__do_softirq(0);
}

void open_softirq(int nr, softirq_handler handler)
{
    ASSERT(nr < NR_SOFTIRQS);

    softirq_handlers[nr] = handler;
}

void cpumask_raise_softirq(cpumask_t mask, unsigned int nr)
{
    int cpu;

    for_each_cpu_mask(cpu, mask)
      transaction_set_bit(nr, (unsigned long *) &softirq_pending(cpu));

    smp_send_event_check_mask(&mask);
}

/*
 * The softirq_pending mask (irqstat) must be coherent between the agency CPUs and MEs CPU
 * since they do not run in the same OS environment, the hardware cache coherency is not guaranteed.
 */
void cpu_raise_softirq(unsigned int cpu, unsigned int nr)
{
  transaction_set_bit(nr, (unsigned long *) &softirq_pending(cpu));

  flush_all();

  smp_send_event_check_cpu(cpu);
}

void raise_softirq(unsigned int nr)
{
  transaction_set_bit(nr, (unsigned long *) &softirq_pending(smp_processor_id()));
}


void __init softirq_init(void)
{
  spin_lock_init(&softirq_pending_lock);
}
