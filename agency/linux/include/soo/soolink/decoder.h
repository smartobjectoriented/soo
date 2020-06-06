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

#ifndef DECODER_H
#define DECODER_H

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/ktime.h>

#include <xenomai/rtdm/driver.h>

#include <soo/soolink/soolink.h>

/* Timeout after which a block is deleted, in ms */
#define SOOLINK_DECODE_BLOCK_TIMEOUT	4000

/* A decoder block maintains all attributes of a given block which may be simple or extended */
typedef struct {
	sl_desc_t 	*sl_desc;

	/* The block frame itself */
	void		*incoming_block;

	size_t		size;
	size_t		total_size;
	size_t		real_size;

	uint32_t	n_total_packets;
	uint32_t	n_recv_packets;

	uint32_t	cur_packetID;
	uint8_t		*cur_pos;
	bool		block_ext_in_progress;
	bool		discarded_block;
	bool		complete;

	s64		last_timestamp;

	/* List of block */
	struct list_head list;
} decoder_block_t;

int decoder_recv(sl_desc_t *sl_desc, void **data);
int decoder_rx(sl_desc_t *sl_desc, void *data, size_t size);

int decoder_stream_recv(sl_desc_t *sl_desc, void **data);
int decoder_stream_rx(sl_desc_t *sl_desc, void *data);

void decoder_init(void);

#endif /* DECODER_H */
