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
#include <sync.h>
#include <device/driver.h>

#include <soo/evtchn.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/console.h>
#include <soo/debug.h>

#include <soo/dev/vuihandler.h>

vuihandler_t vuihandler;

ui_update_spid_t __ui_update_spid = NULL;
ui_interrupt_t __ui_interrupt = NULL;

/* Sent BT packet count */
static uint32_t send_count = 0;

/* In lib/vsprintf.c */
unsigned long simple_strtoul(const char *cp, char **endp, unsigned int base);

/**
 * Read the current connected application ME SPID in vbstore.
 */
static void get_app_spid(uint8_t spid[SPID_SIZE]) {
	uint32_t i;
	int len, res;
	unsigned long spid_number;
	char spid_digit[3] = { 0 };
	char connected_app_spid[3 * SPID_SIZE];

	res = vbus_scanf(VBT_NIL, VUIHANDLER_APP_VBSTORE_DIR, VUIHANDLER_APP_VBSTORE_NODE, "%s", connected_app_spid);
	if (res != 1) {
		lprintk(VUIHANDLER_PREFIX "Error when retrieving connected app ME SPID: %d\n", res);
		BUG();
		return ;
	}

	len = strlen(connected_app_spid);

	if (len != (3 * SPID_SIZE - 1)) {
		lprintk(VUIHANDLER_PREFIX "Invalid connected app ME SPID: %s\n", connected_app_spid);
		BUG();
		return ;
	}

	for (i = 0 ; i < SPID_SIZE ; i++) {
		memcpy(spid_digit, &connected_app_spid[3 * i], 2);
		spid_number = simple_strtoul(spid_digit, NULL, 16);
		spid[i] = (uint8_t) spid_number;
	}
}

/**
 * Function called when the connected application ME SPID changes. This allows the detection
 * of the remote application running on the tablet.
 */
void vuihandler_app_watch_fn(struct vbus_watch *watch) {
	uint8_t spid[SPID_SIZE];

	vuihandler_start();

	get_app_spid(spid);

#ifdef DEBUG
	DBG(VUIHANDLER_PREFIX "ME SPID: ");
	lprintk_buffer(spid, SPID_SIZE);
#endif /* DEBUG */

	if (__ui_update_spid)
		(*__ui_update_spid)(spid);

	vuihandler_end();
}

/**
 * Process pending responses in the tx_ It should not be used in this direction.
 */
static void process_pending_tx_rsp(void) {
	RING_IDX i, rp;

	rp = vuihandler.tx_ring.sring->rsp_prod;
	dmb();

	for (i = vuihandler.tx_ring.sring->rsp_cons; i != rp; i++) {
		/* Do nothing */
	}

	vuihandler.tx_ring.sring->rsp_cons = i;
}

/**
 * tx_ring interrupt. It should not be used in this direction.
 */
irq_return_t vuihandler_tx_interrupt(int irq, void *dev_id) {
	if (!vuihandler_is_connected())
		return IRQ_COMPLETED;

	process_pending_tx_rsp();

	return IRQ_COMPLETED;
}

/**
 * Process pending responses in the rx_
 */
static void process_pending_rx_rsp(void) {
	RING_IDX i, rp;
	vuihandler_rx_response_t *ring_rsp;

	rp = vuihandler.rx_ring.sring->rsp_prod;
	dmb();

	for (i = vuihandler.rx_ring.sring->rsp_cons; i != rp; i++) {
		ring_rsp = RING_GET_RESPONSE(&vuihandler.rx_ring, i);

		if (__ui_interrupt)
			(*__ui_interrupt)(vuihandler.rx_data + (ring_rsp->id % VUIHANDLER_MAX_PACKETS) * VUIHANDLER_MAX_PKT_SIZE, ring_rsp->size);
	}

	vuihandler.rx_ring.sring->rsp_cons = i;
}

/**
 * rx_ring interrupt.
 */
irq_return_t vuihandler_rx_interrupt(int irq, void *dev_id) {
	if (!vuihandler_is_connected())
		return IRQ_COMPLETED;

	process_pending_rx_rsp();

	return IRQ_COMPLETED;
}

/**
 * Send a packet to the tablet/smartphone.
 */
void vuihandler_send(void *data, size_t size) {
	vuihandler_tx_request_t *ring_req;

	vuihandler_start();

	DBG(VUIHANDLER_PREFIX "0x%08x %d\n", (unsigned int) data, size);

	ring_req = RING_GET_REQUEST(&vuihandler.tx_ring, vuihandler.tx_ring.req_prod_pvt);

	ring_req->id = send_count;
	ring_req->size = size;

	memcpy(vuihandler.tx_data + (ring_req->id % VUIHANDLER_MAX_PACKETS) * VUIHANDLER_MAX_PKT_SIZE, data, size);

	dmb();

	vuihandler.tx_ring.req_prod_pvt++;

	RING_PUSH_REQUESTS(&vuihandler.tx_ring);

	notify_remote_via_irq(vuihandler.tx_irq);

	send_count++;

	vuihandler_end();
}

void vuihandler_probe(void) {
	DBG0(VUIHANDLER_PREFIX "Frontend probe\n");
}

void vuihandler_suspend(void) {
	DBG0(VUIHANDLER_PREFIX "Frontend suspend\n");
}

void vuihandler_resume(void) {
	DBG0(VUIHANDLER_PREFIX "Frontend resume\n");

	process_pending_rx_rsp();
}

void vuihandler_connected(void) {
	DBG0(VUIHANDLER_PREFIX "Frontend connected\n");

	/* Force the processing of pending requests, if any */
	notify_remote_via_irq(vuihandler.tx_irq);
	notify_remote_via_irq(vuihandler.rx_irq);
}

void vuihandler_reconfiguring(void) {
	DBG0(VUIHANDLER_PREFIX "Frontend reconfiguring\n");
}

void vuihandler_shutdown(void) {
	DBG0(VUIHANDLER_PREFIX "Frontend shutdown\n");
}

void vuihandler_closed(void) {
	DBG0(VUIHANDLER_PREFIX "Frontend close\n");
}

void vuihandler_register_callback(ui_update_spid_t ui_update_spid, ui_interrupt_t ui_interrupt) {
	__ui_update_spid = ui_update_spid;
	__ui_interrupt = ui_interrupt;
}

static int vuihandler_init(dev_t *dev) {

	vuihandler_vbus_init();

	return 0;
}

REGISTER_DRIVER_POSTCORE("vuihandler,frontend", vuihandler_init);
