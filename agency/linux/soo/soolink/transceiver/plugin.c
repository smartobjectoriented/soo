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
#define VERBOSE
#endif

#include <linux/spinlock.h>
#include <linux/list.h>

#include <soo/soolink/receiver.h>
#include <soo/soolink/plugin.h>
#include <soo/soolink/discovery.h>

#include <soo/core/device_access.h>

#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>

static struct list_head remote_soo_list;

static spinlock_t list_lock;

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
 * Get the MAC address from an agencyUID.
 * The remote SOO must have been discovered. Returns NULL if no mapping exists.
 */
uint8_t *get_mac_addr(agencyUID_t *agencyUID) {
	struct list_head *cur;
	plugin_remote_soo_desc_t *plugin_remote_soo_desc;
	unsigned long flags;

	spin_lock_irqsave(&list_lock, flags);

	list_for_each(cur, &remote_soo_list) {
		plugin_remote_soo_desc = list_entry(cur, plugin_remote_soo_desc_t, list);
		if (!memcmp(&plugin_remote_soo_desc->agencyUID, agencyUID, SOO_AGENCY_UID_SIZE)) {

			spin_unlock_irqrestore(&list_lock, flags);
			return plugin_remote_soo_desc->mac;
		}
	}

	spin_unlock_irqrestore(&list_lock, flags);

	return NULL;
}

/**
 * Identify a remote SOO using its MAC address. This function performs a MAC address-
 * to-agency UID conversion.
 * This function returns true if the remote SOO has been found in the list, false otherwise.
 */
static bool identify_remote_soo(req_type_t req_type, transceiver_packet_t *packet, uint8_t *mac_src, agencyUID_t *agencyUID_from) {
	struct list_head *cur;
	plugin_remote_soo_desc_t *remote_soo_desc_cur;
	unsigned long flags;

#ifdef VERBOSE
	lprintk("%s: looking for MAC: ", __func__);
	lprintk_buffer(mac_src, ETH_ALEN);
	lprintk("\n");
#endif

	spin_lock_irqsave(&list_lock, flags);

	/* Look for the remote SOO in the list */
	list_for_each(cur, &remote_soo_list) {
		remote_soo_desc_cur = list_entry(cur, plugin_remote_soo_desc_t, list);
		if (!memcmp(remote_soo_desc_cur->mac, mac_src, ETH_ALEN)) {
#ifdef VERBOSE
			lprintk("%s: Agency UID found: ", __func__);
			printlnUID(&remote_soo_desc_cur->agencyUID);
#endif
			memcpy(agencyUID_from, &remote_soo_desc_cur->agencyUID, SOO_AGENCY_UID_SIZE);

			spin_unlock_irqrestore(&list_lock, flags);

			return true;
		}
	}

	spin_unlock_irqrestore(&list_lock, flags);

	/*
	 * If the packet is coming from a SOO which is not in the remote SOO table yet,
	 * discard the packet.
	 */
#ifdef VERBOSE
	lprintk("MAC not found. Discard packet\n");
#endif

	return false;
}

/*
 * Check if the remote SOO is known by our internal list for MAC address mapping.
 */
void attach_agencyUID(agencyUID_t *agencyUID, uint8_t *mac_src) {
	plugin_remote_soo_desc_t *remote_soo_desc;
	unsigned long flags;

	spin_lock_irqsave(&list_lock, flags);

	/* Look for the remote SOO in the list */
	list_for_each_entry(remote_soo_desc, &remote_soo_list, list) {

		if (!memcmp(remote_soo_desc->mac, mac_src, ETH_ALEN)) {
#ifdef VERBOSE
			lprintk("%s: agency UID found: ", __func__);
			printlnUID(&remote_soo_desc->agencyUID);
#endif
			spin_unlock_irqrestore(&list_lock, flags);
			return ;
		}
	}
	/*
	 * Create the new entry.
	 * The data contained by the beacon is the agency UID of the sender.
	 */
	remote_soo_desc = (plugin_remote_soo_desc_t *) kmalloc(sizeof(plugin_remote_soo_desc_t), GFP_ATOMIC);
	memcpy(remote_soo_desc->mac, mac_src, ETH_ALEN);

	memcpy(&remote_soo_desc->agencyUID, agencyUID, SOO_AGENCY_UID_SIZE);

	list_add_tail(&remote_soo_desc->list, &remote_soo_list);

#ifdef VERBOSE
	lprintk("%s: added agency UID: ", __func__);
	printlnUID(&remote_soo_desc->agencyUID);
#endif

	spin_unlock_irqrestore(&list_lock, flags);
}

/**
 * Detach the agency UID from the remote SOO list.
 */
void detach_agencyUID(agencyUID_t *agencyUID) {
	plugin_remote_soo_desc_t *remote_soo_desc, *tmp;
	unsigned long flags;

	spin_lock_irqsave(&list_lock, flags);

	list_for_each_entry_safe(remote_soo_desc, tmp, &remote_soo_list, list) {
		if (!memcmp(agencyUID, &remote_soo_desc->agencyUID, SOO_AGENCY_UID_SIZE)) {
#ifdef VERBOSE
			lprintk("%s: delete the agency UID: ", __func__);
			printlnUID(&remote_soo_desc->agencyUID);
#endif
			list_del(&remote_soo_desc->list);
			kfree(remote_soo_desc);

			spin_unlock_irqrestore(&list_lock, flags);

			return ;
		}
	}

	/* Should never happen */
	BUG();
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
void plugin_rx(plugin_desc_t *plugin_desc, req_type_t req_type, void *data, size_t size, uint8_t *mac_src) {
	sl_desc_t *sl_desc;
	bool found;
	transceiver_packet_t *transceiver_packet;
	agencyUID_t agencyUID_from;
	ssize_t payload_size;

	if (req_type == SL_REQ_DISCOVERY) {
		transceiver_packet = (transceiver_packet_t *) data;

		/* Substract the transceiver's packet header size from the total size */
		payload_size = size - sizeof(transceiver_packet_t);

		discovery_rx(plugin_desc, transceiver_packet->payload, payload_size, mac_src);

		return ;
	}

	/* As the SL_REQ_BT requester is not from another Smart Object, we bypass the MAC address
	   check in that case */
	if (mac_src && req_type != SL_REQ_BT) {
		/* If we receive a packet from a neighbour which is not known yet, we simply ignore the packet. */
		found = identify_remote_soo(req_type, data, mac_src, &agencyUID_from);

		if (!found)
			return ;
	} else
		memcpy(&agencyUID_from, get_null_agencyUID(), SOO_AGENCY_UID_SIZE);

	/* Find out a corresponding sl_desc descriptor for this type of requester */
	sl_desc = find_sl_desc_by_req_type(req_type);
	if (!sl_desc)
		/* We did not find any available descriptor able to process this data. Simply ignore it... */
		return ;

	memcpy(&sl_desc->agencyUID_from, &agencyUID_from, SOO_AGENCY_UID_SIZE);

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

	lprintk("Soolink transceiver plugin init ...\n");

	INIT_LIST_HEAD(&plugin_list);
	INIT_LIST_HEAD(&remote_soo_list);

	spin_lock_init(&list_lock);

}
