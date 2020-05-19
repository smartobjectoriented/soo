/*
 * Copyright (C) 2016-2018 Baptiste Delporte <bonel@bonel.net>
 * Copyright (C) 2018 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef __IO_NETIF_H__
#define __IO_NETIF_H__

#include <soo/ring.h>
#include <soo/grant_table.h>

/*
 * Notifications after enqueuing any type of message should be conditional on
 * the appropriate req_event or rsp_event field in the shared ring.
 * If the client sends notification for rx requests then it should specify
 * feature 'feature-rx-notify' via vbus. Otherwise the backend will assume
 * that it cannot safely queue packets (as it may not be kicked to send them).
 */

/*
 * This is the 'wire' format for packets:
 *  Request 1: netif_tx_request  -- VNETTXF_* (any flags)
 * [Request 2: netif_extra_info]    (only if request 1 has VNETTXF_extra_info)
 * [Request 3: netif_extra_info]    (only if request 2 has NETIF_EXTRA_MORE)
 *  Request 4: netif_tx_request  -- VNETTXF_more_data
 *  Request 5: netif_tx_request  -- VNETTXF_more_data
 *  ...
 *  Request N: netif_tx_request  -- 0
 */

/* Protocol checksum field is blank in the packet (hardware offload)? */
#define _VNETTXF_csum_blank		(0)
#define  VNETTXF_csum_blank		(1U<<_VNETTXF_csum_blank)

/* Packet data has been validated against protocol checksum. */
#define _VNETTXF_data_validated	(1)
#define  VNETTXF_data_validated	(1U<<_VNETTXF_data_validated)

/* Packet continues in the next request descriptor. */
#define _VNETTXF_more_data		(2)
#define  VNETTXF_more_data		(1U<<_VNETTXF_more_data)

/* Packet to be followed by extra descriptor(s). */
#define _VNETTXF_extra_info		(3)
#define  VNETTXF_extra_info		(1U<<_VNETTXF_extra_info)


/* "tx" rings structures */

struct netif_tx_request {
    grant_ref_t gref;      /* Reference to buffer page */
    uint16_t offset;       /* Offset within buffer page */
    uint16_t flags;        /* VNETTXF_* */
    uint16_t id;           /* Echoed in response message. */
    uint16_t size;         /* Packet size in bytes.       */
};

/* Types of netif_extra_info descriptors. */
#define NETIF_EXTRA_TYPE_NONE	(0)  /* Never used - invalid */
#define NETIF_EXTRA_TYPE_GSO	(1)  /* u.gso */
#define NETIF_EXTRA_TYPE_MAX	(2)

/* netif_extra_info flags. */
#define _NETIF_EXTRA_FLAG_MORE	(0)
#define  NETIF_EXTRA_FLAG_MORE	(1U<<_NETIF_EXTRA_FLAG_MORE)

/* GSO types - only TCPv4 currently supported. */
#define NETIF_GSO_TYPE_TCPV4	(1)

/*
 * This structure needs to fit within both netif_tx_request and
 * netif_rx_response for compatibility.
 */
struct netif_extra_info {
	uint8_t type;  /* NETIF_EXTRA_TYPE_* */
	uint8_t flags; /* NETIF_EXTRA_FLAG_* */

	union {
		struct {
			/*
			 * Maximum payload size of each segment. For
			 * example, for TCP this is just the path MSS.
			 */
			uint16_t size;

			/*
			 * GSO type. This determines the protocol of
			 * the packet and any extra features required
			 * to segment the packet properly.
			 */
			uint8_t type; /* NETIF_GSO_TYPE_* */

			/* Future expansion. */
			uint8_t pad;

			/*
			 * GSO features. This specifies any extra GSO
			 * features required to process this packet,
			 * such as ECN support for TCPv4.
			 */
			uint16_t features; /* NETIF_GSO_FEAT_* */
		} gso;

		uint16_t pad[3];
	} u;
};

struct netif_tx_response {
	uint16_t id;
	int16_t  status;       /* NETIF_RSP_* */
};

/* "rx" rings structures */

struct netif_rx_request {
	uint16_t    id;        /* Echoed in response message.        */
	grant_ref_t gref;      /* Reference to incoming granted frame */
};

/* Packet data has been validated against protocol checksum. */
#define _VNETRXF_data_validated	(0)
#define  VNETRXF_data_validated	(1U << _VNETRXF_data_validated)

/* Protocol checksum field is blank in the packet (hardware offload)? */
#define _VNETRXF_csum_blank		(1)
#define  VNETRXF_csum_blank		(1U << _VNETRXF_csum_blank)

/* Packet continues in the next request descriptor. */
#define _VNETRXF_more_data		(2)
#define  VNETRXF_more_data		(1U << _VNETRXF_more_data)

/* Packet to be followed by extra descriptor(s). */
#define _VNETRXF_extra_info		(3)
#define  VNETRXF_extra_info		(1U << _VNETRXF_extra_info)

/* GSO Prefix descriptor. */
#define _VNETRXF_gso_prefix		(4)
#define  VNETRXF_gso_prefix		(1U << _VNETRXF_gso_prefix)

struct netif_rx_response {
    uint16_t id;
    uint16_t offset;       /* Offset in page of start of received packet  */
    uint16_t flags;        /* NETRXF_* */
    int16_t  status;       /* -ve: BLKIF_RSP_* ; +ve: Rx'ed pkt size. */
};

/*
 * Generate netif ring structures and types.
 */

DEFINE_RING_TYPES(netif_tx, struct netif_tx_request, struct netif_tx_response);
DEFINE_RING_TYPES(netif_rx, struct netif_rx_request, struct netif_rx_response);

#define NETIF_RSP_DROPPED	-2
#define NETIF_RSP_ERROR	-1
#define NETIF_RSP_OKAY	 0

/* No response: used for auxiliary requests (e.g., netif_extra_info). */
#define NETIF_RSP_NULL	 1

#endif
