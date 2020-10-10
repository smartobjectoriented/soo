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
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>

#include <soo/soolink/transcoder.h>
#include <soo/soolink/coder.h>
#include <soo/soolink/decoder.h>
#include <soo/soolink/sender.h>
#include <soo/soolink/datalink.h>

#include <soo/debug/bandwidth.h>

#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>
#include <soo/uapi/soo.h>

static struct mutex coder_tx_lock;

/**
 * Send data according to requirements based on the sl_desc descriptor and performs
 * consistency algorithms/packet splitting if required.
 */
void coder_send(sl_desc_t *sl_desc, void *data, size_t size) {
	transcoder_packet_t *pkt;
	uint32_t packetID, nr_packets;
	bool completed;

	soo_log("[soo:soolink:transcoder:coder] Sending sending %d bytes...\n", size);

	/* Bypass the Coder if the requester is of Bluetooth or TCP type */
	if ((sl_desc->if_type == SL_IF_BT) || (sl_desc->if_type == SL_IF_TCP) || (sl_desc->req_type == SL_REQ_PEER)) {
		pkt = kmalloc(sizeof(transcoder_packet_format_t) + size, GFP_ATOMIC);

		/* In fact, do not care about the consistency_type field */
		pkt->u.simple.consistency_type = CODER_CONSISTENCY_SIMPLE;
		memcpy(pkt->payload, data, size);

		/* Forward the packet to the Transceiver */
		sender_tx(sl_desc, pkt, sizeof(transcoder_packet_format_t) + size, true);

		kfree(pkt);

		return ;
	}

	/* End of transmission ? */
#warning sending all MEs at each time ?
	if (!data) {
		sender_tx(sl_desc, NULL, 0, true);
		return ;
	}

	/*
	 * Take the lock for managing the block and packets.
	 * Protecting the access to the global transID counter.
	 */
	mutex_lock(&coder_tx_lock);

	/* Check if the block has to be split into multiple packets */
	if (size <= SL_PACKET_PAYLOAD_MAX_SIZE) {
		DBG("Simple packet\n");

		/* Create the simple packet */
		pkt = kmalloc(sizeof(transcoder_packet_format_t) + size, GFP_ATOMIC);

		pkt->u.simple.consistency_type = CODER_CONSISTENCY_SIMPLE;
		memcpy(pkt->payload, data, size);

		/* We forward the packet to the Transceiver. The size at the reception
		 * will be taken from the plugin (RX).
		 */

		sender_tx(sl_desc, pkt, sizeof(transcoder_packet_format_t) + size, true);

		kfree(pkt);

	} else {

		/* Determine the number of packets required for this block */
		nr_packets = DIV_ROUND_UP(size, SL_PACKET_PAYLOAD_MAX_SIZE);

		DBG("Extended packet, nr_packets=%d\n", nr_packets);

		/* Need to iterate over multiple packets */
		pkt = kmalloc(sizeof(transcoder_packet_format_t) + SL_PACKET_PAYLOAD_MAX_SIZE, GFP_ATOMIC);
		BUG_ON(!pkt);

		for (packetID = 1; packetID < nr_packets + 1; packetID++) {

			/* Tell Datalink that this is the last packet in the block */
			completed = (packetID == nr_packets);

			/* Create an extended packet */

			pkt->u.ext.consistency_type = CODER_CONSISTENCY_EXT;
			pkt->u.ext.nr_packets = nr_packets;

			pkt->u.ext.packetID = packetID;
			pkt->u.ext.payload_length = ((size > SL_PACKET_PAYLOAD_MAX_SIZE) ? SL_PACKET_PAYLOAD_MAX_SIZE : size);

			memcpy(pkt->payload, data, pkt->u.ext.payload_length);
			data += pkt->u.ext.payload_length;
			size -= pkt->u.ext.payload_length;

			/*
			 * We forward the packet to the Transceiver. The size at the reception
			 * will be taken from the plugin (RX).
			 */

			if (sender_tx(sl_desc, pkt, sizeof(transcoder_packet_format_t) + pkt->u.ext.payload_length, completed) < 0) {
				/* There has been something wrong with Datalink. Abort the transmission of the block. */
				break;
			}
		}

		kfree(pkt);
	}

	/* Finally ... */
	mutex_unlock(&coder_tx_lock);

	soo_log("[soo:soolink:transcoder:coder] Send completed.\n");
}

/**
 * Initialize the Coder functional block of Soolink.
 */
void coder_init(void) {
	mutex_init(&coder_tx_lock);
}

