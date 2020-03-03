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


struct mwifiex_adapter;
struct mwifiex_private;
struct host_cmd_ds_command;
struct cmd_ctrl_node;

bool mwifiex_bypass_tx_queue(struct mwifiex_private *priv, struct sk_buff *skb);

/* rtdm_main.c */
void rtdm_mwifiex_queue_main_work(struct mwifiex_adapter *adapter);
int rtdm_mwifiex_main_process(struct mwifiex_adapter *adapter);
void rtdm_mwifiex_main_work(struct mwifiex_adapter *adapter);
void rtdm_mwifiex_req_extra_main_process_run(void);

/* main.c */
int mwifiex_process_rx(struct mwifiex_adapter *adapter);

/* sta_cmdresp.c */
int mwifiex_ret_802_11_tx_rate_query(struct mwifiex_private *priv,
					    struct host_cmd_ds_command *resp);

/* rtdm_cmdevt.c */
int rtdm_mwifiex_process_event(struct mwifiex_adapter *adapter);
int rtdm_mwifiex_process_cmdresp(struct mwifiex_adapter *adapter);

/* rtdm_sta_event.c */
int rtdm_mwifiex_process_sta_event(struct mwifiex_private *priv);

/* rtdm_sta_cmdresp.c */
int rtdm_mwifiex_process_sta_cmdresp(struct mwifiex_private *priv, u16 cmdresp_no,
				struct host_cmd_ds_command *resp);
