
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
//#include <rtdm/sdhci.h>
#endif /* CONFIG_ARCH_BCM */

static struct net_device *net_dev = NULL;

static spinlock_t send_lock;
static spinlock_t recv_lock;

static volatile plugin_send_args_t plugin_send_args;
static volatile plugin_recv_args_t plugin_recv_args;

static bool plugin_ready = false;

static uint8_t broadcast_addr[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

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
	const uint8_t *dest;

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

	/* If no valid recipient agency UID is given, broadcast the packet */
	if (!agencyUID_is_valid(&__plugin_send_args.sl_desc->agencyUID_to))
		dest = broadcast_addr;
	else
		dest = get_mac_addr(&__plugin_send_args.sl_desc->agencyUID_to);

	__data = skb_put(skb, __plugin_send_args.size);
	memcpy(__data, __plugin_send_args.data, __plugin_send_args.size);

	dev_hard_header(skb, net_dev, proto, dest, net_dev->dev_addr, skb->len);
	if ((!netif_running(net_dev)) || (!netif_device_present(net_dev))) {
		kfree_skb(skb);
		return;
	}

	txq = skb_get_tx_queue(net_dev, skb);

	/* This works only if the Marvell Wifi driver is active */
#if defined(CONFIG_MARVELL_MWIFIEX_MLAN)
	woal_hard_start_xmit(skb, net_dev);
#endif /* CONFIG_MARVELL_MWIFIEX_MLAN */

}

static plugin_desc_t plugin_wlan_desc = {
	.tx_callback = rtdm_plugin_wlan_tx,
	.if_type = SL_IF_WLAN
};

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

	plugin_rx(&plugin_wlan_desc, __plugin_recv_args.req_type, __plugin_recv_args.data, __plugin_recv_args.size, __plugin_recv_args.mac);
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

/**
 * This function must be executed in the non-RT domain.
 */
static int plugin_wlan_init(void) {
	lprintk("SOOlink plugin Wlan initializing ...\n");

	spin_lock_init(&send_lock);
	spin_lock_init(&recv_lock);

	transceiver_plugin_register(&plugin_wlan_desc);

	/* This works only if the mwifiex driver is active */

	kthread_run(net_dev_detect, NULL, "wlan_detect");

	return 0;
}

soolink_plugin_initcall(plugin_wlan_init);
