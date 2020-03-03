/*
 * Copyright (C) 2017-2019 Baptiste Delporte <bonel@bonel.net>
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

#ifndef MWIFIEX_H
#define MWIFIEX_H

#include <linux/netdevice.h>

#include <xenomai/rtdm/driver.h>

struct mwifiex_adapter;

struct mmc_host;
struct sunxi_mmc_host;

rtdm_task_t *get_packgen(void);

bool is_rtdm_wifi_enabled(void);
int rtdm_mwifiex_process_rx(struct mwifiex_adapter *adapter);
int rtdm_mwifiex_hard_start_xmit(struct sk_buff *skb, struct net_device *dev);
void rtdm_mwifiex_init_main_process(void);
void rtdm_mwifiex_init_tx_queue_event(void);
void rtdm_mwifiex_resume_tx_queue(void);
bool rtdm_mwifiex_tx_queue_stopped(void);
void rtdm_sunxi_sdio_init_funcs(void);
void init_rtdm_mwifiex_mutex(void);
void rtdm_mwifiex_set_adapter(struct mwifiex_adapter *adapter);

static inline netdev_tx_t rtdm_netdev_start_xmit(struct sk_buff *skb, struct net_device *dev, struct netdev_queue *txq, bool more) {
	int rc;
	rc = rtdm_mwifiex_hard_start_xmit(skb, dev);

	if (rc == NETDEV_TX_OK)
		txq_trans_update(txq);

	return rc;
}


#endif /* MWIFIEX_H */

