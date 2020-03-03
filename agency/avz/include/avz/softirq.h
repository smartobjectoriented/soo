#if !defined(__SOFTIRQ_H__) && !defined(__ASSEMBLY__)
#define __SOFTIRQ_H__

/* Low-latency softirqs come first in the following list. */
enum {
    TIMER_SOFTIRQ = 0,
    SCHEDULE_SOFTIRQ,
    NR_COMMON_SOFTIRQS
};

#include <avz/config.h>
#include <avz/lib.h>
#include <avz/smp.h>
#include <asm/bitops.h>
#include <asm/current.h>
#include <asm/softirq.h>

#define NR_SOFTIRQS (NR_COMMON_SOFTIRQS + NR_ARCH_SOFTIRQS)

typedef void (*softirq_handler)(void);

/*
 * Simple wrappers reducing source bloat.  Define all irq_stat fields
 * here, even ones that are arch dependent.  That way we get common
 * definitions instead of differing sets for each arch.
 */

extern uint32_t softirq_stat[];

/* arch independent irq_stat fields */
#define softirq_pending(cpu)	(softirq_stat[cpu])


void do_softirq(void);

void open_softirq(int nr, softirq_handler handler);
void softirq_init(void);

void cpumask_raise_softirq(cpumask_t mask, unsigned int nr);
void cpu_raise_softirq(unsigned int cpu, unsigned int nr);
void raise_softirq(unsigned int nr);




#endif /* __SOFTIRQ_H__ */
