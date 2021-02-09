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

#include <types.h>
#include <init.h>
#include <spinlock.h>
#include <sched.h>
#include <smp.h>
#include <compiler.h>

#include <asm/cpu-single.h>
#include <asm/cputype.h>
#include <asm/ptrace.h>
#include <asm/procinfo.h>
#include <asm/proc-fns.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/cacheflush.h>
#include <asm/cachetype.h>
#include <asm/tlbflush.h>
#include <asm/mach/arch.h>

#include "compat.h"

#ifndef MEM_SIZE
#define MEM_SIZE	(16*1024*1024)
#endif

#define OF_DT_MAGIC 0xedfe0dd0 /* 0xd00dfeed in big-endian */

extern int sprintf(char * buf, const char *fmt, ...);

extern void paging_init(struct machine_desc *mdesc);

unsigned int fdt_paddr __initdata;

struct stack {
	u32 irq[3];
	u32 abt[3];
	u32 und[3];
};

static struct stack stacks[NR_CPUS];

struct cpu_tlb_fns cpu_tlb;
struct cpu_cache_fns cpu_cache;

static const char *cpu_name;

static const char *cache_types[16] = {
	"write-through",
	"write-back",
	"write-back",
	"undefined 3",
	"undefined 4",
	"undefined 5",
	"write-back",
	"write-back",
	"undefined 8",
	"undefined 9",
	"undefined 10",
	"undefined 11",
	"undefined 12",
	"undefined 13",
	"write-back",
	"undefined 15",
};

static const char *proc_arch[] = {
	"undefined/unknown",
	"3",
	"4",
	"4T",
	"5",
	"5T",
	"5TE",
	"5TEJ",
	"6TEJ",
	"7",
	"?(11)",
	"?(12)",
	"?(13)",
	"?(14)",
	"?(15)",
	"?(16)",
	"?(17)",
};

#define CACHE_TYPE(x)	(((x) >> 25) & 15)
#define CACHE_S(x)	((x) & (1 << 24))
#define CACHE_DSIZE(x)	(((x) >> 12) & 4095)	/* only if S=1 */
#define CACHE_ISIZE(x)	((x) & 4095)

#define CACHE_SIZE(y)	(((y) >> 6) & 7)
#define CACHE_ASSOC(y)	(((y) >> 3) & 7)
#define CACHE_M(y)	((y) & (1 << 2))
#define CACHE_LINE(y)	((y) & 3)

static inline void dump_cache(const char *prefix, int cpu, unsigned int cache)
{
	unsigned int mult = 2 + (CACHE_M(cache) ? 1 : 0);

	printk("CPU%u: %s: %d bytes, associativity %d, %d byte lines, %d sets\n",
		cpu, prefix,
		mult << (8 + CACHE_SIZE(cache)),
		(mult << CACHE_ASSOC(cache)) >> 1,
		8 << CACHE_LINE(cache),
		1 << (6 + CACHE_SIZE(cache) - CACHE_ASSOC(cache) -
			CACHE_LINE(cache)));
}

static void __init dump_cpu_info(int cpu)
{
	unsigned int info = read_cpuid(CPUID_CACHETYPE);

	if (info == processor_id)
	{
		printk("CPU%u: D %s %s cache\n", cpu, cache_is_vivt() ? "VIVT" : "VIPT",
		       cache_types[CACHE_TYPE(info)]);
		if (CACHE_S(info)) {
			dump_cache("I cache", cpu, CACHE_ISIZE(info));
			dump_cache("D cache", cpu, CACHE_DSIZE(info));
		} else {
			dump_cache("cache", cpu, CACHE_ISIZE(info));
		}
	}

}

int cpu_architecture(void)
{
	int cpu_arch;

  if ((read_cpuid_id() & 0x0008f000) == 0) {
	  cpu_arch = CPU_ARCH_UNKNOWN;
  } else if ((read_cpuid_id() & 0x0008f000) == 0x00007000) {
	  cpu_arch = (read_cpuid_id() & (1 << 23)) ? CPU_ARCH_ARMv4T : CPU_ARCH_ARMv3;
  } else if ((read_cpuid_id() & 0x00080000) == 0x00000000) {
	  cpu_arch = (read_cpuid_id() >> 16) & 7;

	  if (cpu_arch)
		  cpu_arch += CPU_ARCH_ARMv3;
  } else if ((read_cpuid_id() & 0x000f0000) == 0x000f0000) {
	  unsigned int mmfr0;

	  /* Revised CPUID format. Read the Memory Model Feature
	   * Register 0 and check for VMSAv7 or PMSAv7 */
	  asm("mrc	p15, 0, %0, c0, c1, 4" : "=r" (mmfr0));
	  if ((mmfr0 & 0x0000000f) >= 0x00000003 ||
	      (mmfr0 & 0x000000f0) >= 0x00000030)
		  cpu_arch = CPU_ARCH_ARMv7;
	  else if ((mmfr0 & 0x0000000f) == 0x00000002 ||
		   (mmfr0 & 0x000000f0) == 0x00000020)
		  cpu_arch = CPU_ARCH_ARMv6;
	  else
		  cpu_arch = CPU_ARCH_UNKNOWN;
  } else
	  cpu_arch = CPU_ARCH_UNKNOWN;

  return cpu_arch;
}

static struct machine_desc * __init setup_machine(unsigned int nr)
{
	struct machine_desc *list;

	/*
	 * locate machine in the list of supported machines.
	 */
	list = lookup_machine_type(nr);
	if (!list) {
		printk("Machine configuration botched (nr %d), unable "
		       "to continue.\n", nr);
		while (1);
	}

	printk("Machine: %s\n", list->name);

	return list;
}

/*
 * cpu_init - initialise one CPU.
 *
 * cpu_init dumps the cache information, initialises SMP specific
 * information, and sets up the per-CPU stacks.
 */
void cpu_init(void)
{
	unsigned int cpu = smp_processor_id();
	struct stack *stk = &stacks[cpu];

	if (cpu >= NR_CPUS) {
		printk(KERN_CRIT "CPU%u: bad primary CPU number\n", cpu);
		BUG();
	}

	dump_cpu_info(cpu);

	/*
	 * setup stacks for re-entrant exception handlers
	 */
	__asm__ (
	"msr	cpsr_c, %1\n\t"
	"add	sp, %0, %2\n\t"
	"msr	cpsr_c, %3\n\t"
	"add	sp, %0, %4\n\t"
	"msr	cpsr_c, %5\n\t"
	"add	sp, %0, %6\n\t"
	"msr	cpsr_c, %7"
	    :
	    : "r" (stk),
	      "I" (PSR_F_BIT | PSR_I_BIT | IRQ_MODE),
	      "I" (offsetof(struct stack, irq[0])),
	      "I" (PSR_F_BIT | PSR_I_BIT | ABT_MODE),
	      "I" (offsetof(struct stack, abt[0])),
	      "I" (PSR_F_BIT | PSR_I_BIT | UND_MODE),
	      "I" (offsetof(struct stack, und[0])),
	      "I" (PSR_F_BIT | PSR_I_BIT | SVC_MODE)
	    : "r14");
}

static void (*init_machine)(void) __initdata;
extern void (*init_irq)(void) __initdata;
extern struct sys_timer *system_timer;

int __init customize_machine(void)
{
        /* customizes platform devices, or adds new ones */
        if (init_machine)
                init_machine();
        return 0;
}

struct machine_desc *mdesc;
/*
 * (DRE) This routine is called for machine initialization.
 */
void __init setup_arch(char **cmdline_p)
{

	mdesc = setup_machine(machine_arch_type);

	smp_set_ops(mdesc->smp);
	smp_init_cpus();

	cpu_init();

	init_machine = mdesc->init_machine;
	init_irq = mdesc->init_irq;
	system_timer = mdesc->timer;

}

