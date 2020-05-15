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

#include <linux/list.h>

#include <soo/soolink/receiver.h>
#include <soo/soolink/plugin.h>

#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>

static req_type_t protocol_to_req_type[ETH_P_SL_MAX - ETH_P_SL_MIN] = {
	[ETH_P_SOOLINK_DCM - ETH_P_SL_MIN] = SL_REQ_DCM,
	[ETH_P_SOOLINK_IAMASOO - ETH_P_SL_MIN] = SL_REQ_DISCOVERY,
	[ETH_P_SOOLINK_BT - ETH_P_SL_MIN] = SL_REQ_BT,
	[ETH_P_SOOLINK_TCP - ETH_P_SL_MIN] = SL_REQ_TCP,
	[ETH_P_SOOLINK_PEER - ETH_P_SL_MIN] = SL_REQ_PEER,
	[ETH_P_SOOLINK_DATALINK - ETH_P_SL_MIN] = SL_REQ_DATALINK
};

static uint16_t req_type_to_protocol[SL_REQ_N] = {
	[SL_REQ_DCM] = ETH_P_SOOLINK_DCM,
	[SL_REQ_DISCOVERY] = ETH_P_SOOLINK_IAMASOO,
	[SL_REQ_BT] = ETH_P_SOOLINK_BT,
	[SL_REQ_TCP] = ETH_P_SOOLINK_TCP,
	[SL_REQ_PEER] = ETH_P_SOOLINK_PEER,
	[SL_REQ_DATALINK] = ETH_P_SOOLINK_DATALINK
};

struct list_head plugin_list;

uint8_t broadcast_addr[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/*
 * Look for a specific plugin which matches the if_type
 */
static plugin_desc_t *find_plugin_by_if_type(if_type_t if_type) {
	plugin_desc_t *cur;

	list_for_each_entry(cur, &plugin_list, list)
		if (cur->if_type == if_type)
			return cur;

	return NULL;
}

/**
 * Send a packet using a plugin.
 */
void plugin_tx(sl_desc_t *sl_desc, void *data, size_t size, unsigned long flags) {
	plugin_desc_t *plugin_desc;

	/* Find a plugin descriptor which matches with the if_type */
	plugin_desc = find_plugin_by_if_type(sl_desc->if_type);

	/* Currently, it should not fail... */
	BUG_ON(!plugin_desc);

	plugin_desc->tx_callback(sl_desc, data, size, flags);

}

/**
 * Receive a packet from a plugin.
 */
void plugin_rx(plugin_desc_t *plugin_desc, agencyUID_t *agencyUID_from, req_type_t req_type, void *data, size_t size) {
	sl_desc_t *sl_desc;

	/* Find out a corresponding sl_desc descriptor for this type of requester */
	sl_desc = find_sl_desc_by_req_type(req_type);
	if (!sl_desc)
		/* We did not find any available descriptor able to process this data. Simply ignore it... */
		return ;

	memcpy(&sl_desc->agencyUID_from, agencyUID_from, SOO_AGENCY_UID_SIZE);

	__receiver_rx(sl_desc, plugin_desc, data, size);
}

/**
 * Find the requester type using the protocol ID.
 */
req_type_t get_sl_req_type_from_protocol(uint16_t protocol) {
	/* Clear the flag bits */
	protocol &= 0x10ff;

	BUG_ON((protocol <= ETH_P_SL_MIN) || (protocol >= ETH_P_SL_MAX));

	return protocol_to_req_type[protocol - ETH_P_SL_MIN];
}

/**
 * Find the protocol ID using the requester type.
 */
uint16_t get_protocol_from_sl_req_type(req_type_t req_type) {
	if (unlikely((req_type < 0) || (req_type >= SL_REQ_N)))
		BUG();

	return req_type_to_protocol[req_type];
}

/*
 * Register a new plugin within the Soolink subsystem (Transceiver functional block)
 */
void transceiver_plugin_register(plugin_desc_t *plugin_desc) {

	/* Add it in the list of known plugin */
	list_add_tail(&plugin_desc->list, &plugin_list);

}

/*
 * Main initialization function of the Plugin functional block
 */
void transceiver_plugin_init(void) {
	int i;

	lprintk("Soolink transceiver plugin init ...\n");

	INIT_LIST_HEAD(&plugin_list);

	for (i = 0; i < 5; i++)
		broadcast_addr[i] = 0xff;
}
