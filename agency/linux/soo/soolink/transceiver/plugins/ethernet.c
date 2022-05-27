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
#include <linux/rtnetlink.h>

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

typedef struct {
	bool plugin_ready;
	struct net_device *net_dev;
	plugin_desc_t plugin_eth_desc;
} soo_plugin_eth_t;

static void plugin_ethernet_tx(sl_desc_t *sl_desc, void *data, size_t size) {
	struct sk_buff *skb;
	struct netdev_queue *txq;
	__be16 proto;
	const uint8_t *dest;
	int cpu;
	soo_plugin_eth_t *soo_plugin_eth;

	soo_plugin_eth = container_of(current_soo_plugin->__intf[SL_IF_ETH], soo_plugin_eth_t, plugin_eth_desc);

	if (unlikely(!soo_plugin_eth->plugin_ready))
		return;

	DBG("Requester type: %d\n", sl_desc->req_type);

	/* Abort if the net device is not ready */
	if (unlikely(!soo_plugin_eth->net_dev))
		return ;

	skb = alloc_skb(ETH_HLEN + size + LL_RESERVED_SPACE(soo_plugin_eth->net_dev), GFP_KERNEL);
	BUG_ON(skb == NULL);

	skb_reserve(skb, ETH_HLEN);

	/* By default, the priority is 7 */
	skb->priority = 7;

	proto = get_protocol_from_sl_req_type(sl_desc->req_type);

	/* If no valid recipient agency UID is given, broadcast the packet */
	if (!sl_desc->agencyUID_to)
		dest = broadcast_addr;
	else
		dest = get_mac_addr(sl_desc->agencyUID_to);

	if (dest == NULL)
		return ;

	memcpy(skb->data, data, size);

	skb_put(skb, size);

	dev_hard_header(skb, soo_plugin_eth->net_dev, proto, dest, soo_plugin_eth->net_dev->dev_addr, skb->len);
	if ((!netif_running(soo_plugin_eth->net_dev)) || (!netif_device_present(soo_plugin_eth->net_dev))) {
		kfree_skb(skb);
		return;
	}

	txq = skb_get_tx_queue(soo_plugin_eth->net_dev, skb);

	local_bh_disable();
	cpu = smp_processor_id();

	HARD_TX_LOCK(soo_plugin_eth->net_dev, txq, cpu);

	/* Normally, this should never happen,
	 * but in case of overspeed...
	 */
	while (netif_xmit_stopped(txq))
	{
		HARD_TX_UNLOCK(soo_plugin_eth->net_dev, txq);
		local_bh_enable();

		msleep(100);

		local_bh_disable();
		HARD_TX_LOCK(soo_plugin_eth->net_dev, txq, cpu);
	}


	netdev_start_xmit(skb, soo_plugin_eth->net_dev, txq, 0);

	HARD_TX_UNLOCK(soo_plugin_eth->net_dev, txq);
	local_bh_enable();
}

rx_handler_result_t plugin_ethernet_rx_handler(struct sk_buff **pskb) {
	soo_plugin_eth_t *soo_plugin_eth;
	struct sk_buff *skb = *pskb;
	struct ethhdr *hdr = eth_hdr(skb);
	__be16 skb_protocol;

	/* It may happen that virtnet send an empty skb for some obscure reason... */
	if (skb->len == 0)
		return RX_HANDLER_PASS;

	/* Clear the flag bits */
	skb_protocol = ntohs(skb->protocol) & 0x10ff;

	if (!((skb_protocol > ETH_P_SL_MIN) && (skb_protocol < ETH_P_SL_MAX)))
		return RX_HANDLER_PASS;

	soo_plugin_eth = container_of(current_soo_plugin->__intf[SL_IF_ETH], soo_plugin_eth_t, plugin_eth_desc);

	plugin_rx(&soo_plugin_eth->plugin_eth_desc,
		  get_sl_req_type_from_protocol(ntohs(skb->protocol)), hdr->h_source, skb->data, skb->len);

	kfree_skb(skb);

	*pskb = NULL;

	return RX_HANDLER_CONSUMED;
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
	soo_plugin_eth_t *soo_plugin_eth;

	soo_plugin_eth = container_of(current_soo_plugin->__intf[SL_IF_ETH], soo_plugin_eth_t, plugin_eth_desc);

	while (!soo_plugin_eth->net_dev) {
		msleep(NET_DEV_DETECT_DELAY);
		soo_plugin_eth->net_dev = dev_get_by_name(&init_net, ETHERNET_NET_DEV_NAME);
	}

	/* Wait for the net_device to be running AND operational */
	while (!(netif_running(soo_plugin_eth->net_dev) && netif_oper_up(soo_plugin_eth->net_dev))) {
		msleep(NET_DEV_DETECT_DELAY);
	}
	DBG("NET_DEV now operational and running!\n");

	rtnl_lock();
	netdev_rx_handler_register(soo_plugin_eth->net_dev, plugin_ethernet_rx_handler, NULL);
	rtnl_unlock();

	soo_plugin_eth->plugin_ready = true;

	return 0;
}
/*
 * This function must be executed in the non-RT domain.
 */
void plugin_ethernet_init(void) {
	struct task_struct *__ts;
	soo_plugin_eth_t *soo_plugin_eth;

	lprintk("SOOlink: Ethernet Plugin init...\n");

	soo_plugin_eth = (soo_plugin_eth_t *) kzalloc(sizeof(soo_plugin_eth_t), GFP_KERNEL);
	BUG_ON(!soo_plugin_eth);

	soo_plugin_eth->plugin_eth_desc.tx_callback = plugin_ethernet_tx;
	soo_plugin_eth->plugin_eth_desc.if_type = SL_IF_ETH;

	soo_plugin_eth->plugin_ready = false;
	soo_plugin_eth->net_dev = NULL;

	transceiver_plugin_register(&soo_plugin_eth->plugin_eth_desc);

#if 0
	transceiver_plugin_register(&plugin_tcp_desc);
#endif

	__ts = kthread_create(net_dev_detect, NULL, "eth_detect");
	BUG_ON(!__ts);

	add_thread(current_soo, __ts->pid);
	wake_up_process(__ts);
}

