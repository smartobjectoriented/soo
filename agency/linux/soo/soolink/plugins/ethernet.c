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

#include <soo/uapi/avz.h>
#include <soo/uapi/console.h>
#include <soo/vbus.h>
#include <soo/uapi/debug.h>

#include <soo/soolink/plugin.h>
#include <soo/soolink/plugin/ethernet.h>
#include <soo/soolink/discovery.h>

#include <soo/core/device_access.h>

#include <soo/soolink/lib/tcpbridge.h>

static uint8_t broadcast_addr[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

static struct net_device *net_dev = NULL;

static bool plugin_ready = false;

static void plugin_ethernet_tx(sl_desc_t *sl_desc, void *data, size_t size, unsigned long flags) {
	struct sk_buff *skb;
	struct netdev_queue *txq;
	__be16 proto;
	const uint8_t *dest;
	int cpu;

	if (unlikely(!plugin_ready))
		return;

	DBG("Requester type: %d\n", sl_desc->req_type);

	/* Abort if the net device is not ready */
	if (unlikely(!plugin_ready))
		return ;

	skb = alloc_skb(ETH_HLEN + size + LL_RESERVED_SPACE(net_dev), GFP_ATOMIC);
	BUG_ON(skb == NULL);

	skb_reserve(skb, ETH_HLEN);

	/* By default, the priority is 7 */
	skb->priority = 7;

	proto = get_protocol_from_sl_req_type(sl_desc->req_type);

	/* If no valid recipient agency UID is given, broadcast the packet */
	if (!agencyUID_is_valid(&sl_desc->agencyUID_to))
		dest = broadcast_addr;
	else
		dest = get_mac_addr(&sl_desc->agencyUID_to);

	if (dest == NULL)
		return ;

	memcpy(skb->data, data, size);

	skb_put(skb, size);

	dev_hard_header(skb, net_dev, proto, dest, net_dev->dev_addr, skb->len);
	if ((!netif_running(net_dev)) || (!netif_device_present(net_dev))) {
		kfree_skb(skb);
		return;
	}

	txq = skb_get_tx_queue(net_dev, skb);

	local_bh_disable();
	cpu = smp_processor_id();
	HARD_TX_LOCK(net_dev, txq, cpu);

	/* Normally, this should never happen,
	 * but in case of overspeed...
	 */
	while (netif_xmit_stopped(txq))
	{
		HARD_TX_UNLOCK(net_dev, txq);
		local_bh_enable();

		msleep(100);

		local_bh_disable();
		HARD_TX_LOCK(net_dev, txq, cpu);
	}

	netdev_start_xmit(skb, net_dev, txq, 0);

	HARD_TX_UNLOCK(net_dev, txq);
	local_bh_enable();
}

static plugin_desc_t plugin_ethernet_desc = {
	.tx_callback = plugin_ethernet_tx,
	.if_type = SL_IF_ETH
};

/*
 * This function has to be called in a non-realtime context.
 * Called from drivers/net/tcp/smsc/smsc911x.c
 */
void plugin_ethernet_rx(struct sk_buff *skb, struct net_device *net_dev, uint8_t *mac_src) {
	req_type_t req_type;

	req_type = get_sl_req_type_from_protocol(ntohs(skb->protocol));

	plugin_rx(&plugin_ethernet_desc, req_type, skb->data, skb->len, mac_src);

	kfree_skb(skb);
}


/********************/

static void plugin_tcp_tx(sl_desc_t *sl_desc, void *data, size_t size, unsigned long flags) {

	/* Discard Iamasoo (Discovery) beacons */
	if (sl_desc->req_type == SL_REQ_DISCOVERY)
		return ;

	tcpbridge_sendto(data, size);
}

static plugin_desc_t plugin_tcp_desc = {
	.tx_callback = plugin_tcp_tx,
	.if_type = SL_IF_TCP
};


/*
 * This function has to be called in a non-realtime context.
 * Do not care about the MAC.
 * Called from socketmgr.c
 */
void plugin_tcp_rx(void *data, size_t size) {

	/* In case where external function calls this, before the plugin finishes its initialization (like tcpbridge for instance). */
	if (!plugin_ready)
		return ;

	plugin_rx(&plugin_tcp_desc, SL_REQ_TCP, data, size, NULL);
}

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

	transceiver_plugin_register(&plugin_ethernet_desc);
	transceiver_plugin_register(&plugin_tcp_desc);

	kthread_run(net_dev_detect, NULL, "eth_detect");

	return 0;
}

soolink_plugin_initcall(plugin_ethernet_init);
