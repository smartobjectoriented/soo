/* -*- linux-c -*-
 * linux/kernel/ipipe/core.c
 *
 * Copyright (C) 2002-2012 Philippe Gerum.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
 * USA; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Architecture-independent I-PIPE core support.
 */

#if 0
#define DEBUG
#endif

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kallsyms.h>
#include <linux/bitops.h>
#include <linux/tick.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#endif	/* CONFIG_PROC_FS */
#include <linux/ipipe_trace.h>
#include <linux/ipipe.h>
#include <linux/ipipe_tickdev.h>

#include <ipipe/setup.h>

#include <asm/cacheflush.h>

#include <asm/arch_timer.h>

#include <soo/uapi/soo.h>
#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>
#include <soo/evtchn.h>

/* Will be initialized in the local intc init function. */
void (*ipipe_assign_chip)(ipipe_irqdesc_t *irqdesc) = NULL;

ipipe_irqdesc_t irqdescs[NR_PIRQS + NR_VIRQS];

ipipe_irqdesc_t *ipis_desc;

extern struct irq_chip bcm2835_gpio_irq_chip;
extern struct gpio_chip bcm2835_gpio_chip;

DEFINE_PER_CPU(struct ipipe_percpu_data, ipipe_percpu) = {
	.hrtimer_irq = -1,
#ifdef CONFIG_IPIPE_DEBUG_CONTEXT
	.context_check = 1,
#endif
};
EXPORT_PER_CPU_SYMBOL(ipipe_percpu);

DEFINE_SPINLOCK(__ipipe_lock);

struct proc_dir_entry *ipipe_proc_root;

static int __ipipe_version_info_show(struct seq_file *p, void *data)
{
	seq_printf(p, "%d\n", IPIPE_CORE_RELEASE);
	return 0;
}

static int __ipipe_version_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, __ipipe_version_info_show, NULL);
}

static const struct file_operations __ipipe_version_proc_ops = {
	.open		= __ipipe_version_info_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __ipipe_common_info_show(struct seq_file *p, void *data)
{
	seq_printf(p, "        +--- Handled\n");
	seq_printf(p, "        |+-- Locked\n");
	seq_printf(p, "        ||+- Virtual\n");
	seq_printf(p, " [IRQ]  |||  Handler\n");

	return 0;
}

static int __ipipe_common_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, __ipipe_common_info_show, PDE_DATA(inode));
}

static const struct file_operations __ipipe_info_proc_ops = {
	.owner		= THIS_MODULE,
	.open		= __ipipe_common_info_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};


void  __ipipe_init_early(void)
{
	int i;

	/* Initialize all IRQ descriptors */
	for (i = 0; i < NR_PIRQS + NR_VIRQS; i++)
		memset(&irqdescs[i], 0, sizeof(ipipe_irqdesc_t));

	for (i = 16; i < NR_PIRQS + NR_VIRQS; i++) {
		irqdescs[i].irq = i;
		raw_spin_lock_init(&irqdescs[i].lock);
		ipipe_assign_chip(&irqdescs[i]);
	}

	/* irqdesc used to manage ipis */
	ipis_desc = kzalloc(sizeof(*ipis_desc), GFP_KERNEL);
	BUG_ON(!ipis_desc);

	ipipe_assign_chip(ipis_desc);
}

/*
 * Since the ARM timer IRQ number is found out from the device tree quite later
 * in the boot process during device initcalls, we have to postpone this initialization.
 */
void __ipipe_init_post(void) {
#ifndef CONFIG_X86
	irqdescs[__xntimer_rt->irq].irq = __xntimer_rt->irq;
	ipipe_assign_chip(&irqdescs[__xntimer_rt->irq]);
#endif
}

void ipipe_request_irq(unsigned int irq, ipipe_irq_handler_t handler, void *cookie)
{
	BUG_ON(irqdescs[irq].handler != NULL);

	irqdescs[irq].handler = handler;
	irqdescs[irq].data = cookie;
}


extern void kstat_incr_irq_this_cpu(unsigned int irq);
extern void rtdm_clear_irq(void);

/* SOO.tech */

/*
 * Main IRQ/IPI dispatch function - IRQs are disabled.
 *
 */
void __ipipe_dispatch_irq(unsigned int irq, bool reset) {
	struct irq_chip *chip;

	DBG("%s %d CPU %d irq: %d\n", __func__, __LINE__, smp_processor_id(), irq);

	BUG_ON(smp_processor_id() == 0);

	chip = irqdescs[irq].irq_data.chip;

	/* Perform the acknowledge (and mask) */
	if (chip)
		chip->irq_mask(&irqdescs[irq].irq_data);


	/* At the very beginning, a first IRQ might be sent to CPU #1 to awake it up.
	 * However, Xenomai/cobalt is not initialized yet.
	 */
	if (unlikely(!__cobalt_ready)) {

#ifndef CONFIG_X86
		if (irq == __xntimer_rt->irq)
			__ipipe_timer_handler(__xntimer_rt->irq, NULL);
#endif

		if (chip) {
			chip->irq_unmask(&irqdescs[irq].irq_data);
			chip->irq_eoi(&irqdescs[irq].irq_data);
		}
		return ;
	}

#warning Todo: to implement a mechanism to count the number of IRQ (timer, other phys, etc.)

	barrier();

	__xnintr_irq_handler(irq);

	BUG_ON(!hard_irqs_disabled());

	if (chip) {
		chip->irq_unmask(&irqdescs[irq].irq_data);
		if (chip->irq_ack)
			chip->irq_ack(&irqdescs[irq].irq_data);
	}

	/* An IPI does not have an eoi routine. */
	if (chip && (chip->irq_eoi)) /* There is no eoi function for VIRQ */
		chip->irq_eoi(&irqdescs[irq].irq_data);

	/* Now we are safe to process the tick handler leading to potential context switching. */
#ifndef CONFIG_X86
	if (irq == __xntimer_rt->irq)
		xnintr_core_clock_handler();
#endif

#ifdef CONFIG_ARCH_BCM2835
	if (reset)
		rtdm_clear_irq();
#endif
	/* Invoke the scheduler since the state could have been changed during the interrupt processing */
	/* Remark: if this function from evtchn_do_upcall() so that in_upcall_progress is true, the
	 * scheduler will not be activated here, but in irq_exit after the end of evtchn_do_upcall along
	 * the IPI handling termination (see smp.c).
	 */
	xnsched_run();

	return ;

}

ipipe_irqdesc_t *ipipe_irq_to_desc(unsigned int irq)
{
	return &irqdescs[irq];
}

struct irq_chip *ipipe_irq_desc_get_chip(ipipe_irqdesc_t *desc)
{
	return desc->irq_data.chip;
}

#define __ipipe_preempt_schedule_irq()	do { } while (0)



