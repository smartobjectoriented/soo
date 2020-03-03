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

#ifndef SDIO_H
#define SDIO_H

#include <linux/netdevice.h>
#include <linux/mmc/sdio_func.h>

struct mwifiex_adapter;
struct mwifiex_tx_param;
struct sdio_func;

int rtdm_mwifiex_write_reg(struct mwifiex_adapter *adapter, u32 reg, u8 data);
int rtdm_mwifiex_read_reg(struct mwifiex_adapter *adapter, u32 reg, u8 *data);
int rtdm_mwifiex_process_int_status(struct mwifiex_adapter *adapter);

int rtdm_mwifiex_host_to_card_mp_aggr(struct mwifiex_adapter *adapter, u8 *payload, u32 pkt_len, u32 port, u32 next_pkt_len);

int rtdm_mwifiex_read_data_sync(struct mwifiex_adapter *adapter, u8 *buffer, u32 len, u32 port, u8 claim);
int rtdm_mwifiex_write_data_sync(struct mwifiex_adapter *adapter, u8 *buffer, u32 pkt_len, u32 port);

void rtdm_mwifiex_sdio_interrupt(struct sdio_func *func);

int rtdm_mwifiex_pm_wakeup_card(struct mwifiex_adapter *adapter);

struct sdio_func *get_sdio_func(int fn);

bool is_claim_host_owned_by(int cpu);

void init_rtdm_mwifiex_mutex(void);
int mwifiex_decode_rx_packet(struct mwifiex_adapter *adapter, struct sk_buff *skb, u32 upld_typ);

#endif /* SDIO_H */

