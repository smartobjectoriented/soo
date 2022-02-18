#ifndef __SMP_H__
#define __SMP_H__

#include <config.h>

/*
 * The following IPIs are specific to AVZ.
 */

#define IPI_WAKEUP		0

#define IPI_EVENT_CHECK		4

void handle_IPI(int ipinr);

void psci_smp_boot_secondary(unsigned int cpu);

/*
 * Initial data for bringing up a secondary CPU.
 */
struct secondary_data {
	unsigned long pgdir;
	void *stack;
};


extern volatile int pen_release;

void smp_cross_call(long cpu_mask, unsigned int ipinr);
void smp_trigger_event(int target_cpu);

void smp_prepare_cpus(unsigned int max_cpus);
void smp_init_cpus(void);
void smp_secondary_init(unsigned int cpu);
void smp_boot_secondary(unsigned int cpu);

/*
 * Mark the boot cpu "online" so that it can call console drivers in
 * printk() and can access its per-cpu storage.
 */
extern void smp_prepare_boot_cpu(void);
void smp_init(void);

void write_pen_release(int val);

#endif /* __SMP_H__ */
