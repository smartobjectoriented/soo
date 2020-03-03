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

#ifndef __ASM_SMP_H
#define __ASM_SMP_H


#ifndef __ASSEMBLY__
#include <avz/config.h>
#include <avz/kernel.h>
#include <avz/smp.h>
#include <avz/cpumask.h>

#include <asm/current.h>
#include <asm/percpu.h>


extern volatile int pen_release;
void write_pen_release(int val);
int read_pen_release(void);

/*
 * The following IPIs are reserved to AVZ.
 * IPI nr prior to 12 can be processed by the Cobalt realtime agency.
 */

#define IPI_AVZ_BASE	12

#define IPI_WAKEUP		IPI_AVZ_BASE		
#define IPI_EVENT_CHECK		(IPI_AVZ_BASE + 1)

/*
 * Logical CPU mapping.
 */
extern int __cpu_logical_map[];
#define cpu_logical_map(cpu)	__cpu_logical_map[cpu]

void smp_clear_cpu_maps (void);

#define BAD_APICID -1U

DECLARE_PER_CPU(cpumask_t, cpu_core_map);
#endif /* __ASSEMBLY__ */

extern void (*smp_cross_call)(const struct cpumask *, unsigned int);

/*
 * Return true if we are running on a SMP platform
 */
static inline bool is_smp(void)
{
	return true;
}

#ifndef __ASSEMBLY__

/*
 * Private routines/data
 */
 
extern void smp_alloc_memory(void);

void smp_send_nmi_allbutself(void);

void  send_IPI_mask(const cpumask_t *mask, int vector);

extern void (*mtrr_hook) (void);

extern void zap_low_mappings(l2_pgentry_t *base);
#define MAX_APICID 256
extern u32 x86_cpu_to_apicid[];
extern u32 cpu_2_logical_apicid[];

#define cpu_physical_id(cpu)	x86_cpu_to_apicid[cpu]
void handle_IPI(int ipinr);

int psci_smp_boot_secondary(unsigned int cpu);

extern int smp_get_max_cpus (void);

#define cpu_is_offline(cpu) unlikely(!cpu_online(cpu))
extern int cpu_down(unsigned int cpu);
extern int cpu_up(unsigned int cpu);
extern void cpu_exit_clear(void);
extern void cpu_uninit(void);
extern void disable_nonboot_cpus(void);
extern void enable_nonboot_cpus(void);
int cpu_add(uint32_t apic_id, uint32_t acpi_id, uint32_t pxm);

extern cpumask_t cpu_callout_map;
extern cpumask_t cpu_callin_map;

/* We don't mark CPUs online until __cpu_up(), so we need another measure */
static inline int num_booting_cpus(void)
{
	return cpus_weight(cpu_callout_map);
}

extern int __cpu_disable(void);
extern void __cpu_die(unsigned int cpu);

void __stop_this_cpu(void);

#endif /* !__ASSEMBLY__ */

#define NO_PROC_ID		0xFF		/* No processor magic marker */

struct smp_operations {
	/*
	 * Setup the set of possible CPUs (via set_cpu_possible)
	 */
	void (*smp_init_cpus)(void);
	/*
	 * Initialize cpu_possible map, and enable coherency
	 */
	void (*smp_prepare_cpus)(unsigned int max_cpus);

	/*
	 * Perform platform specific initialisation of the specified CPU.
	 */
	void (*smp_secondary_init)(unsigned int cpu);
	/*
	 * Boot a secondary CPU, and assign it the specified idle task.
	 * This also gives us the initial stack to use for this CPU.
	 */
	int  (*smp_boot_secondary)(unsigned int cpu);

};

/*
 * Setup the set of possible CPUs (via set_cpu_possible)
 */
extern void smp_init_cpus(void);
extern void platform_smp_prepare_cpus(unsigned int);

void __cpuinit platform_secondary_init(unsigned int cpu);

/*
 * set platform specific SMP operations
 */
extern void smp_set_ops(struct smp_operations *);

/*
 * Initial data for bringing up a secondary CPU.
 */
struct secondary_data {
	unsigned long pgdir;
	void *stack;
};

#endif
