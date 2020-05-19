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

#include <soo/dev/vdummy.h>

vdummy_t vdummy;

static bool thread_created = false;

irq_return_t vdummy_interrupt(int irq, void *dev_id) {
	vdummy_response_t *vdummy_response;

	if (!vdummy_is_connected())
		return IRQ_COMPLETED;

	DBG("%s, %d\n", __func__, ME_domID());

	vdummy_response = vdummy_ring_response_next(&vdummy.ring);

	/* Do something... */

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

	ring_req = vdummy_ring_request_next(&vdummy.ring);
	if (ring_req) {
		memcpy(ring_req->buffer, buffer, VDUMMY_PACKET_SIZE);

		vdummy_ring_request_ready(&vdummy.ring);

		notify_remote_via_irq(vdummy.irq);
	}

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
