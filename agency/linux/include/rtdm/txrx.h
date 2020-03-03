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

#ifndef TXRX_H
#define TXRX_H

struct mwifiex_adapter;
struct mwifiex_tx_param;
struct mwifiex_private;

int rtdm_mwifiex_handle_rx_packet(struct mwifiex_adapter *adapter, struct sk_buff *skb);

int rtdm_mwifiex_host_to_card(struct mwifiex_adapter *adapter, struct sk_buff *skb, struct mwifiex_tx_param *tx_param);
int rtdm_mwifiex_process_tx(struct mwifiex_private *priv, struct sk_buff *skb, struct mwifiex_tx_param *tx_param);

int rtdm_mwifiex_write_data_complete(struct mwifiex_adapter *adapter, struct sk_buff *skb, int aggr, int status);
void rtdm_mwifiex_event_data_sent(void);

void rtdm_txrx_init_data_sent(void);

#endif /* TXRX_H */

