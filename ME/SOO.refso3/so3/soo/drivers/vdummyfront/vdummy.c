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
#include <mutex.h>
#include <delay.h>

#include <device/driver.h>

#include <soo/evtchn.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/console.h>
#include <soo/debug.h>

#include "common.h"

vdummy_t vdummy;

static bool thread_created = false;

irq_return_t vdummy_interrupt(int irq, void *dev_id) {
	RING_IDX i, rp;

	if (!vdummy_is_connected())
		return IRQ_COMPLETED;

	DBG("%s, %d\n", __func__, ME_domID());

	rp = vdummy.ring.sring->rsp_prod;
	dmb(); /* Ensure we see queued responses up to 'rp'. Just to make sure ;-) not necessary in all cases... */

	for (i = vdummy.ring.sring->rsp_cons; i != rp; i++) {
		DBG("%s, cons=%d\n", __func__, i);

		/* Do something with the response */
	}

	/* At the end rsp_cons = rsp_prod and refers to a free response index */
	vdummy.ring.sring->rsp_cons = i;

#if 0
	/* Example of batch of responses processing */

	again:

	rp = vdummy.ring.sring->rsp_prod;
	dmb(); /* Ensure we see queued responses up to 'rp'. Just to make sure ;-) not necessary in all cases... */

	for (i = vdummy.ring.sring->rsp_cons; i != rp; i++) {
		ring_rsp = RING_GET_RESPONSE(&vdummy.ring, i);

		/* Do something with the response */
	}

	vdummy.ring.sring->rsp_cons = i;

	RING_FINAL_CHECK_FOR_RESPONSES(&vdummy.ring, work_to_do);

	if (work_to_do)
		goto again;


#endif /* 0 */

	return IRQ_COMPLETED;
}

/*
 * The following function is given as an example.
 *
 */
void vdummy_generate_request(char *buffer) {
	vdummy_request_t *ring_req;

	vdummy_start();

	/*
	 * Try to generate a new request to the backend
	 */
	if (!RING_FULL(&vdummy.ring)) {
		ring_req = RING_GET_REQUEST(&vdummy.ring, vdummy.ring.req_prod_pvt);

		memcpy(ring_req->buffer, buffer, VDUMMY_PACKET_SIZE);

		/* Fill in the ring_req structure */

		/* Make sure the other end "sees" the request when updating the index */
		dmb();

		vdummy.ring.req_prod_pvt++;

		RING_PUSH_REQUESTS(&vdummy.ring);

		notify_remote_via_irq(vdummy.irq);
	}

#if 0

	/* Example of batch of request processing */

	if (!RING_FULL(&vdummy.ring)) {

		/* We generate the first request */

		/* Fill in the ring_req structure */

		vdummy.ring.req_prod_pvt++;

		/* At this time the request is not visible yet to the other end.
		 * We could proceed with additional requests.
		 */


		/* Now, we are ready to push the first available requests
		 * and we let the macro decide if a notification is required or not.
		 */

		RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(&vdummy.ring, notify);

		if (notify)
			notify_remote_via_irq(vdummy.irq);

	}

#endif /* 0 */

	vdummy_end();
}

void vdummy_probe(void) {

	DBG0("[" VDUMMY_NAME "] Frontend probe\n");
}

/* At this point, the FE is not connected. */
void vdummy_reconfiguring(void) {

	DBG0("[" VDUMMY_NAME "] Frontend reconfiguring\n");
}

void vdummy_shutdown(void) {

	DBG0("[" VDUMMY_NAME "] Frontend shutdown\n");
}

void vdummy_closed(void) {

	DBG0("[" VDUMMY_NAME "] Frontend close\n");
}

void vdummy_suspend(void) {

	DBG0("[" VDUMMY_NAME "] Frontend suspend\n");
}

void vdummy_resume(void) {

	DBG0("[" VDUMMY_NAME "] Frontend resume\n");
}

int notify_fn(void *arg) {

	while (1) {
		msleep(50);

		vdummy_start();

		/* Make sure the backend is connected and ready for interactions. */

		notify_remote_via_irq(vdummy.irq);

		vdummy_end();

	}

	return 0;
}

void vdummy_connected(void) {
	DBG0("[" VDUMMY_NAME "] Frontend connected\n");

	/* Force the processing of pending requests, if any */
	notify_remote_via_irq(vdummy.irq);

	if (!thread_created) {
		thread_created = true;
#if 1
		kernel_thread(notify_fn, "notify_th", NULL, 0);
#endif
	}
}

static int vdummy_init(dev_t *dev) {

	vdummy_vbus_init();

	return 0;
}

REGISTER_DRIVER_POSTCORE("vdummy,frontend", vdummy_init);
