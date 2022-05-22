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

#include <linux/bug.h>
#include <linux/mutex.h>

#include <soo/soolink/datalink.h>
#include <soo/soolink/plugin.h>
#include <soo/soolink/transcoder.h>
#include <soo/soolink/transceiver.h>

#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>
#include <soo/debug/time.h>

#include <soo/debug.h>

struct soo_transceiver_env {
	struct mutex sender_lock;
        struct mutex receiver_lock;
};

/*
 * This function is called when a plugin has data available. Datalink will
 * forward the packet to the selected protocol. Datalink decides when
 * the packet has to be given back to the Receiver.
 * The size parameter refers to the whole transceiver packet.
 */
void __receiver_rx(sl_desc_t *sl_desc, void *packet, uint32_t size) {
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

	mutex_lock(&current_soo_transceiver->receiver_lock);

#if 0
	{
		int i;

		for (i = 0; i < packet->size; i++)
			lprintk("%x ", packet->payload[i]);
		lprintk("\n");
	}
#endif

	decoder_rx(sl_desc, packet->payload, packet->size);

	mutex_unlock(&current_soo_transceiver->receiver_lock);
}


/**
 * This function requests to send a packet. Datalink will forward the packet
 * to the selected protocol. Datalink decides when the packet has to be sent.
 * This function is called by the producer.
 * The size parameter refers to the payload.
 */
int sender_tx(sl_desc_t *sl_desc, void *data, uint32_t size, bool completed) {
	int ret;
	transceiver_packet_t *packet;

	if (!data) {
		datalink_tx(sl_desc, NULL, true);
		return 0;
	}

	/* Allocate a transceiver packet and reserve enough memory for the payload */
	packet = (transceiver_packet_t *) kzalloc(size + sizeof(transceiver_packet_t), GFP_KERNEL);
	BUG_ON(!packet);

	packet->packet_type = TRANSCEIVER_PKT_DATA;
	packet->size = size;

	/* Copy the data into the transceiver packet's payload */
	memcpy(packet->payload, data, size);

	ret = datalink_tx(sl_desc, packet, completed);

	/* Release the transceiver packet */
	kfree(packet);

	return ret;
}

/**
 * This function is called by Datalink when the packet is ready to be
 * forwarded to the plugin(s). It should not be called by anyone else.
 * The size parameter refers to the payload.
 */
void __sender_tx(sl_desc_t *sl_desc, transceiver_packet_t *packet) {
	
        mutex_lock(&current_soo_transceiver->sender_lock);
	plugin_tx(sl_desc, packet, packet->size + sizeof(transceiver_packet_t));
	mutex_unlock(&current_soo_transceiver->sender_lock);
}

void receiver_cancel_rx(sl_desc_t *sl_desc) {
	datalink_cancel_rx(sl_desc);
}

void transceiver_init(void) {
	
	current_soo->soo_transceiver = kzalloc(sizeof(struct soo_transceiver_env), GFP_KERNEL);
	BUG_ON(!current_soo->soo_transceiver);

	mutex_init(&current_soo_transceiver->sender_lock);
        mutex_init(&current_soo_transceiver->receiver_lock);

        transceiver_plugin_init();

}


