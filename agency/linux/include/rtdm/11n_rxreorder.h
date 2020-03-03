/*
 * Copyright (C) 2017-2018 Baptiste Delporte <bonel@bonel.net>
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

#ifndef _11N_RXREORDER_H
#define _11N_RXREORDER_H

int rtdm_mwifiex_11n_rx_reorder_pkt(struct mwifiex_private *priv,
				u16 seq_num, u16 tid,
				u8 *ta, u8 pkt_type, void *payload);

#endif /* _11N_RXREORDER_H */

