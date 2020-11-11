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
 * On our side, we can have packet payload of 1'472 bytes taking into
 * account the various headers.
 */
#define SL_PACKET_PAYLOAD_MAX_SIZE 	1472

#define CODER_CONSISTENCY_SIMPLE	0x00
#define CODER_CONSISTENCY_EXT		0x01

/*
 * Simple packet format is used when a data block does not exceed SL_CODER_PACKET_MAX_SIZE
 */
typedef struct {
	uint8_t	consistency_type;  	/* Consistency algorithm ID */
} simple_packet_format_t;

/*
 * Extended packet format used when a block must be splitted into multiple packets.
 */
typedef struct {
	 uint8_t	consistency_type; 	/* Consistency ID refering to a specific consistency algorithm */
	 uint32_t	packetID;		/* Sequential packet ID */
	 uint32_t	nr_packets;		/* Number of packets */
	 uint16_t	payload_length;		/* Length of the payload */
} ext_packet_format_t;

typedef union {
	simple_packet_format_t	simple;
	ext_packet_format_t	ext;
} transcoder_packet_format_t;

/*
 * General transcoder packet format; it has to be noted that the payload is refered by itsbyte
 */
typedef struct {

	transcoder_packet_format_t u;

	uint8_t payload[0];

} transcoder_packet_t;

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

void transcoder_init(void);

void coder_send(sl_desc_t *sl_desc, void *data, size_t size);

#endif /* TRANSCODER_H */
