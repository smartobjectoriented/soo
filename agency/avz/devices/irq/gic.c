/*
 *  Copyright (C) 2002 ARM Limited, All Rights Reserved.
 *  Copyright (C) 2016,2022 Daniel Rossier <daniel.rossier@heig-vd.ch>
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
 *
 * Some part of code related to GIC virtualization is borrowed fro
 * the Jailhouse project.
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

#define MAX_PENDING_IRQS	256

struct pending_irqs {
	/* synchronizes parallel insertions of SGIs into the pending ring */
	spinlock_t lock;

	u16 irqs[MAX_PENDING_IRQS];

	/* contains the calling CPU ID in case of a SGI */
	unsigned int head;

	/* removal from the ring happens lockless, thstatic void gic_set_active_irq(unsigned int irq)
{
	u32 mask = 1 << (irq % 32);
	int cpu = smp_processor_id();

	spin_lock(&per_cpu(intc_lock, cpu));
	writei(mask, gicd_base + GIC_DIST_ACTIVE_SET + (irq / 32) * 4);
	spin_unlock(&per_cpu(intc_lock, cpu));
}
	 * us tail is volatile */
	volatile unsigned int tail;
};

struct pending_irqs pending_irqs;

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

void gicc_init(void)
{
	unsigned int cpu = smp_processor_id();
	u32 bypass = 0;
	int i;

	spin_lock_init(&per_cpu(intc_lock, cpu));

	/*
	 * Deal with the banked PPI and SGI interrupts - disable all
	 * PPI interrupts, ensure all SGI interrupts are enabled.
	 */
	writei(GICD_INT_EN_CLR_PPI, gicd_base + GIC_DIST_ENABLE_CLEAR);
	writei(GICD_INT_EN_SET_SGI, gicd_base + GIC_DIST_ENABLE_SET);

	/*
	 * Set priority on PPI and SGI interrupts
	 */
	for (i = 0; i < 32; i += 4)
		writei(GICD_INT_DEF_PRI_X4, gicd_base + GIC_DIST_PRI + i * 4 / 4);

	writei(GICC_INT_PRI_THRESHOLD, gicc_base + GIC_CPU_PRIMASK);

	/*
	 * Preserve bypass disable bits to be written back later
	 */
	bypass = readi(gicc_base + GIC_CPU_CTRL);
	bypass &= GICC_DIS_BYPASS_MASK;

	writei(bypass | GICC_ENABLE | GIC_CPU_EOI, gicc_base + GIC_CPU_CTRL);
}

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

static int gic_inject_irq(u16 irq_id)
{
	unsigned int n;
	int first_free = -1;
	u32 lr;
	unsigned long elsr[2];

	elsr[0] = readi(gich_base + GICH_ELSR0);
	elsr[1] = readi(gich_base + GICH_ELSR1);

	for (n = 0; n < gic_num_lr; n++) {
		if (test_bit(n, elsr)) {
			/* Entry is available */
			if (first_free == -1)
				first_free = n;
			continue;
		}

		/* Check that there is no overlapping */
		lr = gic_read_lr(n);
		if ((lr & GICH_LR_VIRT_ID_MASK) == irq_id)
			return -EEXIST;
	}

	if (first_free == -1)
		return -EBUSY;

	/* Inject group 0 interrupt (seen as IRQ by the guest) */
	lr = irq_id;
	lr |= GICH_LR_PENDING_BIT;

	lr |= GICH_LR_HW_BIT;
	lr |= (u32)irq_id << GICH_LR_PHYS_ID_SHIFT;

	gic_write_lr(first_free, lr);

	return 0;
}

void gic_inject_pending(void)
{
	u16 irq_id;

	while (pending_irqs.head != pending_irqs.tail) {
		irq_id = pending_irqs.irqs[pending_irqs.head];

		if (gic_inject_irq(irq_id) == -EBUSY) {
			/*
			 * The list registers are full, trigger maintenance
			 * interrupt and leave.
			 */
			gic_enable_maint_irq(true);
			return;
		}

		/*
		 * Ensure that the entry was read before updating the head
		 * index.
		 */
		dmb(ish);

		pending_irqs.head = (pending_irqs.head + 1) % MAX_PENDING_IRQS;
	}

	/*
	 * The software interrupt queue is empty - turn off the maintenance
	 * interrupt.
	 */
	gic_enable_maint_irq(false);
}

void gic_set_pending(u16 irq_id)
{
	unsigned int new_tail;

	if (gic_inject_irq(irq_id) != -EBUSY)
		return;

	spin_lock(&pending_irqs.lock);

	new_tail = (pending_irqs.tail + 1) % MAX_PENDING_IRQS;

	/* Queue space available? */
	if (new_tail != pending_irqs.head) {
		pending_irqs.irqs[pending_irqs.tail] = irq_id;

		/*
		 * Make the entry content is visible before updating the tail
		 * index.
		 */
		dmb(ish);

		pending_irqs.tail = new_tail;
	}

	/*
	 * The unlock has memory barrier semantic on ARM v7 and v8. Therefore
	 * the change to tail will be visible when sending SGI_INJECT later on.
	 */
	spin_unlock(&pending_irqs.lock);

	/*
	 * The list registers are full, trigger maintenance interrupt if we are
	 * on the target CPU. In the other case, send SGI_INJECT to the target
	 * CPU.
	 */

	gic_enable_maint_irq(true);
}

static void gic_eoi_irq(u32 irq_id, bool deactivate)
{
	/*
	 * The GIC doesn't seem to care about the CPUID value written to EOIR,
	 * which is rather convenient...
	 */
	writei(irq_id, gicc_base + GICC_EOIR);
	if (deactivate)
		writei(irq_id, gicc_base + GICC_DIR);
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

static void gic_handle(cpu_regs_t *cpu_regs) {
	u32 irqstat, irqnr;

	ASSERT(local_irq_is_disabled());

	while (true) {
		irqstat = readi(gicc_base + GIC_CPU_INTACK);
		irqnr = irqstat & GICC_IAR_INT_ID_MASK;

		smp_mb();

		if (irqnr == 1023) {
			if (smp_processor_id() == 0)
				printk("## OUT irq via 1023\n");
			return ;
		}

		/* SPIs 32-irqmax */
		if (likely((irqnr > 31) && (irqnr < 1021))) {
			printk("## GIC failure: unexpected IRQ : %d\n", irqnr);
			BUG();
		}

		if (irqnr < 16) {
			gic_eoi_irq(irqnr, false);
			handle_IPI(irqnr);
		} else {

			if (irqnr == IRQ_ARCH_ARM_TIMER_EL2) {
				asm_do_IRQ(irqnr);
				gic_eoi_irq(irqnr, false);
			} else {

				if (irqnr == IRQ_ARCH_ARM_MAINT) {
					gic_inject_pending();
					gic_eoi_irq(irqnr, true);
					continue;
				} else {
					gic_set_pending(irqnr);
					gic_eoi_irq(irqnr, false);
				}
			}
		}
	}
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

void gic_cpu_init(void) {
	u32 vtr, vmcr;
	u32 gicc_ctlr, gicc_pmr;
	u32 gicd_isacter;
	unsigned int n;

	/* Ensure all IPIs and the maintenance PPI are enabled. */
	writei(0x0000ffff | (1 << IRQ_ARCH_ARM_MAINT), gicd_base + GIC_DIST_ENABLE_SET);

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
	 *   interrupts.
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

	/* Deactivate all active SGIs */
	gicd_isacter = readi(gicd_base + GIC_DIST_ACTIVE_SET);
	writei(gicd_isacter & 0xffff, gicd_base + GIC_DIST_ACTIVE_SET);

	/*
	 * Forward any pending physical SGIs to the virtual queue.
	 * We will convert them into self-inject SGIs, ignoring the original
	 * source. But Linux doesn't care about that anyway.
	 */
	for (n = 0; n < 16; n++) {
		if (readb(gicd_base + GIC_DIST_SGI_PENDING_CLEAR + n)) {
			writeb(0xff, gicd_base + GIC_DIST_SGI_PENDING_CLEAR + n);
			gic_set_pending(n);
		}
	}

}

void gic_init(addr_t *dist_base, addr_t *cpu_base, addr_t *hyp_base)
{
	pending_irqs.head = 0;
	pending_irqs.tail = 0;

	gicd_base = dist_base;
	gicc_base = cpu_base;
	gich_base = hyp_base;

	gic_clear_pending_irqs();

	/* Ensure all IPIs and the maintenance PPI are enabled */
	writei(0x0000ffff | (1 << IRQ_ARCH_ARM_MAINT), gicd_base + GIC_DIST_ENABLE_SET);

	/* Disable PPIs, except for the maintenance interrupt. */
	writei(0xffff0000 & ~(1 << IRQ_ARCH_ARM_MAINT), gicd_base + GIC_DIST_ENABLE_SET);

	/* Deactivate all active PPIs */
	writei(0xffff0000, gicd_base + GIC_DIST_ACTIVE_CLEAR);

	writei(0, gich_base + GICH_VMCR);

	gic_cpu_init();

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
	spin_lock_init(&pending_irqs.lock);

	gic_init((addr_t *) io_map(GIC_DIST_PHYS, GIC_DIST_SIZE),
		 (addr_t *) io_map(GIC_CPU_PHYS, GIC_CPU_SIZE),
		 (addr_t *) io_map(GIC_HYP_PHYS, GIC_HYP_SIZE));

}
