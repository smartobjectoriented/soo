/*/*
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

#ifndef WINENET_H
#define WINENET_H

#include <soo/soolink/transceiver.h>
#include <soo/soolink/discovery.h>

/* Maximal number of retries */
#define WNET_RETRIES_MAX 6

/* Conversion from us to ns */
#define WNET_TIME_US_TO_NS(x) ((x) * 1000ull)

#define WNET_MAX_PACKET_TRANSID 0xffffff
#define WNET_LAST_PACKET		(1 << 24)

/* Number of bufferized packets in a frame for the n pkt / 1 ACK strategy */
#define WNET_N_PACKETS_IN_FRAME 2048

/* Express in microsecs */
#define WNET_MIN_DRAND		1000
#define WNET_MAX_DRAND		2000

#define WNET_TSPEAKER_ACK_MS	300

/*
 * Winenet states FSM
 * - INIT: first state used during early set up.
 * - IDLE: starting state and used when there is no neighbor.
 * - SPEAKER: the opportunity to send MEs until completion.
 * - LISTENER: the mode in which ME can be received.
 */
typedef enum {
	WNET_STATE_INIT = 0,
	WNET_STATE_IDLE,
	WNET_STATE_SPEAKER,
	WNET_STATE_LISTENER,
	WNET_STATE_N
} wnet_state_t;

/*
 * When a new smart object wants to join the neighborhood.
 * The rule is that the smart object with the lower agencyUID sends
 * a PING REQUEST and waits for the response to be considered as valid.
 */
typedef enum {
	WNET_PING_REQUEST = 0,
	WNET_PING_RESPONSE = 1,
} wnet_ping_t;

/*
 * Winenet beacons:
 *
 * - GO_SPEAKER: a (previous speaker) gives the turn to the next smart object.
 *
 * - ACKNOWLEDGMENT: most beacons and data must be acknowledged.
 *
 * - BROADCAST_SPEAKER: to inform the other neighbours that we are ready to send.
 *   the smart object which received this beacon binds itself to the speaker by using
 *   the private data of the neighbour_desc_t structure of Discovery.
 *
 * - PING: to establish the link within the neighborhood.
 */
typedef enum {
	WNET_BEACON_GO_SPEAKER = 0,
	WNET_BEACON_ACKNOWLEDGMENT = 1,
	WNET_BEACON_BROADCAST_SPEAKER = 2,
	WNET_BEACON_PING = 3,
	WNET_BEACON_N
} wnet_beacon_id_t;

typedef struct {

	uint8_t id;
	void *priv;
	uint8_t agencyUID[SOO_AGENCY_UID_SIZE];

} wnet_beacon_t;

typedef struct {
	sl_desc_t *sl_desc;

	bool pending;
	transceiver_packet_t *packet;
	size_t	size;
	uint32_t transID;
	bool completed;
	int ret;
	//rtdm_event_t xmit_event;
	struct completion xmit_event;

} wnet_tx_t;

typedef struct {
	sl_desc_t *sl_desc;

	bool data_received;
	bool completed;
	uint32_t transID;

	/* Last received beacon */
	wnet_beacon_t last_beacon;

} wnet_rx_t;

typedef struct {
	struct list_head list;

	/* valid means the neighbour has been confirmed through the initial ping procedure. */
	bool valid;

	/* Helper field to make a round of neighbours */
	bool processed;

	neighbour_desc_t *neighbour;
	uint32_t last_transID;
	uint8_t speakerUID[SOO_AGENCY_UID_SIZE];

} wnet_neighbour_t;

typedef void (*wnet_state_fn_t)(wnet_state_t old_state);

typedef struct {
	wnet_state_fn_t	*funcs;

	//rtdm_event_t	event;
	struct completion event;
	wnet_state_t	old_state;
	wnet_state_t	state;
} wnet_fsm_handle_t;

uint8_t *winenet_get_state_string(wnet_state_t state);
uint8_t *winenet_get_beacon_id_string(wnet_beacon_id_t beacon_id);

void winenet_send_beacon(wnet_beacon_t *outgoing_beacon, wnet_beacon_id_t beacon_id, agencyUID_t *dest_agencyUID, void *arg);

void winenet_xmit_data_processed(int ret);
void winenet_wait_xmit_event(void);
void winenet_get_last_beacon(wnet_beacon_t *last_beacon);

struct list_head *winenet_get_neighbours(void);
int winenet_get_my_index_and_listener(uint8_t *index, neighbour_desc_t *listener);
void winenet_dump_neighbours(void);
void winenet_dump_state(void);

void winenet_rx(sl_desc_t *sl_desc, plugin_desc_t *plugin_desc, void *packet, size_t size);

void winenet_change_state(wnet_fsm_handle_t *fsm_handle, wnet_state_t new_state);
wnet_state_t winenet_get_state(wnet_fsm_handle_t *fsm_handle);
void winenet_start_fsm_task(char *name, wnet_fsm_handle_t *fsm_handle);
void winenet_init(void);

/* Winenet netstream */
int winenet_netstream_request_xmit(sl_desc_t *sl_desc);
int winenet_netstream_xmit(sl_desc_t *sl_desc, void *packet, size_t size, bool completed);
void winenet_netstream_rx(sl_desc_t *sl_desc, plugin_desc_t *plugin_desc, void *packet, size_t size);
void winenet_netstream_init(void);

#endif /* WINENET_H */
