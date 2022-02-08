/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

/*
 * evtchn manager
 *
 * Functions are in charge of processing event channels at the low level.
 */

#include <linux/smp.h>
#include <linux/slab.h>
#include <linux/irqflags.h>

#include <linux/sched/debug.h>

#include <asm/cacheflush.h>
#include <asm/tlbflush.h>

#include <opencn/opencn.h>

#include <soo/uapi/debug.h>

#include <soo/evtchn.h>

#include <opencn/opencn.h>

#include <opencn/soo/soo.h>
#include <opencn/soo/evtchn-mgr.h>

static int evtchn_set_pending(struct domain *d, int evtchn);
extern void smp_send_event_check_cpu(int cpu);


int get_free_evtchn(struct domain *d) {
	int i = 0;

	for (i = 0; i < NR_EVTCHN; i++)
		if (d->evtchn[i].state == ECS_FREE)
			return i;

	return -ENOSPC;
}

static long evtchn_alloc_unbound(evtchn_alloc_unbound_t *alloc) {
	struct evtchn *chn;
	int evtchn;
	long rc = 0;
	struct domain *d;

	d = ((alloc->dom == DOMID_SELF) ? &per_cpu(domain, smp_processor_id()) : &per_cpu(domain, alloc->dom));

	spin_lock(&d->event_lock);

	if ((evtchn = get_free_evtchn(d)) < 0)
		BUG();

	chn = &d->evtchn[evtchn];

	d->evtchn[evtchn].state = ECS_UNBOUND;

	chn->unbound.remote_domid = alloc->remote_dom;

	alloc->evtchn = evtchn;

	spin_unlock(&d->event_lock);

	return rc;
}

static long evtchn_bind_interdomain(evtchn_bind_interdomain_t *bind) {
	struct evtchn *lchn, *rchn;
	struct domain *ld, *rd;
	int levtchn, revtchn;
	domid_t rdom;
	long rc = 0;
	int valid = 0;

	ld = &per_cpu(domain, smp_processor_id());

	rdom = bind->remote_dom;
	revtchn = bind->remote_evtchn;

	if (rdom == DOMID_SELF)
		rdom = per_cpu(domain, smp_processor_id()).domain_id;

	rd = &per_cpu(domain, rdom);

	/* Avoid deadlock by first acquiring lock of domain with smaller id. */
	if (ld < rd) {
		spin_lock(&ld->event_lock);
		spin_lock(&rd->event_lock);
	} else {
		if (ld != rd)
			spin_lock(&rd->event_lock);
		spin_lock(&ld->event_lock);
	}

	if ((levtchn = get_free_evtchn(ld)) < 0)
		BUG();

	lchn = &ld->evtchn[levtchn];
	rchn = &rd->evtchn[revtchn];

	valid = ((rchn->state == ECS_INTERDOMAIN) && (rchn->interdomain.remote_dom == NULL));

	if (!valid && ((rchn->state != ECS_UNBOUND) || (rchn->unbound.remote_domid != ld->domain_id)))
		BUG();

	lchn->interdomain.remote_dom = rd;
	lchn->interdomain.remote_evtchn = revtchn;
	lchn->state = ECS_INTERDOMAIN;

	rchn->interdomain.remote_dom = ld;
	rchn->interdomain.remote_evtchn = levtchn;
	rchn->state = ECS_INTERDOMAIN;

	bind->local_evtchn = levtchn;

	spin_unlock(&ld->event_lock);

	if (ld != rd)
		spin_unlock(&rd->event_lock);

	return rc;
}

long __evtchn_close(struct domain *d1, int chn) {
	struct domain *d2 = NULL;
	struct evtchn *chn1, *chn2;
	int evtchn2;
	long rc = 0;

	again:
	spin_lock(&d1->event_lock);

	chn1 = &d1->evtchn[chn];

	switch (chn1->state) {
	case ECS_FREE:
	case ECS_RESERVED:
		rc = -EINVAL;
		goto out;

	case ECS_UNBOUND:
		break;

	case ECS_VIRQ:
		/* Not handled */
		BUG();
		break;

	case ECS_INTERDOMAIN:

		if (d2 == NULL) {

			d2 = chn1->interdomain.remote_dom;

			if (d2 != NULL) {
				/* If we unlock d1 then we could lose d2. Must get a reference. */

				if (d1 < d2) {
					spin_lock(&d2->event_lock);
				} else if (d1 != d2) {
					spin_unlock(&d1->event_lock);
					spin_lock(&d2->event_lock);
					goto again;
				}
			}
		} else if (d2 != chn1->interdomain.remote_dom) {
			/*
			 * We can only get here if the evtchn was closed and re-bound after
			 * unlocking d1 but before locking d2 above. We could retry but
			 * it is easier to return the same error as if we had seen the
			 * evtchn in ECS_CLOSED. It must have passed through that state for
			 * us to end up here, so it's a valid error to return.
			 */
			rc = -EINVAL;
			goto out;
		}

		if (d2 != NULL) {
			evtchn2 = chn1->interdomain.remote_evtchn;

			chn2 = &d2->evtchn[evtchn2];

			BUG_ON(chn2->state != ECS_INTERDOMAIN);
			BUG_ON(chn2->interdomain.remote_dom != d1);

			chn2->state = ECS_UNBOUND;
			chn2->unbound.remote_domid = d1->domain_id;
		}
		break;

	default:
		BUG();
	}

	/* Clear pending event to avoid unexpected behavior on re-bind. */
	clear_bit(chn, (unsigned long *)&d1->shared_info->evtchn_pending);

	/* Reset binding when the channel is freed. */
	chn1->state = ECS_FREE;

	out:

	if (d2 != NULL) {
		if (d1 != d2)
			spin_unlock(&d2->event_lock);
	}

	spin_unlock(&d1->event_lock);

	return rc;
}


static long evtchn_close(evtchn_close_t *close) {
	return __evtchn_close(&per_cpu(domain, smp_processor_id()), close->evtchn);
}

int evtchn_send(struct domain *d, unsigned int levtchn) {
	struct evtchn *lchn;
	struct domain *ld = d, *rd;
	int revtchn = 0, ret = 0;

	lchn = &ld->evtchn[levtchn];

	rd = lchn->interdomain.remote_dom;

	if (lchn->state != ECS_INTERDOMAIN) {
		/* Abnormal situation */
		printk("%s: failure, undefined state: %d, local domain: %d, remote domain: %d, revtchn: %d, levtchn: %d, CPU: %d\n", __func__, lchn->state, ld->domain_id, ((rd != NULL) ? rd->domain_id : -1), revtchn, levtchn, smp_processor_id());

		BUG();
	}

	/* Avoid deadlock by first acquiring lock of domain with smaller id. */
	if (ld < rd) {
		spin_lock(&ld->event_lock);
		spin_lock(&rd->event_lock);
	} else {
		if (ld != rd)
			spin_lock(&rd->event_lock);
		spin_lock(&ld->event_lock);
	}

	revtchn = lchn->interdomain.remote_evtchn;

	evtchn_set_pending(rd, revtchn);
	spin_unlock(&ld->event_lock);

	if (ld != rd)
		spin_unlock(&rd->event_lock);

	return ret;
}

static int evtchn_set_pending(struct domain *d, int evtchn) {

	/*
	 * The following bit operations must happen in strict order.
	 */
	BUG_ON(!irqs_disabled());

	d->shared_info->evtchn_pending[evtchn] = true;
	d->shared_info->evtchn_upcall_pending = 1;

	smp_mb();

	if (smp_processor_id() != d->processor)
		smp_send_event_check_cpu(d->processor);

	return 0;
}

static long evtchn_status(evtchn_status_t *status) {
	struct domain *d = &per_cpu(domain, status->dom);
	int evtchn = status->evtchn;
	struct evtchn *chn;
	long rc = 0;

	spin_lock(&d->event_lock);

	chn = &d->evtchn[evtchn];

	switch (chn->state) {
	case ECS_FREE:
	case ECS_RESERVED:
		status->status = EVTCHNSTAT_closed;
		break;

	case ECS_UNBOUND:
		status->status = EVTCHNSTAT_unbound;
		status->u.unbound.dom = chn->unbound.remote_domid;
		break;

	case ECS_INTERDOMAIN:
		status->status = EVTCHNSTAT_interdomain;
		status->u.interdomain.dom =
				chn->interdomain.remote_dom->domain_id;
		status->u.interdomain.evtchn = chn->interdomain.remote_evtchn;
		break;

	case ECS_VIRQ:
		status->status = EVTCHNSTAT_virq;
		status->u.virq = chn->virq;
		break;

	default:
		BUG();
	}

	spin_unlock(&d->event_lock);

	return rc;
}

long do_event_channel_op(int cmd, void *args) {
	struct evtchn_alloc_unbound alloc_unbound;

	switch (cmd) {
	case EVTCHNOP_alloc_unbound: {

		memcpy(&alloc_unbound, args, sizeof(struct evtchn_alloc_unbound));
		evtchn_alloc_unbound(&alloc_unbound);
		memcpy(args, &alloc_unbound, sizeof(struct evtchn_alloc_unbound));

		break;
	}

	case EVTCHNOP_bind_interdomain: {

		struct evtchn_bind_interdomain bind_interdomain;

		memcpy(&bind_interdomain, args, sizeof(struct evtchn_bind_interdomain));
		evtchn_bind_interdomain(&bind_interdomain);
		memcpy(args, &bind_interdomain, sizeof(struct evtchn_bind_interdomain));

		break;
	}

	case EVTCHNOP_bind_existing_interdomain: {

		/* Not handled... */
		BUG();

		break;
	}

	case EVTCHNOP_bind_virq: {

		/* Not handled... */
		BUG();
	}

	case EVTCHNOP_close: {

		struct evtchn_close close;

		memcpy(&close, args, sizeof(struct evtchn_close));

		evtchn_close(&close);
		break;
	}

	case EVTCHNOP_send: {

		struct evtchn_send send;

		memcpy(&send, args, sizeof(struct evtchn_send));

		evtchn_send(&per_cpu(domain, smp_processor_id()), send.evtchn);

		break;
	}

	case EVTCHNOP_status: {

		struct evtchn_status status;

		memcpy(&status, args, sizeof(struct evtchn_status));
		evtchn_status(&status);
		memcpy(args, &status, sizeof(struct evtchn_status));

		break;
	}

	default:
		BUG();
		break;
	}

	return 0;
}

int evtchn_init(struct domain *d) {
	int i;

	spin_lock_init(&d->event_lock);

	d->evtchn[0].state = ECS_RESERVED;
	d->evtchn[0].can_notify = true;

	for (i = 1; i < NR_EVTCHN; i++) {
		d->evtchn[i].state = ECS_FREE;
		d->evtchn[i].can_notify = true;
	}

	return 0;
}

void evtchn_destroy(struct domain *d) {
	int i;

	/* After this barrier no new event-channel allocations can occur. */
	spin_lock(&d->event_lock);

	/* Close all existing event channels. */
	for (i = 0; i < NR_EVTCHN; i++)
		(void) __evtchn_close(d, i);
}

static void domain_dump_evtchn_info(struct domain *d) {
	unsigned int i;

	spin_lock(&d->event_lock);

	for (i = 1; i < NR_EVTCHN; i++) {
		const struct evtchn *chn;

		chn = &d->evtchn[i];
		if (chn->state == ECS_FREE)
			continue;

		printk("  Dom: %d  chn: %d pending:%d: state: %d",
				d->domain_id, i,
				!!test_bit(i, (unsigned long *) &d->shared_info->evtchn_pending),
				chn->state);

		switch (chn->state) {
		case ECS_UNBOUND:
			printk(" unbound:remote_domid:%d",
					chn->unbound.remote_domid);
			break;

		case ECS_INTERDOMAIN:
			printk(" interdomain remote_dom:%d remove_evtchn: %d",
					chn->interdomain.remote_dom->domain_id,
					chn->interdomain.remote_evtchn);
			break;

		case ECS_VIRQ:
			printk(" VIRQ: %d", chn->virq);
			break;
		}

		printk("\n");

	}

	spin_unlock(&d->event_lock);
}

void dump_evtchn_info(void) {

	printk("*** Dumping event-channel info\n");

	domain_dump_evtchn_info(&per_cpu(domain, AGENCY_CPU0));
	domain_dump_evtchn_info(&per_cpu(domain, AGENCY_RT_CPU));

}

