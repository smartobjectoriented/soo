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

#include <xenomai/rtdm/driver.h>

#include <soo/uapi/soo.h>

/* Sender / plugin flags */

/*
 * SOOlink requester type.
 *
 * The following requester type are defined:
 * - SL_REQ_DCM:	used by the DCM for ME migration
 * - SL_REQ_TCP:	used by application which needs tcp/ip routing
 * - SL_REQ_BT:
 *
 */
typedef enum {
	SL_REQ_DCM = 0,
	SL_REQ_TCP,
	SL_REQ_BT,
	SL_REQ_DISCOVERY,
	SL_REQ_NETSTREAM,
	SL_REQ_PEER,
	SL_REQ_N
} req_type_t;

/* Type of interface a requester can use with Soolink */
typedef enum {
	SL_IF_WLAN = 0,
	SL_IF_ETH,
	SL_IF_TCP,
	SL_IF_BT,
	SL_IF_LOOP
} if_type_t;

/* Transmission mode type */
typedef enum {
	SL_MODE_BROADCAST,
	SL_MODE_UNICAST,
	SL_MODE_UNIBROAD,
	SL_MODE_NETSTREAM,
} trans_mode_t;

/* Datalink protocol */
typedef enum {
	SL_DL_PROTO_DISABLED = 0,
	SL_DL_PROTO_WINENET,
	SL_DL_PROTO_N
} datalink_proto_t;

struct sl_desc;
typedef struct sl_desc sl_desc_t;

typedef void (*rtdm_recv_callback_t)(sl_desc_t *sl_desc, void *data, size_t size);

/* Soolink descriptor */
typedef struct sl_desc {

	struct list_head list;  /* Used to keep a list of requesters */

	req_type_t	req_type;
	if_type_t	if_type;
	trans_mode_t	trans_mode;

	/* prio can be 1 to 100 (the greater the higher priority) */
	uint32_t	prio;

	/* Tell soolink that the requester has exclusive access on the associated interface */
	bool		exclusive;

	/* Identification of the source, if any */
	agencyUID_t	agencyUID_from;

	/* (optional) Identification of the target, if any */
	agencyUID_t	agencyUID_to;

	/* (optional) Receive callback function */
	rtdm_recv_callback_t rtdm_recv_callback;

	/* Event and parameters to perform synchronous call to the Decoder receive function */
	rtdm_event_t	recv_event;
	void		*incoming_block;

	/*
	 * Number of received bytes.
	 * In case of netstream mode, the field is used to store the fixed packet length.
	 */
	size_t		incoming_block_size;

} sl_desc_t;

typedef struct {
	sl_desc_t	*sl_desc;
} sl_tx_request_args_t;

typedef struct {
	sl_desc_t	*sl_desc;
	void		*data;
	size_t		size;
	agencyUID_t	*agencyUID;
	uint32_t	prio;
} sl_send_args_t;

typedef struct {
	sl_desc_t	*sl_desc;
	void		**data;
	size_t		*size_p;
} sl_recv_args_t;

void rtdm_propagate_sl_tx_request(void);
void rtdm_propagate_sl_send(void);
void rtdm_propagate_sl_recv(void);

void sl_set_exclusive(sl_desc_t *sl_desc, bool active);

bool is_exclusive(sl_desc_t *sl_desc);

void rtdm_sl_set_recv_callback(sl_desc_t *sl_desc, rtdm_recv_callback_t rtdm_recv_fn);

sl_desc_t *sl_register(req_type_t req_type, if_type_t if_type, trans_mode_t trans_mode);
void sl_unregister(sl_desc_t *sl_desc);
sl_desc_t *find_sl_desc_by_req_type(req_type_t req_type);

void sl_tx_request(sl_desc_t *sl_desc);
void rtdm_sl_tx_request(sl_desc_t *sl_desc);

bool sl_try_update_tx(void);
void sl_release_update_tx(void);

bool sl_ready_to_send(sl_desc_t *sl_desc);

/* prio can be 0 to 99 (the greater the higher priority) */
void sl_send(sl_desc_t *sl_desc, void *data, size_t size, agencyUID_t *agencyUID, uint32_t prio);
int sl_recv(sl_desc_t *sl_desc, void **data);
void rtdm_sl_send(sl_desc_t *sl_desc, void *data, size_t size, agencyUID_t *agencyUID, uint32_t prio);
void rtdm_sl_recv(sl_desc_t *sl_desc, void **data, size_t *size_p);

/* Helpers for streams */
void rtdm_sl_stream_init(sl_desc_t *sl_desc, void *data, size_t packet_size);
void rtdm_sl_stream_send(sl_desc_t *sl_desc, void *data);
int rtdm_sl_stream_recv(sl_desc_t *sl_desc, void **data);
void rtdm_sl_stream_terminate(sl_desc_t *sl_desc);

void sl_discovery_start(void);

int sl_get_neighbours(struct list_head *new_list);
int rtdm_sl_get_neighbours(struct list_head *new_list);

#endif /* SOOLINK_H */
