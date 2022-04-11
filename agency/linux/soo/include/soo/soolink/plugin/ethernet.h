/*
 * Copyright (C) 2016-2018 Baptiste Delporte <bonel@bonel.net>
 * Copyright (C) 2018-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef PLUGIN_ETHERNET_H
#define PLUGIN_ETHERNET_H

#include <linux/skbuff.h>

#include <soo/soolink/soolink.h>

#define ETHERNET_NET_DEV_NAME 	"eth0"

void plugin_ethernet_rx(struct sk_buff *skb, struct net_device *net_dev, uint8_t *mac_src);
void plugin_tcp_rx(void *data, size_t size);

void plugin_ethernet_delete_remote(uint64_t agencyUID);

#endif /* PLUGIN_ETHERNET_H */
