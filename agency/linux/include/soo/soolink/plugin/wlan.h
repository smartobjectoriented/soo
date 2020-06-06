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

#ifndef PLUGIN_WLAN_H
#define PLUGIN_WLAN_H

#include <linux/skbuff.h>

#include <soo/soolink/soolink.h>

#ifdef CONFIG_WLAN_VENDOR_MARVELL
#define WLAN_NET_DEV_NAME 	"mlan0"
#endif

#ifdef CONFIG_BRCMFMAC_SDIO
#define WLAN_NET_DEV_NAME 	"wlan0"
#endif

#ifndef WLAN_NET_DEV_NAME
#define WLAN_NET_DEV_NAME	"wlandummy"
#endif

void sl_plugin_wlan_rx_skb(struct sk_buff *skb, struct net_device *net_dev, uint8_t *mac_src);

void propagate_plugin_wlan_send(void);
void rtdm_propagate_sl_plugin_wlan_rx(void);

void plugin_wlan_delete_remote(agencyUID_t *agencyUID);
bool is_rtdm_wifi_enabled(void);
void rtdm_reconfigure_wifi(void);
void reconfigure_wifi(void);

#endif /* PLUGIN_WLAN_H */
