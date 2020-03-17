
/*
 * Copyright (C) 2016-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
 * Copyright (C) 2016-2019 Baptiste Delporte <bonel@bonel.net>
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

#include <soo/soolink/soolink.h>
#include <soo/soolink/plugin.h>
#include <soo/soolink/plugin/wlan.h>
#include <soo/soolink/discovery.h>

#include <soo/core/device_access.h>

#include <soo/uapi/debug.h>

#include <soo/uapi/console.h>
#include <soo/evtchn.h>

#include <soo/debug/dbgvar.h>
#include <soo/debug/gpio.h>

#if defined(CONFIG_MARVELL_MWIFIEX_MLAN)
#include <rtdm/sdio_ops.h>
#include <rtdm/sdio.h>
#include <rtdm/sunxi-mmc.h>

extern int woal_hard_start_xmit(struct sk_buff *skb, struct net_device *dev);
#endif /* CONFIG_MARVELL_MWIFIEX_MLAN */

#if defined(CONFIG_ARCH_BCM)
#include <rtdm/sdhci.h>
#endif /* CONFIG_ARCH_BCM */

static struct net_device *net_dev = NULL;

static struct list_head remote_soo_list;

static spinlock_t send_lock;
static spinlock_t recv_lock;

static spinlock_t list_lock;

static volatile plugin_send_args_t plugin_send_args;
static volatile plugin_recv_args_t plugin_recv_args;

static bool plugin_ready = false;

/**
 * Identify a remote SOO using its MAC address. This function performs a MAC address-
 * to-agency UID conversion.
 * This function returns true if the remote SOO has been found in the list, false otherwise.
 * This is a non-RT function.
 */
static bool identify_remote_soo_by_mac(req_type_t req_type, transceiver_packet_t *packet, uint8_t *mac_src, agencyUID_t *agencyUID_from) {
	struct list_head *cur;
	plugin_remote_soo_desc_t *remote_soo_desc_cur, *new_remote_soo_desc;

	DBG("Looking for MAC: ");
	DBG_BUFFER(mac_src, ETH_ALEN);

	spin_lock(&list_lock);

	/* Look for the remote SOO in the list */
	list_for_each(cur, &remote_soo_list) {
		remote_soo_desc_cur = list_entry(cur, plugin_remote_soo_desc_t, list);
		if (!memcmp(remote_soo_desc_cur->mac, mac_src, ETH_ALEN)) {
			DBG("Agency UID found: ");
			DBG_BUFFER(&remote_soo_desc_cur->agencyUID, SOO_AGENCY_UID_SIZE);

			memcpy(agencyUID_from, &remote_soo_desc_cur->agencyUID, SOO_AGENCY_UID_SIZE);

			spin_unlock(&list_lock);

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

		spin_unlock(&list_lock);

		return true;
	}

	spin_unlock(&list_lock);

	/*
	 * If the packet is coming from a SOO which is not in the remote SOO table yet,
	 * discard the packet.
	 */
	DBG("MAC not found. Discard packet\n");

	return false;
}

static void identify_remote_soo_by_agencyUID(agencyUID_t *agencyUID_to, uint8_t *mac_src) {
	struct list_head *cur;
	plugin_remote_soo_desc_t *remote_soo_desc;

	DBG("Looking for agencyUID: ");
	DBG_BUFFER(agencyUID_to, SOO_AGENCY_UID_SIZE);

	spin_lock(&list_lock);

	/* Look for the remote SOO in the list */
	list_for_each(cur, &remote_soo_list) {
		remote_soo_desc = list_entry(cur, plugin_remote_soo_desc_t, list);
		if (!memcmp(&remote_soo_desc->agencyUID, agencyUID_to, SOO_AGENCY_UID_SIZE)) {
			DBG("MAC found: ");
			DBG_BUFFER(remote_soo_desc->mac, ETH_ALEN);

			memcpy(mac_src, &remote_soo_desc->mac, ETH_ALEN);

			spin_unlock(&list_lock);

			return ;
		}
	}

	spin_unlock(&list_lock);;
}

/*
 * Transmit on the WLAN interface from the RT domain.
 * At this point, we assume that the interface is up and available in the non-RT domain.
 */
void rtdm_plugin_wlan_tx(sl_desc_t *sl_desc, void *data, size_t size, unsigned long flags) {

	if (!plugin_ready)
		return ;

	spin_lock(&send_lock);

	plugin_send_args.sl_desc = sl_desc;
	plugin_send_args.data = data;
	plugin_send_args.size = size;

	do_sync_dom(DOMID_AGENCY, DC_PLUGIN_WLAN_SEND);
}

void propagate_plugin_wlan_send(void) {
	plugin_send_args_t __plugin_send_args;
	struct sk_buff *skb;
	struct netdev_queue *txq;
	__be16 proto;
	uint8_t *__data;
	uint8_t target_addr[ETH_ALEN];

	__plugin_send_args = plugin_send_args;

	DBG("Requester type: %d\n", __plugin_send_args.sl_desc->req_type);

	spin_unlock(&send_lock);

	/* Abort if the net device is not ready */
	if (unlikely(!plugin_ready))
		return ;

	skb = alloc_skb(ETH_HLEN + __plugin_send_args.size + LL_RESERVED_SPACE(net_dev), GFP_ATOMIC);
	BUG_ON(skb == NULL);

	skb_reserve(skb, LL_RESERVED_SPACE(net_dev));

	proto = get_protocol_from_sl_req_type(__plugin_send_args.sl_desc->req_type);

	/* A priority of 7 is not compliant with aggregation, use it only in Netstream mode */
	if (__plugin_send_args.sl_desc->trans_mode == SL_MODE_NETSTREAM)
		skb->priority = 7;

	/*
	 * If no valid recipient agency UID is given, broadcast the packet.
	 * identify_remote_soo_by_agencyUID updates target_addr only if the agency UID has been found.
	 */
	memset(target_addr, 0xff, ETH_ALEN);
	if (agencyUID_is_valid(&__plugin_send_args.sl_desc->agencyUID_to))
		identify_remote_soo_by_agencyUID(&__plugin_send_args.sl_desc->agencyUID_to, target_addr);

	__data = skb_put(skb, __plugin_send_args.size);
	memcpy(__data, __plugin_send_args.data, __plugin_send_args.size);

	dev_hard_header(skb, net_dev, proto, target_addr, net_dev->dev_addr, skb->len);
	if ((!netif_running(net_dev)) || (!netif_device_present(net_dev))) {
		kfree_skb(skb);
		return;
	}

	txq = skb_get_tx_queue(net_dev, skb);

	/* This works only if the Marvell Wifi driver is active */
#if defined(CONFIG_MARVELL_MWIFIEX_MLAN)
	woal_hard_start_xmit(skb, net_dev);
#endif /* CONFIG_MARVELL_MWIFIEX_MLAN */

#if defined(CONFIG_ARCH_BCM2835)
	brcmf_netdev_start_xmit(skb, net_dev);
#endif /* CONFIG_ARCH_BCM */

}

static plugin_desc_t plugin_wlan_desc = {
	.tx_callback = rtdm_plugin_wlan_tx,
	.if_type = SL_IF_WLAN
};

static void rtdm_sl_plugin_wlan_rx(req_type_t req_type, void *data, size_t size, uint8_t *mac_src) {
	agencyUID_t agencyUID_from;
	bool found;

	/* If we receive a packet from a neighbour which is not known yet, we simply ignore the packet. */
	found = identify_remote_soo_by_mac(req_type, data, mac_src, &agencyUID_from);
	if (found)
		plugin_rx(&plugin_wlan_desc, &agencyUID_from, req_type, data, size);
}

/**
 * This function has to be called in a realtime context, from the directcomm RT thread.
 */
void rtdm_propagate_sl_plugin_wlan_rx(void) {
	plugin_recv_args_t __plugin_recv_args;

	__plugin_recv_args.req_type = plugin_recv_args.req_type;
	__plugin_recv_args.data = plugin_recv_args.data;
	__plugin_recv_args.size = plugin_recv_args.size;
	memcpy(__plugin_recv_args.mac, (void *) plugin_recv_args.mac, ETH_ALEN);

	spin_unlock(&recv_lock);

	rtdm_sl_plugin_wlan_rx(__plugin_recv_args.req_type, __plugin_recv_args.data, __plugin_recv_args.size, __plugin_recv_args.mac);
}

/*
 * This function has to be called in a non-realtime context.
 * Called from drivers/net/wireless/marvell/mlinux/moal_shim.c
 */
void sl_plugin_wlan_rx_skb(struct sk_buff *skb, struct net_device *net_dev, uint8_t *mac_src) {

	req_type_t req_type;

	req_type = get_sl_req_type_from_protocol(ntohs(skb->protocol));

	spin_lock(&recv_lock);

	plugin_recv_args.req_type = req_type;
	plugin_recv_args.data = skb->data;
	plugin_recv_args.size = skb->len;
	memcpy((void *) plugin_recv_args.mac, mac_src, ETH_ALEN);
	do_sync_dom(DOMID_AGENCY_RT, DC_PLUGIN_WLAN_RECV);

	kfree_skb(skb);
}


/**
 * Detach the agency UID from the remote SOO list.
 */
static void detach_agencyUID(agencyUID_t *agencyUID) {
	struct list_head *cur, *tmp;
	plugin_remote_soo_desc_t *remote_soo_desc;

	spin_lock(&list_lock);

	list_for_each_safe(cur, tmp, &remote_soo_list) {
		remote_soo_desc = list_entry(cur, plugin_remote_soo_desc_t, list);
		if (!memcmp(agencyUID, &remote_soo_desc->agencyUID, SOO_AGENCY_UID_SIZE)) {
			DBG("Delete the agency UID: ");
			DBG_BUFFER(&remote_soo_desc->agencyUID, SOO_AGENCY_UID_SIZE);

			list_del(cur);
			kfree(remote_soo_desc);
		}
	}

	spin_unlock(&list_lock);
}

static void plugin_wlan_remove_neighbour(neighbour_desc_t *neighbour) {
	detach_agencyUID(&neighbour->agencyUID);
}

static discovery_listener_t plugin_wlan_discovery_desc = {
	.remove_neighbour_callback = plugin_wlan_remove_neighbour
};

#if defined(CONFIG_SOOLINK_PLUGIN_WLAN)
/**
 * As the plugin is initialized before the net device, the plugin cannot be used until the net dev
 * is properly initialized. The net device detection thread loops until the interface is initialized.
 * In the case of the WLAN plugin, the net dev is not initialized until the join operation is successful.
 */
static int net_dev_detect(void *args) {
	while (!net_dev) {
		msleep(NET_DEV_DETECT_DELAY);
		net_dev = dev_get_by_name(&init_net, WLAN_NET_DEV_NAME);
	}

	plugin_ready = true;

	return 0;
}
#endif /* CONFIG_MARVELL_MWIFIEX_MLAN */

/**
 * This function must be executed in the non-RT domain.
 */
static int plugin_wlan_init(void) {
	lprintk("SOOlink plugin Wlan initializing ...\n");

	spin_lock_init(&send_lock);
	spin_lock_init(&recv_lock);

	spin_lock_init(&list_lock);
	INIT_LIST_HEAD(&remote_soo_list);

	transceiver_plugin_register(&plugin_wlan_desc);

	discovery_listener_register(&plugin_wlan_discovery_desc);

	/* This works only if the mwifiex driver is active */
#if defined(CONFIG_SOOLINK_PLUGIN_WLAN)
	kthread_run(net_dev_detect, NULL, "wlan_detect");
#endif /* CONFIG_MARVELL_MWIFIEX_MLAN && CONFIG_MMC_SUNXI */

	return 0;
}

soolink_plugin_initcall(plugin_wlan_init);
