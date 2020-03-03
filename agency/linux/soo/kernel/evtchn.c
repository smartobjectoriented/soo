
/*
 * Copyright (C) 2014-2016 Daniel Rossier <daniel.rossier@heig-vd.ch>
 * Copyright (C) 2016 Baptiste Delporte <bonel@bonel.net>
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

#if 0
#define DEBUG
#endif

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/irqnr.h>
#include <linux/irqdomain.h>
#include <linux/sched.h>
#include <linux/kernel_stat.h>
#include <linux/random.h>
#include <linux/bootmem.h>
#include <linux/kthread.h>
#include <linux/ipipe_base.h>

#include <asm/atomic.h>
#include <asm/ptrace.h>
#include <asm/irqflags.h>

#include <asm-generic/irq_regs.h>
#ifdef CONFIG_IPIPE
#include <asm/ipipe.h>
#include <soo/uapi/soo.h>
#endif

#include <soo/hypervisor.h>
#include <soo/evtchn.h>

#include <soo/uapi/event_channel.h>
#include <soo/uapi/console.h>

#include <soo/uapi/debug.h>
#include <soo/debug/dbgvar.h>

#include <soo/uapi/physdev.h>

/* Convenient shorthand for packed representation of an unbound IRQ. */
#define IRQ_UNBOUND	mk_virq_info(IRQT_UNBOUND, 0, 0)


/*
 * This lock protects updates to the following mapping and reference-count
 * arrays. The lock does not need to be acquired to read the mapping tables.
 */
static DEFINE_SPINLOCK(virq_mapping_update_lock);

typedef struct {
	int evtchn_to_virq[NR_EVTCHN];	/* evtchn -> IRQ */
	u32 virq_to_evtchn[NR_VIRQS];	/* IRQ -> evtchn */
	bool valid[NR_EVTCHN]; /* Indicate if the event channel can be used for notification for example */
	unsigned long evtchn_mask[NR_EVTCHN/32];
	int virq_bindcount[NR_VIRQS];
} evtchn_info_t;

static DEFINE_PER_CPU(evtchn_info_t, evtchn_info);

DEFINE_PER_CPU(bool, in_upcall_progress);

/*
 * Accessors for packed IRQ information.
 */

/*
 * Get the evtchn from a irq_data structure which contains a (unique) CPU bound to the IRQ processing.
 */
inline unsigned int evtchn_from_virq_and_cpu(unsigned int virq, unsigned int cpu) {
	return per_cpu(evtchn_info, cpu).virq_to_evtchn[virq];
}

inline unsigned int evtchn_from_virq(int virq)
{
	return evtchn_from_virq_and_cpu(virq, smp_processor_id());
}

inline unsigned int evtchn_from_irq_data(struct irq_data *irq_data)
{
	return evtchn_from_virq(irq_data->irq - VIRQ_BASE);
}

/* __evtchn_from_irq : helpful for irq_chip handlers */
#define __evtchn_from_virq   evtchn_from_irq_data

static inline unsigned evtchn_is_masked(unsigned int b) {
	return test_bit(b, per_cpu(evtchn_info, smp_processor_id()).evtchn_mask);
}

void dump_evtchn_pending(void) {

	int i;
	unsigned char *ptr_pending, *ptr_mask;
	volatile shared_info_t *s = avz_shared_info;

#if 0
	lprintk("   Evtchn info in Agency/ME domain %d\n\n", ME_domID());
	for (i = 0; i < NR_EVTCHN; i++) {

		lprintk("e:%d m:%d p:%d  ", i, test_bit(i, per_cpu(evtchn_info, smp_processor_id()).evtchn_mask),
					       test_bit(i, s->evtchn_pending));
	}
	lprintk("\n\n");
#endif

	ptr_pending = (unsigned char *) &s->evtchn_pending;
	ptr_mask = (unsigned char *) &per_cpu(evtchn_info, smp_processor_id()).evtchn_mask;

	lprintk("%s: evtchn pending: ", __func__);
	for (i = 0; i < 32; i++) {
		lprintk("%.2lx ", (unsigned long) *ptr_pending);
		ptr_pending++;
	}
	lprintk("\n");

	lprintk("%s: evtchn mask   : ", __func__);
	for (i = 0; i < 32; i++) {
		lprintk("%.2lx ", (unsigned long) *ptr_mask);
		ptr_mask++;
	}
	lprintk("\n");

}

/* NB. Interrupts are disabled on entry. */
extern asmlinkage void asm_do_IRQ(unsigned int virq, struct pt_regs *regs);
static struct irq_chip virtirq_chip;

/*
 * evtchn_do_upcall
 *
 * This is the main entry point for processing IRQs and VIRQs for this domain.
 *
 * The function runs with IRQs OFF during the whole execution of the function. As such, this function is executed in top half processing of the IRQ.
 * - All pending event channels are processed one after the other, according to their bit position within the bitmap.
 * - Checking pending IRQs in AVZ (low-level IRQ) is performed at the end of the loop so that we can react immediately if some new IRQs have been generated
 *   and are present in the GIC.
 *
 *  Ipipe behaviour with interrupts.
 * -> if ipipe_request() is called (for example from rtdm_request_irq(), for a PIRQ, it is OK, for a VIRQ, it is necessary to install a ack function of level type (ipipe_level_handler -> see timer)
 *
 * -> For VIRQ_TIMER_IRQ, avoid change the bind_virq_to_irqhandler.....
 *
 */
asmlinkage void evtchn_do_upcall(struct pt_regs *regs)
{
	unsigned int   evtchn;
	int            l1, virq;
	volatile shared_info_t *s = avz_shared_info;
	volatile vcpu_info_t *vcpu_info = &s->vcpu_info;

	int loopmax = 0;
	int at_least_one_processed;

	BUG_ON(!hard_irqs_disabled());

	/* It may happen than an hypercall is executed during a top half ISR. Since the hypercall will
	 * call this function along its return path.
	 */
	if (per_cpu(in_upcall_progress, smp_processor_id()))
		return ;

	/* Check if the (local) IRQs are off. In this case, pending events are not processed at this time,
	 * but will be once the local IRQs will be re-enabled (either by the GIC loop or an active assert
	 * of the IRQ line).
	 */

	if (irqs_disabled_flags(regs->uregs[16])) /* uregs[16] matches with SPSR */
		return ;

	per_cpu(in_upcall_progress, smp_processor_id()) = true;

retry:

	l1 = xchg(&vcpu_info->evtchn_upcall_pending, 0);

	evtchn = find_first_bit((void *) &s->evtchn_pending, NR_EVTCHN);

	do {
		at_least_one_processed = 0; /* If all interrupts are masked, we avoid to loop at infinity */

		while (evtchn < NR_EVTCHN) {

			BUG_ON(!per_cpu(evtchn_info, smp_processor_id()).valid[evtchn]);

			loopmax++;

			if (loopmax > 500)   /* Probably something wrong ;-) */
				lprintk("%s: Warning trying to process evtchn: %d IRQ: %d for quite a long time (dom ID: %d) on CPU %d / masked: %d...\n",
						__func__, evtchn, per_cpu(evtchn_info, smp_processor_id()).evtchn_to_virq[evtchn], ME_domID(), smp_processor_id(), evtchn_is_masked(evtchn));

			if (!evtchn_is_masked(evtchn)) {

				at_least_one_processed = 1;

				virq = per_cpu(evtchn_info, smp_processor_id()).evtchn_to_virq[evtchn];

				clear_evtchn(evtchn_from_virq(virq));

				asm_do_IRQ(VIRQ_BASE + virq, regs);

				BUG_ON(!hard_irqs_disabled());

			}
			evtchn = find_next_bit((void *) &s->evtchn_pending, NR_EVTCHN, evtchn+1);
		}

	} while (at_least_one_processed);

	if (vcpu_info->evtchn_upcall_pending)
		goto retry;

	per_cpu(in_upcall_progress, smp_processor_id()) = false;
}

static int find_unbound_virq(int cpu)
{
	int virq;

	for (virq = 0; virq < NR_VIRQS; virq++)
		if (per_cpu(evtchn_info, cpu).virq_bindcount[virq] == 0)
			break;

	if (virq == NR_VIRQS)
		panic("No available IRQ to bind to: increase NR_IRQS!\n");

	return virq;
}

bool in_upcall_process(void) {
	return per_cpu(in_upcall_progress, smp_processor_id());
}

static int bind_evtchn_to_virq(unsigned int evtchn)
{
	int virq;
	int cpu = smp_processor_id();

	spin_lock(&virq_mapping_update_lock);

	if ((virq = per_cpu(evtchn_info, cpu).evtchn_to_virq[evtchn]) == -1) {
		virq = find_unbound_virq(cpu);
		per_cpu(evtchn_info, cpu).evtchn_to_virq[evtchn] = virq;
		per_cpu(evtchn_info, cpu).virq_to_evtchn[virq] = evtchn;
		per_cpu(evtchn_info, cpu).valid[evtchn] = true;

	}

	per_cpu(evtchn_info, cpu).virq_bindcount[virq]++;

	spin_unlock(&virq_mapping_update_lock);

	return virq;
}

int unbind_domain_evtchn(unsigned int domID, unsigned int evtchn)
{
	struct evtchn_bind_interdomain bind_interdomain;
	int err;

	bind_interdomain.remote_dom = domID;
	bind_interdomain.local_evtchn = evtchn;

	err = hypercall_trampoline(__HYPERVISOR_event_channel_op, EVTCHNOP_unbind_domain, (long) &bind_interdomain, 0, 0);

	evtchn_info.valid[evtchn] = false;

	return err;
}

static int bind_interdomain_evtchn_to_virq(unsigned int remote_domain, unsigned int remote_evtchn)
{
	struct evtchn_bind_interdomain bind_interdomain;
	int err;

	bind_interdomain.remote_dom  = remote_domain;
	bind_interdomain.remote_evtchn = remote_evtchn;

	err = hypercall_trampoline(__HYPERVISOR_event_channel_op, EVTCHNOP_bind_interdomain, (long) &bind_interdomain, 0, 0);

	return err ? : bind_evtchn_to_virq(bind_interdomain.local_evtchn);
}

int bind_existing_interdomain_evtchn(unsigned local_evtchn, unsigned int remote_domain, unsigned int remote_evtchn)
{
	struct evtchn_bind_interdomain bind_interdomain;
	int err;

	bind_interdomain.local_evtchn = local_evtchn;
	bind_interdomain.remote_dom  = remote_domain;
	bind_interdomain.remote_evtchn = remote_evtchn;

	err = hypercall_trampoline(__HYPERVISOR_event_channel_op, EVTCHNOP_bind_existing_interdomain, (long) &bind_interdomain, 0, 0);

	return err ? : bind_evtchn_to_virq(bind_interdomain.local_evtchn);
}

static void unbind_from_virq(unsigned int virq)
{
	evtchn_close_t op;
	int evtchn = evtchn_from_virq(virq);
	int cpu = smp_processor_id();
	int ret;

	spin_lock(&virq_mapping_update_lock);

	if (--per_cpu(evtchn_info, cpu).virq_bindcount[virq] == 0) {
		op.evtchn = evtchn;
		ret = hypercall_trampoline(__HYPERVISOR_event_channel_op, EVTCHNOP_close, (long) &op, 0, 0);
		BUG_ON(ret < 0);

		per_cpu(evtchn_info, cpu).evtchn_to_virq[evtchn] = -1;
		per_cpu(evtchn_info, cpu).valid[evtchn] = false;
	}

	spin_unlock(&virq_mapping_update_lock);
}


int bind_evtchn_to_virq_handler(unsigned int evtchn, irq_handler_t handler, irq_handler_t thread_fn, unsigned long irqflags, const char *devname, void *dev_id)
{
	unsigned int virq;
	int retval;

	virq = bind_evtchn_to_virq(evtchn);
	retval = request_threaded_irq(VIRQ_BASE + virq, handler, thread_fn, irqflags, devname, dev_id);
	if (retval != 0) {
		unbind_from_virq(virq);
		return retval;
	}

	return virq;
}

int bind_interdomain_evtchn_to_virqhandler(unsigned int remote_domain, unsigned int remote_evtchn, irq_handler_t handler, irq_handler_t thread_fn, unsigned long irqflags, const char *devname, void *dev_id)
{
	int virq, retval;

	DBG("%s: devname = %s / remote evtchn: %d remote domain: %d\n", __func__, devname, remote_evtchn, remote_domain);
	virq = bind_interdomain_evtchn_to_virq(remote_domain, remote_evtchn);
	if (virq < 0)
		return virq;

	if (handler != NULL) {
		retval = request_threaded_irq(VIRQ_BASE + virq, handler, thread_fn, irqflags, devname, dev_id);
		if (retval != 0)
			BUG();
	}

	return virq;
}

int rtdm_bind_evtchn_to_virq_handler(rtdm_irq_t *irq_handle, unsigned int evtchn, rtdm_irq_handler_t handler, unsigned long irqflags, const char *devname, void *dev_id)
{
	unsigned int virq;
	int retval;

	virq = bind_evtchn_to_virq(evtchn);

	/* First, we bind our specific chip to this irq */
	irqdescs[VIRQ_BASE + virq].irq_data.chip = &virtirq_chip;

	retval = rtdm_irq_request(irq_handle, VIRQ_BASE + virq, handler, irqflags, devname, dev_id);

	if (retval != 0)
		BUG();

	return virq;
}

int rtdm_bind_interdomain_evtchn_to_virqhandler(rtdm_irq_t *irq_handle, unsigned int remote_domain, unsigned int remote_evtchn, rtdm_irq_handler_t handler, unsigned long irqflags, const char *devname, void *dev_id)
{
	int virq, retval;

	DBG("%s: devname = %s / remote evtchn: %d remote domain: %d\n", __func__, devname, remote_evtchn, remote_domain);
	virq = bind_interdomain_evtchn_to_virq(remote_domain, remote_evtchn);
	if (virq < 0)
		BUG();

	/* First, we bind our specific chip to this irq */
	irqdescs[VIRQ_BASE + virq].irq_data.chip = &virtirq_chip;

	retval = rtdm_irq_request(irq_handle, VIRQ_BASE + virq, handler, irqflags, devname, dev_id);

	if (retval != 0)
		BUG();

	return virq;
}

void rtdm_unbind_from_virqhandler(rtdm_irq_t *irq_handle)
{
	rtdm_irq_free(irq_handle);
	unbind_from_virq(irq_handle->irq - VIRQ_BASE);
}

void unbind_from_virqhandler(unsigned int virq, void *dev_id)
{
	free_irq(VIRQ_BASE + virq, dev_id);
	unbind_from_virq(virq);
}

/*
 * Interface to generic handling in irq.c
 */
static unsigned int startup_virtirq(struct irq_data *irq)
{
	int evtchn = evtchn_from_irq_data(irq);

	unmask_evtchn(evtchn);
	return 0;
}

static void shutdown_virtirq(struct irq_data *irq)
{
	int evtchn = evtchn_from_irq_data(irq);

	mask_evtchn(evtchn);
}

static void enable_virtirq(struct irq_data *irq)
{
	int evtchn = evtchn_from_irq_data(irq);

	unmask_evtchn(evtchn);
}

static void disable_virtirq(struct irq_data *irq)
{
	int evtchn = evtchn_from_irq_data(irq);

	mask_evtchn(evtchn);
}

static struct irq_chip virtirq_chip = {
		.name             = "avz_virt-irq",
		.irq_startup      = startup_virtirq,
		.irq_shutdown     = shutdown_virtirq,
		.irq_enable       = enable_virtirq,
		.irq_disable      = disable_virtirq,
		.irq_unmask       = enable_virtirq,
		.irq_mask         = disable_virtirq,
};


void notify_remote_via_virq(int virq)
{
	int evtchn = evtchn_from_virq(virq);

	notify_remote_via_evtchn(evtchn);
}

void mask_evtchn(int evtchn)
{
	transaction_set_bit(evtchn, &per_cpu(evtchn_info, smp_processor_id()).evtchn_mask[0]);
}

void unmask_evtchn(int evtchn)
{
	transaction_clear_bit(evtchn, &per_cpu(evtchn_info, smp_processor_id()).evtchn_mask[0]);
}

void virtshare_mask_irq(struct irq_data *irq_data) {
	int evtchn;

	if (cpumask_test_cpu(AGENCY_RT_CPU, irq_data->common->affinity))
	{
		evtchn = evtchn_from_virq_and_cpu(irq_data->irq - VIRQ_BASE, AGENCY_RT_CPU);
		transaction_set_bit(evtchn, &per_cpu(evtchn_info, AGENCY_RT_CPU).evtchn_mask[0]);
	} else {
		evtchn = evtchn_from_virq_and_cpu(irq_data->irq - VIRQ_BASE, AGENCY_CPU);
		transaction_set_bit(evtchn, &per_cpu(evtchn_info, AGENCY_CPU).evtchn_mask[0]);
	}

}

void virtshare_unmask_irq(struct irq_data *irq_data) {
	int evtchn;

	if (cpumask_test_cpu(AGENCY_RT_CPU, irq_data->common->affinity))
	{
		evtchn = evtchn_from_virq_and_cpu(irq_data->irq, AGENCY_RT_CPU);
		transaction_clear_bit(evtchn, &per_cpu(evtchn_info, AGENCY_RT_CPU).evtchn_mask[0]);
	} else {
		evtchn = evtchn_from_virq_and_cpu(irq_data->irq, AGENCY_CPU);
		transaction_clear_bit(evtchn, &per_cpu(evtchn_info, AGENCY_CPU).evtchn_mask[0]);
	}
}

void virq_init(void)
{
	struct irq_desc *irqdesc; /* Linux IRQ descriptor */

	int i, cpu;
	cpumask_t cpumask = CPU_MASK_ALL;

	/*
	 * For each CPU, initialize event channels for all IRQs.
	 * An IRQ will processed by only one CPU, but it may be rebound to another CPU as well.
	 */

	for_each_cpu(cpu, &cpumask) {

		per_cpu(in_upcall_progress, cpu) = false;

		/* No event-channel -> IRQ mappings. */
		for (i = 0; i < NR_EVTCHN; i++) {
			per_cpu(evtchn_info, cpu).evtchn_to_virq[i] = -1;
			per_cpu(evtchn_info, cpu).valid[i] = false;

			transaction_set_bit(i, &per_cpu(evtchn_info, cpu).evtchn_mask[0]);
		}

#ifdef CONFIG_SPARSE_IRQ
		irq_alloc_descs(VIRQ_BASE, VIRQ_BASE, NR_VIRQS, numa_node_id());
#endif

		/* Dynamic IRQ space is currently unbound. Zero the refcnts. */
		for (i = 0; i < NR_VIRQS; i++)
			per_cpu(evtchn_info, cpu).virq_bindcount[i] = 0;
	}

	/* Configure the irqdesc associated to VIRQs */
	for (i = 0; i < NR_VIRQS; i++) {
		irqdesc = irq_to_desc(VIRQ_BASE + i);

		irqdesc->irq_data.common->state_use_accessors |= IRQD_IRQ_DISABLED;
		irqdesc->status_use_accessors &= ~IRQ_NOREQUEST;

		irqdesc->action = NULL;
		irqdesc->depth = 1;
		irqdesc->irq_data.chip = &virtirq_chip;

		irq_set_handler(VIRQ_BASE + i, handle_level_irq);
	}
}
