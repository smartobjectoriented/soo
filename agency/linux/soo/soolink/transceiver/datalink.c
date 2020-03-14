/*
 * Copyright (C) 2018 Baptiste Delporte <bonel@bonel.net>
 * Copyright (C) 2018-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <soo/soolink/soolink.h>
#include <soo/soolink/datalink.h>
#include <soo/soolink/transceiver.h>
#include <soo/soolink/sender.h>
#include <soo/soolink/receiver.h>

#include <soo/soolink/datalink/winenet.h>

/*
 * Protocol table. It contains the Datalink protocol descriptors associated to the
 * SL_PROTO_* values. If the entry is NULL, this means that no protocol associated
 * to this value has been registered yet.
 */
static datalink_proto_desc_t *datalink_protocols[SL_DL_PROTO_N] = { NULL };

/**
 * Register a protocol with Datalink. The protocol is associated to a unique
 * ID that is also the index of the protocol in the protocol table.
 */
void datalink_register_protocol(datalink_proto_t proto, datalink_proto_desc_t *proto_desc) {
	/* It is forbidden to get out of bounds! */
	if ((unlikely(proto >= SL_DL_PROTO_N)) || (unlikely(proto < 0)))
		return;

	datalink_protocols[proto] = proto_desc;
}

/**
 * Datalink XMIT request.
 * This function triggers a XMIT request.
 */
int datalink_request_xmit(sl_desc_t *sl_desc) {
	switch (sl_desc->trans_mode) {
	case SL_MODE_BROADCAST:
	case SL_MODE_UNIBROAD:
	case SL_MODE_NETSTREAM:
		if (datalink_protocols[SL_DL_PROTO_WINENET])
			return datalink_protocols[SL_DL_PROTO_WINENET]->request_xmit_callback(sl_desc);
		else
			return 0;
		break;

	default:
		return 0;
	}

	return 0;
}

bool datalink_ready_to_send(sl_desc_t *sl_desc) {
	return datalink_protocols[SL_DL_PROTO_WINENET]->ready_to_send(sl_desc);
}

/**
 * Datalink XMIT function (TX).
 * If a Datalink protocol is registered, its datalink_xmit callback is called.
 * Otherwise, the sender XMIT function is directly called.
 * This function is called by the Sender.
 * packet is a netstream transceiver packet in netstream mode, otherwise it is a (standard) transceiver packet.
 * The size parameter refers to the payload.
 */
int datalink_xmit(sl_desc_t *sl_desc, void *packet, size_t size, bool completed) {

	/*
	 * Currently, we are using the Winenet protocol for most transmission types.
	 */
	switch (sl_desc->trans_mode) {
		case SL_MODE_BROADCAST:
		case SL_MODE_UNIBROAD:
		case SL_MODE_NETSTREAM:
			if (datalink_protocols[SL_DL_PROTO_WINENET])
				return datalink_protocols[SL_DL_PROTO_WINENET]->xmit_callback(sl_desc, packet, size, completed);
			else {
				sender_tx(sl_desc, packet, size, 0);
				return 0;
			}
			break;

		default:
			/* SL_MODE_UNICAST */
			sender_tx(sl_desc, packet, size, 0);
			return 0;

	}
}

/**
 * Datalink RX function.
 * If a Datalink protocol is registered, its datalink_rx callback is called.
 * Otherwise, the receiver RX function is directly called.
 * The size parameter refers to the whole transceiver packet.
 */
void datalink_rx(sl_desc_t *sl_desc, plugin_desc_t *plugin_desc, void *packet, size_t size) {
	switch (sl_desc->trans_mode) {
		case SL_MODE_BROADCAST:
		case SL_MODE_UNIBROAD:
		case SL_MODE_NETSTREAM:
			if (datalink_protocols[SL_DL_PROTO_WINENET])
				datalink_protocols[SL_DL_PROTO_WINENET]->rx_callback(sl_desc, plugin_desc, packet, size);
			else
				receiver_rx(sl_desc, plugin_desc, packet, size);
			break;

		default:
			/* SL_MODE_UNICAST */
			receiver_rx(sl_desc, plugin_desc, packet, size);
			break;
	}
}

/*
 * Main initialization function of the Datalink.
 */
void datalink_init(void) {
	/* Initialize the Winenet protocol */
	winenet_init();
}
