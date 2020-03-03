/*
 * Copyright (C) 2017-2019 Baptiste Delporte <bonel@bonel.net>
 * Copyright (C) 2017-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <linux/bug.h>

#include <soo/soolink/sender.h>
#include <soo/soolink/datalink.h>
#include <soo/soolink/plugin.h>

#include <xenomai/rtdm/driver.h>

#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>
#include <soo/debug/time.h>

#include <soo/soolink/receiver.h>

extern bool datalink_ready_to_send(sl_desc_t *sl_desc);

bool sender_ready_to_send(sl_desc_t *sl_desc) {
	return datalink_ready_to_send(sl_desc);
}

/*
 * Prepare a transmission along the TX path. We give the Datalink layer a change to acquire a medium.
 * Typically, modification on the contents can still be done.
 */
void sender_request_xmit(sl_desc_t *sl_desc) {
	datalink_request_xmit(sl_desc);
}

/**
 * This function requests to send a packet. Datalink will forward the packet
 * to the selected protocol. Datalink decides when the packet has to be sent.
 * This function is called by the producer.
 * The size parameter refers to the payload.
 */
int sender_xmit(sl_desc_t *sl_desc, void *data, size_t size, bool completed) {
	int ret;
	transceiver_packet_t *packet;

	if (!data) {
		datalink_xmit(sl_desc, NULL, 0, true);
		return 0;
	}

	/* Allocate a transceiver packet and reserve enough memory for the payload */
	packet = (transceiver_packet_t *) kmalloc(size + sizeof(transceiver_packet_t), GFP_ATOMIC);

	packet->packet_type = TRANSCEIVER_PKT_DATA;
	packet->size = size;

	/* Copy the data into the transceiver packet's payload */
	memcpy(packet->payload, data, size);

	ret = datalink_xmit(sl_desc, packet, size, completed);

	/* Release the transcoder packet */
	kfree(packet);

	return ret;
}

/**
 * Send data in netstream mode.
 * The data pointer points to the payload.
 */
int sender_stream_xmit(sl_desc_t *sl_desc, void *data) {
	/* No netstream transceiver packet allocation is necessary */
	return datalink_xmit(sl_desc, data, 0, false);
}

/**
 * This function is called by Datalink when the packet is ready to be
 * forwarded to the plugin(s). It should not be called by anyone else.
 * The size parameter refers to the payload.
 */
void sender_tx(sl_desc_t *sl_desc, void *packet, size_t size, unsigned long flags) {
	size_t packet_size = 0;

	switch (sl_desc->trans_mode) {
	case SL_MODE_BROADCAST:
	case SL_MODE_UNIBROAD:
	case SL_MODE_UNICAST:
		/* Add the transceiver's packet header size to the total size */
		packet_size = size + sizeof(transceiver_packet_t);
		break;
	case SL_MODE_NETSTREAM:
		/* Add the transceiver's packet header size to the total size */
		packet_size = size + sizeof(netstream_transceiver_packet_t);
		break;
	}

	plugin_tx(sl_desc, packet, packet_size, flags);
}

void sender_init(void) {
	
}
