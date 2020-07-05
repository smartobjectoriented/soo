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

#define SL_CODER_PACKET_MAX_SIZE 	1472

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

void transcoder_init(void);

#endif /* TRANSCODER_H */
