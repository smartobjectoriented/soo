/*
 * Copyright (C) 2018-2019 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2018-2019 Baptiste Delporte <bonel@bonel.net>
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

#include <heap.h>
#include <device/driver.h>

#include <soo/evtchn.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/console.h>
#include <soo/debug.h>

#include "common.h"

vdummyrt_t vdummyrt;

static void process_pending_rsp(void) {
	RING_IDX i, rp;

	DBG("%s\n", __func__);

	rp = vdummyrt.ring.sring->rsp_prod;
	dmb(); /* Ensure we see queued responses up to 'rp'. Just to make sure ;-) not necessary in all cases... */

	for (i = vdummyrt.ring.sring->rsp_cons; i != rp; i++) {
		DBG("%s, cons=%d\n", __func__, i);

		/* Do something with the response */
	}

	/* At the end rsp_cons = rsp_prod and refers to a free response index */
	vdummyrt.ring.sring->rsp_cons = i;

#if 0
	/* Example of batch of responses processing */

	again:

	rp = vdummyrt.ring.sring->rsp_prod;
	dmb(); /* Ensure we see queued responses up to 'rp'. Just to make sure ;-) not necessary in all cases... */

	for (i = vdummyrt.ring.sring->rsp_cons; i != rp; i++) {
		ring_rsp = RING_GET_RESPONSE(&vdummyrt.ring, i);

		/* Do something with the response */
	}

	vdummyrt.ring.sring->rsp_cons = i;

	RING_FINAL_CHECK_FOR_RESPONSES(&vdummyrt.ring, work_to_do);

	if (work_to_do)
		goto again;
#endif /* 0 */
}

irq_return_t vdummyrt_interrupt(int irq, void *dev_id) {
	if (!vdummyrt_is_connected())
		return IRQ_COMPLETED;

	process_pending_rsp();

	return IRQ_COMPLETED;
}

/*
 * The following function is given as an example.
 *
 */
void vdummyrt_generate_request(char *buffer) {
	vdummyrt_request_t *ring_req;

	vdummyrt_start();

	/*
	 * Try to generate a new request to the backend
	 */
	if (!RING_FULL(&vdummyrt.ring)) {
		ring_req = RING_GET_REQUEST(&vdummyrt.ring, vdummyrt.ring.req_prod_pvt);

		memcpy(ring_req->buffer, buffer, VDUMMYRT_PACKET_SIZE);

		/* Fill in the ring_req structure */

		/* Make sure the other end "sees" the request when updating the index */
		dmb();

		vdummyrt.ring.req_prod_pvt++;

		RING_PUSH_REQUESTS(&vdummyrt.ring);

		notify_remote_via_irq(vdummyrt.irq);
	}

	vdummyrt_end();

#if 0

	/* Example of batch of request processing */

	if (!RING_FULL(&vdummyrt.ring)) {

		/* We generate the first request */

		/* Fill in the ring_req structure */

		vdummyrt.ring.req_prod_pvt++;

		/* At this time the request is not visible yet to the other end.
		 * We could proceed with additional requests.
		 */


		/* Now, we are ready to push the first available requests
		 * and we let the macro decide if a notification is required or not.
		 */

		RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(&vdummyrt.ring, notify);

		if (notify)
			notify_remote_via_irq(vdummyrt.irq);

	}

#endif /* 0 */

}

void vdummyrt_probe(void) {
	DBG0(VDUMMYRT_PREFIX "Frontend probe\n");
}

void vdummyrt_reconfiguring(void) {
	DBG0(VDUMMYRT_PREFIX "Frontend reconfiguring\n");

	process_pending_rsp();
}

void vdummyrt_shutdown(void) {
	DBG0(VDUMMYRT_PREFIX "Frontend shutdown\n");
}

void vdummyrt_closed(void) {
	DBG0(VDUMMYRT_PREFIX "Frontend close\n");
}

void vdummyrt_suspend(void) {
	DBG0(VDUMMYRT_PREFIX "Frontend suspend\n");
}

void vdummyrt_resume(void) {
	DBG0(VDUMMYRT_PREFIX "Frontend resume\n");

	process_pending_rsp();
}

void vdummyrt_connected(void) {
	DBG0(VDUMMYRT_PREFIX "Frontend connected\n");

	/* Force the processing of pending requests, if any */
	notify_remote_via_irq(vdummyrt.irq);
}

static int vdummyrt_init(dev_t *dev) {
	vdummyrt_vbus_init();

	return 0;
}

REGISTER_DRIVER_POSTCORE("vdummyrt,frontend", vdummyrt_init);
