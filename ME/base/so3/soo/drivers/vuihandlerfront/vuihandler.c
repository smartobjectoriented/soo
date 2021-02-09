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

#include <soo/evtchn.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/console.h>
#include <soo/debug.h>

#include <soo/dev/vuihandler.h>

ui_update_spid_t __ui_update_spid = NULL;
ui_interrupt_t __ui_interrupt = NULL;

/* Sent BT packet count */
static uint32_t send_count = 0;

static completion_t *send_compl;

/* Global pointer */
vuihandler_t *__vuihandler;


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
 * Process pending responses in the tx_ It should not be used in this direction.
 */
static void process_pending_tx_rsp(struct vbus_device *vdev) {
	RING_IDX i, rp;
	vuihandler_t *vuihandler = to_vuihandler(vdev);
	rp = vuihandler->tx_ring.sring->rsp_prod;
	dmb();

	for (i = vuihandler->tx_ring.sring->rsp_cons; i != rp; i++) {
		/* Do nothing */
	}

	vuihandler->tx_ring.sring->rsp_cons = i;
}

/**
 * tx_ring interrupt. It should not be used in this direction.
 */
irq_return_t vuihandler_tx_interrupt(int irq, void *dev_id) {
	struct vbus_device *vdev = (struct vbus_device *) dev_id;

	process_pending_tx_rsp(vdev);

	return IRQ_COMPLETED;
}

/**
 * Process pending responses in the rx_
 */
static void process_pending_rx_rsp(struct vbus_device *vdev) {
	RING_IDX i, rp;
	vuihandler_rx_response_t *ring_rsp;
	vuihandler_t *vuihandler = to_vuihandler(vdev);

	rp = vuihandler->rx_ring.sring->rsp_prod;
	dmb();

	while ((ring_rsp = vuihandler_rx_ring_response(&vuihandler->rx_ring)) != NULL) {

		if (__ui_interrupt)
			(*__ui_interrupt)(vuihandler->rx_data + (ring_rsp->id % VUIHANDLER_MAX_PACKETS) * VUIHANDLER_MAX_PKT_SIZE, ring_rsp->size);
	
	}

	vuihandler->rx_ring.sring->rsp_cons = i;
}

/**
 * rx_ring interrupt.
 */
irq_return_t vuihandler_rx_interrupt(int irq, void *dev_id) {
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vuihandler_t *vuihandler = to_vuihandler(vdev);

	process_pending_rx_rsp(vdev);

	return IRQ_COMPLETED;
}

struct send_param {
	void *data;
	size_t size;
};

static struct send_param sp;

/**
 * Send a packet to the tablet/smartphone.
 */
void vuihandler_send(void *data, size_t size) {
	sp.data = data;
	sp.size = size;
	complete(send_compl);
	// vuihandler_tx_start();

	// vuihandler_tx_end();
}

int vuihandler_send_fn(void *arg) {
	// struct send_params *sp = (struct send_params *) arg;
	struct vbus_device *vdev = (struct vbus_device *) arg;
	vuihandler_tx_request_t *ring_req;

	vuihandler_t *vuihandler = to_vuihandler(vdev);

	while(1) {
		wait_for_completion(send_compl);
		DBG(VUIHANDLER_PREFIX "0x%08x %d\n", (unsigned int) data, size);
		ring_req = RING_GET_REQUEST(&vuihandler->tx_ring, vuihandler->tx_ring.req_prod_pvt);

		ring_req->id = send_count;
		ring_req->size = sp.size;

		memcpy(vuihandler->tx_data + (ring_req->id % VUIHANDLER_MAX_PACKETS) * VUIHANDLER_MAX_PKT_SIZE, sp.data, sp.size);

		dmb();

		vuihandler->tx_ring.req_prod_pvt++;

		RING_PUSH_REQUESTS(&vuihandler->tx_ring);

		notify_remote_via_irq(vuihandler->tx_irq);

		send_count++;

	}

	return 0;
}

void vuihandler_probe(struct vbus_device *vdev) {
	DBG0(VUIHANDLER_PREFIX "Frontend probe\n");
}

void vuihandler_suspend(struct vbus_device *vdev) {
	DBG0(VUIHANDLER_PREFIX "Frontend suspend\n");
}

void vuihandler_resume(struct vbus_device *vdev) {
	DBG0(VUIHANDLER_PREFIX "Frontend resume\n");

	process_pending_rx_rsp(vdev);
}

void vuihandler_connected(struct vbus_device *vdev) {
	vuihandler_t *vuihandler = to_vuihandler(vdev);


	send_compl = malloc(sizeof(completion_t));
	
	// sp->vdev = vdev;
	// sp->send_completion = send_compl;


	DBG0(VUIHANDLER_PREFIX "Frontend connected\n");

	/* Force the processing of pending requests, if any */
	notify_remote_via_irq(vuihandler->tx_irq);
	notify_remote_via_irq(vuihandler->rx_irq);

	init_completion(send_compl);

	kernel_thread(vuihandler_send_fn, "vuihandler_send_fn", (void *) vdev, 0);
}

void vuihandler_reconfiguring(struct vbus_device *vdev) {
	DBG0(VUIHANDLER_PREFIX "Frontend reconfiguring\n");
}

void vuihandler_shutdown(struct vbus_device *vdev) {
	DBG0(VUIHANDLER_PREFIX "Frontend shutdown\n");
}

void vuihandler_closed(struct vbus_device *vdev) {
	DBG0(VUIHANDLER_PREFIX "Frontend close\n");
}

void vuihandler_register_callback(ui_update_spid_t ui_update_spid, ui_interrupt_t ui_interrupt) {
	__ui_update_spid = ui_update_spid;
	__ui_interrupt = ui_interrupt;
}

vdrvfront_t vuihandlerdrv = {
	.probe = vuihandler_probe,
	.reconfiguring = vuihandler_reconfiguring,
	.shutdown = vuihandler_shutdown,
	.closed = vuihandler_closed,
	.suspend = vuihandler_suspend,
	.resume = vuihandler_resume,
	.connected = vuihandler_connected
};

static int vuihandler_init(dev_t *dev) {

	vdevfront_init(VUIHANDLER_NAME, &vuihandlerdrv);

	return 0;
}

REGISTER_DRIVER_POSTCORE("vuihandler,frontend", vuihandler_init);
