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

#include <mutex.h>
#include <heap.h>
#include <completion.h>

#include <device/driver.h>
#include <device/irq.h>

#include <asm/atomic.h>

#include <soo/evtchn.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/console.h>
#include <soo/debug.h>

#include <soo/dev/vuart.h>

vuart_t vuart;
completion_t reader_wait;

bool vuart_ready(void) {
	return vuart_is_connected();
}

void process_pending_rsp(void) {
	complete(&reader_wait);
}

irq_return_t vuart_interrupt(int irq, void *dev_id) {

	if (!vuart_is_connected())
		return IRQ_COMPLETED;

	process_pending_rsp();

	return IRQ_COMPLETED;
}

/**
 * Send a string on the vuart device.
 */
void vuart_write(char *buffer, int count) {
	int pos;
	vuart_request_t *ring_req;

	vuart_start();

	pos = 0;

	while ((pos < count) && RING_FREE_REQUESTS(&vuart.ring)) {
		ring_req = RING_GET_REQUEST(&vuart.ring, vuart.ring.req_prod_pvt);

		ring_req->c = buffer[pos];

		dmb();

		vuart.ring.req_prod_pvt++;

		pos++;
	}

	RING_PUSH_REQUESTS(&vuart.ring);

	notify_remote_via_irq(vuart.irq);

	vuart_end();

}

char vuart_read_char(void) {
	vuart_response_t *ring_rsp;
	char byte;

	while (vuart.ring.sring->rsp_cons == vuart.ring.sring->rsp_prod)
		/* Wait for a character available. */
		wait_for_completion(&reader_wait);

	ring_rsp = RING_GET_RESPONSE(&vuart.ring, vuart.ring.sring->rsp_cons);
	byte = ring_rsp->c;

	vuart.ring.sring->rsp_cons++;

	return byte;
}

void vuart_probe(void) {
	DBG0(VUART_PREFIX "Frontend probe\n");
}

void vuart_reconfiguring(void) {

	DBG0(VUART_PREFIX "Frontend reconfiguring\n");

	process_pending_rsp();
}

void vuart_shutdown(void) {

	DBG0(VUART_PREFIX "Frontend shutdown\n");
}

void vuart_closed(void) {

	DBG0(VUART_PREFIX "Frontend close\n");
}

void vuart_suspend(void) {

	DBG0(VUART_PREFIX "Frontend suspend\n");
}

void vuart_resume(void) {

	DBG0(VUART_PREFIX "Frontend resume\n");

	process_pending_rsp();
}

void vuart_connected(void) {

	DBG0(VUART_PREFIX "Frontend connected\n");

	/* Force the processing of pending requests, if any */
	notify_remote_via_irq(vuart.irq);
}

static int vuart_init(dev_t *dev) {

	init_completion(&reader_wait);

	vuart_vbus_init();

	return 0;
}

REGISTER_DRIVER_POSTCORE("vuart,frontend", vuart_init);
