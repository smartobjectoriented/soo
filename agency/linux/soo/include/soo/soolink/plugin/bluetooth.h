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

#ifndef PLUGIN_BLUETOOTH_H
#define PLUGIN_BLUETOOTH_H

#include <linux/skbuff.h>

#include <soo/soolink/soolink.h>

void sl_plugin_bluetooth_rx(struct sk_buff *skb);
void propagate_plugin_bluetooth_send(void);
void rtdm_propagate_sl_plugin_bluetooth_rx(void);
void plugin_bluetooth_delete_remote(agencyUID_t *agencyUID);

#endif /* PLUGIN_BLUETOOTH_H */
