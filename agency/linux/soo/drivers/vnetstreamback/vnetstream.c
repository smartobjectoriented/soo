/*
 * Copyright (C) 2018,2019 Baptiste Delporte <bonel@bonel.net>
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

#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/of.h>

#include <soolink/soolink.h>
#include <soolink/transceiver.h>
#include <soolink/datalink/winenet.h>

#include <soo/core/device_access.h>

#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/uapi/console.h>
#include <soo/uapi/soo.h>
#include <soo/guest_api.h>
#include <soo/uapi/debug.h>

#include <stdarg.h>
#include <linux/kthread.h>

#include <xenomai/rtdm/driver.h>
#include <rtdm/soo.h>

#include "common.h"

vnetstream_t vnetstream;

/* Soolink descriptor for the vNetstream interface */
static sl_desc_t *vnetstream_sl_desc;

/* Global variables for the command processing task */
static rtdm_task_t cmd_task;
static rtdm_event_t cmd_event;
static struct vbus_device *cmd_dev = NULL;
static vnetstream_cmd_t cmd_req = VNETSTREAM_CMD_NULL;
static long cmd_arg = 0;

/* Global variables for the packet TX task */
static rtdm_task_t send_task;
static rtdm_event_t send_event;
static void *send_data = NULL;

/* Global variables for the packet RX task */
static rtdm_task_t recv_task;

/* Must be global because of the on demand initialization of the shared buffer */
size_t vnetstream_packet_size = 0;

/**
 * Process a command coming from a frontend.
 */
int vnetstream_cmd_interrupt(rtdm_irq_t *handle) {
	struct vbus_device *dev = rtdm_irq_get_arg(handle, struct vbus_device);
	RING_IDX i, rp;
	vnetstream_cmd_request_t *ring_req;

	if (!vnetstream_is_connected(dev->otherend_id))
		return RTDM_IRQ_HANDLED;

	rp = vnetstream.cmd_rings[dev->otherend_id].ring.sring->req_prod;
	dmb();

	for (i = vnetstream.cmd_rings[dev->otherend_id].ring.sring->req_cons; i != rp; i++) {
		ring_req = RING_GET_REQUEST(&vnetstream.cmd_rings[dev->otherend_id].ring, i);

		DBG("0x%08x\n", ring_req->cmd);

		switch (ring_req->cmd) {
		case VNETSTREAM_CMD_STREAM_INIT:
		case VNETSTREAM_CMD_GET_NEIGHBOURHOOD:
		case VNETSTREAM_CMD_STREAM_TERMINATE:
			cmd_dev = dev;
			cmd_req = ring_req->cmd;
			cmd_arg = ring_req->arg;
			rtdm_event_signal(&cmd_event);
			break;

		default:
			BUG();
		}
	}

	vnetstream.cmd_rings[dev->otherend_id].ring.sring->req_cons = i;

	return RTDM_IRQ_HANDLED;
}

/**
 * Process a TX request coming from a frontend.
 */
int vnetstream_tx_interrupt(rtdm_irq_t *handle) {
	struct vbus_device *dev = rtdm_irq_get_arg(handle, struct vbus_device);
	RING_IDX i, rp;
	vnetstream_tx_request_t *ring_req;

	if (!vnetstream_is_connected(dev->otherend_id))
		return RTDM_IRQ_HANDLED;

	rp = vnetstream.tx_rings[dev->otherend_id].ring.sring->req_prod;
	dmb();

	for (i = vnetstream.tx_rings[dev->otherend_id].ring.sring->req_cons; i != rp; i++) {
		ring_req = RING_GET_REQUEST(&vnetstream.tx_rings[dev->otherend_id].ring, i);

		DBG("Offset=%d\n", ring_req->offset);
		/* Do not care about the offset */
		send_data = vnetstream.txrx_buffers[dev->otherend_id].data;

		rtdm_event_signal(&send_event);
	}

	vnetstream.tx_rings[dev->otherend_id].ring.sring->req_cons = i;

	return RTDM_IRQ_HANDLED;
}

/**
 * The rx_ring should not be used in this direction.
 */
int vnetstream_rx_interrupt(rtdm_irq_t *handle) {
	struct vbus_device *dev = rtdm_irq_get_arg(handle, struct vbus_device);
	RING_IDX i, rp;
	vnetstream_rx_request_t *ring_req;

	if (!vnetstream_is_connected(dev->otherend_id))
		return RTDM_IRQ_HANDLED;

	rp = vnetstream.rx_rings[dev->otherend_id].ring.sring->req_prod;
	dmb();

	for (i = vnetstream.rx_rings[dev->otherend_id].ring.sring->req_cons; i != rp; i++) {
		ring_req = RING_GET_REQUEST(&vnetstream.rx_rings[dev->otherend_id].ring, i);

		/* Nothing to do */
	}

	vnetstream.rx_rings[dev->otherend_id].ring.sring->req_cons = i;

	return RTDM_IRQ_HANDLED;
}

/**
 * Send a return value back to a frontend after the execution of a command.
 */
static void do_cmd_feedback(int ret, void *data) {
	uint32_t i;
	vnetstream_cmd_response_t *ring_rsp;

	for (i = 1; i < MAX_DOMAINS; i++) {
		if (!vnetstream_start(i))
			continue;

		ring_rsp = RING_GET_RESPONSE(&vnetstream.cmd_rings[i].ring, vnetstream.cmd_rings[i].ring.sring->rsp_prod);

		DBG("ret=%d\n", ret);
		ring_rsp->ret = ret;

		if (data)
			memcpy(ring_rsp->data, data, VNETSTREAM_CMD_DATA_SIZE);

		dmb();

		vnetstream.cmd_rings[i].ring.rsp_prod_pvt++;

		RING_PUSH_RESPONSES(&vnetstream.cmd_rings[i].ring);

		notify_remote_via_virq(vnetstream.cmd_rings[i].irq_handle.irq);

		vnetstream_end(i);
	}
}

/**
 * Send a return value back to a frontend after the transmission of a packet.
 */
static void do_tx_feedback(int ret) {
	uint32_t i;
	vnetstream_tx_response_t *ring_rsp;
	
	for (i = 1; i < MAX_DOMAINS; i++) {
		if (!vnetstream_start(i))
			continue;

		ring_rsp = RING_GET_RESPONSE(&vnetstream.tx_rings[i].ring, vnetstream.tx_rings[i].ring.sring->rsp_prod);

		DBG("ret=%d\n", ret);
		ring_rsp->ret = ret;

		dmb();

		vnetstream.tx_rings[i].ring.rsp_prod_pvt++;

		RING_PUSH_RESPONSES(&vnetstream.tx_rings[i].ring);

		notify_remote_via_virq(vnetstream.tx_rings[i].irq_handle.irq);

		vnetstream_end(i);
	}
}

/**
 * Forward an incoming message to a frontend.
 */
static void do_rx(void *data) {
	uint32_t i;
	vnetstream_rx_response_t *ring_rsp;

	for (i = 1; i < MAX_DOMAINS; i++) {
		if (!vnetstream_start(i))
			continue;

		ring_rsp = RING_GET_RESPONSE(&vnetstream.rx_rings[i].ring, vnetstream.rx_rings[i].ring.sring->rsp_prod);

#if 0 /* Do not care about the offset */
		ring_rsp->offset = (uint32_t) data - (uint32_t) vnetstream.txrx_buffers[i].data;
#else
		ring_rsp->offset = 0;
#endif /* 0*/
		DBG("Offset=%d\n", ring_rsp->offset);

		dmb();

		vnetstream.rx_rings[i].ring.rsp_prod_pvt++;

		RING_PUSH_RESPONSES(&vnetstream.rx_rings[i].ring);

		notify_remote_via_virq(vnetstream.rx_rings[i].irq_handle.irq);

		vnetstream_end(i);
	}
}

/**
 * Request a stream initialization operation, which tells the Datalink layer in netstream mode where the
 * data is, and the size of the burst packets.
 * The size refers to the payload.
 */
static void stream_init(struct vbus_device *dev, size_t size) {
	vnetstream_packet_size = size;

	vnetstream_setup_shared_buffer(dev);

	DBG("shared data=%08x\n", vnetstream.txrx_buffers[dev->otherend_id].data);

	rtdm_sl_stream_init(vnetstream_sl_desc, (void *) vnetstream.txrx_buffers[dev->otherend_id].data, vnetstream_packet_size);

	do_cmd_feedback(0, NULL);
}

/**
 * Send a buffer of the concatenated Smart Object agency UIDs in the neighbourhood.
 */
static void get_neighbourhood(void) {
	char data[VNETSTREAM_CMD_DATA_SIZE];
	struct list_head neighbour_list;
	struct list_head *cur;
	neighbour_desc_t *cur_neighbour;
	uint32_t count = 0;

	/* Reset the data buffer contents to 0 */
	memset(data, 0, VNETSTREAM_CMD_DATA_SIZE);

	INIT_LIST_HEAD(&neighbour_list);
	rtdm_sl_get_neighbours(&neighbour_list);

	list_for_each(cur, &neighbour_list) {
		cur_neighbour = list_entry(cur, neighbour_desc_t, list);
		DBG("Add neighbour %d: ", count); DBG_BUFFER(&cur_neighbour->agencyUID, SOO_AGENCY_UID_SIZE);
		memcpy(&data[count * SOO_AGENCY_UID_SIZE], &cur_neighbour->agencyUID, SOO_AGENCY_UID_SIZE);
		count++;
	}

	do_cmd_feedback(0, data);
}

/**
 * Send a return value back to a frontend after a stream termination command.
 */
static void stream_terminate(void) {
	vnetstream_packet_size = 0;

	do_cmd_feedback(0, NULL);
}

/**
 * Command processing task.
 */
static void cmd_task_fn(void *args) {
	while (1) {
		rtdm_event_wait(&cmd_event);

		switch (cmd_req) {
		case VNETSTREAM_CMD_STREAM_INIT:
			stream_init(cmd_dev, cmd_arg);
			break;

		case VNETSTREAM_CMD_GET_NEIGHBOURHOOD:
			get_neighbourhood();
			break;

		case VNETSTREAM_CMD_STREAM_TERMINATE:
			stream_terminate();
			break;

		default:
			BUG();
		}

		cmd_dev = NULL;
		cmd_req = VNETSTREAM_CMD_NULL;
		cmd_arg = 0;
	}
}

/**
 * Packet TX task.
 */
static void send_task_fn(void *args) {
	while (1) {
		rtdm_event_wait(&send_event);

		/* If Netstreamsim is enabled, do not interact with Soolink */
		rtdm_sl_stream_send(vnetstream_sl_desc, send_data);

		send_data = NULL;

		do_tx_feedback(0);
	}
}

/**
 * Packet receival task.
 */
static void recv_task_fn(void *args) {
	void *data;

	while (1) {
		rtdm_sl_stream_recv(vnetstream_sl_desc, &data);

		DBG("Recv %08x\n", data);
		do_rx(data);
	}
}

void vnetstream_probe(struct vbus_device *dev) {
	DBG(VNETSTREAM_PREFIX "Backend probe: %d\n", dev->otherend_id);
}

void vnetstream_close(struct vbus_device *dev) {
	DBG(VNETSTREAM_PREFIX "Backend close: %d\n", dev->otherend_id);
}

void vnetstream_suspend(struct vbus_device *dev) {
	DBG(VNETSTREAM_PREFIX "Backend suspend: %d\n", dev->otherend_id);
}

void vnetstream_resume(struct vbus_device *dev) {
	DBG(VNETSTREAM_PREFIX "Backend resume: %d\n", dev->otherend_id);
}

void vnetstream_reconfigured(struct vbus_device *dev) {
	DBG(VNETSTREAM_PREFIX "Backend reconfigured: %d\n", dev->otherend_id);
}

void vnetstream_connected(struct vbus_device *dev) {
	DBG(VNETSTREAM_PREFIX "Backend connected: %d\n", dev->otherend_id);

	notify_remote_via_virq(vnetstream.cmd_rings[dev->otherend_id].irq_handle.irq);
	notify_remote_via_virq(vnetstream.tx_rings[dev->otherend_id].irq_handle.irq);
}

int vnetstream_init(void) {
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "soo,vnetstream");

	/* Check if DTS has vuihandler enabled */
	if (!of_device_is_available(np))
		return 0;

	rtdm_event_init(&cmd_event, 0);
	rtdm_event_init(&send_event, 0);

	vnetstream_vbus_init();

	/* Register to SOOlink */
#if defined(CONFIG_MARVELL_MWIFIEX_MLAN)
	vnetstream_sl_desc = sl_register(SL_REQ_NETSTREAM, SL_IF_WLAN, SL_MODE_NETSTREAM);
#else
	vnetstream_sl_desc = sl_register(SL_REQ_NETSTREAM, SL_IF_ETH, SL_MODE_NETSTREAM);
#endif /* CONFIG_MARVELL_MWIFIEX_MLAN */

	rtdm_task_init(&cmd_task, "vnetstream_cmd", cmd_task_fn, NULL, VNETSTREAM_TASK_PRIO, 0);
	rtdm_task_init(&send_task, "vnetstream_send", send_task_fn, NULL, VNETSTREAM_TASK_PRIO, 0);
	rtdm_task_init(&recv_task, "vnetstream_recv", recv_task_fn, NULL, VNETSTREAM_TASK_PRIO, 0);

	/* Set the associated dev capability */
	devaccess_set_devcaps(DEVCAPS_CLASS_NET, DEVCAP_NET_STREAMING, true);
	devaccess_set_devcaps(DEVCAPS_CLASS_NET, DEVCAP_NET_MESSAGING, true);

	return 0;
}

module_init(vnetstream_init);
