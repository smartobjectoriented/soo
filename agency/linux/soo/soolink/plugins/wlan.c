
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
extern int woal_hard_start_xmit(struct sk_buff *skb, struct net_device *dev);
#endif /* CONFIG_MARVELL_MWIFIEX_MLAN */

static struct net_device *net_dev = NULL;

static bool plugin_ready = false;

static uint8_t broadcast_addr[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/*
 * Transmit on the WLAN interface from the RT domain.
 * At this point, we assume that the interface is up and available in the non-RT domain.
 */
void plugin_wlan_tx(sl_desc_t *sl_desc, void *data, size_t size, unsigned long flags) {
	struct sk_buff *skb;
	__be16 proto;
	uint8_t *__data;
	const uint8_t *dest;
	int cpu;
	struct netdev_queue *txq;

	if (!plugin_ready)
			return ;

	DBG("Requester type: %d\n", __plugin_send_args.sl_desc->req_type);

	/* Abort if the net device is not ready */
	if (unlikely(!plugin_ready))
		return ;

	skb = alloc_skb(ETH_HLEN + size + LL_RESERVED_SPACE(net_dev), GFP_ATOMIC);
	BUG_ON(skb == NULL);

	skb_reserve(skb, LL_RESERVED_SPACE(net_dev));

	proto = get_protocol_from_sl_req_type(sl_desc->req_type);


	/* If no valid recipient agency UID is given, broadcast the packet */
	if (!agencyUID_is_valid(&sl_desc->agencyUID_to))
		dest = broadcast_addr;
	else
		dest = get_mac_addr(&sl_desc->agencyUID_to);

	__data = skb_put(skb, size);
	memcpy(__data, data, size);

	dev_hard_header(skb, net_dev, proto, dest, net_dev->dev_addr, skb->len);
	if ((!netif_running(net_dev)) || (!netif_device_present(net_dev))) {
		kfree_skb(skb);
		return;
	}

	skb->dev = net_dev;
	skb->protocol = htons(proto);

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

static plugin_desc_t plugin_wlan_desc = {
	.tx_callback = plugin_wlan_tx,
	.if_type = SL_IF_WLAN
};

/*
 * This function has to be called in a non-realtime context.
 * Called from drivers/net/wireless/marvell/mlinux/moal_shim.c
 */
void plugin_wlan_rx(struct sk_buff *skb, struct net_device *net_dev, uint8_t *mac_src) {
	req_type_t req_type;

	req_type = get_sl_req_type_from_protocol(ntohs(skb->protocol));

	plugin_rx(&plugin_wlan_desc, req_type, skb->data, skb->len, mac_src);

	kfree_skb(skb);
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
 * In the case of the WLAN plugin, the net dev is not initialized until the join operation is successful.
 */
static int net_dev_detect(void *args) {
	while (!net_dev) {
		msleep(NET_DEV_DETECT_DELAY);
		net_dev = dev_get_by_name(&init_net, WLAN_NET_DEV_NAME);
	}

	plugin_ready = true;

#if 0 /* Debugging purpose */
	kthread_run(streampacket, NULL, "streampacket");
#endif

	return 0;
}

/**
 * This function must be executed in the non-RT domain.
 */
static int plugin_wlan_init(void) {
	lprintk("SOOlink plugin Wlan initializing ...\n");

	transceiver_plugin_register(&plugin_wlan_desc);

	/* This works only if the mwifiex driver is active */

	kthread_run(net_dev_detect, NULL, "wlan_detect");

	return 0;
}

soolink_plugin_initcall(plugin_wlan_init);
