
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
#include <linux/spinlock.h>
#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/kthread.h>

#include <soo/uapi/console.h>
#include <soo/vbus.h>

#include <soo/soolink/plugin.h>
#include <soo/soolink/plugin/loopback.h>
#include <soo/soolink/discovery.h>

#include <soo/core/device_access.h>

static spinlock_t send_lock;
static spinlock_t recv_lock;

static volatile plugin_send_args_t plugin_send_args;
static volatile plugin_recv_args_t plugin_recv_args;

static uint8_t broadcast_addr[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/* Linux net_device structure */
static struct net_device *net_dev = NULL;

static void plugin_loopback_tx(sl_desc_t *sl_desc, void *data, size_t size) {

	if (unlikely(!net_dev))
		return;

	/* Send to the underlying loopback driver which runs in the non-RT domain */

	/* Lock to prepare args to be transfered in the RT domain. The spinlock will be unlocked in the RT domain */
	spin_lock(&send_lock);

	/* Prepare the args to send */

	plugin_send_args.sl_desc = sl_desc;
	plugin_send_args.data = data;
	plugin_send_args.size = size;

	do_sync_dom(DOMID_AGENCY, DC_PLUGIN_LOOPBACK_SEND);
}

/*
 * Called from the DC event in the RT domain.
 *
 * Sending a PHY frame requires a Linux skb to be allocated and will be completed by the formal header using
 * fields in the sl_desc descriptor if necessary.
 */
void propagate_plugin_loopback_send(void) {
	plugin_send_args_t __plugin_send_args;
	struct sk_buff *skb;
	struct netdev_queue *txq;
	__be16 proto;
	const uint8_t *dest;
	uint8_t *__data;

	__plugin_send_args = plugin_send_args;
	DBG("Requester type: %d\n", __plugin_send_args.sl_desc->req_type);

	spin_unlock(&send_lock);

	if (unlikely(!net_dev))
		BUG();

	skb = alloc_skb(ETH_HLEN + __plugin_send_args.size + LL_RESERVED_SPACE(net_dev), GFP_ATOMIC);
	BUG_ON(skb == NULL);

	skb_reserve(skb, ETH_HLEN);
	skb->priority = 7;

	proto = get_protocol_from_sl_req_type(__plugin_send_args.sl_desc->req_type);

	if (!__plugin_send_args.sl_desc->agencyUID_to)
		dest = broadcast_addr;

	__data = skb_put(skb, __plugin_send_args.size);
	memcpy(__data, __plugin_send_args.data, __plugin_send_args.size);

	dev_hard_header(skb, net_dev, proto, dest, net_dev->dev_addr, skb->len);
	if ((!netif_running(net_dev)) || (!netif_device_present(net_dev))) {
		kfree_skb(skb);
		return;
	}

	txq = netdev_pick_tx(net_dev, skb, NULL);
	netdev_start_xmit(skb, net_dev, txq, false);
}

static plugin_desc_t plugin_loopback_desc = {
	.tx_callback		= plugin_loopback_tx,
	.if_type		= SL_IF_LOOP
};


/*
 * This function is synchronous and used by non-RT agency side (Linux) in the non-realtime domain.
 */
void sl_plugin_loopback_rx(struct sk_buff *skb) {
	req_type_t req_type;

	/* Retrieve the requester type associated to this incoming data */
	req_type = get_sl_req_type_from_protocol(ntohs(skb->protocol));

	/* Lock to prepare args to be transfered in the RT domain. The spinlock will be unlocked in the RT domain */
	spin_lock(&recv_lock);

	/* Prepare the args to send */
	plugin_recv_args.req_type = req_type;
	plugin_recv_args.data = skb->data;
	plugin_recv_args.size = skb->len;

	do_sync_dom(DOMID_AGENCY_RT, DC_PLUGIN_LOOPBACK_RECV);

	/* Data are received and processed... */

	/* Now, we can release the allocated skb */
	kfree_skb(skb);
}

void rtdm_propagate_sl_plugin_loopback_rx(void) {
	plugin_recv_args_t __plugin_recv_args;

	__plugin_recv_args.req_type = plugin_recv_args.req_type;
	__plugin_recv_args.data = plugin_recv_args.data;
	__plugin_recv_args.size = plugin_recv_args.size;
	memcpy(__plugin_recv_args.mac, (void *) plugin_recv_args.mac, ETH_ALEN);

	/* Now unlock the spinlock used to protect args */
	spin_unlock(&recv_lock);

	plugin_rx(&plugin_loopback_desc, __plugin_recv_args.req_type, __plugin_recv_args.data, __plugin_recv_args.size, NULL);
}

/**
 * As the plugin is initialized before the net device, the plugin cannot be used until the net dev
 * is properly initialized. The net device detection thread loops until the interface is initialized.
 */
static int net_dev_detect(void *args) {
	while (!net_dev) {
		msleep(NET_DEV_DETECT_DELAY);
		net_dev = dev_get_by_name(&init_net, LOOPBACK_NET_DEV_NAME);
	}

	return 0;
}

/*
 * Main loopback init function
 * This function is executed in the non-RT domain.
 */
static int loopback_init(void) {
	lprintk("SOOlink: plugin Loopback initializing ...\n");

	spin_lock_init(&send_lock);
	spin_lock_init(&recv_lock);

	transceiver_plugin_register(&plugin_loopback_desc);

	kthread_run(net_dev_detect, NULL, "lo_detect");

	return 0;
}

late_initcall(loopback_init);

