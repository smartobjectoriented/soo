/*
 * Copyright (C) 2016-2018 Daniel Rossier <daniel.rossier@soo.tech>
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

#include <mach/a64.h>

#define PSCI_0_2_FN_BASE			0x84000000
#define PSCI_0_2_FN(n)				(PSCI_0_2_FN_BASE + (n))

#define PSCI_0_2_FN_CPU_OFF			PSCI_0_2_FN(2)
#define PSCI_0_2_FN_CPU_ON			PSCI_0_2_FN(3)

extern void secondary_startup(void);

/* We keep a minimal compatibility with PSCI API since ELMGR is compliant with this API
 * which is required by Linux (MERIDA BSP).
 */
enum psci_function {
	PSCI_FN_CPU_SUSPEND,
	PSCI_FN_CPU_ON,
	PSCI_FN_CPU_OFF,
	PSCI_FN_MIGRATE,
	PSCI_FN_AFFINITY_INFO,
	PSCI_FN_MIGRATE_INFO_TYPE,
	PSCI_FN_MAX,
};

/* Taken from Linux */
static noinline int __invoke_smc(u32 function_id, u32 arg0, u32 arg1, u32 arg2)
{
	register u32 function_id_r0 asm ("r0") = function_id;
	register u32 arg0_r1 asm ("r1") = arg0;
	register u32 arg1_r2 asm ("r2") = arg1;
	register u32 arg2_r3 asm ("r3") = arg2;

	asm volatile(	__SMC(0)
			: "+r" (function_id_r0)
			: "r" (arg0_r1), "r" (arg1_r2), "r" (arg2_r3));

	return function_id_r0;

}

/* Switch off the current CPU */
int cpu_off(void)
{
	int ret;

	ret = __invoke_smc(PSCI_0_2_FN_CPU_ON, 0, 0, 0);

	return ret;
}
/* Switch on a CPU */
static int cpu_on(unsigned long cpuid, unsigned long entry_point)
{
	int ret;

	ret = __invoke_smc(PSCI_0_2_FN_CPU_ON, cpuid, entry_point, 0);

	return ret;
}

/*
 * Initialise the CPU possible map early - this describes the CPUs
 * which may be present or become present in the system.
 */
void __init sun50i_smp_init_cpus(void)
{
	int i;

	for (i = 0; i < NR_CPUS; i++)
		cpu_set(i, cpu_possible_map);

	set_smp_cross_call(gic_raise_softirq);
}


static DEFINE_SPINLOCK(cpu_lock);

static void __init sun50i_smp_prepare_cpus(unsigned int max_cpus)
{


}

static int sun50i_smp_boot_secondary(unsigned int cpu)
{
	printk("%s: booting CPU: %d\n", __func__, cpu);

	spin_lock(&cpu_lock);
	cpu_on(cpu, (u32) virt_to_phys(secondary_startup));
	spin_unlock(&cpu_lock);

	return 0;
}

void __cpuinit sun50i_secondary_init(unsigned int cpu)
{
	/*
	 * CPUx needs to setup his own cpu interface and
	 * banked stuff in the global distributor.
	 *
	 * "[..] when the GIC-400 is implemented as
	 * part of a multiprocessor system, registers associated
	 * with PPIs or SGIs are Banked to provide a separate copy
	 * for each connected processor." GIC-400 manual p.33
	 */

	gic_secondary_init(0);

	/*
	 * Synchronize with the boot thread.
	 */
	spin_lock(&cpu_lock);
	spin_unlock(&cpu_lock);
}

struct smp_operations sun50i_smp_ops __initdata = {
	.smp_init_cpus          = sun50i_smp_init_cpus,
	.smp_prepare_cpus	= sun50i_smp_prepare_cpus,
	.smp_secondary_init	= sun50i_secondary_init,
	.smp_boot_secondary	= sun50i_smp_boot_secondary,
};

