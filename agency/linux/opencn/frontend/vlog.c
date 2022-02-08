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

#include <linux/slab.h>

#include <xenomai/rtdm/driver.h>

#include <opencn/dev/vlog.h>

#include <soo/evtchn.h>

#include <opencn/backend/vlog.h>

static vlog_front_ring_t ring;
static rtdm_irq_t irq_handle;
static rtdm_task_t rtdm_vlog_init_task;

bool vlog_enabled = false;

bool rt_log_enabled(void) {
	return vlog_enabled;
}

static int vlog_interrupt(rtdm_irq_t *dummy) {

	/* Nothing at the frontend */

	return RTDM_IRQ_HANDLED;
}

void vlog_flush(void) {

	RING_PUSH_REQUESTS(&ring);
	notify_remote_via_virq(irq_handle.irq);
}

void vlog_sync_flush(dc_event_t dc_event) {
	vlog_flush();

	/* Wait until all request have been consumed by the backend, */
	while (ring.sring->req_cons != ring.sring->req_prod) ;

	tell_dc_stable(dc_event);
}

void vlog_send(char *line) {
	vlog_request_t *ring_req;
	static int nr_msg = 0;

	/*
	 * Try to generate a new request to the backend
	 */
	if (!RING_REQ_FULL(&ring)) {
		ring_req = RING_GET_REQUEST(&ring, ring.req_prod_pvt);

		strcpy(ring_req->line, line);

		/* Fill in the ring_req structure */

		/* Make sure the other end "sees" the request when updating the index */
		mb();

		ring.req_prod_pvt++;
		nr_msg++;

		if (nr_msg == 1) {

			vlog_flush();
			nr_msg = 0;
		}

	}

}

void rtdm_vlog_init_task_fn(void *args) {
	int res = 0;
	unsigned int evtchn = 0;
	vlog_sring_t *sring;
	struct evtchn_alloc_unbound alloc_unbound;
	int err;

	BUG_ON(smp_processor_id() != AGENCY_RT_CPU);

	/* Allocate an event channel associated to the ring */

	alloc_unbound.dom = DOMID_SELF;
	alloc_unbound.remote_dom = AGENCY_CPU;

	err = hypercall_trampoline(__HYPERVISOR_event_channel_op, EVTCHNOP_alloc_unbound, (long) &alloc_unbound, 0, 0);
	if (err) {
	  	lprintk("%s - line %d: allocating event channel for vlog failed evtchn: %d\n", __func__, __LINE__, evtchn);
		BUG();
	} else
		evtchn = alloc_unbound.evtchn;

	res = rtdm_bind_evtchn_to_virq_handler(&irq_handle, evtchn, vlog_interrupt, 0, "vlog-frontend", NULL);
	if (res <= 0) {
		lprintk("%s - line %d: Binding event channel failed.\n", __func__, __LINE__);
		BUG();
	}

	irq_handle.irq = res;

	/* Allocate a shared page for the ring */
	sring = (vlog_sring_t *) kmalloc(VLOG_RING_SIZE, GFP_ATOMIC);
	if (!sring) {
		lprintk("%s - line %d: Allocating shared ring failed.\n", __func__, __LINE__);
		BUG();
	}

	SHARED_RING_INIT(sring);
	FRONT_RING_INIT(&ring, sring, VLOG_RING_SIZE);

	rtdm_register_dc_event_callback(DC_VLOG_FLUSH, vlog_sync_flush);

	probe_vlogback(sring, evtchn);

	vlog_enabled = true;
}

/**
 * Free the ring and deallocate the proper data.
 */
void vlog_free_sring(void) {

	/* Free resources associated with old device channel. */
	kfree(ring.sring);

	rtdm_unbind_from_virqhandler(&irq_handle);

#warning still dc_vlog_free to be implemented...
}

static int __rtdm_vlog_prologue(void *args) {

	rtdm_task_init(&rtdm_vlog_init_task, "rtdm_vlog_init", rtdm_vlog_init_task_fn, NULL, VBUS_TASK_PRIO, 0);

	/* We can leave this thread die. Our system is living anyway... */
	do_exit(0);

	return 0;
}

int vlogrt_start(void) {
	/* Prepare to initiate a Cobalt RT task */
	kernel_thread(__rtdm_vlog_prologue, NULL, 0);

	return 0;
}

/* Initcall to be called after a *standard* device_initcall, once all backends have been initalized */
device_initcall_sync(vlogrt_start);

