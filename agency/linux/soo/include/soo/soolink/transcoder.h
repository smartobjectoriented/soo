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

#ifndef TRANSCODER_H
#define TRANSCODER_H

#include <linux/types.h>

#include <soo/soolink/soolink.h>

/*
 * On Rpi4, iperf3 shows skb len of 1'514 bytes.
 * On our side, we can have packet payload of 1'481 bytes taking into
 * account the various headers (compatible with aarch64 too).
 */

#define SL_PACKET_PAYLOAD_MAX_SIZE 	1481

/*
 * General transcoder packet format; it has to be noted that the payload is refered by its byte
 */
typedef struct __attribute__((packed)) {

	uint32_t	packetID;		/* Sequential packet ID */
	uint32_t	nr_packets;		/* Number of packets */
	uint16_t	payload_length;		/* Length of the payload */

	uint8_t payload[0];

} transcoder_packet_t;

/* Maximum number of available blocks for a given sl_desc */
#define MAX_READY_BLOCK_COUNT		128

/* Timeout after which a block is deleted, in ms */
#define SOOLINK_DECODE_BLOCK_TIMEOUT	4000

/* A decoder block maintains all attributes of a given block which may be simple or extended */
typedef struct {
	sl_desc_t	*sl_desc;

	/* The block frame itself */
	void		*incoming_block;

	/* Accumulated size of the block */
	uint32_t	size;

	uint32_t	cur_packetID;
	uint8_t		*cur_pos;

	bool		block_ext_in_progress;

	s64		last_timestamp;

	/* When a block is fully available to the requester, it is marked as ready. */
	bool		ready;

	/* List of block */
	struct list_head list;

} decoder_block_t;

int decoder_recv(sl_desc_t *sl_desc, void **data);
void decoder_rx(sl_desc_t *sl_desc, void *data, uint32_t size);

void transcoder_init(void);

void coder_send(sl_desc_t *sl_desc, void *data, uint32_t size);

#endif /* TRANSCODER_H */
