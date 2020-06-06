/*
 * Copyright (C) 2016-2018 Baptiste Delporte <bonel@bonel.net>
 * Copyright (C) 2018 Jean-Pierre Miceli <jean-pierre.miceli@heig-vd.ch>
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

#ifndef VNETSTREAM_H
#define VNETSTREAM_H

#include <soo/uapi/soo.h>

#include <soo/ring.h>
#include <soo/grant_table.h>

#define VNETSTREAM_NAME		"vnetstream"
#define VNETSTREAM_PREFIX	"[" VNETSTREAM_NAME "] "

#define VNETSTREAM_CMD_DATA_SIZE	256

/* Maximal number of supported SOOs in the ecosystem */
#define VNETSTREAM_MAX_SOO	6

#define VNETSTREAM_MESSAGE_SIZE	32

#if defined(CONFIG_SOO_ME)

/* From transceiver/transceiver.h in the agency */
typedef struct {
	uint8_t packet_type;
	uint32_t peerID;
	uint8_t	payload[0];
} netstream_transceiver_packet_t;

#endif /* CONFIG_SOO_ME */

/* Messaging packet format */

typedef struct {
	uint32_t	status;
	uint8_t		sender_agencyUID[SOO_AGENCY_UID_SIZE];
	char		message[VNETSTREAM_MESSAGE_SIZE];
} vnetstream_msg_t;

typedef struct {
	vnetstream_msg_t	msg[VNETSTREAM_MAX_SOO];
} vnetstream_pkt_t;

/* Transceiver packet payload size */
#define VNETSTREAM_PACKET_SIZE	(sizeof(vnetstream_pkt_t))

/* Commands */

typedef enum {
	VNETSTREAM_CMD_NULL = 0,
	VNETSTREAM_CMD_STREAM_INIT,
	VNETSTREAM_CMD_GET_NEIGHBOURHOOD,
	VNETSTREAM_CMD_STREAM_TERMINATE
} vnetstream_cmd_t;

typedef struct {
	vnetstream_cmd_t	cmd;
	long			arg;
} vnetstream_cmd_request_t;

typedef struct  {
	int	ret;
	char	data[VNETSTREAM_CMD_DATA_SIZE]; /* Multiple purpose data */
} vnetstream_cmd_response_t;

DEFINE_RING_TYPES(vnetstream_cmd, vnetstream_cmd_request_t, vnetstream_cmd_response_t);

/* TX */

typedef struct {
	uint32_t	offset;
} vnetstream_tx_request_t;

typedef struct  {
	int	ret;
} vnetstream_tx_response_t;

DEFINE_RING_TYPES(vnetstream_tx, vnetstream_tx_request_t, vnetstream_tx_response_t);

/* RX */

typedef struct {
	int ret;
} vnetstream_rx_request_t;

typedef struct {
	uint32_t	offset;
} vnetstream_rx_response_t;

DEFINE_RING_TYPES(vnetstream_rx, vnetstream_rx_request_t, vnetstream_rx_response_t);

bool vnetstream_ready(void);

#if defined(CONFIG_SOO_ME)

void vnetstream_stream_init(void *data, size_t size);
char *vnetstream_get_neighbourhood(void);
void vnetstream_send(void *data);
void vnetstream_stream_terminate(void);

#endif /* CONFIG_SOO_ME */

#endif /* VNETSTREAM_H */
