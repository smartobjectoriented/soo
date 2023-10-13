/*
 * Copyright (C) 2001,2002,2003 Philippe Gerum <rpm@xenomai.org>.
 * Copyright (C) 2005,2006 Dmitry Adamushko <dmitry.adamushko@gmail.com>.
 * Copyright (C) 2007 Jan Kiszka <jan.kiszka@web.de>.
 *
 * Xenomai is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * Xenomai is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Xenomai; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
*/
#include <linux/mutex.h>
#include <linux/ipipe.h>
#include <linux/ipipe_base.h>
#include <linux/ipipe_tickdev.h>

#include <cobalt/kernel/sched.h>
#include <cobalt/kernel/intr.h>
#include <cobalt/kernel/stat.h>
#include <cobalt/kernel/clock.h>
#include <cobalt/kernel/assert.h>
#include <trace/events/cobalt-core.h>

#include <soo/uapi/console.h>

/**
 * @ingroup cobalt_core
 * @defgroup cobalt_core_irq Interrupt management
 * @{
 */
#define XNINTR_MAX_UNHANDLED	1000

static DEFINE_MUTEX(intrlock);

struct xnintr_vector {
#if defined(CONFIG_SMP) || XENO_DEBUG(LOCKING)
	DECLARE_XNLOCK(lock);
#endif /* CONFIG_SMP || XENO_DEBUG(LOCKING) */
} ____cacheline_aligned_in_smp;

#ifdef CONFIG_XENO_OPT_STATS
struct xnintr nktimer;	     /* Only for statistics */
static int xnintr_count = 1; /* Number of attached xnintr objects + nktimer */
static int xnintr_list_rev;  /* Modification counter of xnintr list */

/* Both functions update xnintr_list_rev at the very end.
 * This guarantees that module.c::stat_seq_open() won't get
 * an up-to-date xnintr_list_rev and old xnintr_count. */

static inline void stat_counter_inc(void)
{
	xnintr_count++;
	smp_mb();
	xnintr_list_rev++;
}

static inline void stat_counter_dec(void)
{
	xnintr_count--;
	smp_mb();
	xnintr_list_rev++;
}

static inline void sync_stat_references(struct xnintr *intr)
{
	struct xnirqstat *statp;
	struct xnsched *sched;
	int cpu;

	for_each_realtime_cpu(cpu) {
		sched = xnsched_struct(cpu);
		statp = per_cpu_ptr(intr->stats, cpu);
		/* Synchronize on all dangling references to go away. */
		while (sched->current_account == &statp->account)
			cpu_relax();
	}
}

static void clear_irqstats(struct xnintr *intr)
{
	struct xnirqstat *p;
	int cpu;

	for_each_realtime_cpu(cpu) {
		p = per_cpu_ptr(intr->stats, cpu);
		memset(p, 0, sizeof(*p));
	}
}

static inline void alloc_irqstats(struct xnintr *intr)
{
	intr->stats = alloc_percpu(struct xnirqstat);
	clear_irqstats(intr);
}

static inline void free_irqstats(struct xnintr *intr)
{
	free_percpu(intr->stats);
}

static inline void query_irqstats(struct xnintr *intr, int cpu,
				  struct xnintr_iterator *iterator)
{
	struct xnirqstat *statp;
	xnticks_t last_switch;

	statp = per_cpu_ptr(intr->stats, cpu);
	iterator->hits = xnstat_counter_get(&statp->hits);
	last_switch = xnsched_struct(cpu)->last_account_switch;
	iterator->exectime_period = statp->account.total;
	iterator->account_period = last_switch - statp->account.start;
	statp->sum.total += iterator->exectime_period;
	iterator->exectime_total = statp->sum.total;
	statp->account.total = 0;
	statp->account.start = last_switch;
}

static void inc_irqstats(struct xnintr *intr, struct xnsched *sched, xnticks_t start)
{
	struct xnirqstat *statp;

	statp = raw_cpu_ptr(intr->stats);
	xnstat_counter_inc(&statp->hits);
	xnstat_exectime_lazy_switch(sched, &statp->account, start);
}

static inline void switch_irqstats(struct xnintr *intr, struct xnsched *sched)
{
	struct xnirqstat *statp;

	statp = raw_cpu_ptr(intr->stats);
	xnstat_exectime_switch(sched, &statp->account);
}

static inline xnstat_exectime_t *switch_core_irqstats(struct xnsched *sched)
{
	struct xnirqstat *statp;
	xnstat_exectime_t *prev;

	statp = xnstat_percpu_data;
	prev = xnstat_exectime_switch(sched, &statp->account);
	xnstat_counter_inc(&statp->hits);

	return prev;
}

#else  /* !CONFIG_XENO_OPT_STATS */

static inline void stat_counter_inc(void) {}

static inline void stat_counter_dec(void) {}

static inline void sync_stat_references(struct xnintr *intr) {}

static inline void alloc_irqstats(struct xnintr *intr) {}

static inline void free_irqstats(struct xnintr *intr) {}

static inline void clear_irqstats(struct xnintr *intr) {}

static inline void query_irqstats(struct xnintr *intr, int cpu,
				  struct xnintr_iterator *iterator) {}

static inline void inc_irqstats(struct xnintr *intr, struct xnsched *sched, xnticks_t start) {}

static inline void switch_irqstats(struct xnintr *intr, struct xnsched *sched) {}


#endif /* !CONFIG_XENO_OPT_STATS */

/*
 * Low-level core clock irq handler. This one forwards ticks from the
 * Xenomai platform timer to nkclock exclusively.
 */
void xnintr_core_clock_handler(void)
{
	struct xnsched *sched = xnsched_current();
	int cpu  __maybe_unused = xnsched_cpu(sched);

	if (!__cobalt_ready)
		return ;

	xnclock_tick();
}


/* Wrapper for managing low-level interrupt handler */
/* Args are stored in cookie field in xnintr */
int __xintr_irq_handler(unsigned int irq, void *dummy) {
	return ((struct xnintr *) irqdescs[irq].xnintr)->isr(irqdescs[irq].xnintr);
}


static inline void xnintr_irq_attach(struct xnintr *intr)
{
	ipipe_request_irq(intr->irq, __xintr_irq_handler, intr->cookie);
}

static inline void xnintr_irq_detach(struct xnintr *intr)
{
	sync_stat_references(intr);
}

/*
 * Low-level interrupt handler dispatching non-shared ISRs -- Called
 * with interrupts off.
 */
void __xnintr_irq_handler(unsigned int irq)
{
	struct xnsched *sched = xnsched_current();
	xnstat_exectime_t *prev;
	xnticks_t start;
	int s = 0;

	prev  = xnstat_exectime_get_current(sched);
	start = xnstat_exectime_now();
	trace_cobalt_irq_entry(irq);

	++sched->inesting;
	sched->lflags |= XNINIRQ;

	/* SOO.tech */
	/* Call to the higher layer handler */

	if (!irqdescs[irq].handler) {
		lprintk("%s: failure, irq %d on RT CPU is not bound to a valid handler.\n", __func__, irq);
		BUG();
	}

	s = irqdescs[irq].handler(irq, irqdescs[irq].data);

	XENO_WARN_ON_ONCE(USER, (s & XN_IRQ_STATMASK) == 0);
	if (unlikely(!(s & XN_IRQ_HANDLED)))
		BUG();
	else
		inc_irqstats(irqdescs[irq].xnintr, sched, start);

	BUG_ON(!irqs_disabled());

	xnstat_exectime_switch(sched, prev);

	if (--sched->inesting == 0)
		sched->lflags &= ~XNINIRQ;

	trace_cobalt_irq_exit(irq);
}

/**
 * @fn int xnintr_init(struct xnintr *intr,const char *name,unsigned int irq,xnisr_t isr,xniack_t iack,int flags)
 * @brief Initialize an interrupt descriptor.
 *
 * When an interrupt occurs on the given @a irq line, the interrupt
 * service routine @a isr is fired in order to deal with the hardware
 * event. The interrupt handler may call any non-blocking service from
 * the Cobalt core.
 *
 * Upon receipt of an IRQ, the interrupt handler @a isr is immediately
 * called on behalf of the interrupted stack context, the rescheduling
 * procedure is locked, and the interrupt line is masked in the system
 * interrupt controller chip.  Upon return, the status of the
 * interrupt handler is checked for the following bits:
 *
 * - XN_IRQ_HANDLED indicates that the interrupt request was
 * successfully handled.
 *
 * - XN_IRQ_NONE indicates the opposite to XN_IRQ_HANDLED, meaning
 * that no interrupt source could be identified for the ongoing
 * request by the handler.
 *
 * In addition, one of the following bits may be present in the
 * status:
 *
 * - XN_IRQ_DISABLE tells the Cobalt core to disable the interrupt
 * line before returning from the interrupt context.
 *
 * - XN_IRQ_PROPAGATE propagates the IRQ event down the interrupt
 * pipeline to Linux. Using this flag is strongly discouraged, unless
 * you fully understand the implications of such propagation.
 *
 * @warning The handler should not use these bits if it shares the
 * interrupt line with other handlers in the real-time domain. When
 * any of these bits is detected, the interrupt line is left masked.
 *
 * A count of interrupt receipts is tracked into the interrupt
 * descriptor, and reset to zero each time such descriptor is
 * attached. Since this count could wrap around, it should be used as
 * an indication of interrupt activity only.
 *
 * @param intr The address of a descriptor the Cobalt core will use to
 * store the interrupt-specific data.
 *
 * @param name An ASCII string standing for the symbolic name of the
 * interrupt or NULL.
 *
 * @param irq The IRQ line number associated with the interrupt
 * descriptor. This value is architecture-dependent. An interrupt
 * descriptor must be attached to the system by a call to
 * xnintr_attach() before @a irq events can be received.
 *
 * @param isr The address of an interrupt handler, which is passed the
 * address of the interrupt descriptor receiving the IRQ.
 *
 * @param iack The address of an optional interrupt acknowledge
 * routine, aimed at replacing the default one. Only very specific
 * situations actually require to override the default setting for
 * this parameter, like having to acknowledge non-standard PIC
 * hardware. @a iack should return a non-zero value to indicate that
 * the interrupt has been properly acknowledged. If @a iack is NULL,
 * the default routine will be used instead.
 *
 * @param flags A set of creation flags affecting the operation. The
 * valid flags are:
 *
 * - XN_IRQTYPE_SHARED enables IRQ-sharing with other interrupt
 * objects.
 *
 * - XN_IRQTYPE_EDGE is an additional flag need to be set together
 * with XN_IRQTYPE_SHARED to enable IRQ-sharing of edge-triggered
 * interrupts.
 *
 * @return 0 is returned on success. Otherwise, -EINVAL is returned if
 * @a irq is not a valid interrupt number.
 *
 * @coretags{secondary-only}
 */
int xnintr_init(struct xnintr *intr, const char *name,
		unsigned int irq, xnisr_t isr, xniack_t iack,
		int flags)
{
	int i;

	if (irq >= NR_PIRQS + NR_VIRQS)
		return -EINVAL;

	/* Validate the interrupt request. No existing association must exist. */
	if (irqdescs[irq].handler != NULL) {
		lprintk("%s: failed to initialize IRQ %d (existing handler %p)\n", __func__, irq, irqdescs[irq].handler);
		BUG();
	}

	for (i = 0; i < NR_PIRQS; i++)
		if (irqdescs[i].xnintr == intr) {
			lprintk("%s: failed to initialize IRQ %d (already bounded handle)\n", __func__, irq);
			BUG();
		}

	intr->irq = irq;
	intr->isr = isr;
	intr->iack = iack;
	intr->cookie = NULL;
	intr->name = name ? : "<unknown>";
	intr->flags = flags;
	intr->status = _XN_IRQSTAT_DISABLED;
	intr->unhandled = 0;

	irqdescs[irq].xnintr = intr;

	raw_spin_lock_init(&intr->lock);

	alloc_irqstats(intr);

	return 0;
}
EXPORT_SYMBOL_GPL(xnintr_init);

/**
 * @fn void xnintr_destroy(struct xnintr *intr)
 * @brief Destroy an interrupt descriptor.
 *
 * Destroys an interrupt descriptor previously initialized by
 * xnintr_init(). The descriptor is automatically detached by a call
 * to xnintr_detach(). No more IRQs will be received through this
 * descriptor after this service has returned.
 *
 * @param intr The address of the interrupt descriptor to destroy.
 *
 * @coretags{secondary-only}
 */
void xnintr_destroy(struct xnintr *intr)
{
	xnintr_detach(intr);
	free_irqstats(intr);
}
EXPORT_SYMBOL_GPL(xnintr_destroy);

/**
 * @fn int xnintr_attach(struct xnintr *intr, void *cookie)
 * @brief Attach an interrupt descriptor.
 *
 * Attach an interrupt descriptor previously initialized by
 * xnintr_init(). This operation registers the descriptor at the
 * interrupt pipeline, but does not enable the interrupt line yet. A
 * call to xnintr_enable() is required to start receiving IRQs from
 * the interrupt line associated to the descriptor.
 *
 * @param intr The address of the interrupt descriptor to attach.
 *
 * @param cookie A user-defined opaque value which is stored into the
 * descriptor for further retrieval by the interrupt handler.
 *
 * @return 0 is returned on success. Otherwise:
 *
 * - -EINVAL is returned if an error occurred while attaching the
 * descriptor.
 *
 * - -EBUSY is returned if the descriptor was already attached.
 *
 * @note The caller <b>must not</b> hold nklock when invoking this service,
 * this would cause deadlocks.
 *
 * @coretags{secondary-only}
 *
 * @note Attaching an interrupt descriptor resets the tracked number
 * of IRQ receipts to zero.
 */
void xnintr_attach(struct xnintr *intr, void *cookie)
{

	trace_cobalt_irq_attach(intr->irq);

	intr->cookie = cookie;
	clear_irqstats(intr);

	ipipe_set_irq_affinity(intr->irq, cobalt_cpu_affinity);

	raw_spin_lock(&intr->lock);

	if (test_and_set_bit(XN_IRQSTAT_ATTACHED, &intr->status))
		BUG();

	xnintr_irq_attach(intr);

	stat_counter_inc();

	raw_spin_unlock(&intr->lock);

}
EXPORT_SYMBOL_GPL(xnintr_attach);

/**
 * @fn int xnintr_detach(struct xnintr *intr)
 * @brief Detach an interrupt descriptor.
 *
 * This call unregisters an interrupt descriptor previously attached
 * by xnintr_attach() from the interrupt pipeline. Once detached, the
 * associated interrupt line is disabled, but the descriptor remains
 * valid. The descriptor can be attached anew by a call to
 * xnintr_attach().
 *
 * @param intr The address of the interrupt descriptor to detach.
 *
 * @note The caller <b>must not</b> hold nklock when invoking this
 * service, this would cause deadlocks.
 *
 * @coretags{secondary-only}
 */
void xnintr_detach(struct xnintr *intr)
{
	trace_cobalt_irq_detach(intr->irq);

	raw_spin_lock(&intr->lock);

	if (test_and_clear_bit(XN_IRQSTAT_ATTACHED, &intr->status)) {
		xnintr_irq_detach(intr);
		stat_counter_dec();
	}

	raw_spin_unlock(&intr->lock);
}
EXPORT_SYMBOL_GPL(xnintr_detach);

/**
 * @fn void xnintr_enable(struct xnintr *intr)
 * @brief Enable an interrupt line.
 *
 * Enables the interrupt line associated with an interrupt descriptor.
 *
 * @param intr The address of the interrupt descriptor.
 *
 * @coretags{secondary-only}
 */
void xnintr_enable(struct xnintr *intr)
{
	unsigned long flags;

	trace_cobalt_irq_enable(intr->irq);

	raw_spin_lock_irqsave(&intr->lock, flags);

	/*
	 * If disabled on entry, there is no way we could race with
	 * disable_irq_line().
	 */
	if (test_and_clear_bit(XN_IRQSTAT_DISABLED, &intr->status))
		ipipe_enable_irq(intr->irq);

	/* SOO.tech */
	ipipe_set_irq_affinity(intr->irq, cobalt_cpu_affinity);

	raw_spin_unlock_irqrestore(&intr->lock, flags);
}
EXPORT_SYMBOL_GPL(xnintr_enable);

/**
 * @fn void xnintr_disable(struct xnintr *intr)
 * @brief Disable an interrupt line.
 *
 * Disables the interrupt line associated with an interrupt
 * descriptor.
 *
 * @param intr The address of the interrupt descriptor.
 *
 * @coretags{secondary-only}
 */
void xnintr_disable(struct xnintr *intr)
{
	unsigned long flags;

	trace_cobalt_irq_disable(intr->irq);

	/* We only need a virtual masking. */
	raw_spin_lock_irqsave(&intr->lock, flags);

	/*
	 * Racing with disable_irq_line() is innocuous, the pipeline
	 * would serialize calls to ipipe_disable_irq() across CPUs,
	 * and the descriptor status would still properly match the
	 * line status in the end.
	 */
	if (!test_and_set_bit(XN_IRQSTAT_DISABLED, &intr->status))
		ipipe_disable_irq(intr->irq);

	raw_spin_unlock_irqrestore(&intr->lock, flags);
}
EXPORT_SYMBOL_GPL(xnintr_disable);

/**
 * @fn void xnintr_affinity(struct xnintr *intr, cpumask_t cpumask)
 * @brief Set processor affinity of interrupt.
 *
 * Restricts the IRQ line associated with the interrupt descriptor @a
 * intr to be received only on processors which bits are set in @a
 * cpumask.
 *
 * @param intr The address of the interrupt descriptor.
 *
 * @param cpumask The new processor affinity.
 *
 * @note Depending on architectures, setting more than one bit in @a
 * cpumask could be meaningless.
 *
 * @coretags{secondary-only}
 */
void xnintr_affinity(struct xnintr *intr, cpumask_t cpumask)
{
	ipipe_set_irq_affinity(intr->irq, cpumask);
}
EXPORT_SYMBOL_GPL(xnintr_affinity);




