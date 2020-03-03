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

#include <avz/config.h>
#include <avz/init.h>
#include <avz/lib.h>
#include <avz/errno.h>
#include <avz/sched.h>
#include <avz/event.h>
#include <avz/irq.h>
#include <avz/sched-if.h>

#include <avz/keyhandler.h>

#include <asm/current.h>
#include <asm/cacheflush.h>

#include <soo/soo.h>

#include <soo/uapi/avz.h>
#include <soo/uapi/event_channel.h>
#include <soo/uapi/debug.h>

#define ERROR_EXIT(_errno)                                          \
		do {                                                            \
			gdprintk(KERN_ERR,                                    \
					"EVTCHNOP failure: error %d\n",                     \
					(_errno));                                          \
					rc = (_errno);                                              \
					BUG();                                                   \
		} while ( 0 )
#define ERROR_EXIT_DOM(_errno, _dom)                                \
		do {                                                            \
			gdprintk(KERN_ERR,                                    \
					"EVTCHNOP failure: domain %d, error %d\n",          \
					(_dom)->domain_id, (_errno));                       \
					rc = (_errno);                                              \
					BUG();                                                   \
		} while ( 0 )

static int evtchn_set_pending(struct vcpu *v, int evtchn);

int get_free_evtchn(struct domain *d) {
	int i = 0;

	if (d->is_dying)
		return -EINVAL;

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

	d = ((alloc->dom == DOMID_SELF) ? current->domain : domains[alloc->dom]);

	spin_lock(&d->event_lock);

	if ((evtchn = get_free_evtchn(d)) < 0)
		ERROR_EXIT_DOM(evtchn, d);

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

	ld = current->domain;

	rdom = bind->remote_dom;
	revtchn = bind->remote_evtchn;

	if (rdom == DOMID_SELF)
		rdom = current->domain->domain_id;

	rd = domains[rdom];

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
		ERROR_EXIT(levtchn);

	lchn = &ld->evtchn[levtchn];
	rchn = &rd->evtchn[revtchn];

	valid = ((rchn->state == ECS_INTERDOMAIN) && (rchn->interdomain.remote_dom == NULL));

	if (!valid && ((rchn->state != ECS_UNBOUND) || (rchn->unbound.remote_domid != ld->domain_id)))
		ERROR_EXIT_DOM(-EINVAL, rd);

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

/*
 * Special routine called directly from hypervisor during post-migration sync to bind two event channels that we know exist and should now be connected!
 * as well as from guest kernel in case of ME IMEC setup. In this latter case, event channels have been previously allocated with a remote_domid equal to local_domid.
 *
 */
long evtchn_bind_existing_interdomain(struct domain *ld, struct domain *remote, int levtchn, int revtchn) {
	struct evtchn *lchn, *rchn;
	struct domain *rd;
	long rc = 0;

	DBG("%s\n", __FUNCTION__);

	rd = domains[remote->domain_id];

	/* Avoid deadlock by first acquiring lock of domain with smaller id. */
	if (ld < rd) {
		spin_lock(&ld->event_lock);
		spin_lock(&rd->event_lock);
	} else {
		if (ld != rd)
			spin_lock(&rd->event_lock);
		spin_lock(&ld->event_lock);
	}

	lchn = &ld->evtchn[levtchn];
	rchn = &rd->evtchn[revtchn];

	/* Check if the remote is still equal to local (due to IMEC initializing for example) */

	if (lchn->unbound.remote_domid == ld->domain_id)
		rchn->unbound.remote_domid = remote->domain_id;

	if ((rchn->state != ECS_UNBOUND) || ((rchn->unbound.remote_domid != DOMID_SELF) && (rchn->unbound.remote_domid != ld->domain_id)))
		ERROR_EXIT_DOM(-EINVAL, rd);

	lchn->interdomain.remote_dom = rd;
	lchn->interdomain.remote_evtchn = revtchn;
	lchn->state = ECS_INTERDOMAIN;

	rchn->interdomain.remote_dom = ld;
	rchn->interdomain.remote_evtchn = levtchn;
	rchn->state = ECS_INTERDOMAIN;

	spin_unlock(&ld->event_lock);
	if (ld != rd)
		spin_unlock(&rd->event_lock);

	return rc;
}

static long evtchn_bind_virq(evtchn_bind_virq_t *bind) {
	struct evtchn *chn;
	struct vcpu *v;
	struct domain *d = current->domain;
	int evtchn, virq = bind->virq;
	long rc = 0;

	if ((v = d->vcpu[0]) == NULL)
		BUG();

	if ((virq < 0) || (virq >= ARRAY_SIZE(v->virq_to_evtchn)))
		BUG();

	spin_lock(&d->event_lock);

	if (v->virq_to_evtchn[virq] != 0)
		BUG();

	if ((evtchn = get_free_evtchn(d)) < 0)
		BUG();

	chn = &d->evtchn[evtchn];
	chn->state = ECS_VIRQ;
	chn->virq = virq;

	v->virq_to_evtchn[virq] = bind->evtchn = evtchn;

	spin_unlock(&d->event_lock);

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
		d1->vcpu[0]->virq_to_evtchn[chn1->virq] = 0;
		spin_barrier_irq(&d1->vcpu[0]->virq_lock);
		break;

	case ECS_INTERDOMAIN:

		if (d2 == NULL) {

			d2 = chn1->interdomain.remote_dom;

			if (d2 != NULL) {
				/* If we unlock d1 then we could lose d2. Must get a reference. */
				if (unlikely(!get_domain(d2)))
					BUG();

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
	clear_bit(chn, (unsigned long *)&shared_info(d1, evtchn_pending));

	/* Reset binding when the channel is freed. */
	chn1->state = ECS_FREE;

	out:

	if (d2 != NULL) {
		if (d1 != d2)
			spin_unlock(&d2->event_lock);

		put_domain(d2);
	}

	spin_unlock(&d1->event_lock);

	return rc;
}

static long evtchn_close(evtchn_close_t *close) {
	return __evtchn_close(current->domain, close->evtchn);
}

int evtchn_send(struct domain *d, unsigned int levtchn) {
	struct evtchn *lchn;
	struct domain *ld = d, *rd;
	struct vcpu *rvcpu;
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
	rvcpu = rd->vcpu[0];
	
	evtchn_set_pending(rvcpu, revtchn);
	spin_unlock(&ld->event_lock);
	
	if (ld != rd)
		spin_unlock(&rd->event_lock);

	return ret;
}

static int evtchn_set_pending(struct vcpu *v, int evtchn) {
	struct domain *d = v->domain;

	/*
	 * The following bit operations must happen in strict order.
	 */
	ASSERT(local_irq_is_disabled());

	/* Here, it is necessary to use a transactional set bit function for manipulating the bitmap since
	 * we are working at the bit level and some simultaneous write can be done by the agency or ME.
	 * Hence, we avoid to be overwritten by such a write.
	 */

	transaction_set_bit(evtchn, (unsigned long *) &shared_info(d, evtchn_pending));

	v->vcpu_info->evtchn_upcall_pending = 1;

	dmb();

	if (smp_processor_id() != v->processor)
		smp_send_event_check_cpu(v->processor);

	return 0;
}

void send_guest_vcpu_virq(struct vcpu *v, int virq) {
	unsigned long flags;
	int evtchn;
	bool __already_locked = false;

	spin_lock_irqsave(&v->virq_lock, flags);
	evtchn = v->virq_to_evtchn[virq];

	if (unlikely(evtchn == 0))
		goto out;

	if (spin_is_locked(&v->sched->sched_data.schedule_lock))
		__already_locked = true;
	else
		spin_lock(&v->sched->sched_data.schedule_lock);


	spin_lock(&v->domain->event_lock);

	evtchn_set_pending(v, evtchn);

	spin_unlock(&v->domain->event_lock);

	if (!__already_locked)
		spin_unlock(&v->sched->sched_data.schedule_lock);

	out:
	spin_unlock_irqrestore(&v->virq_lock, flags);
}

static long evtchn_status(evtchn_status_t *status) {
	struct domain *d = domains[status->dom];
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
	long rc;
	struct evtchn_alloc_unbound alloc_unbound;

	switch (cmd) {
	case EVTCHNOP_alloc_unbound: {

		memcpy(&alloc_unbound, args, sizeof(struct evtchn_alloc_unbound));

		rc = evtchn_alloc_unbound(&alloc_unbound);
		memcpy(args, &alloc_unbound, sizeof(struct evtchn_alloc_unbound));
		break;
	}

	case EVTCHNOP_bind_interdomain: {

		struct evtchn_bind_interdomain bind_interdomain;

		memcpy(&bind_interdomain, args, sizeof(struct evtchn_bind_interdomain));

		rc = evtchn_bind_interdomain(&bind_interdomain);
		memcpy(args, &bind_interdomain, sizeof(struct evtchn_bind_interdomain));
		break;
	}

	case EVTCHNOP_bind_existing_interdomain: {

		struct evtchn_bind_interdomain bind_interdomain;

		memcpy(&bind_interdomain, args, sizeof(struct evtchn_bind_interdomain));

		rc = evtchn_bind_existing_interdomain(current->domain,
				domains[bind_interdomain.remote_dom],
				bind_interdomain.local_evtchn,
				bind_interdomain.remote_evtchn);

		memcpy(args, &bind_interdomain, sizeof(struct evtchn_bind_interdomain));
		break;
	}

	case EVTCHNOP_bind_virq: {

		struct evtchn_bind_virq bind_virq;

		memcpy(&bind_virq, args, sizeof(struct evtchn_bind_virq));

		rc = evtchn_bind_virq(&bind_virq);
		memcpy(args, &bind_virq, sizeof(struct evtchn_bind_virq));
		break;
	}

	case EVTCHNOP_close: {

		struct evtchn_close close;

		memcpy(&close, args, sizeof(struct evtchn_close));

		rc = evtchn_close(&close);
		break;
	}

	case EVTCHNOP_send: {

		struct evtchn_send send;

		memcpy(&send, args, sizeof(struct evtchn_send));

		rc = evtchn_send(current->domain, send.evtchn);
		break;
	}

	case EVTCHNOP_status: {

		struct evtchn_status status;

		memcpy(&status, args, sizeof(struct evtchn_status));

		rc = evtchn_status(&status);
		memcpy(args, &status, sizeof(struct evtchn_status));
		break;
	}

	default:
		BUG();
		break;
	}

	return rc;
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
	BUG_ON(!d->is_dying);
	spin_barrier(&d->event_lock);

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
				!!test_bit(i, (unsigned long *) &shared_info(d, evtchn_pending)),
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

static void dump_evtchn_info(unsigned char key) {
	int i;

	printk("'%c' pressed -> dumping event-channel info\n", key);

	for (i = 0; i < NR_CPUS; i++)
		spin_lock(&per_cpu(intc_lock, i));

	for (i = 0; i < MAX_DOMAINS; i++)
		if (domains[i] != NULL)
			domain_dump_evtchn_info(domains[i]);

	domain_dump_evtchn_info(domains[DOMID_AGENCY_RT]);

	for (i = 0; i < NR_CPUS; i++)
		spin_unlock(&per_cpu(intc_lock, i));

}

static struct keyhandler dump_evtchn_info_keyhandler = { .diagnostic = 1,
		.u.fn = dump_evtchn_info, .desc = "dump evtchn info" };

static int __init dump_evtchn_info_key_init(void) {
	register_keyhandler('e', &dump_evtchn_info_keyhandler);
	return 0;
}
__initcall(dump_evtchn_info_key_init);

