/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Copyright (C) 2012 ARM Limited
 *
 * Author: Will Deacon <will.deacon@arm.com>
 */

#include <errno.h>
#include <spinlock.h>
#include <smp.h>
#include <psci.h>
#include <arm-smccc.h>
#include <memory.h>

#include <asm/processor.h>
#include <asm/mmu.h>

/*
 * psci_smp assumes that the following is true about PSCI:
 *
 * cpu_suspend   Suspend the execution on a CPU
 * @state        we don't currently describe affinity levels, so just pass 0.
 * @entry_point  the first instruction to be executed on return
 * returns 0  success, < 0 on failure
 *
 * cpu_off       Power down a CPU
 * @state        we don't currently describe affinity levels, so just pass 0.
 * no return on successful call
 *
 * cpu_on        Power up a CPU
 * @cpuid        cpuid of target CPU, as from MPIDR
 * @entry_point  the first instruction to be executed on return
 * returns 0  success, < 0 on failure
 *
 * migrate       Migrate the context to a different CPU
 * @cpuid        cpuid of target CPU, as from MPIDR
 * returns 0  success, < 0 on failure
 *
 */

extern void secondary_startup(void);

static DEFINE_SPINLOCK(cpu_lock);

unsigned long __invoke_psci_fn_smc(unsigned long function_id,
			unsigned long arg0, unsigned long arg1,
			unsigned long arg2)
{
	struct arm_smccc_res res;

	arm_smccc_smc(function_id, arg0, arg1, arg2, 0, 0, 0, 0, &res);
	return res.a0;
}

unsigned long __invoke_psci_fn_hvc(unsigned long function_id,
			unsigned long arg0, unsigned long arg1,
			unsigned long arg2)
{
	struct arm_smccc_res res;

	arm_smccc_hvc(function_id, arg0, arg1, arg2, 0, 0, 0, 0, &res);
	return res.a0;
}

static int psci_to_errno(int errno)
{
	switch (errno) {
	case PSCI_RET_SUCCESS:
		return 0;
	case PSCI_RET_NOT_SUPPORTED:
		return -EOPNOTSUPP;
	case PSCI_RET_INVALID_PARAMS:
	case PSCI_RET_INVALID_ADDRESS:
		return -EINVAL;
	case PSCI_RET_DENIED:
		return -EPERM;
	};

	return -EINVAL;
}

/* Switch off the current CPU */
int cpu_off(unsigned long cpuid)
{
	int ret;

#ifdef CONFIG_ARM_TRUSTZONE
	ret = __invoke_psci_fn_smc(PSCI_0_2_FN_CPU_OFF, cpuid, 0, 0);
#else
	ret = __invoke_psci_fn_hvc(PSCI_0_2_FN_CPU_OFF, cpuid, 0, 0);
#endif

	return psci_to_errno(ret);
}

/* Switch on a CPU */
static int cpu_on(unsigned long cpuid, unsigned long entry_point)
{
	int ret;

#ifdef CONFIG_ARM_TRUSTZONE
	ret = __invoke_psci_fn_smc(PSCI_0_2_FN_CPU_ON, cpuid, entry_point, 0);
#else
	ret = __invoke_psci_fn_hvc(PSCI_0_2_FN_CPU_ON, cpuid, entry_point, 0);
#endif
	return psci_to_errno(ret);;
}

void psci_smp_boot_secondary(unsigned int cpu)
{
	printk("%s: booting CPU: %d\n", __func__, cpu);

	spin_lock(&cpu_lock);
	cpu_on(cpu, (u32) virt_to_phys(secondary_startup));
	spin_unlock(&cpu_lock);
}

