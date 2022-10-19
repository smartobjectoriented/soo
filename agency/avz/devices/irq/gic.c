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

#include <config.h>
#include <sched.h>
#include <types.h>
#include <percpu.h>
#include <io.h>
#include <smp.h>

#include <device/irq.h>

#include <device/arch/gic.h>

#include <mach/gic.h>

#include <asm-generic/errno.h>

DEFINE_PER_CPU(spinlock_t, intc_lock);

static void *gicd_base;
static void *gicc_base;
static void *gich_base;

static unsigned int gic_num_lr;

static u32 gic_read_lr(unsigned int n)
{
	return readi(gich_base + GICH_LR_BASE + n * 4);
}

static void gic_write_lr(unsigned int n, u32 value)
{
	writei(value, gich_base + GICH_LR_BASE + n * 4);
}

static void gic_mask_irq(unsigned int irq)
{
	u32 mask = 1 << (irq % 32);
	int cpu = smp_processor_id();

	spin_lock(&per_cpu(intc_lock, cpu));
	writei(mask, gicd_base + GIC_DIST_ENABLE_CLEAR + (irq / 32) * 4);
	spin_unlock(&per_cpu(intc_lock, cpu));

}

static void gic_unmask_irq(unsigned int irq)
{
	u32 mask = 1 << (irq % 32);
	int cpu = smp_processor_id();

	spin_lock(&per_cpu(intc_lock, cpu));
	writei(mask, gicd_base + GIC_DIST_ENABLE_SET + (irq / 32) * 4);
	spin_unlock(&per_cpu(intc_lock, cpu));

}

void gic_set_prio(unsigned int irq, unsigned char prio)
{
	u32 primask = 0xff << (irq % 4) * 8;
	u32 prival = prio << (irq % 4) * 8;
	u32 prioff = (irq / 4) * 4;
	u32 val;

	val = readi(gicd_base + GIC_DIST_PRI + prioff);
	val &= ~primask;
	val |= prival;
	writei(val, gicd_base + GIC_DIST_PRI + prioff);
}

int irq_set_affinity(unsigned int irq, int cpu)
{
	void *reg = gicd_base + GIC_DIST_TARGET + (irq & ~3);
	unsigned int shift = (irq % 4) * 8;
	u32 val;
	struct irqdesc *desc;
	int __cpu = smp_processor_id();

	spin_lock(&per_cpu(intc_lock, __cpu));
	desc = irq_to_desc(irq);
	if (desc == NULL) {
		spin_unlock(&per_cpu(intc_lock, __cpu));
		return -EINVAL;
	}

	val = readi(reg) & ~(0xff << shift);
	val |= 1 << (cpu + shift);
	writei(val, reg);
	spin_unlock(&per_cpu(intc_lock, __cpu));

	return 0;
}

static void gicd_init(void)
{
	unsigned int gic_irqs, i;

	/* Disable the controller so we can configure it before it passes any
	 * interrupts to the CPU
	 */

	writei(GICD_DISABLE, gicd_base + GIC_DIST_CTRL);

	/*
	 * Find out how many interrupts are supported.
	 * The GIC only supports up to 1020 interrupt sources.
	 */
	gic_irqs = readi(gicd_base + GIC_DIST_CTR) & 0x1f;

	gic_irqs = (gic_irqs + 1) * 32;
	if (gic_irqs > 1020)
		gic_irqs = 1020;

	/*
	 * Set all global interrupts to be level triggered, active low.
	 */
	for (i = 32; i < gic_irqs; i += 16)
		writei(0, gicd_base + GIC_DIST_CONFIG + i * 4 / 16);

	/*
	 * Major IRQs are routed to CPU #0
	 */
	for (i = 32; i < gic_irqs; i += 4)
		writei(0x01010101, gicd_base + GIC_DIST_TARGET + i * 4 / 4);

	/*
	 * Set priority on all global interrupts.
	 */
	for (i = 32; i < gic_irqs; i += 4)
		writei(0xa0a0a0a0, gicd_base + GIC_DIST_PRI + i * 4 / 4);

	/*
	 * Disable all interrupts.  Leave the PPI and SGIs alone
	 * as these enables are banked registers.
	 */
	for (i = 32; i < gic_irqs; i += 32)
		writei(0xffffffff, gicd_base + GIC_DIST_ENABLE_CLEAR + i * 4 / 32);

	writei(1, gicd_base + GIC_DIST_CTRL);

	/*
	 * By default, route all IRQs to CPU #0. Re-routing to other CPU can be performed subsequently using
	 * irq_set_affinity()
	 */
	for (i = 32; i < gic_irqs; i++)
		irq_set_affinity(i, AGENCY_CPU);

	smp_mb();
}

void gic_cpu_config(void *base)
{
	int i;

	/*
	 * Deal with the banked PPI and SGI interrupts - disable all
	 * PPI interrupts, ensure all SGI interrupts are enabled.
	 */
	writei(GICD_INT_EN_CLR_PPI, base + GIC_DIST_ENABLE_CLEAR);
	writei(GICD_INT_EN_SET_SGI, base + GIC_DIST_ENABLE_SET);

	/*
	 * Set priority on PPI and SGI interrupts
	 */
	for (i = 0; i < 32; i += 4)
		writei(GICD_INT_DEF_PRI_X4, base + GIC_DIST_PRI + i * 4 / 4);

}

static void gic_cpu_if_up(void)
{
	u32 bypass = 0;

	/*
	 * Preserve bypass disable bits to be written back later
	 */
	bypass = readi(gicc_base + GIC_CPU_CTRL);
	bypass &= GICC_DIS_BYPASS_MASK;

	writei(bypass | GICC_ENABLE | GIC_CPU_EOI, gicc_base + GIC_CPU_CTRL);

}

void gicc_init(void)
{
	void *dist_base = gicd_base;
	void *base = gicc_base;
	unsigned int cpu = smp_processor_id();

	spin_lock_init(&per_cpu(intc_lock, cpu));

	gic_cpu_config(dist_base);

	writei(GICC_INT_PRI_THRESHOLD, base + GIC_CPU_PRIMASK);

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

void check_irq(void) {
	u32 irqstat, irqnr;

	irqstat = readi(gicc_base + GIC_CPU_INTACK);
	irqnr = irqstat & GICC_IAR_INT_ID_MASK;
	printk("## IRQ: %d\n", irqnr);
}

static void gic_handle(cpu_regs_t *cpu_regs) {
	u32 irqstat, irqnr;

	ASSERT(local_irq_is_disabled());

	do {
		irqstat = readi(gicc_base + GIC_CPU_INTACK);
		irqnr = irqstat & GICC_IAR_INT_ID_MASK;

		smp_mb();
#if 0
		if ((smp_processor_id() == AGENCY_CPU) || (smp_processor_id() == AGENCY_RT_CPU)){
			printk("## GIC failure: getting IRQ %d on CPU %d\n", irqnr, smp_processor_id());
			BUG();
		}
#endif

		if (irqnr == 1023)
			return ;

		/* SPIs 32-irqmax */
		if (likely((irqnr > 31) && (irqnr < 1021))) {
			printk("## GIC failure: unexpected IRQ : %d\n", irqnr);
			BUG();
		}

		if (irqnr < 16) {
			/* IPI are end-of-interrupt'ed like this. */
			writei(irqstat, gicc_base + GIC_CPU_EOI);

			handle_IPI(irqnr);

		} else if (irqnr < 32) {

			/* Only PPI #IRQ_ARCH_ARM_TIMER is used for architected timer on ARM dedicated to the agency */
			/* The other PPI is not used. */

			if (irqnr == IRQ_ARCH_ARM_TIMER_EL2)
				asm_do_IRQ(irqnr);
			else
				BUG();
		}

		/* End-of-Interrupt */
		writei(irqnr, gicc_base + GIC_CPU_EOI);

	} while (true);
}

static void gic_clear_pending_irqs(void)
{
	unsigned int n;

	/* Clear list registers. */
	for (n = 0; n < gic_num_lr; n++)
		gic_write_lr(n, 0);

	/* Clear active priority bits. */
	writei(0, gich_base + GICH_APR);
}

#if 0 /* Not used at the moment */
static void gic_enable_maint_irq(bool enable)
{
	u32 hcr;

	hcr = readi(gich_base + GICH_HCR);
	if (enable)
		hcr |= GICH_HCR_UIE;
	else
		hcr &= ~GICH_HCR_UIE;
	writei(hcr, gich_base + GICH_HCR);
}
#endif

void gich_init(void) {
	u32 vtr, vmcr;
	u32 gicc_ctlr, gicc_pmr;

	/* Ensure all IPIs and the maintenance PPI are enabled. */
	writei(0x0000ffff | (1 << 25), gicd_base + GIC_DIST_ENABLE_SET);

	gicc_ctlr = readi(gicc_base + GICC_CTLR);
	gicc_pmr = readi(gicc_base + GICC_PMR);

	vtr = readi(gich_base + GICH_VTR);
	gic_num_lr = (vtr & 0x3f) + 1;

	/* VMCR only contains 5 bits of priority */
	vmcr = (gicc_pmr >> GICV_PMR_SHIFT) << GICH_VMCR_PMR_SHIFT;
	/*
	 * All virtual interrupts are group 0 in this driver since the GICV
	 * layout seen by the guest corresponds to GICC without security
	 * extensions:
	 * - A read from GICV_IAR doesn't acknowledge group 1 interrupts
	 *   (GICV_AIAR does it, but the guest never attempts to accesses it)
	 * - A write to GICV_CTLR.GRP0EN corresponds to the GICC_CTLR.GRP1EN bit
	 *   Since the guest's driver thinks that it is accessing a GIC with
	 *   security extensions, a write to GPR1EN will enable group 0
	 *   interrups.
	 * - Group 0 interrupts are presented as virtual IRQs (FIQEn = 0)
	 */
	if (gicc_ctlr & GICC_CTLR_GRPEN1)
		vmcr |= GICH_VMCR_EN0;
	if (gicc_ctlr & GICC_CTLR_EOImode)
		vmcr |= GICH_VMCR_EOImode;

	writei(vmcr, gich_base + GICH_VMCR);
	writei(GICH_HCR_EN, gich_base + GICH_HCR);

	/*
	 * Clear pending virtual IRQs in case anything is left from previous
	 * use. Physically pending IRQs will be forwarded to Linux once we
	 * enable interrupts for the hypervisor, except for SGIs, see below.
	 */
	gic_clear_pending_irqs();

}

void gic_init(addr_t *dist_base, addr_t *cpu_base, addr_t *hyp_base)
{
	gicd_base = dist_base;
	gicc_base = cpu_base;
	gich_base = hyp_base;

	writei(0, gich_base + GICH_VMCR);

	gicd_init();
	gicc_init();
	gich_init();

	irq_ops.irq_handle = gic_handle;
	irq_ops.irq_enable = gic_unmask_irq;
	irq_ops.irq_disable = gic_mask_irq;
	irq_ops.irq_mask = gic_mask_irq;
	irq_ops.irq_unmask = gic_unmask_irq;
}

void smp_cross_call(long cpu_mask, unsigned int irq)
{
	unsigned long flags;
	int cpu = smp_processor_id();

	flags = spin_lock_irqsave(&per_cpu(intc_lock, cpu));

	/*
	 * Ensure that stores to Normal memory are visible to the
	 * other CPUs before they observe us issuing the IPI.
	 */
	smp_mb();

	/* This always happens on GIC0 */
	writei((cpu_mask << 16) | irq, gicd_base + GIC_DIST_SOFTINT);

	spin_unlock_irqrestore(&per_cpu(intc_lock, cpu), flags);
}

void init_gic(void) {
	gic_init((addr_t *) io_map(GIC_DIST_PHYS, GIC_DIST_SIZE),
		 (addr_t *) io_map(GIC_CPU_PHYS, GIC_CPU_SIZE),
		 (addr_t *) io_map(GIC_HYP_PHYS, GIC_HYP_SIZE));

}
