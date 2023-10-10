/*
 * Copyright (C) 2020 Julien Quartier <julien.quartier@bluewin.ch>
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

#include <stdarg.h>

#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/if_ether.h>
#include <linux/kthread.h>
#include <linux/ethtool.h>
#include <linux/rtnetlink.h>
#include <linux/if_vlan.h>
#include <linux/vmalloc.h>
#include <linux/syscalls.h>
#include <linux/inetdevice.h>

#include <soo/vbus.h>
#include <soo/dev/vnet.h>

#include <net/arp.h>
#include <net/ip.h>
#include <net/route.h>
#include <net/ip_fib.h>
#include <net/rtnetlink.h>
#include <net/net_namespace.h>
#include <net/addrconf.h>

#include "vnetif.h"
#include "vnetifutil_priv.h"

static struct net_device_stats *vnetif_get_stats(struct net_device *dev)
{
	struct vnetif *vif = netdev_priv(dev);
	return &vif->dev->stats;
}

void netif_rx_packet(struct net_device *dev, void* data, size_t pktlen)
{
	struct sk_buff *skb;

	if (!pktlen) {
		dev->stats.rx_errors++;
		return;
	}

	skb = alloc_skb(pktlen, GFP_ATOMIC);

	if (!skb) {
		netdev_dbg(dev, "%s: out of mem (buf_alloc failed)\n",
			   __func__);
		dev->stats.rx_dropped++;
		return;
	}
	skb->pkt_type = PACKET_OUTGOING;

	skb_put_data(skb, data, pktlen);
	skb->protocol = eth_type_trans(skb, dev);

	skb->ip_summed = CHECKSUM_NONE;
	skb->pkt_type = PACKET_HOST;

	if (unlikely(netif_rx(skb) != NET_RX_SUCCESS))
		printk("RX ERROR");

	dev->stats.rx_packets++;
	dev->stats.rx_bytes += pktlen;

	/* and enqueue packet */
	return;
}

static netdev_tx_t
vnetif_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct vnetif *vif = netdev_priv(dev);

	if(vif->vnet == NULL || vif->vnet->send == NULL)
		return NETDEV_TX_BUSY;

	vif->vnet->send(vif->vnet, skb->data, skb->len);

	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;

	return NETDEV_TX_OK;
}

static int vnetif_open(struct net_device *dev)
{
	return 0;
}


static int vnetif_change_mtu(struct net_device *dev, int mtu)
{
	struct vnetif *vif = netdev_priv(dev);
	int max = vif->can_sg ? ETH_MAX_MTU - VLAN_ETH_HLEN : ETH_DATA_LEN;

	if (mtu > max)
		return -EINVAL;
	dev->mtu = mtu;
	return 0;
}



static const struct ethtool_ops vnetif_ethtool_ops = {
	.get_link	= NULL,

	.get_sset_count = NULL,
	.get_ethtool_stats = NULL,
	.get_strings = NULL,
};

static const struct net_device_ops vnetif_netdev_ops = {
	.ndo_start_xmit	= vnetif_start_xmit,
	.ndo_get_stats	= vnetif_get_stats,
	.ndo_open	= vnetif_open,
	.ndo_change_mtu	= vnetif_change_mtu,
	.ndo_set_mac_address = eth_mac_addr,
	.ndo_validate_addr   = eth_validate_addr,
};

void link_vnet(struct net_device *dev, vnet_t *vnet){
	struct vnetif *vif = netdev_priv(dev);

	vif->vnet = vnet;

	/* Other network architectures use bridge ip address */
#ifdef CONFIG_VNET_BACKEND_ARCH_NAT
	vnetifutil_if_set_ips(dev->name, vnet->shared_data->network, vnet->shared_data->mask);
#endif

	vnetifutil_if_up(dev->name);

	/* Set the network card up */
	netif_carrier_on(dev);

}

void unlink_vnet(struct net_device *dev){
	struct vnetif *vif = netdev_priv(dev);

	vnetifutil_if_down(dev->name);
	netif_carrier_off(dev);

	vif->vnet = NULL;
}

struct net_device * vnetif_init(int domid) {
	int err;
	struct net_device *dev;
	struct vnetif *vif;
	char name[IFNAMSIZ] = {};

	snprintf(name, IFNAMSIZ - 1, "vif%u", domid);

	/* Allocate a netdev with the max. supported number of queues.
	 * When the guest selects the desired number, it will be updated
	 * via netif_set_real_num_*_queues().
	 */
	dev = alloc_netdev_mq(sizeof(struct vnetif), name, NET_NAME_UNKNOWN,
			      ether_setup, 1);
	if (dev == NULL) {
		pr_warn("Could not allocate netdev for %s\n", name);
		return ERR_PTR(-ENOMEM);
	}

	vif = netdev_priv(dev);

	vif->domid  = domid;
	vif->handle = 0;
	vif->can_sg = 1;
	vif->ip_csum = 1;
	vif->dev = dev;
	vif->disabled = false;
	vif->drain_timeout = msecs_to_jiffies(1000);
	vif->stall_timeout = msecs_to_jiffies(1000);

	/* Start out with no queues. */
	vif->queues = NULL;
	vif->num_queues = 0;

	spin_lock_init(&vif->lock);
	INIT_LIST_HEAD(&vif->fe_mcast_addr);

	dev->netdev_ops	= &vnetif_netdev_ops;
	dev->ethtool_ops = &vnetif_ethtool_ops;

	dev->tx_queue_len = 32;

	dev->min_mtu = ETH_MIN_MTU;
	dev->max_mtu = ETH_MAX_MTU - VLAN_ETH_HLEN;

	/* Attribute a MAC address. */
	dev->dev_addr[0] = 0xde;
	dev->dev_addr[1] = 0xad;
	dev->dev_addr[2] = 0xbe;
	dev->dev_addr[3] = 0xaf;
	dev->dev_addr[4] = 0xaa;
	dev->dev_addr[5] = (u8)domid;

	netif_carrier_off(dev);

	err = register_netdev(dev);
	if (err) {
		netdev_warn(dev, "Could not register device: err=%d\n", err);
		free_netdev(dev);
		return NULL;
	}

	return dev;
}















