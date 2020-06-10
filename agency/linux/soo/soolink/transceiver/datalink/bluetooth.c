/*
 * Copyright (C) 2020 David Truan <david.truan@heig-vd.ch>
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

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/time.h>

#include <soo/soolink/soolink.h>

#if 1
#define DEBUG
#endif


#include <soo/soolink/datalink/bluetooth.h>
#include <soo/soolink/datalink.h>
#include <soo/soolink/sender.h>
#include <soo/soolink/receiver.h>
#include <soo/soolink/discovery.h>

#include <soo/core/device_access.h>

#include <xenomai/rtdm/driver.h>

#include <soo/uapi/soo.h>

#include <soo/uapi/console.h>
#include <soo/uapi/soo.h>

/* Buffer to receive incoming packets before forwarding the to the Decoder */
static transceiver_packet_t *buffers[NB_BUF];
/* Current index in the packet buffer */
static int cur_buf_idx = 0;

/**
 * RX Bluetooth Datalink protocol.
 * 
 * It is based on the Winenet protocol but heavily simplified. It waits the reception of NB_BUF
 * packets before forwarding them to the Decoder. 
 */
void bluetooth_rx(sl_desc_t *sl_desc, plugin_desc_t *plugin_desc, void *packet_ptr, size_t size) {
	int i;
	/* Used to retrieve the info from the incoming data */
	transceiver_packet_t *packet;
	/* New buffer which will be allocated for the incoming packet */
	transceiver_packet_t *new_packet;

	packet = (transceiver_packet_t *)packet_ptr;
	/* Allocate the new packet */
	new_packet = (transceiver_packet_t *) kmalloc(packet->size + sizeof(transceiver_packet_t), GFP_ATOMIC);
	BUG_ON(new_packet == NULL);
	/* Store the packet in the buffer and copy the incoming data in it */
	buffers[cur_buf_idx] = new_packet; 
	memcpy(buffers[cur_buf_idx], packet, size + sizeof(transceiver_packet_t));

	/* Upgrade the index in the buffers */
	cur_buf_idx++;

	/* If we received NB_BUF packet or the transID indicates that it is the last packet,
	forward the currently received packets to the Decoder and free them afterward. */
	if (cur_buf_idx == NB_BUF || packet->transID == BT_LAST_PACKET) {
		cur_buf_idx = 0;
			
		for (i = 0; i < NB_BUF; ++i) {
			if (buffers[i] != NULL) {
				receiver_rx(sl_desc, plugin_desc, buffers[i], buffers[i]->size);
			}
		}

		for (i = 0; i < NB_BUF; ++i) {
			if (buffers[i] != NULL) {
				kfree(buffers[i]);
				buffers[i] = NULL;
			}
		}
	}

}

/**
 * Callbacks of the bluetooth protocol
 */
static datalink_proto_desc_t bluetooth_proto = {
	.rx_callback = bluetooth_rx,
};

/**
 * Register the bluetooth protocol with the Datalink subsystem. The protocol is associated
 * to the SL_PROTO_bluetooth ID.
 */
static void bluetooth_register(void) {
	datalink_register_protocol(SL_DL_PROTO_BT, &bluetooth_proto);
}

/**
 * Initialization of bluetooth.
 */
void bluetooth_init(void) {
	bluetooth_register();
}