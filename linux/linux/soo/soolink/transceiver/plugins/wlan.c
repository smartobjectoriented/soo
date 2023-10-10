/*
 * Copyright (C) 2016-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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
#include <linux/rtnetlink.h>

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

static uint8_t broadcast_addr[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

typedef struct {
	bool plugin_ready;
	struct net_device *net_dev;
	plugin_desc_t plugin_wlan_desc;
} soo_plugin_wlan_t;

/*
 * Transmit on the WLAN interface
 * At this point, we assume that the interface is up and available.
 */
void plugin_wlan_tx(sl_desc_t *sl_desc, void *data, size_t size) {
	struct sk_buff *skb;
	__be16 proto;
	const uint8_t *dest;
	int cpu;
	struct netdev_queue *txq;
	soo_plugin_wlan_t *soo_plugin_wlan;

	soo_plugin_wlan = container_of(current_soo_plugin->__intf[SL_IF_WLAN], soo_plugin_wlan_t, plugin_wlan_desc);

	if (unlikely(!soo_plugin_wlan->plugin_ready))
		return;

	DBG("Requester type: %d\n", __plugin_send_args.sl_desc->req_type);

	/* Abort if the net device is not ready */
	if (unlikely(!soo_plugin_wlan->net_dev))
		return ;

	skb = alloc_skb(ETH_HLEN + size + LL_RESERVED_SPACE(soo_plugin_wlan->net_dev), GFP_KERNEL);
	BUG_ON(skb == NULL);

	skb_reserve(skb, ETH_HLEN);

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

	dev_hard_header(skb, soo_plugin_wlan->net_dev, proto, dest, soo_plugin_wlan->net_dev->dev_addr, skb->len);
	if ((!netif_running(soo_plugin_wlan->net_dev)) || (!netif_device_present(soo_plugin_wlan->net_dev))) {
		kfree_skb(skb);
		return;
	}

	txq = skb_get_tx_queue(soo_plugin_wlan->net_dev, skb);

	local_bh_disable();
	cpu = smp_processor_id();

	HARD_TX_LOCK(soo_plugin_wlan->net_dev, txq, cpu);

	/* Normally, this should never happen,
	 * but in case of overspeed...
	 */
	while (netif_xmit_stopped(txq))
	{
		HARD_TX_UNLOCK(soo_plugin_wlan->net_dev, txq);
		local_bh_enable();

		msleep(100);

		local_bh_disable();
		HARD_TX_LOCK(soo_plugin_wlan->net_dev, txq, cpu);
	}

	netdev_start_xmit(skb, soo_plugin_wlan->net_dev, txq, 0);

	HARD_TX_UNLOCK(soo_plugin_wlan->net_dev, txq);
	local_bh_enable();

}

rx_handler_result_t plugin_wlan_rx_handler(struct sk_buff **pskb) {
	soo_plugin_wlan_t *soo_plugin_wlan;
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

	soo_plugin_wlan = container_of(current_soo_plugin->__intf[SL_IF_WLAN], soo_plugin_wlan_t, plugin_wlan_desc);

	plugin_rx(&soo_plugin_wlan->plugin_wlan_desc,
		  get_sl_req_type_from_protocol(ntohs(skb->protocol)), hdr->h_source, skb->data, skb->len);

	kfree_skb(skb);

	*pskb = NULL;

#if 0 /* Debugging purpose for measure bandwidth */
	{
		static bool lock = false;
		static int ii;
		static int total;
		static s64 start, end;

		if (!lock && (rsp->req_type == SL_REQ_DCM)) {

			lock = true;
			start = ktime_to_ns(ktime_get());
			ii = 0;
			total = 0;
		}

		if (lock && (rsp->req_type == SL_REQ_DCM)) {
			ii++;
			total += skb->len;
			if (ii == 1400) {
				end = ktime_to_ns(ktime_get());
				lprintk("## delta: %lld   total bytes: %d\n", end-start, total);
				ii = 0;
				lock = false;

			}
			kfree_skb(skb);
			return ;
		}
	}
#endif

	return RX_HANDLER_CONSUMED;
}

#if 0 /* Debugging purpose for bandwidth assessment */
char data[1500];

static int streampacket(void *args) {
	struct sk_buff *skb;
	__be16 proto;
	uint8_t *__data;
	uint8_t dest[6];
	int i;
	int cpu;
	struct netdev_queue *txq;

	for (i = 0; i < 1500; i++)
		data[i] = i;

	msleep(5000);

	dest[0] = 0xdc;
	dest[1] = 0xa6;
	dest[2] = 0x32;
	dest[3] = 0x7e;
	dest[4] = 0x8;
	dest[5] = 0x6e;

	while (1) {

		skb = alloc_skb(ETH_HLEN + 1500 + LL_RESERVED_SPACE(net_dev), GFP_ATOMIC);
		if (!skb) {
			lprintk("## ERROR\n");
			BUG();
		}

		skb_reserve(skb, LL_RESERVED_SPACE(net_dev));

		proto = get_protocol_from_sl_req_type(SL_REQ_DCM);

		__data = skb_put(skb, 1500);

		memcpy(__data, data, 1500);

		dev_hard_header(skb, net_dev, proto, dest, net_dev->dev_addr, skb->len);

		skb->dev = net_dev;
		skb->protocol = htons(proto);

		txq = skb_get_tx_queue(net_dev, skb);

		local_bh_disable();
		cpu = smp_processor_id();
		HARD_TX_LOCK(net_dev, txq, cpu);

		while (netif_xmit_stopped(txq))
			schedule();

		netdev_start_xmit(skb, net_dev, txq, 0);

		HARD_TX_UNLOCK(net_dev, txq);
		local_bh_enable();

	}
	return 0;
}

#endif

/**
 * As the plugin is initialized before the net device, the plugin cannot be used until the net dev
 * is properly initialized. The net device detection thread loops until the interface is initialized.
 */
static int net_dev_detect(void *args) {
	soo_plugin_wlan_t *soo_plugin_wlan;

	soo_plugin_wlan = container_of(current_soo_plugin->__intf[SL_IF_WLAN], soo_plugin_wlan_t, plugin_wlan_desc);

	while (!soo_plugin_wlan->net_dev) {
		msleep(NET_DEV_DETECT_DELAY);
		soo_plugin_wlan->net_dev = dev_get_by_name(&init_net, WLAN_NET_DEV_NAME);
	}
	/* Wait for the net_device to be running AND operational */
	while (!(netif_running(soo_plugin_wlan->net_dev) && netif_oper_up(soo_plugin_wlan->net_dev))) {
		msleep(NET_DEV_DETECT_DELAY);
	}

	DBG("NET_DEV now operational and running!\n");

	soo_plugin_wlan->plugin_ready = true;

	rtnl_lock();
	netdev_rx_handler_register(soo_plugin_wlan->net_dev, plugin_wlan_rx_handler, NULL);
	rtnl_unlock();

#if 0 /* Debugging purpose */
	kthread_run(streampacket, NULL, "streampacket");
#endif

	return 0;
}

/**
 * This function must be executed in the non-RT domain.
 */
void plugin_wlan_init(void) {
	struct task_struct *__ts;
	soo_plugin_wlan_t *soo_plugin_wlan;

	lprintk("SOOlink: WLAN Plugin init...\n");

	soo_plugin_wlan = (soo_plugin_wlan_t *) kzalloc(sizeof(soo_plugin_wlan_t), GFP_KERNEL);
	BUG_ON(!soo_plugin_wlan);

	soo_plugin_wlan->plugin_wlan_desc.tx_callback = plugin_wlan_tx;
	soo_plugin_wlan->plugin_wlan_desc.if_type = SL_IF_WLAN;

	soo_plugin_wlan->plugin_ready = false;
	soo_plugin_wlan->net_dev = NULL;

	transceiver_plugin_register(&soo_plugin_wlan->plugin_wlan_desc);

#if 0
	transceiver_plugin_register(&plugin_tcp_desc);
#endif

	__ts = kthread_create(net_dev_detect, NULL, "wlan_detect");
	BUG_ON(!__ts);

	add_thread(current_soo, __ts->pid);
	wake_up_process(__ts);
}

