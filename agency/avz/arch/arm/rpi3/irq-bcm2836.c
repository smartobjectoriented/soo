/*
 * Root interrupt controller for the BCM2836 (Raspberry Pi 2).
 *
 * Copyright 2015 Broadcom
 * Copyright (C) 2018 Daniel Rossier <daniel.rossier@soo.tech>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define LOCAL_CONTROL			0x000
#define LOCAL_PRESCALER			0x008

#include <avz/cpumask.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/smp.h>

#include <mach/bcm2835.h>

#include <asm/hardware/arm_timer.h>


DEFINE_PER_CPU(spinlock_t, intc_lock);

/*
 * The low 4 bits of this are the CPU's timer IRQ enables, and the
 * next 4 bits are the CPU's timer FIQ enables (which override the IRQ
 * bits).
 */
#define LOCAL_TIMER_INT_CONTROL0	0x040

/*
 * The CPU's interrupt status register.  Bits are defined by the the
 * LOCAL_IRQ_* bits below.
 */
#define LOCAL_IRQ_PENDING0		0x060
/* Same status bits as above, but for FIQ. */
#define LOCAL_FIQ_PENDING0		0x070

#define LOCAL_IRQ_CNTPSIRQ	0
#define LOCAL_IRQ_CNTPNSIRQ	1
#define LOCAL_IRQ_CNTHPIRQ	2
#define LOCAL_IRQ_CNTVIRQ	3
#define LOCAL_IRQ_MAILBOX0	4
#define LOCAL_IRQ_MAILBOX1	5
#define LOCAL_IRQ_MAILBOX2	6
#define LOCAL_IRQ_MAILBOX3	7
#define LOCAL_IRQ_GPU_FAST	8
#define LOCAL_IRQ_PMU_FAST	9
#define LAST_IRQ		LOCAL_IRQ_PMU_FAST

struct bcm2836_arm_irqchip_intc intc;

void bcm2836_arm_irqchip_mask_per_cpu_irq(unsigned int reg_offset, unsigned int bit, int cpu)
{
	void __iomem *reg = intc.base + reg_offset + 4 * cpu;

	writel(readl(reg) & ~BIT(bit), reg);
}

void bcm2836_arm_irqchip_unmask_per_cpu_irq(unsigned int reg_offset, unsigned int bit, int cpu)
{
	void __iomem *reg = intc.base + reg_offset + 4 * cpu;

	writel(readl(reg) | BIT(bit), reg);
}

static void bcm2836_arm_irqchip_mask_timer_irq(unsigned int irq)
{
	bcm2836_arm_irqchip_mask_per_cpu_irq(LOCAL_TIMER_INT_CONTROL0, irq - LOCAL_IRQ_CNTPSIRQ, smp_processor_id());
}

static void bcm2836_arm_irqchip_unmask_timer_irq(unsigned int irq)
{
	bcm2836_arm_irqchip_unmask_per_cpu_irq(LOCAL_TIMER_INT_CONTROL0,irq - LOCAL_IRQ_CNTPSIRQ, smp_processor_id());
}

static struct irqchip bcm2836_arm_irqchip_timer = {
	.name	= "bcm2836-timer",
	.mask	= bcm2836_arm_irqchip_mask_timer_irq,
	.unmask	= bcm2836_arm_irqchip_unmask_timer_irq,
};

void unmask_timer_irq(void) {
	bcm2836_arm_irqchip_unmask_timer_irq(LOCAL_IRQ_CNTVIRQ);
}

static void bcm2836_arm_irqchip_register_irq(int hwirq, struct irqchip *chip)
{
	set_irq_chip(hwirq, chip);

	set_irq_handler(hwirq, handle_fasteoi_irq);
	set_irq_flags(hwirq, IRQF_VALID | IRQF_NOAUTOEN);

}

void ll_entry_irq(void)
{
	int cpu = smp_processor_id();
	u32 stat;

	stat = readl(intc.base + LOCAL_IRQ_PENDING0 + 4 * cpu);
	if (stat & BIT(LOCAL_IRQ_MAILBOX0)) {
		void *mailbox0 = (intc.base + LOCAL_MAILBOX0_CLR0 + 16 * cpu);
		u32 mbox_val = readl(mailbox0);
		u32 ipi = ffs(mbox_val) - 1;

		writel(1 << ipi, mailbox0);
		handle_IPI(ipi);

	} else if (stat) {

		u32 hwirq = ffs(stat) - 1;

		if (hwirq == LOCAL_IRQ_CNTVIRQ)
			asm_do_IRQ(hwirq);
		else
			BUG();
	}
}


void bcm2836_arm_irqchip_send_ipi(const struct cpumask *mask, unsigned int ipi)
{
	int cpu;
	void __iomem *mailbox0_base = intc.base + LOCAL_MAILBOX0_SET0;

	/*
	 * Ensure that stores to normal memory are visible to the
	 * other CPUs before issuing the IPI.
	 */
	dmb(ishst);

	for_each_cpu_mask(cpu, *mask) {
		writel(1 << ipi, mailbox0_base + 16 * cpu);
	}
}

/*
 * The LOCAL_IRQ_CNT* timer firings are based off of the external
 * oscillator with some scaling.  The firmware sets up CNTFRQ to
 * report 19.2Mhz, but doesn't set up the scaling registers.
 */
static void bcm2835_init_local_timer_frequency(void)
{
	/*
	 * Set the timer to source from the 19.2Mhz crystal clock (bit
	 * 8 unset), and only increment by 1 instead of 2 (bit 9
	 * unset).
	 */
	writel(0, intc.base + LOCAL_CONTROL);

	/*
	 * Set the timer prescaler to 1:1 (timer freq = input freq *
	 * 2**31 / prescaler)
	 */
	writel(0x80000000, intc.base + LOCAL_PRESCALER);
}

int bcm2836_arm_irqchip_l1_intc_init(void *base)
{
	intc.base = base;
	bcm2835_init_local_timer_frequency();

	bcm2836_arm_irqchip_register_irq(LOCAL_IRQ_CNTPSIRQ, &bcm2836_arm_irqchip_timer);
	bcm2836_arm_irqchip_register_irq(LOCAL_IRQ_CNTPNSIRQ, &bcm2836_arm_irqchip_timer);
	bcm2836_arm_irqchip_register_irq(LOCAL_IRQ_CNTHPIRQ, &bcm2836_arm_irqchip_timer);
	bcm2836_arm_irqchip_register_irq(LOCAL_IRQ_CNTVIRQ, &bcm2836_arm_irqchip_timer);

	return 0;
}


