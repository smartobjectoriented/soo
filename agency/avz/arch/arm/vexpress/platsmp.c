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

#include <asm/delay.h>
#include <asm/io.h>
#include <asm/cacheflush.h>
#include <asm/hardware/gic.h>
#include <asm/mach/map.h>
#include <asm/smp.h>

#include <asm/types.h>
#include <mach/motherboard.h>
#include <mach/vexpress.h>


extern void secondary_startup(void);
extern void vexpress_secondary_startup(void);

extern struct smp_operations zynq_smp_ops;

static DEFINE_SPINLOCK(boot_lock);

#include "core.h"


int __cpuinit vexpress_boot_secondary(unsigned int cpu) {

	/*
	 * Set synchronisation state between this boot processor
	 * and the secondary one
	 */
	spin_lock(&boot_lock);

	/*
	 * This is really belt and braces; we hold unintended secondary
	 * CPUs in the holding pen until we're ready for them.  However,
	 * since we haven't sent them a soft interrupt, they shouldn't
	 * be there.
	 */
	write_pen_release(cpu_logical_map(cpu));

	/*
	 * Send the secondary CPU a soft interrupt, thereby causing
	 * the boot monitor to read the system wide flags register,
	 * and branch to the address found there.
	 */
	smp_cross_call(cpumask_of(cpu), IPI_WAKEUP);


	do {
		dmb();
		if (pen_release == -1)
			break;

		udelay(10);
	} while (1);

	gic_secondary_init(0);

	/*
	 * now the secondary core is starting up let it run its
	 * calibrations, then wait for it to finish
	 */
	spin_unlock(&boot_lock);

	return pen_release != -1 ? -ENOSYS : 0;

}

/* Secondary CPU kernel startup is a 2 step process. The primary CPU
 * starts the secondary CPU by giving it the address of the kernel and
 * then sending it an event to wake it up. The secondary CPU then
 * starts the kernel and tells the primary CPU it's up and running.
 *
 * platform_secondary_init() is called during the bootstrap on the second CPU and
 * is called by arch/arm/kernel/smp.c:secondary_start_kernel() which is itself called
 * by arch/arm/kernel/head.S: secondary_startup(), called by headsmp.S
 *
 */
void __cpuinit vexpress_secondary_init(unsigned int cpu)
{

	gic_secondary_init(0);

	/*
	 * let the primary processor know we're out of the
	 * pen, then head off into the C entry point
	 */
	write_pen_release(-1);

	/*
	 * Synchronise with the boot thread.
	 */
	spin_lock(&boot_lock);
	spin_unlock(&boot_lock);
}

/*
 * Initialise the CPU possible map early - this describes the CPUs
 * which may be present or become present in the system.
 */
void __init vexpress_smp_init_cpus(void)
{
	int i, ncores;

	ncores = NR_CPUS;

	for (i = 0; i < ncores; i++)
		cpu_set(i, cpu_possible_map);

	set_smp_cross_call(gic_raise_softirq);
}

void __init vexpress_smp_prepare_cpus(unsigned int max_cpus)
{
	int i;
	unsigned int sysreg_base;

	sysreg_base = (unsigned int) ioremap(VEXPRESS_SYSREG_BASE, VEXPRESS_SYSREG_SIZE);
	if (!sysreg_base) {
		printk(KERN_WARNING "!!!! BOOTUP jump vectors can't be used !!!!\n");
		while (1)
			;
	}

	writel(~0, sysreg_base + SYS_FLAGSCLR);
	writel((void *) virt_to_phys(vexpress_secondary_startup), sysreg_base + SYS_FLAGSSET);

	/*
	 * Initialise the present map, which describes the set of CPUs
	 * actually populated at the present time.
	 */
	for (i = 0; i < max_cpus; i++)
		cpu_set(i, cpu_present_map);

}


struct smp_operations vexpress_smp_ops __initdata = {

  .smp_init_cpus          = vexpress_smp_init_cpus,

#ifndef CONFIG_PSCI
  .smp_prepare_cpus       = vexpress_smp_prepare_cpus,
  .smp_secondary_init	    = vexpress_secondary_init,
  .smp_boot_secondary     = vexpress_boot_secondary,
#else
  .smp_boot_secondary = psci_smp_boot_secondary,
#endif

};
