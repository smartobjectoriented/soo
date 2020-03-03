
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


void rtdm_mwifiex_wmm_add_buf_txqueue(struct mwifiex_private *priv, struct sk_buff *skb);
void rtdm_mwifiex_wmm_process_tx(struct mwifiex_adapter *adapter);

struct mwifiex_ra_list_tbl *mwifiex_wmm_get_highest_priolist_ptr(struct mwifiex_adapter *adapter, struct mwifiex_private **priv, int *tid);
int mwifiex_is_11n_aggragation_possible(struct mwifiex_private *priv, struct mwifiex_ra_list_tbl *ptr, int max_buf_size);

int mwifiex_bypass_txlist_empty(struct mwifiex_adapter *adapter);
