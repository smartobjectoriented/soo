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

#include <soo/soolink/receiver.h>
#include <soo/soolink/datalink.h>
#include <soo/soolink/decoder.h>
#include <soo/soolink/discovery.h>
#include <soo/soolink/plugin.h>

#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>

static rtdm_mutex_t receiver_lock;

/*
 * This function is called when a plugin has data available. Datalink will
 * forward the packet to the selected protocol. Datalink decides when
 * the packet has to be given back to the Receiver.
 * The size parameter refers to the whole transceiver packet.
 */
void receiver_request_rx(sl_desc_t *sl_desc, plugin_desc_t *plugin_desc, void *packet, size_t size) {
	transceiver_packet_t *transceiver_packet;

	if (sl_desc->req_type != SL_REQ_NETSTREAM) {
		transceiver_packet = (transceiver_packet_t *) packet;
		transceiver_packet->size = size;
	}

	datalink_rx(sl_desc, plugin_desc, packet, size);
}

/**
 * This function is called by Datalink when the packet is ready to be
 * forwarded to the consumer(s).
 * The size parameter refers to the whole transceiver packet. It is set to 0 in netstream mode.
 */
void receiver_rx(sl_desc_t *sl_desc, plugin_desc_t *plugin_desc, void *packet, size_t size) {
	transceiver_packet_t *transceiver_packet;
	size_t payload_size;

	rtdm_mutex_lock(&receiver_lock);

	switch (sl_desc->req_type) {
	case SL_REQ_NETSTREAM:
		decoder_stream_rx(sl_desc, packet);
		break;

	case SL_REQ_DISCOVERY:
		transceiver_packet = (transceiver_packet_t *) packet;
		/* Substract the transceiver's packet header size from the total size */
		payload_size = size - sizeof(transceiver_packet_t);
		discovery_rx(plugin_desc, transceiver_packet->payload, payload_size);
		break;

	default:
		transceiver_packet = (transceiver_packet_t *) packet;
		/* Substract the transceiver's packet header size from the total size */
		payload_size = size - sizeof(transceiver_packet_t);
		decoder_rx(sl_desc, transceiver_packet->payload, payload_size);
	}

	rtdm_mutex_unlock(&receiver_lock);
}

void receiver_init(void) {
	rtdm_mutex_init(&receiver_lock);
}
