/*
 * Copyright (C) 2016,2017 Daniel Rossier <daniel.rossier@soo.tech>
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

#include <avz/init.h>
#include <avz/errno.h>
#include <avz/spinlock.h>
#include <avz/delay.h>
#include <avz/smp.h>

#include <asm/delay.h>
#include <asm/io.h>
#include <asm/mach/map.h>
#include <asm/cacheflush.h>

#include <asm/hardware/gic.h>
#include <asm/hardware/arm_timer.h>

#include <asm/opcodes.h>

#include <mach/bcm2835.h>

extern void secondary_startup(void);

/*
 * Initialise the CPU possible map early - this describes the CPUs
 * which may be present or become present in the system.
 */
void __init bcm2836_smp_init_cpus(void)
{
	int i;

	for (i = 0; i < NR_CPUS; i++)
		cpu_set(i, cpu_possible_map);

	set_smp_cross_call(bcm2836_arm_irqchip_send_ipi);
}

static void __init bcm2836_smp_prepare_cpus(unsigned int max_cpus)
{


}

static int bcm2836_smp_boot_secondary(unsigned int cpu)
{
	unsigned long secondary_startup_phys = (unsigned long)virt_to_phys((void *)secondary_startup);

	printk("%s: booting CPU: %d...\n", __func__, cpu);

	writel(secondary_startup_phys, intc.base + LOCAL_MAILBOX3_SET0 + 16 * cpu);

	return 0;
}

void __cpuinit bcm2836_smp_secondary_init(unsigned int cpu) {
	/* Unmask IPIs to the boot CPU. */
	bcm2836_arm_irqchip_unmask_per_cpu_irq(LOCAL_MAILBOX_INT_CONTROL0, 0, cpu);

	unmask_timer_irq();
}

struct smp_operations bcm2836_smp_ops __initdata = {
	.smp_init_cpus          = bcm2836_smp_init_cpus,
	.smp_prepare_cpus	= bcm2836_smp_prepare_cpus,
	.smp_boot_secondary	= bcm2836_smp_boot_secondary,
	.smp_secondary_init	= bcm2836_smp_secondary_init,
};

