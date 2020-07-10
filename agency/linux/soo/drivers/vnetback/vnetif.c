/*
 * Copyright (C) 2018,2019 Baptiste Delporte <bonel@bonel.net>
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

#include <stdarg.h>

#include <soo/dev/vnetif.h>
#include <soo/vbus.h>
#include <soo/dev/vnet.h>

#include <linux/syscalls.h>
#include <linux/inetdevice.h>

#include "vnetbridge_priv.h"



static struct net_device_stats *vnetif_get_stats(struct net_device *dev)
{
	struct vnetif *vif = netdev_priv(dev);
	struct xenvif_queue *queue = NULL;
	unsigned int num_queues;
	u64 rx_bytes = 0;
	u64 rx_packets = 0;
	u64 tx_bytes = 0;
	u64 tx_packets = 0;
	unsigned int index;

	rcu_read_lock();
	num_queues = READ_ONCE(vif->num_queues);

	/* Aggregate tx and rx stats from each queue */
	/*for (index = 0; index < num_queues; ++index) {
		queue = &vif->queues[index];
		rx_bytes += queue->stats.rx_bytes;
		rx_packets += queue->stats.rx_packets;
		tx_bytes += queue->stats.tx_bytes;
		tx_packets += queue->stats.tx_packets;
	}*/

	rcu_read_unlock();

	/*vif->dev->stats.rx_bytes = rx_bytes;
	vif->dev->stats.rx_packets = rx_packets;
	vif->dev->stats.tx_bytes = tx_bytes;
	vif->dev->stats.tx_packets = tx_packets;*/

	return &vif->dev->stats;
}

void netif_rx_packet(struct net_device *dev, void* data, size_t len)
{
	struct nfeth_private *priv = netdev_priv(dev);
	unsigned short pktlen;
	struct sk_buff *skb;
	int i = 0;

	/* read packet length (excluding 32 bit crc) */
	pktlen = len;

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

	skb_reserve(skb, 0);	/* 16 byte IP header align */
	//skb_copy_to_linear_data(skb, (unsigned char *)data, pktlen);
	skb_put_data(skb, data, pktlen);
	//memcpy(skb->mac_header, data, pktlen);
	skb->protocol = eth_type_trans(skb, dev);

	//skb->dev = dev;
	//skb_put(skb, pktlen);		/* make room */
	//memcpy(skb->data, data, pktlen);

	//skb->protocol = eth_type_trans(skb, dev);


	//skb->data = skb->head;


	/*printk("[SKB]");
	while(i < skb->len){
		printk(KERN_CONT "%02x ", skb->data[i]);
		i++;
	}*/


	if (likely(netif_receive_skb(skb) == NET_RX_SUCCESS))
		printk("RX OK");
	else
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
	vnet_response_t *ring_rsp;
	int i = 0;
	static u8 broadcast[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

	/*uint32_t addr_start = *(uint32_t*)(skb->head);
	uint16_t addr_end = *(uint16_t*)(skb->head + 4);

	uint32_t dev_addr_start = *(uint32_t*)(dev->dev_addr);
	uint16_t dev_addr_end = *(uint16_t*)(dev->dev_addr + 4);

	printk("%08x%04x", addr_start, addr_end);*/
	printk("-");
	while(i < 6){
		printk(KERN_CONT "%02x ", skb->data[i]);
		i++;
	}
	i = 0;
	printk("-");
	while(i < 6){
		printk(KERN_CONT "%02x ", dev->dev_addr[i]);
		i++;
	}
	i = 0;
	printk("-");
	while(i < 6){
		printk(KERN_CONT "%02x ", broadcast[i]);
		i++;
	}
	i = 0;
	printk("-");

	if(!memcmp(skb->data, broadcast, 6) || !memcmp(skb->data, dev->dev_addr, 6)){

	/*if((addr_start == 0xffffffff && addr_end == 0xffff)
	    || (addr_start == dev_addr_start && addr_end == dev_addr_end)){*/

		if(vif->vnet == NULL || vif->vnet->send == NULL)
			return NETDEV_TX_BUSY;

		vif->vnet->send(vif->vnet, skb->data, skb->len);

		dev->stats.tx_packets++;
		dev->stats.tx_bytes += skb->len;

		printk("MATCH");
	} else {
		printk("NO MATCH");
	}


	printk("XMIT\n");

	printk("[Print rx] length: %d\n", skb->len);

	while(i < skb->len){
		printk(KERN_CONT "%02x ", skb->data[i]);
		i++;
	}

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
	.ndo_select_queue = NULL,
	.ndo_start_xmit	= vnetif_start_xmit,
	.ndo_get_stats	= vnetif_get_stats,
	.ndo_open	= vnetif_open,
	.ndo_stop	= NULL,
	.ndo_change_mtu	= vnetif_change_mtu,
	.ndo_fix_features = NULL,
	.ndo_set_mac_address = eth_mac_addr,
	.ndo_validate_addr   = eth_validate_addr,
};
#include <net/arp.h>
#include <net/ip.h>
#include <net/route.h>
#include <net/ip_fib.h>
#include <net/rtnetlink.h>
#include <net/net_namespace.h>
#include <net/addrconf.h>

void link_vnet(struct net_device *dev, vnet_t *vnet){
	struct sockaddr sockaddr;
	struct vnetif *vif = netdev_priv(dev);
	struct in_ifaddr *ifa;

	vif->vnet = vnet;

	rtnl_lock();

	/* Set the mac address */
	sockaddr.sa_family = dev->type;
	memcpy(sockaddr.sa_data, vnet->shared_data->ethaddr, ETH_ALEN);
	dev_set_mac_address(dev, &sockaddr, NULL);

	//dev->flags |= IFF_UP | IFF_RUNNING;



	/*ifa = inet_alloc_ifa();
	INIT_HLIST_NODE(&ifa->hash);
	memcpy(ifa->ifa_label, dev->name, IFNAMSIZ);
	ifa->ifa_address = ifa->ifa_local = 0xc0a835c8;
	ifa->ifa_prefixlen = 24;
	ifa->ifa_mask = 0xffffff;

	ifa->ifa_broadcast = ifa->ifa_address | ~ifa->ifa_mask;
	set_ifa_lifetime(ifa, INFINITY_LIFE_TIME, INFINITY_LIFE_TIME);
	inet_set_ifa(dev, ifa);*/

	rtnl_unlock();


	vnetbridge_if_set_ip(dev->name);

	vnetbridge_if_conf(dev->name, IFF_UP | IFF_RUNNING | IFF_BROADCAST, 1);

	//vnetbridge_add_if(SOO_BRIDGE_NAME, dev->name);

	/* Set the network card up */
	netif_carrier_on(dev);

}

void unlink_vnet(struct net_device *dev){
	struct vnetif *vif = netdev_priv(dev);
	vif->vnet = NULL;

	rtnl_lock();
	dev->flags &= ~IFF_UP & ~IFF_RUNNING;
	rtnl_unlock();

	vnetbridge_if_conf(dev->name, dev->flags | IFF_UP | IFF_RUNNING, 0);

	netif_carrier_off(dev);

	vnetbridge_remove_if(SOO_BRIDGE_NAME, dev->name);
}

struct net_device * vnetif_init(int domid) {
	int ret;
	struct device_node *np;

	unsigned int vnetif_max_queues = num_online_cpus();

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
			      ether_setup, vnetif_max_queues);
	if (dev == NULL) {
		pr_warn("Could not allocate netdev for %s\n", name);
		return ERR_PTR(-ENOMEM);
	}

	//SET_NETDEV_DEV(dev, parent);

	vif = netdev_priv(dev);

	vif->domid  = domid;
	vif->handle = 0;
	vif->can_sg = 1;
	vif->ip_csum = 1;
	vif->dev = dev;
	vif->disabled = false;
	vif->drain_timeout = msecs_to_jiffies(1000/*rx_drain_timeout_msecs*/);
	vif->stall_timeout = msecs_to_jiffies(1000/*rx_stall_timeout_msecs*/);

	/* Start out with no queues. */
	vif->queues = NULL;
	vif->num_queues = 0;

	spin_lock_init(&vif->lock);
	INIT_LIST_HEAD(&vif->fe_mcast_addr);

	dev->netdev_ops	= &vnetif_netdev_ops;
	/*dev->hw_features = NETIF_F_SG |
			   NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM |
			   NETIF_F_TSO | NETIF_F_TSO6 | NETIF_F_FRAGLIST;*/
	//dev->features = dev->hw_features | NETIF_F_RXCSUM;
	dev->ethtool_ops = &vnetif_ethtool_ops;

	dev->tx_queue_len = 32;

	dev->min_mtu = ETH_MIN_MTU;
	dev->max_mtu = ETH_MAX_MTU - VLAN_ETH_HLEN;

	/* use the same mac as the connected ME */
	//memcpy(dev->dev_addr, ethaddr, ETH_ALEN);

	dev->dev_addr[0] = 0xff;
	dev->dev_addr[1] = 1;
	dev->dev_addr[2] = 2;
	dev->dev_addr[3] = 3;
	dev->dev_addr[4] = 0xff;
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















