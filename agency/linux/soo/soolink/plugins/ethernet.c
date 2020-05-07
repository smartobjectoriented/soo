/*
 * Copyright (C) 2017-2019 Baptiste Delporte <bonel@bonel.net>
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

#include <linux/init.h>
#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>

#include <soo/uapi/avz.h>
#include <soo/uapi/console.h>
#include <soo/vbus.h>
#include <soo/uapi/debug.h>

#include <soo/soolink/plugin.h>
#include <soo/soolink/plugin/ethernet.h>
#include <soo/soolink/discovery.h>

#include <soo/core/device_access.h>

#include <soo/soolink/lib/tcpbridge.h>

static spinlock_t send_ethernet_lock;
static spinlock_t recv_ethernet_lock;

static spinlock_t list_lock;

static volatile plugin_send_args_t plugin_send_args;
static volatile plugin_recv_args_t plugin_recv_args;

static struct net_device *net_dev = NULL;

static struct list_head remote_soo_list;

static bool plugin_ready = false;

static void rtdm_plugin_ethernet_tx(sl_desc_t *sl_desc, void *data, size_t size, unsigned long flags) {

	if (unlikely(!plugin_ready))
		return;

	spin_lock(&send_ethernet_lock);
	plugin_send_args.sl_desc = sl_desc;
	plugin_send_args.data = data;
	plugin_send_args.size = size;
	
	do_sync_dom(DOMID_AGENCY, DC_PLUGIN_ETHERNET_SEND);

}

int propagate_plugin_ethernet_send_fn(void *args) {
	propagate_plugin_ethernet_send();

	return 0;
}

static plugin_desc_t plugin_ethernet_desc = {
	.tx_callback = rtdm_plugin_ethernet_tx,
	.if_type = SL_IF_ETH
};

static void plugin_tcp_tx(sl_desc_t *sl_desc, void *data, size_t size, unsigned long flags) {

	/* Discard Iamasoo (Discovery) beacons */
	if (sl_desc->req_type == SL_REQ_DISCOVERY)
		return ;

	spin_lock(&send_ethernet_lock);

	plugin_send_args.sl_desc = sl_desc;
	plugin_send_args.data = data;
	plugin_send_args.size = size;

	do_sync_dom(DOMID_AGENCY, DC_PLUGIN_TCP_SEND);
}

int propagate_plugin_tcp_send_fn(void *args) {
	propagate_plugin_tcp_send();

	return 0;
}

static plugin_desc_t plugin_tcp_desc = {
	.tx_callback = plugin_tcp_tx,
	.if_type = SL_IF_TCP
};

/**
 * Get the MAC address from an agencyUID.
 * The remote SOO must have been discovered. Returns NULL if no mapping exists.
 */
static uint8_t *get_mac_addr(agencyUID_t *agencyUID) {
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
	plugin_remote_soo_desc_t *remote_soo_desc_cur, *new_remote_soo_desc;
	unsigned long flags;

	DBG("Looking for MAC: ");
	DBG_BUFFER(mac_src, ETH_ALEN);

	spin_lock_irqsave(&list_lock, flags);

	/* Look for the remote SOO in the list */
	list_for_each(cur, &remote_soo_list) {
		remote_soo_desc_cur = list_entry(cur, plugin_remote_soo_desc_t, list);
		if (!memcmp(remote_soo_desc_cur->mac, mac_src, ETH_ALEN)) {
			DBG("Agency UID found: ");
			DBG_BUFFER(&remote_soo_desc_cur->agencyUID, SOO_AGENCY_UID_SIZE);

			memcpy(agencyUID_from, &remote_soo_desc_cur->agencyUID, SOO_AGENCY_UID_SIZE);

			spin_unlock_irqrestore(&list_lock, flags);

			return true;
		}
	}

	/* Only Discovery beacons can be used to create an entry in the remote SOO list */
	if (unlikely(req_type == SL_REQ_DISCOVERY)) {
		DBG("Beacon received\n");

		/*
		 * Create the new entry.
		 * The data contained by the beacon is the agency UID of the sender.
		 */
		new_remote_soo_desc = (plugin_remote_soo_desc_t *) kmalloc(sizeof(plugin_remote_soo_desc_t), GFP_ATOMIC);
		memcpy(new_remote_soo_desc->mac, mac_src, ETH_ALEN);
		memcpy(&new_remote_soo_desc->agencyUID, packet->payload, SOO_AGENCY_UID_SIZE);
		list_add_tail(&new_remote_soo_desc->list, &remote_soo_list);

		DBG("Added agency UID: ");
		DBG_BUFFER(&new_remote_soo_desc->agencyUID, SOO_AGENCY_UID_SIZE);

		memcpy(agencyUID_from, &new_remote_soo_desc->agencyUID, SOO_AGENCY_UID_SIZE);

		spin_unlock_irqrestore(&list_lock, flags);

		return true;
	}

	spin_unlock_irqrestore(&list_lock, flags);

	/*
	 * If the packet is coming from a SOO which is not in the remote SOO table yet,
	 * discard the packet.
	 */
	DBG("MAC not found. Discard packet\n");

	return false;
}

void propagate_plugin_ethernet_send(void) {
	plugin_send_args_t __plugin_send_args;
	struct sk_buff *skb;
	struct netdev_queue *txq;
	__be16 proto;
	const uint8_t *dest;

	__plugin_send_args = plugin_send_args;
	DBG("Requester type: %d\n", __plugin_send_args.sl_desc->req_type);

	spin_unlock(&send_ethernet_lock);

	/* Abort if the net device is not ready */
	if (unlikely(!plugin_ready))
		return ;

	skb = alloc_skb(ETH_HLEN + __plugin_send_args.size + LL_RESERVED_SPACE(net_dev), GFP_ATOMIC);
	BUG_ON(skb == NULL);

	skb_reserve(skb, ETH_HLEN);

	/* By default, the priority is 7 */
	skb->priority = 7;

	proto = get_protocol_from_sl_req_type(__plugin_send_args.sl_desc->req_type);

	/* If no valid recipient agency UID is given, broadcast the packet */
	if (!agencyUID_is_valid(&__plugin_send_args.sl_desc->agencyUID_to))
		dest = broadcast_addr;
	else
		dest = get_mac_addr(&__plugin_send_args.sl_desc->agencyUID_to);

	if (dest == NULL)
		return ;

	memcpy(skb->data, __plugin_send_args.data, __plugin_send_args.size);

	skb_put(skb, __plugin_send_args.size);

	dev_hard_header(skb, net_dev, proto, dest, net_dev->dev_addr, skb->len);
	if ((!netif_running(net_dev)) || (!netif_device_present(net_dev))) {
		kfree_skb(skb);
		return;
	}

	txq = netdev_pick_tx(net_dev, skb, NULL);

	netdev_start_xmit(skb, net_dev, txq, false);
}

/*
 * This function has to be called in a non-realtime context.
 * Called from drivers/net/tcp/smsc/smsc911x.c
 */
void sl_plugin_ethernet_rx(struct sk_buff *skb, struct net_device *net_dev, uint8_t *mac_src) {
	req_type_t req_type;
	transceiver_packet_t *packet;

	req_type = get_sl_req_type_from_protocol(ntohs(skb->protocol));

	spin_lock(&recv_ethernet_lock);

	plugin_recv_args.req_type = req_type;
	plugin_recv_args.data = skb->data;
	plugin_recv_args.size = skb->len;
	memcpy((void *) plugin_recv_args.mac, mac_src, ETH_ALEN);

	packet = (transceiver_packet_t *) skb->data;

	do_sync_dom(DOMID_AGENCY_RT, DC_PLUGIN_ETHERNET_RECV);

	kfree_skb(skb);
}

void propagate_plugin_tcp_send(void) {
	plugin_send_args_t __plugin_send_args;

	__plugin_send_args = plugin_send_args;
	DBG("TCP: Requester type: %d\n", __plugin_send_args.sl_desc->req_type);

	spin_unlock(&send_ethernet_lock);

	tcpbridge_sendto(__plugin_send_args.data, __plugin_send_args.size);
}

/*
 * This function has to be called in a non-realtime context.
 * Do not care about the MAC.
 * Called from socketmgr.c
 */
void sl_plugin_tcp_rx(void *data, size_t size) {

	/* In case where external function calls this, before the plugin finishes its initialization (like tcpbridge for instance). */
	if (!plugin_ready)
		return ;

	spin_lock(&recv_ethernet_lock);

	plugin_recv_args.req_type = SL_REQ_TCP;
	plugin_recv_args.data = data;
	plugin_recv_args.size = size;

	do_sync_dom(DOMID_AGENCY_RT, DC_PLUGIN_TCP_RECV);
}

/**
 * Detach the agency UID from the remote SOO list.
 */
static void detach_agencyUID(agencyUID_t *agencyUID) {
	struct list_head *cur, *tmp;
	plugin_remote_soo_desc_t *remote_soo_desc;
	unsigned long flags;

	spin_lock_irqsave(&list_lock, flags);

	list_for_each_safe(cur, tmp, &remote_soo_list) {
		remote_soo_desc = list_entry(cur, plugin_remote_soo_desc_t, list);
		if (!memcmp(agencyUID, &remote_soo_desc->agencyUID, SOO_AGENCY_UID_SIZE)) {
			DBG("Delete the agency UID: ");
			DBG_BUFFER(&remote_soo_desc->agencyUID, SOO_AGENCY_UID_SIZE);

			list_del(cur);
			kfree(remote_soo_desc);
		}
	}

	spin_unlock_irqrestore(&list_lock, flags);
}

static void rtdm_sl_plugin_ethernet_rx(req_type_t req_type, void *data, size_t size, uint8_t *mac_src) {
	agencyUID_t agencyUID_from;
	bool found;

	/* If we receive a packet from a neighbour which is not known yet, we simply ignore the packet. */
	found = identify_remote_soo(req_type, data, mac_src, &agencyUID_from);

	if (found)
		plugin_rx(&plugin_ethernet_desc, &agencyUID_from, req_type, data, size);
}

/**
 * This function has to be called in a realtime context, from the directcomm RT thread.
 */
void rtdm_propagate_sl_plugin_ethernet_rx(void) {
	plugin_recv_args_t __plugin_recv_args;

	__plugin_recv_args.req_type = plugin_recv_args.req_type;
	__plugin_recv_args.data = plugin_recv_args.data;
	__plugin_recv_args.size = plugin_recv_args.size;
	memcpy(__plugin_recv_args.mac, (void *) plugin_recv_args.mac, ETH_ALEN);

	spin_unlock(&recv_ethernet_lock);

	rtdm_sl_plugin_ethernet_rx(__plugin_recv_args.req_type, __plugin_recv_args.data, __plugin_recv_args.size, __plugin_recv_args.mac);
}

/**
 * Do not care about the MAC.
 */
static void rtdm_sl_plugin_tcp_rx(req_type_t req_type, void *data, size_t size) {
	plugin_rx(&plugin_tcp_desc, get_null_agencyUID(), req_type, data, size);
}

/**
 * This function has to be called in a realtime context, from the directcomm RT thread.
 */
void rtdm_propagate_sl_plugin_tcp_rx(void) {
	plugin_recv_args_t __plugin_recv_args;

	__plugin_recv_args.req_type = plugin_recv_args.req_type;
	__plugin_recv_args.data = plugin_recv_args.data;
	__plugin_recv_args.size = plugin_recv_args.size;

	spin_unlock(&recv_ethernet_lock);

	rtdm_sl_plugin_tcp_rx(__plugin_recv_args.req_type, __plugin_recv_args.data, __plugin_recv_args.size);
}

static void plugin_ethernet_remove_neighbour(neighbour_desc_t *neighbour) {
	detach_agencyUID(&neighbour->agencyUID);
}

static discovery_listener_t plugin_ethernet_discovery_desc = {
	.remove_neighbour_callback = plugin_ethernet_remove_neighbour
};

/**
 * As the plugin is initialized before the net device, the plugin cannot be used until the net dev
 * is properly initialized. The net device detection thread loops until the interface is initialized.
 */
static int net_dev_detect(void *args) {
	while (!net_dev) {
		msleep(NET_DEV_DETECT_DELAY);
		net_dev = dev_get_by_name(&init_net, ETHERNET_NET_DEV_NAME);
	}

	plugin_ready = true;

	return 0;
}
/*
 * This function must be executed in the non-RT domain.
 */
static int plugin_ethernet_init(void) {
	lprintk("Soolink: Ethernet Plugin init...\n");

	INIT_LIST_HEAD(&remote_soo_list);

	spin_lock_init(&send_ethernet_lock);
	spin_lock_init(&recv_ethernet_lock);

	spin_lock_init(&list_lock);

	transceiver_plugin_register(&plugin_ethernet_desc);
	transceiver_plugin_register(&plugin_tcp_desc);

	discovery_listener_register(&plugin_ethernet_discovery_desc);

	plugin_send_args.data = NULL;

	kthread_run(net_dev_detect, NULL, "eth_detect");

	return 0;
}

soolink_plugin_initcall(plugin_ethernet_init);
