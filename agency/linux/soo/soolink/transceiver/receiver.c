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

#include <linux/mutex.h>

#include <soo/soolink/receiver.h>
#include <soo/soolink/datalink.h>
#include <soo/soolink/decoder.h>
#include <soo/soolink/discovery.h>
#include <soo/soolink/plugin.h>

#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>

static struct mutex receiver_lock;

/*
 * This function is called when a plugin has data available. Datalink will
 * forward the packet to the selected protocol. Datalink decides when
 * the packet has to be given back to the Receiver.
 * The size parameter refers to the whole transceiver packet.
 */
void __receiver_rx(sl_desc_t *sl_desc, void *packet, size_t size) {
	transceiver_packet_t *transceiver_packet;

	transceiver_packet = (transceiver_packet_t *) packet;
	transceiver_packet->size = size - sizeof(transceiver_packet_t);

	datalink_rx(sl_desc, packet);
}

/**
 * This function is called by Datalink when the packet is ready to be
 * forwarded to the consumer(s).
 * The size parameter refers to the whole transceiver packet.
 */
void receiver_rx(sl_desc_t *sl_desc, transceiver_packet_t *packet) {

	mutex_lock(&receiver_lock);

#if 0
	{
		int i;

		for (i = 0; i < transceiver_packet->size; i++)
			lprintk("%x ", transceiver_packet->payload[i]);
		lprintk("\n");
	}
#endif

	decoder_rx(sl_desc, packet->payload, packet->size);

	mutex_unlock(&receiver_lock);
}

void receiver_init(void) {
	mutex_init(&receiver_lock);
}
