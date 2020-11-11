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
#include <linux/mutex.h>

#include <soo/soolink/sender.h>
#include <soo/soolink/datalink.h>
#include <soo/soolink/plugin.h>

#include <xenomai/rtdm/driver.h>

#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>
#include <soo/debug/time.h>

#include <soo/soolink/receiver.h>

struct soo_sender_env {

	struct mutex sender_lock;
};

/**
 * This function requests to send a packet. Datalink will forward the packet
 * to the selected protocol. Datalink decides when the packet has to be sent.
 * This function is called by the producer.
 * The size parameter refers to the payload.
 */
int sender_tx(sl_desc_t *sl_desc, void *data, size_t size, bool completed) {
	int ret;
	transceiver_packet_t *packet;

	if (!data) {
		datalink_tx(sl_desc, NULL, true);
		return 0;
	}

	/* Allocate a transceiver packet and reserve enough memory for the payload */
	packet = (transceiver_packet_t *) kmalloc(size + sizeof(transceiver_packet_t), GFP_ATOMIC);

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
	mutex_lock(&current_soo_sender->sender_lock);

	plugin_tx(sl_desc, packet, packet->size + sizeof(transceiver_packet_t));
	mutex_unlock(&current_soo_sender->sender_lock);
}

void sender_init(void) {
	
	current_soo->soo_sender = kzalloc(sizeof(struct soo_sender_env), GFP_KERNEL);
	BUG_ON(!current_soo->soo_sender);

	mutex_init(&current_soo_sender->sender_lock);

}
