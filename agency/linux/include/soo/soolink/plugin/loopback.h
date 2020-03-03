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

#ifndef PLUGIN_LOOPBACK_H
#define PLUGIN_LOOPBACK_H

#include <linux/skbuff.h>

#include <soo/soolink/soolink.h>

#define LOOPBACK_NET_DEV_NAME 	"lo"

void sl_plugin_loopback_rx(struct sk_buff *skb);
void propagate_plugin_loopback_send(void);
void rtdm_propagate_sl_plugin_loopback_rx(void);

#endif /* PLUGIN_LOOPBACK_H */
