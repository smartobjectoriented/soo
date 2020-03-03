/*
 *  Copyright (C) 2002 ARM Limited, All Rights Reserved.
 *  Copyright (C) 2016 Daniel Rossier <daniel.rossier@soo.tech>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Interrupt architecture for the GIC:
 *
 * o There is one Interrupt Distributor, which receives interrupts
 *   from system devices and sends them to the Interrupt Controllers.
 *
 * o There is one CPU Interface per CPU, which sends interrupts sent
 *   by the Distributor, and interrupts generated locally, to the
 *   associated CPU. The base address of the CPU interface is usually
 *   aliased so that the same address points to different chips depending
 *   on the CPU it is accessed from.
 *
 * Note that IRQs 0-31 are special - they are local to each CPU.
 * As such, the enable set/clear, pending set/clear and active bit
 * registers are banked per-cpu for these sources.
 */

#include <avz/config.h>
#include <avz/init.h>
#include <avz/cpumask.h>
#include <avz/sched.h>
#include <avz/types.h>

#include <asm/irq.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/smp.h>

#include <asm/mach/irq.h>

#include <asm/hardware/gic.h>
#include <asm-generic/errno.h>
#include <asm/bug.h>

DEFINE_PER_CPU(spinlock_t, intc_lock);

/* Address of GIC 0 CPU interface */
void __iomem *gic_cpu_base_addr __read_mostly;
unsigned int gic_irq_offset = 0;

struct gic_chip_data {
	unsigned int irq_offset;
	void __iomem *dist_base;
	void __iomem *cpu_base;
	unsigned int max_irq;

};

#ifndef MAX_GIC_NR
#define MAX_GIC_NR	1
#endif

#define NR_GIC_CPU_IF 8
static u8 gic_cpu_map[NR_GIC_CPU_IF] __read_mostly;

static struct gic_chip_data gic_data[MAX_GIC_NR] __read_mostly;

static inline void __iomem *gic_dist_base(unsigned int irq)
{
	struct gic_chip_data *gic_data = get_irq_chip_data(irq);
	return gic_data->dist_base;
}

static inline void __iomem *gic_cpu_base(unsigned int irq)
{
	struct gic_chip_data *gic_data = get_irq_chip_data(irq);
	return gic_data->cpu_base;
}

static inline unsigned int gic_irq(unsigned int irq)
{
	struct gic_chip_data *gic_data = get_irq_chip_data(irq);
	return irq - gic_data->irq_offset;
}

/*
 * Routines to acknowledge, disable and enable interrupts
 */
static void gic_eoi_irq(unsigned int irq)
{
	int cpu = smp_processor_id();

	spin_lock(&per_cpu(intc_lock, cpu));
	writel(gic_irq(irq), gic_cpu_base(irq) + GIC_CPU_EOI);
	spin_unlock(&per_cpu(intc_lock, cpu));
}

static void force_eoi_irq(unsigned int irq) {
	int cpu = smp_processor_id();

	if (gic_cpu_base_addr != NULL) {
		spin_lock(&per_cpu(intc_lock, cpu));
		writel(irq - gic_irq_offset, (unsigned long) gic_cpu_base_addr + GIC_CPU_EOI);
		spin_unlock(&per_cpu(intc_lock, cpu));
	}
}


static void gic_mask_irq(unsigned int irq)
{
	u32 mask = 1 << (irq % 32);
	int cpu = smp_processor_id();

	spin_lock(&per_cpu(intc_lock, cpu));
	writel(mask, gic_dist_base(irq) + GIC_DIST_ENABLE_CLEAR + (gic_irq(irq) / 32) * 4);
	spin_unlock(&per_cpu(intc_lock, cpu));

}

static void gic_unmask_irq(unsigned int irq)
{
	u32 mask = 1 << (irq % 32);
	int cpu = smp_processor_id();

	spin_lock(&per_cpu(intc_lock, cpu));
	writel(mask, gic_dist_base(irq) + GIC_DIST_ENABLE_SET + (gic_irq(irq) / 32) * 4);
	spin_unlock(&per_cpu(intc_lock, cpu));

}

void gic_set_prio(unsigned int irq, unsigned char prio)
{
	void __iomem *base = gic_dist_base(irq);
	unsigned int gicirq = gic_irq(irq);
	u32 primask = 0xff << (gicirq % 4) * 8;
	u32 prival = prio << (gicirq % 4) * 8;
	u32 prioff = (gicirq / 4) * 4;
	u32 val;

	val = readl(base + GIC_DIST_PRI + prioff);
	val &= ~primask;
	val |= prival;
	writel(val, base + GIC_DIST_PRI + prioff);
}

int irq_set_affinity(unsigned int irq, struct cpumask *mask_val)
{
	void __iomem *reg = gic_dist_base(irq) + GIC_DIST_TARGET + (gic_irq(irq) & ~3);
	unsigned int shift = (irq % 4) * 8;
	unsigned int cpu = first_cpu(*mask_val);
	u32 val;
	struct irqdesc *desc;
	int __cpu = smp_processor_id();

	spin_lock(&per_cpu(intc_lock, __cpu));
	desc = irq_to_desc(irq);
	if (desc == NULL) {
		spin_unlock(&per_cpu(intc_lock, __cpu));
		return -EINVAL;
	}

	val = readl(reg) & ~(0xff << shift);
	val |= 1 << (cpu + shift);
	writel(val, reg);
	spin_unlock(&per_cpu(intc_lock, __cpu));

	return 0;
}

static struct irqchip gic_chip = {
		.name			= "GIC",
		.eoi			= gic_eoi_irq,
		.mask			= gic_mask_irq,
		.unmask			= gic_unmask_irq,
};

static inline void *gic_data_cpu_base(struct gic_chip_data *data)
{
	return data->cpu_base;
}

static inline void *gic_data_dist_base(struct gic_chip_data *data)
{
	return data->dist_base;
}

static void __init gic_dist_init(struct gic_chip_data *gic, unsigned int irq_start)
{
	unsigned int gic_irqs, irq_limit, i;
	void __iomem *base = gic->dist_base;
	u32 cpumask = 1 << smp_processor_id();
	struct cpumask mask;

	cpumask |= cpumask << 8;
	cpumask |= cpumask << 16;

	writel(0, base + GIC_DIST_CTRL);

	/*
	 * Find out how many interrupts are supported.
	 * The GIC only supports up to 1020 interrupt sources.
	 */
	gic_irqs = readl(base + GIC_DIST_CTR) & 0x1f;
	gic_irqs = (gic_irqs + 1) * 32;
	if (gic_irqs > 1020)
		gic_irqs = 1020;

	/*
	 * Set all global interrupts to be level triggered, active low.
	 */
	for (i = 32; i < gic_irqs; i += 16)
		writel(0, base + GIC_DIST_CONFIG + i * 4 / 16);

	/*
	 * Major IRQs are routed to CPU #0
	 */
	for (i = 32; i < gic_irqs; i += 4)
		writel(cpumask, base + GIC_DIST_TARGET + i * 4 / 4);

	/*
	 * Set priority on all global interrupts.
	 */
	for (i = 32; i < gic_irqs; i += 4)
		writel(0xa0a0a0a0, base + GIC_DIST_PRI + i * 4 / 4);

	/*
	 * Disable all interrupts.  Leave the PPI and SGIs alone
	 * as these enables are banked registers.
	 */
	for (i = 32; i < gic_irqs; i += 32)
		writel(0xffffffff, base + GIC_DIST_ENABLE_CLEAR + i * 4 / 32);

	/*
	 * Limit number of interrupts registered to the platform maximum
	 */
	irq_limit = gic->irq_offset + gic_irqs;
	if (irq_limit > NR_IRQS)
		irq_limit = NR_IRQS;

	/*
	 * Setup the avz subsystem.
	 */
	for (i = irq_start; i < irq_limit; i++) {
		set_irq_chip(i, &gic_chip);
		set_irq_chip_data(i, gic);

		set_irq_handler(i, handle_fasteoi_irq);
		set_irq_flags(i, IRQF_VALID | IRQF_PROBE);
	}

	gic->max_irq = gic_irqs;

	writel(1, base + GIC_DIST_CTRL);

	/*
	 * By default, route all IRQs to CPU #0. Re-routing to other CPU can be performed subsequently using
	 * irq_set_affinity()
	 */
	for (i = 32; i < gic_irqs; i++) {
		mask = cpumask_of_cpu(AGENCY_CPU);
		irq_set_affinity(i, &mask);
	}

	dmb();
}

void gic_cpu_config(void __iomem *base)
{
	int i;

	/*
	 * Deal with the banked PPI and SGI interrupts - disable all
	 * PPI interrupts, ensure all SGI interrupts are enabled.
	 */
	writel(GICD_INT_EN_CLR_PPI, base + GIC_DIST_ENABLE_CLEAR);
	writel(GICD_INT_EN_SET_SGI, base + GIC_DIST_ENABLE_SET);

	/*
	 * Set priority on PPI and SGI interrupts
	 */
	for (i = 0; i < 32; i += 4)
		writel(GICD_INT_DEF_PRI_X4, base + GIC_DIST_PRI + i * 4 / 4);

}

static void gic_cpu_if_up(void)
{
	void __iomem *cpu_base = gic_data_cpu_base(&gic_data[0]);
	u32 bypass = 0;

	/*
	 * Preserve bypass disable bits to be written back later
	 */
	bypass = readl(cpu_base + GIC_CPU_CTRL);
	bypass &= GICC_DIS_BYPASS_MASK;

	writel(bypass | GICC_ENABLE, cpu_base + GIC_CPU_CTRL);
}

static void __cpuinit gic_cpu_init(struct gic_chip_data *gic)
{
	void __iomem *dist_base = gic->dist_base;
	void __iomem *base = gic->cpu_base;
	unsigned int cpu = smp_processor_id();

	/*
	 * Get what the GIC says our CPU mask is.
	 */
	BUG_ON(cpu >= NR_GIC_CPU_IF);

	spin_lock_init(&per_cpu(intc_lock, cpu));

	gic_cpu_config(dist_base);

	writel(GICC_INT_PRI_THRESHOLD, base + GIC_CPU_PRIMASK);

	gic_cpu_if_up();

}

/*
 * The interrupt numbering scheme is defined in the
 * interrupt controller spec.  To wit:
 *
 * Interrupts 0-15 are IPI
 * 16-28 are reserved
 * 29-31 are local.  We allow 30 to be used for the watchdog.
 * 32-1020 are global
 * 1021-1022 are reserved
 * 1023 is "spurious" (no interrupt)
 *
 * For now, we ignore all local interrupts so only return an interrupt if it's
 * between 30 and 1020.  The test_for_ipi routine below will pick up on IPIs.
 *
 * A simple read from the controller will tell us the number of the highest
 * priority enabled interrupt.  We then just need to check whether it is in the
 * valid range for an IRQ (30-1020 inclusive).
 *
 */
void ll_entry_irq(void)
{
	u32 irqstat, irqnr;
	struct gic_chip_data *gic = &gic_data[0];
	void __iomem *cpu_base = gic_data_cpu_base(gic);

	ASSERT(local_irq_is_disabled());

	do {
		irqstat = readl(cpu_base + GIC_CPU_INTACK);
		irqnr = irqstat & GICC_IAR_INT_ID_MASK;

		dmb();

		BUG_ON((smp_processor_id() == AGENCY_CPU) || (smp_processor_id() == AGENCY_RT_CPU));

		if (likely((irqnr > 31) && (irqnr < 1021))) /* SPIs 32-irqmax */
			BUG();

		if (irqnr < 16) {
			/* IPI are end-of-interrupt'ed like this. */
			writel(irqstat, cpu_base + GIC_CPU_EOI);

			handle_IPI(irqnr);
			continue;

		} else if (irqnr < 32) {

			/* Only PPI #IRQ_ARCH_ARM_TIMER is used for architected timer on ARM dedicated to the agency */
			/* The other PPI is not used. */

			if (irqnr == IRQ_ARCH_ARM_TIMER)
				asm_do_IRQ(irqnr);
			else
				BUG();
			continue;
		}

		/* At the end, we might get a spurious interrupt on CPU #1 from the GIC. In this case,
		 * we simply ignore it...
		 */
		if (irqnr == 1023)
			force_eoi_irq(irqnr);

		break;

	} while (true);
}

void __init gic_init(unsigned int gic_nr, unsigned int irq_start, void __iomem *dist_base, void __iomem *cpu_base)
{
	struct gic_chip_data *gic;
	int i;

	BUG_ON(gic_nr >= MAX_GIC_NR);

	/*
	 * Initialize the CPU interface map to all CPUs.
	 * It will be refined as each CPU probes its ID.
	 */
	for (i = 0; i < NR_GIC_CPU_IF; i++)
		gic_cpu_map[i] = 0xff;

	gic = &gic_data[gic_nr];
	gic->dist_base = dist_base;
	gic->cpu_base = cpu_base;

	if (irq_start > 0) /* validate irq_start = 0 */
		gic->irq_offset = (irq_start - 1) & ~31;

	if (gic_nr == 0)  {
		gic_cpu_base_addr = cpu_base;
		gic_irq_offset = gic->irq_offset;
	}

	gic_dist_init(gic, irq_start);
	gic_cpu_init(gic);

	/*
	 * We still initialize the default IPI handler in case IPIs have to be forwarded to the guests.
	 */
	for (i = 0; i <= 15; i++) {
		set_irq_chip(i, &gic_chip);
		set_irq_chip_data(i, gic);

		set_irq_handler(i, handle_fasteoi_irq);
		set_irq_flags(i, IRQF_VALID | IRQF_NOAUTOEN);
	}

	/*
	 * We also prepare to process PPI #IRQ_ARCH_ARM_TIMER for arch timer to the agency.
	 * The arch timer has a specific handler that propagates the VIRQ_TIMER to the non RT domains.
	 */

	set_irq_chip(IRQ_ARCH_ARM_TIMER, &gic_chip);
	set_irq_chip_data(IRQ_ARCH_ARM_TIMER, gic);
	set_irq_flags(IRQ_ARCH_ARM_TIMER, IRQF_VALID | IRQF_NOAUTOEN);
	set_irq_handler(IRQ_ARCH_ARM_TIMER, handle_fasteoi_irq);

}

void __cpuinit gic_secondary_init(unsigned int gic_nr)
{
	BUG_ON(gic_nr >= MAX_GIC_NR);

	gic_cpu_init(&gic_data[gic_nr]);
}

void gic_raise_softirq_by_map(unsigned long map, unsigned int irq) {
	/* This always happens on GIC0 */
	writel(map << 16 | irq, gic_data_dist_base(&gic_data[0]) + GIC_DIST_SOFTINT);

}

void gic_raise_softirq(const struct cpumask *mask, unsigned int irq)
{
	unsigned long flags;
	unsigned long map = *cpus_addr(*mask);
	int cpu = smp_processor_id();

	spin_lock_irqsave(&per_cpu(intc_lock, cpu), flags);

	/*
	 * Ensure that stores to Normal memory are visible to the
	 * other CPUs before they observe us issuing the IPI.
	 */
	dmb(ishst);

	gic_raise_softirq_by_map(map, irq);

	spin_unlock_irqrestore(&per_cpu(intc_lock, cpu), flags);

}

