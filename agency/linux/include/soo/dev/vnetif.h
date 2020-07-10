/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef VNETIF_H
#define VNETIF_H

#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of.h>

#include <soo/vbus.h>
#include <soo/dev/vnet.h>


struct vnetif {
	/* Unique identifier for this interface. */
	domid_t          domid;
	unsigned int     handle;

	u8               fe_dev_addr[6];
	struct list_head fe_mcast_addr;
	unsigned int     fe_mcast_count;

	/* Frontend feature information. */
	int gso_mask;

	u8 can_sg:1;
	u8 ip_csum:1;
	u8 ipv6_csum:1;
	u8 multicast_control:1;

	/* Is this interface disabled? True when backend discovers
	 * frontend is rogue.
	 */
	bool disabled;
	unsigned long status;
	unsigned long drain_timeout;
	unsigned long stall_timeout;

	/* Queues */
	struct xenvif_queue *queues;
	unsigned int num_queues; /* active queues, resource allocated */
	unsigned int stalled_queues;

	spinlock_t lock;

#ifdef CONFIG_DEBUG_FS
	struct dentry *xenvif_dbg_root;
#endif
	unsigned int ctrl_irq;

	/* Miscellaneous private stuff. */
	struct net_device *dev;

	vnet_t *vnet;
};

void link_vnet(struct net_device *dev, vnet_t *vnet);
void unlink_vnet(struct net_device *dev);

struct net_device * vnetif_init(int domid);

void netif_rx_packet(struct net_device *dev, void* data, size_t len);


#endif //VNETIF_H
