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

#include "common.h"

vnetstream_t vnetstream;

static completion_t cmd_completion;
static int cmd_ret = 0;
static char *cmd_data = NULL;

static completion_t send_completion;
static int tx_ret = 0;

/**
 * Process pending responses in the cmd ring.
 */
static void process_pending_cmd_rsp(void) {
	RING_IDX i, rp;
	vnetstream_cmd_response_t *ring_rsp;

	rp = vnetstream.cmd_ring.sring->rsp_prod;
	dmb();

	for (i = vnetstream.cmd_ring.sring->rsp_cons; i != rp; i++) {
		ring_rsp = RING_GET_RESPONSE(&vnetstream.cmd_ring, i);

		DBG("Ret=%d\n", ring_rsp->ret);
		cmd_ret = ring_rsp->ret;
		cmd_data = ring_rsp->data;

		complete(&cmd_completion);
	}

	vnetstream.cmd_ring.sring->rsp_cons = i;
}

/**
 * Process a command return value coming from the backend.
 */
irq_return_t vnetstream_cmd_interrupt(int irq, void *dev_id) {
	if (!vnetstream_is_connected())
		return IRQ_COMPLETED;

	process_pending_cmd_rsp();

	return IRQ_COMPLETED;
}

/**
 * Process pending responses in the tx ring.
 */
static void process_pending_tx_rsp(void) {
	RING_IDX i, rp;
	vnetstream_tx_response_t *ring_rsp;

	rp = vnetstream.tx_ring.sring->rsp_prod;
	dmb();

	for (i = vnetstream.tx_ring.sring->rsp_cons; i != rp; i++) {
		ring_rsp = RING_GET_RESPONSE(&vnetstream.tx_ring, i);

		DBG(VNETSTREAM_PREFIX "Ret=%d\n", ring_rsp->ret);
		tx_ret = ring_rsp->ret;

		complete(&send_completion);
	}

	vnetstream.tx_ring.sring->rsp_cons = i;
}

/**
 * Process a TX request return value coming from the backend.
 */
irq_return_t vnetstream_tx_interrupt(int irq, void *dev_id) {

	if (!vnetstream_is_connected())
		return IRQ_COMPLETED;

	process_pending_tx_rsp();

	return IRQ_COMPLETED;
}

/**
 * Process pending responses in the rx ring.
 */
static void process_pending_rx_rsp(void) {
	RING_IDX i, rp;
#ifdef DEBUG
	vnetstream_rx_response_t *ring_rsp;
#endif /* DEBUG */

	rp = vnetstream.rx_ring.sring->rsp_prod;
	dmb();

	for (i = vnetstream.rx_ring.sring->rsp_cons; i != rp; i++) {
#ifdef DEBUG
		ring_rsp = RING_GET_RESPONSE(&vnetstream.rx_ring, i);

		DBG("Offset=%d, recv %08x\n", ring_rsp->offset, (uint32_t) vnetstream.txrx_data + ring_rsp->offset);
#endif /* DEBUG */

#if defined(CONFIG_NETSTREAM_MESSAGING)
		/* This function must be provided by the client app */
		recv_interrupt(vnetstream.txrx_data + ring_rsp->offset);
#endif /* CONFIG_NETSTREAM_MESSAGING */
	}

	vnetstream.rx_ring.sring->rsp_cons = i;
}

/**
 * Process a RX message event coming from the backend.
 */
irq_return_t vnetstream_rx_interrupt(int irq, void *dev_id) {

	if (!vnetstream_is_connected())
		return IRQ_COMPLETED;

	process_pending_rx_rsp();

	return IRQ_COMPLETED;
}

/**
 * Forward a command to the backend.
 */
static void do_cmd(vnetstream_cmd_t cmd, long arg) {
	vnetstream_cmd_request_t *ring_req;

	vnetstream_start();

	DBG("Cmd=0x%08x\n", cmd);

	switch (cmd) {
	case VNETSTREAM_CMD_STREAM_INIT:
	case VNETSTREAM_CMD_GET_NEIGHBOURHOOD:
	case VNETSTREAM_CMD_STREAM_TERMINATE:
		break;

	default:
		BUG();
	}

	ring_req = RING_GET_REQUEST(&vnetstream.cmd_ring, vnetstream.cmd_ring.req_prod_pvt);

	ring_req->cmd = cmd;
	ring_req->arg = arg;

	dmb();

	vnetstream.cmd_ring.req_prod_pvt++;

	RING_PUSH_REQUESTS(&vnetstream.cmd_ring);

	notify_remote_via_irq(vnetstream.cmd_irq);

	vnetstream_end();
}

/**
 * Forward a TX request to the backend.
 */
static void do_tx(void *data) {
	vnetstream_tx_request_t *ring_req;

	vnetstream_start();

	ring_req = RING_GET_REQUEST(&vnetstream.tx_ring, vnetstream.tx_ring.req_prod_pvt);

#if 0 /* Do not care about the offset */
	ring_req->offset = (uint32_t) data - (uint32_t) vnetstream.txrx_buffer.data;
#else
	ring_req->offset = 0;
#endif /* 0*/
	DBG("Offset=%d\n", ring_req->offset);

	dmb();

	vnetstream.tx_ring.req_prod_pvt++;

	RING_PUSH_REQUESTS(&vnetstream.tx_ring);

	notify_remote_via_irq(vnetstream.tx_irq);

	vnetstream_end();
}

/**
 * Request a stream initialization operation, which tells the Datalink layer in netstream mode where the
 * data is, and the size of the burst packets.
 * The size refers to the payload.
 */
void vnetstream_stream_init(void *data, size_t size) {
	/* Set the information related to the data buffer */
	vnetstream_set_shared_buffer(data, size);
	/* Compute the pfn and store it into vbstore */
	vnetstream_setup_shared_buffer();

	/* Forward the stream initialization request to the backend */
	do_cmd(VNETSTREAM_CMD_STREAM_INIT, (long) size);

	wait_for_completion(&cmd_completion);
}

/**
 * Get the number of neighbours and a snapshot of the neighbour list.
 * This function should be called if the stream initialization has been done because the neighbourhood
 * has to be stable.
 * The returned pointer points to dynmically allocated data that must be freed after use.
 */
char *vnetstream_get_neighbourhood(void) {
	char *ret = (char *) malloc(VNETSTREAM_CMD_DATA_SIZE);

	do_cmd(VNETSTREAM_CMD_GET_NEIGHBOURHOOD, 0);

	wait_for_completion(&cmd_completion);

	memset(ret, 0, VNETSTREAM_CMD_DATA_SIZE);
	memcpy(ret, cmd_data, VNETSTREAM_CMD_DATA_SIZE);

	return ret;
}

/**
 * Send a packet.
 */
void vnetstream_send(void *data) {
	do_tx((void *) data);

	/* Wait for the command feedback */
	wait_for_completion(&send_completion);
}

/**
 * Request the stream termination.
 */
void vnetstream_stream_terminate(void) {
	do_cmd(VNETSTREAM_CMD_STREAM_TERMINATE, 0);

	wait_for_completion(&cmd_completion);
}

void vnetstream_probe(void) {
	DBG0(VNETSTREAM_PREFIX "Frontend probe\n");

}

void vnetstream_reconfiguring(void) {
	DBG0(VNETSTREAM_PREFIX "Frontend reconfiguring\n");

	process_pending_cmd_rsp();
	process_pending_tx_rsp();
	process_pending_rx_rsp();
}

void vnetstream_shutdown(void) {
	DBG0(VNETSTREAM_PREFIX "Frontend shutdown\n");
}

void vnetstream_closed(void) {
	DBG0(VNETSTREAM_PREFIX "Frontend close\n");
}

void vnetstream_suspend(void) {
	DBG0(VNETSTREAM_PREFIX "Frontend suspend\n");
}

void vnetstream_resume(void) {
	DBG0(VNETSTREAM_PREFIX "Frontend resume\n");

	process_pending_cmd_rsp();
	process_pending_tx_rsp();
	process_pending_rx_rsp();
}

void vnetstream_connected(void) {
	DBG0(VNETSTREAM_PREFIX "Frontend connected\n");

	notify_remote_via_irq(vnetstream.cmd_irq);
	notify_remote_via_irq(vnetstream.tx_irq);
	notify_remote_via_irq(vnetstream.rx_irq);
}

int vnetstream_init(dev_t *dev) {
	vnetstream_vbus_init();

	return 0;
}

REGISTER_DRIVER_POSTCORE("vnetstream,frontend", vnetstream_init);
