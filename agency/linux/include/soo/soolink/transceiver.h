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

#ifndef TRANSCEIVER_H
#define TRANSCEIVER_H

#include <linux/types.h>

#define TRANSCEIVER_PKT_DATA	 	1
#define TRANSCEIVER_PKT_DATALINK 	2

typedef struct {

	uint8_t packet_type;

	/* The size is the size of the payload */
	size_t size;

	uint32_t transID;

	/*
	 * First byte of the payload. Accessing to its address gives a direct access to the
	 * payload buffer.
	 */
	uint8_t	payload[0];

} transceiver_packet_t;

void transceiver_init(void);

#endif /* TRANSCEIVER_H */
