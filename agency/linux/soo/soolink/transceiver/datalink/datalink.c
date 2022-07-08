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

#include <soo/soolink/datalink/winenet.h>
#include <soo/soolink/datalink/bluetooth.h>

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
 * Datalink TX function.
 * If a Datalink protocol is registered, its datalink_tx callback is called.
 * Otherwise, the sender tx function is directly called.
 * This function is called by the Sender.
 * packet is a (standard) transceiver packet.
 * The size parameter refers to the payload.
 */
int datalink_tx(sl_desc_t *sl_desc, transceiver_packet_t *packet, bool completed) {

	/*
	 * Currently, we are using the Winenet protocol for most transmission types.
	 */
	if ((sl_desc->trans_mode == SL_MODE_UNIBROAD) && datalink_protocols[SL_DL_PROTO_WINENET])
		return datalink_protocols[SL_DL_PROTO_WINENET]->tx_callback(sl_desc, packet, completed);

	__sender_tx(sl_desc, packet);

	return 0;

}

/**
 * Datalink RX function.
 * If a Datalink protocol is registered, its datalink_rx callback is called.
 * Otherwise, the receiver RX function is directly called.
 * The size parameter refers to the whole transceiver packet.
 */
void datalink_rx(sl_desc_t *sl_desc, transceiver_packet_t *packet) {

	if ((sl_desc->trans_mode == SL_MODE_UNIBROAD) && datalink_protocols[SL_DL_PROTO_WINENET])
		datalink_protocols[SL_DL_PROTO_WINENET]->rx_callback(sl_desc, packet);
	else if ((sl_desc->trans_mode == SL_MODE_UNICAST) && datalink_protocols[SL_DL_PROTO_BT])
		datalink_protocols[SL_DL_PROTO_BT]->rx_callback(sl_desc, packet);
	else
		receiver_rx(sl_desc, packet);

}

void datalink_cancel_rx(sl_desc_t *sl_desc) {
	if ((sl_desc->trans_mode == SL_MODE_UNIBROAD) && datalink_protocols[SL_DL_PROTO_WINENET])
		datalink_protocols[SL_DL_PROTO_WINENET]->rx_cancel_callback(sl_desc);

}

/*
 * Main initialization function of the Datalink.
 */
void datalink_init(void) {
	/* Initialize the Winenet protocol */
	winenet_init();
	bluetooth_init();
}
