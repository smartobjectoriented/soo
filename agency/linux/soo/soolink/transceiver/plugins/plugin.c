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

#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/kthread.h>
#include <linux/mutex.h>

#include <soo/soolink/transceiver.h>
#include <soo/soolink/plugin.h>
#include <soo/soolink/discovery.h>

#include <soo/core/device_access.h>

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

struct mutex rx_lock;

/**
 * Get the MAC address from an agencyUID.
 * The remote SOO must have been discovered. Returns NULL if no mapping exists.
 */
uint8_t *get_mac_addr(uint64_t agencyUID) {
	struct list_head *cur;
	plugin_remote_soo_desc_t *plugin_remote_soo_desc;
	unsigned long flags;

	spin_lock_irqsave(&current_soo_plugin->list_lock, flags);

	list_for_each(cur, &current_soo_plugin->remote_soo_list) {
		plugin_remote_soo_desc = list_entry(cur, plugin_remote_soo_desc_t, list);
		if (plugin_remote_soo_desc->agencyUID == agencyUID) {

			spin_unlock_irqrestore(&current_soo_plugin->list_lock, flags);
			return plugin_remote_soo_desc->mac;
		}
	}

	spin_unlock_irqrestore(&current_soo_plugin->list_lock, flags);

	return NULL;
}

/**
 * Identify a remote SOO using its MAC address. This function performs a MAC address-
 * to-agency UID conversion.
 * This function returns true if the remote SOO has been found in the list, false otherwise.
 */
static bool identify_remote_soo(req_type_t req_type, transceiver_packet_t *packet, uint8_t *mac_src, uint64_t *agencyUID_from) {
	struct list_head *cur;
	plugin_remote_soo_desc_t *remote_soo_desc_cur;
	unsigned long flags;

	soo_log("[soo:soolink:plugin] Looking for MAC ");
	soo_log_buffer(mac_src, ETH_ALEN);

	spin_lock_irqsave(&current_soo_plugin->list_lock, flags);

	/* Look for the remote SOO in the list */
	list_for_each(cur, &current_soo_plugin->remote_soo_list) {
		remote_soo_desc_cur = list_entry(cur, plugin_remote_soo_desc_t, list);
		if (!memcmp(remote_soo_desc_cur->mac, mac_src, ETH_ALEN)) {

			soo_log("[soo:soolink:plugin] Found agency UID: ");
			soo_log_printlnUID(remote_soo_desc_cur->agencyUID);

			*agencyUID_from = remote_soo_desc_cur->agencyUID;

			spin_unlock_irqrestore(&current_soo_plugin->list_lock, flags);

			return true;
		}
	}

	spin_unlock_irqrestore(&current_soo_plugin->list_lock, flags);

	/*
	 * If the packet is coming from a SOO which is not in the remote SOO table yet,
	 * discard the packet.
	 */

	soo_log("[soo:soolink:plugin] MAC not found. Discard packet\n");

	return false;
}

/*
 * Check if the remote SOO is known by our internal list for MAC address mapping.
 */
void attach_agencyUID(uint64_t agencyUID, uint8_t *mac_src) {
	plugin_remote_soo_desc_t *remote_soo_desc;
	unsigned long flags;

	spin_lock_irqsave(&current_soo_plugin->list_lock, flags);

	/* Sanity check */
	if (agencyUID == current_soo->agencyUID) {
		lprintk("!! Hacker bip! We are trying to attach a remote SOO with the same agency UID as us !!\n");
		BUG();
	}

	/* Look for the remote SOO in the list */
	list_for_each_entry(remote_soo_desc, &current_soo_plugin->remote_soo_list, list) {

		if (!memcmp(remote_soo_desc->mac, mac_src, ETH_ALEN)) {

			soo_log("[soo:soolink:plugin] agency UID found: ");
			soo_log_printlnUID(remote_soo_desc->agencyUID);

			spin_unlock_irqrestore(&current_soo_plugin->list_lock, flags);
			return ;
		}
	}
	/*
	 * Create the new entry.
	 * The data contained by the beacon is the agency UID of the sender.
	 */
	remote_soo_desc = (plugin_remote_soo_desc_t *) kzalloc(sizeof(plugin_remote_soo_desc_t), GFP_KERNEL);
	BUG_ON(!remote_soo_desc);

	memcpy(remote_soo_desc->mac, mac_src, ETH_ALEN);

	remote_soo_desc->agencyUID = agencyUID;

	list_add_tail(&remote_soo_desc->list, &current_soo_plugin->remote_soo_list);

	soo_log("[soo:soolink:plugin] added agency UID: ");
	soo_log_printUID(remote_soo_desc->agencyUID);
	soo_log(" with MAC address: ");
	soo_log_buffer(mac_src, ETH_ALEN);

	spin_unlock_irqrestore(&current_soo_plugin->list_lock, flags);
}

/**
 * Detach the agency UID from the remote SOO list.
 */
void detach_agencyUID(uint64_t agencyUID) {
	plugin_remote_soo_desc_t *remote_soo_desc, *tmp;
	unsigned long flags;

	spin_lock_irqsave(&current_soo_plugin->list_lock, flags);

	list_for_each_entry_safe(remote_soo_desc, tmp, &current_soo_plugin->remote_soo_list, list) {
		if (agencyUID == remote_soo_desc->agencyUID) {

			soo_log("[soo:soolink:plugin] delete the agency UID: ");
			soo_log_printlnUID(remote_soo_desc->agencyUID);

			list_del(&remote_soo_desc->list);
			kfree(remote_soo_desc);

			spin_unlock_irqrestore(&current_soo_plugin->list_lock, flags);

			return ;
		}
	}

	/* Should never happen */
	BUG();
}

void plugin_rx(plugin_desc_t *plugin_desc, req_type_t req_type, uint8_t *mac_src, void *data, size_t size) {
	medium_rx_t *rsp;

	/* Prepare to propagate the data to the plugin block */
	/* Cannot schedule because we are in an atomic context. */

	if (RING_RSP_FULL(&current_soo->soo_plugin->rx_ring_back))
		return ;

	mutex_lock(&rx_lock);

	rsp = medium_rx_new_ring_response(&current_soo->soo_plugin->rx_ring_back);

	rsp->plugin_desc = plugin_desc;
	rsp->req_type = req_type;

	rsp->size = size;

	/* Allocate the payload which will be freed in the rx processing thread. */
	rsp->data = kzalloc(rsp->size, GFP_KERNEL);
	BUG_ON(!rsp->data);

	memcpy(rsp->data, data, size);

	rsp->mac_src = kzalloc(ETH_ALEN, GFP_KERNEL);
	BUG_ON(!rsp->mac_src);

	memcpy(rsp->mac_src, mac_src, ETH_ALEN);

	medium_rx_ring_response_ready(&current_soo->soo_plugin->rx_ring_back);

	complete(&current_soo->soo_plugin->rx_event);

	mutex_unlock(&rx_lock);
}


/**
 * Send a packet using a plugin.
 */
void plugin_tx(sl_desc_t *sl_desc, void *data, size_t size) {
	plugin_desc_t *plugin_desc;

	/* Find a plugin descriptor which matches with the if_type */
	plugin_desc = current_soo_plugin->__intf[sl_desc->if_type];

	/* Currently, it should not fail... */
	BUG_ON(!plugin_desc);

	plugin_desc->tx_callback(sl_desc, data, size);
}

/**
 * Main rx processing in a separate thread.
 */
static int plugin_rx_fn(void *args) {
	sl_desc_t *sl_desc;
	bool found;
	transceiver_packet_t *transceiver_packet;
	uint64_t agencyUID_from;
	ssize_t payload_size;
	medium_rx_t *rsp;

	while (true) {

		soo_log("[soo:soolink:plugin] Waiting on RX packets...\n");

		wait_for_completion(&current_soo_plugin->rx_event);

		soo_log("[soo:soolink:plugin] Got something...\n");

		rsp = medium_rx_get_ring_response(&current_soo_plugin->rx_ring_front);

		if (rsp->req_type == SL_REQ_DISCOVERY) {
			transceiver_packet = (transceiver_packet_t *) rsp->data;

			/* Substract the transceiver's packet header size from the total size */
			payload_size = rsp->size - sizeof(transceiver_packet_t);

			discovery_rx(rsp->plugin_desc, transceiver_packet->payload , payload_size, rsp->mac_src);

			continue;
		}

		if (rsp->mac_src && rsp->req_type != SL_REQ_BT) {

			/* If we receive a packet from a neighbour which is not known yet, we simply ignore the packet. */
			found = identify_remote_soo(rsp->req_type, rsp->data, rsp->mac_src, &agencyUID_from);

			if (!found)
				continue;
		} else
			agencyUID_from = 0;

		/* Find out a corresponding sl_desc descriptor for this type of requester */
		sl_desc = find_sl_desc_by_req_type(rsp->req_type);
		if (!sl_desc)
			/* We did not find any available descriptor able to process this data. Simply ignore it... */
			continue;

		sl_desc->agencyUID_from = agencyUID_from;

		__receiver_rx(sl_desc, rsp->data, rsp->size);

		kfree(rsp->mac_src);
		kfree(rsp->data);
	}

	return 0;
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

	/* Attatch this plugin to our environment */
	current_soo_plugin->__intf[plugin_desc->if_type] = plugin_desc;
}

/*
 * Main initialization function of the Plugin functional block
 */
void transceiver_plugin_init(void) {
	medium_rx_sring_t *sring;
	struct task_struct *__ts;

	lprintk("Soolink transceiver plugin init ...\n");

	current_soo->soo_plugin = kzalloc(sizeof(struct soo_plugin_env), GFP_KERNEL);
	BUG_ON(!current_soo->soo_plugin);

	INIT_LIST_HEAD(&current_soo_plugin->remote_soo_list);

	spin_lock_init(&current_soo_plugin->list_lock);

	init_completion(&current_soo_plugin->rx_event);

	/* Allocate the shared ring structure */

	sring = (medium_rx_sring_t *) vmalloc(32 * PAGE_SIZE);
	BUG_ON(!sring);

	SHARED_RING_INIT(sring);
	FRONT_RING_INIT(&current_soo_plugin->rx_ring_front, sring, 32 * PAGE_SIZE);

	/* Prepare the backend-style ring side */
	BACK_RING_INIT(&current_soo_plugin->rx_ring_back, current_soo_plugin->rx_ring_front.sring, 32 * PAGE_SIZE);

	mutex_init(&rx_lock);

	__ts = kthread_create(plugin_rx_fn, NULL, "plugin_rx");
	BUG_ON(!__ts);

	add_thread(current_soo, __ts->pid);

	wake_up_process(__ts);
}
