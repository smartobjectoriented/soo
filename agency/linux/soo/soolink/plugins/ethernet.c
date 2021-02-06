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

#include <soo/netsimul.h>

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

struct soo_plugin_ethernet {

	bool plugin_ready;

	struct net_device *net_dev;

	medium_rx_back_ring_t rx_ring;

	plugin_desc_t plugin_ethernet_desc;
};

#define current_soo_plugin_eth     ((struct soo_plugin_ethernet *) current_soo->soo_plugin->priv)

static void plugin_ethernet_tx(sl_desc_t *sl_desc, void *data, size_t size) {
	struct sk_buff *skb;
	struct netdev_queue *txq;
	__be16 proto;
	const uint8_t *dest;
	int cpu;

	if (unlikely(!current_soo_plugin_eth->plugin_ready))
		return;

	DBG("Requester type: %d\n", sl_desc->req_type);

	/* Abort if the net device is not ready */
	if (unlikely(!current_soo_plugin_eth->plugin_ready))
		return ;

	skb = alloc_skb(ETH_HLEN + size + LL_RESERVED_SPACE(current_soo_plugin_eth->net_dev), GFP_KERNEL);
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

	dev_hard_header(skb, current_soo_plugin_eth->net_dev, proto, dest, current_soo_plugin_eth->net_dev->dev_addr, skb->len);
	if ((!netif_running(current_soo_plugin_eth->net_dev)) || (!netif_device_present(current_soo_plugin_eth->net_dev))) {
		kfree_skb(skb);
		return;
	}

	txq = skb_get_tx_queue(current_soo_plugin_eth->net_dev, skb);

	local_bh_disable();
	cpu = smp_processor_id();

	HARD_TX_LOCK(current_soo_plugin_eth->net_dev, txq, cpu);

	/* Normally, this should never happen,
	 * but in case of overspeed...
	 */
	while (netif_xmit_stopped(txq))
	{
		HARD_TX_UNLOCK(current_soo_plugin_eth->net_dev, txq);
		local_bh_enable();

		msleep(100);

		local_bh_disable();
		HARD_TX_LOCK(current_soo_plugin_eth->net_dev, txq, cpu);
	}

	netdev_start_xmit(skb, current_soo_plugin_eth->net_dev, txq, 0);

	HARD_TX_UNLOCK(current_soo_plugin_eth->net_dev, txq);
	local_bh_enable();
}

/*
 * This function has to be called in a non-realtime context.
 * Called from drivers/net/ethernet/smsc/smsc911x.c
 */
void plugin_ethernet_rx(struct sk_buff *skb, struct net_device *net_dev, uint8_t *mac_src) {
	medium_rx_t *rsp;

	/* Prepare to propagate the data to the plugin block */
	while (RING_RSP_FULL(&current_soo_plugin_eth->rx_ring))
		schedule();

	rsp = medium_rx_new_ring_response(&current_soo_plugin_eth->rx_ring);

	rsp->plugin_desc = &current_soo_plugin_eth->plugin_ethernet_desc;
	rsp->req_type = get_sl_req_type_from_protocol(ntohs(skb->protocol));;

	rsp->size = skb->len;

	rsp->data = kzalloc(rsp->size, GFP_KERNEL);
	BUG_ON(!rsp->data);

	memcpy(rsp->data, skb->data, rsp->size);

	rsp->mac_src = kzalloc(ETH_ALEN, GFP_KERNEL);
	BUG_ON(!rsp->mac_src);

	memcpy(rsp->mac_src, mac_src, ETH_ALEN);

	kfree_skb(skb);

	medium_rx_ring_response_ready(&current_soo_plugin_eth->rx_ring);

	complete(&current_soo->soo_plugin->rx_event);

}


/********************/
#if 0
static void plugin_tcp_tx(sl_desc_t *sl_desc, void *data, size_t size) {

	/* Discard Iamasoo (Discovery) beacons */
	if (sl_desc->req_type == SL_REQ_DISCOVERY)
		return ;

	tcpbridge_sendto(data, size);
}

static plugin_desc_t plugin_tcp_desc = {
	.tx_callback = plugin_tcp_tx,
	.if_type = SL_IF_TCP
};
#endif

/*
 * This function has to be called in a non-realtime context.
 * Do not care about the MAC.
 * Called by socketmgr.c
 */
void plugin_tcp_rx(void *data, size_t size) {
#if 0
	/* In case where external function calls this, before the plugin finishes its initialization (like tcpbridge for instance). */
	if (!plugin_ready)
		return ;

	plugin_rx(&plugin_tcp_desc, SL_REQ_TCP, data, size, NULL);
#endif
}


/**
 * As the plugin is initialized before the net device, the plugin cannot be used until the net dev
 * is properly initialized. The net device detection thread loops until the interface is initialized.
 */
static int net_dev_detect(void *args) {
	while (!current_soo_plugin_eth->net_dev) {
		msleep(NET_DEV_DETECT_DELAY);
		current_soo_plugin_eth->net_dev = dev_get_by_name(&init_net, ETHERNET_NET_DEV_NAME);
	}

	current_soo_plugin_eth->plugin_ready = true;

	return 0;
}
/*
 * This function must be executed in the non-RT domain.
 */
int plugin_ethernet_init(void) {
	struct task_struct *__ts;

	lprintk("Soolink: Ethernet Plugin init...\n");

	current_soo->soo_plugin->priv = (struct soo_plugin_ethernet *) kzalloc(sizeof(struct soo_plugin_ethernet), GFP_KERNEL);
	BUG_ON(!current_soo->soo_plugin->priv);

	current_soo_plugin_eth->plugin_ethernet_desc.tx_callback = plugin_ethernet_tx;
	current_soo_plugin_eth->plugin_ethernet_desc.if_type = SL_IF_ETH;

	transceiver_plugin_register(&current_soo_plugin_eth->plugin_ethernet_desc);

	current_soo_plugin_eth->plugin_ready = false;
	current_soo_plugin_eth->net_dev = NULL;

	/*
	 * Initialize the backend shared ring for RX packet.
	 * The shared ring is initialized by the plugin block as frontend, so we need to use
	 * this ring.
	 */
	BACK_RING_INIT(&current_soo_plugin_eth->rx_ring, current_soo_plugin->rx_ring.sring, 32 * PAGE_SIZE);

	transceiver_plugin_register(&current_soo_plugin_eth->plugin_ethernet_desc);

#if 0
	transceiver_plugin_register(&plugin_tcp_desc);
#endif

	__ts = kthread_create(net_dev_detect, NULL, "eth_detect");
	BUG_ON(!__ts);

	add_thread(current_soo, __ts->pid);
	wake_up_process(__ts);

	return 0;
}

