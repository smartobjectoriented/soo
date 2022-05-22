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

#ifndef SOOLINK_H
#define SOOLINK_H

#include <linux/types.h>
#include <linux/completion.h>

#include <soo/uapi/soo.h>

/* Sender / plugin flags */

/*
 * SOOlink requester type.
 *
 * The following requester type are defined:
 * - SL_REQ_DCM:	used by the DCM for ME migration
 * - SL_REQ_TCP:	currently used by vuihandler to manage Ethernet configuration (remote access from Internet to applications)
 * - SL_REQ_BT:		currently used by vuihandler and its stuff
 * - SL_REQ_DISCOVERY:  reserved to the Discovery for handling Iamasoo beacons sent between smart objects
 * - SL_REQ_PEER:	simple transmission between smart objects. Used for debugging purposes mainly.
 * - SL_REQ_DATALINK:   used by the Winenet datalink protocol for beacon management.
 *
 */
typedef enum {
	SL_REQ_DCM = 0,
	SL_REQ_TCP,
	SL_REQ_BT,
	SL_REQ_DISCOVERY,
	SL_REQ_PEER,
	SL_REQ_DATALINK,
	SL_REQ_N
} req_type_t;

/* Type of interface a requester can use with SOOlink */
typedef enum {
	SL_IF_WLAN = 0,
	SL_IF_ETH,
	SL_IF_TCP,
	SL_IF_BT,
	SL_IF_LOOP,
	SL_IF_SIM,

	/* Number of plugins */
	SL_IF_MAX
} if_type_t;

/* Transmission mode type */
typedef enum {
	SL_MODE_BROADCAST,
	SL_MODE_UNICAST,
	SL_MODE_UNIBROAD,
} trans_mode_t;

/* Datalink protocol */
typedef enum {
	SL_DL_PROTO_DISABLED = 0,
	SL_DL_PROTO_WINENET,
	SL_DL_PROTO_BT,
	SL_DL_PROTO_N
} datalink_proto_t;

struct sl_desc;
typedef struct sl_desc sl_desc_t;

/* SOOlink descriptor */
typedef struct sl_desc {

	struct list_head list;  /* Used to keep a list of requesters */

	req_type_t req_type;
	if_type_t if_type;
	trans_mode_t trans_mode;

	/* prio can be 1 to 100 (the greater the higher priority) */
	uint32_t prio;

	/* Tell soolink that the requester has exclusive access on the associated interface */
	bool exclusive;

	/* Identification of the source, if any */
	uint64_t agencyUID_from;

	/* (optional) Identification of the target, if any */
	uint64_t agencyUID_to;

	/* Event and parameters to perform synchronous call to the Decoder receive function */
	struct completion recv_event;

} sl_desc_t;

typedef struct {
	sl_desc_t	*sl_desc;
} sl_tx_request_args_t;

typedef struct {
	sl_desc_t	*sl_desc;
	void		*data;
	uint32_t	size;
	uint64_t	agencyUID;
	uint32_t	prio;
} sl_send_args_t;

typedef struct {
	sl_desc_t	*sl_desc;
	void		**data;
	uint32_t	*size_p;
} sl_recv_args_t;

void sl_set_exclusive(sl_desc_t *sl_desc, bool active);

bool is_exclusive(sl_desc_t *sl_desc);

sl_desc_t *sl_register(req_type_t req_type, if_type_t if_type, trans_mode_t trans_mode);
void sl_unregister(sl_desc_t *sl_desc);
sl_desc_t *find_sl_desc_by_req_type(req_type_t req_type);

uint32_t sl_neighbour_count(void);

int soolink_init(void);

/* prio can be 0 to 99 (the greater the higher priority) */
void sl_send(sl_desc_t *sl_desc, void *data, uint32_t size, uint64_t agencyUID, uint32_t prio);
int sl_recv(sl_desc_t *sl_desc, void **data);

#endif /* SOOLINK_H */
