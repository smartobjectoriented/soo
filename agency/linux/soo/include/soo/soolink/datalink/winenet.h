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
#define WNET_RETRIES_MAX 3

/* Conversion from us to ns */
#define WNET_TIME_US_TO_NS(x) ((x) * 1000ull)

#define WNET_MAX_PACKET_TRANSID 0xffffff

/* Last packet of the buffer (ME) */
#define WNET_LAST_PACKET	(1 << 24)

/*
 * Number of bufferized packets in a frame for the n pkt / 1 ACK strategy
 *
 * - On Ethernet: packet collision can lead to packet lost and therefore we cannot have
 *   a big frame.
 * - On Wifi: the physical layer handles receipt of packets correctly and we can reach
 *   a max. bandwidth with biggest frame size.
 *
 */

#ifdef CONFIG_SOOLINK_PLUGIN_WLAN
#define WNET_N_PACKETS_IN_FRAME 64
#else
#define WNET_N_PACKETS_IN_FRAME 8
#endif /* CONFIG_SOOLINK_PLUGIN_WLAN */

#define WNET_ACK_TIMEOUT_MS		800
#define WNET_LISTENER_TIMEOUT_MS	800

/*
 * State of TX processing, storedin pendig field, as follows:
 *
 * - TX_NO_DATA 		no producer required to send data out.
 * - TX_DATA_READY 		a set of frames is available and ready to be sent out.
 * - TX_DATA_COMPLETED		The producer finished to send data entirely.
 * - TX_DATA_IN_PROGRESS	Between a set of frames, waiting for subsequent send
 *
 */
#define TX_NO_DATA		0
#define TX_DATA_READY		1
#define TX_DATA_COMPLETED	2
#define TX_DATA_IN_PROGRESS	3

/* The following values are called ACK cause and
 * enables to check the status of an acknowledgment message
 */
typedef enum {
	ACK_STATUS_OK = 0,
	ACK_STATUS_TIMEOUT,
	ACK_STATUS_ABORT
} wnet_ack_status_t;

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
 * For PING beacon, When a new smart object wants to join the neighborhood.
 * The rule is that the smart object with the lower agencyUID sends
 * a PING REQUEST and waits for the response to be considered as valid.
 *
 * For a QUERY_STATE request, the response will contain the general state of the
 * smart object.
 */

typedef enum {
	WNET_NONE = 0,
	WNET_REQUEST = 1,
	WNET_RESPONSE = 2,
} wnet_reqrsp_t;

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
 * - PING (REQUEST): to establish the link within the neighborhood. The smart object which receives
 *   this beacon must check its agencyUID against the sender. The lowest agencyUID is
 *   the new speaker. It responds with a PING (RESPONSE)
 *
 * - QUERY_STATE: ask a smart object its current state, i.e. the paired speaker and the processing state.
 *
 */

#define WNET_BEACON_ANY			(0xffff)
#define WNET_BEACON_GO_SPEAKER		(1 << 0)
#define WNET_BEACON_ACKNOWLEDGMENT	(1 << 1)
#define WNET_BEACON_BROADCAST_SPEAKER	(1 << 2)
#define WNET_BEACON_PING		(1 << 3)
#define WNET_BEACON_QUERY_STATE		(1 << 4)

typedef struct __attribute__((packed)) {

	uint8_t id;

	/* The <cause> field can be used to obtain further information
	 * about a beacon like a ACK for example.
	 * The values depend on the beacon (see the code)
	 */
	uint8_t cause;

	uint8_t priv_len;
	uint8_t priv[0];

} wnet_beacon_t;

typedef struct {

	/* Used to store pending beacons */
	struct list_head list;

	/* Source */
	uint64_t agencyUID_from;

	wnet_beacon_t *this;

} pending_beacon_t;

typedef struct {
	sl_desc_t *sl_desc;

	volatile uint32_t transID;
	volatile int pending;
	volatile int ret;

	struct completion xmit_event;

} wnet_tx_t;

typedef struct {

	sl_desc_t *sl_desc;
	volatile uint32_t transID;

} wnet_rx_t;

/*
 * General state of the neighbour (paired speaker, processing state)
 * Used by the WNET_BEACON_QUERY_STATE
 */
typedef struct __attribute__((packed)) {

	/*
	 * Paired speaker is used to bind a listener to a speaker during transmission
	 * from ack'd broadcast untill the last frame reception.
	 */
	uint64_t paired_speaker;

	/*
	 * Keep track of the ongoing frame processing (used with PKT_DATA)
	 * Used for specific ACK operations.
	 */
	uint32_t transID;
	bool pkt_data;

	/*
	 * Random 32-bit number to help deciding which take over the speaker state.
	 */
	uint32_t randnr;

} neighbour_state_t;

typedef struct {
	struct list_head list;

	/* valid means the neighbour has been confirmed through the initial ping procedure. */
	bool valid;

	/* Helper field to make a round of neighbours */
	bool processed;

	neighbour_desc_t *neighbour;

	uint64_t paired_speaker;

	uint32_t randnr;

} wnet_neighbour_t;

typedef void (*wnet_state_fn_t)(wnet_state_t old_state);

typedef struct {
	wnet_state_fn_t	*funcs;

	wnet_state_t	old_state;
	wnet_state_t	state;
} wnet_fsm_handle_t;

uint8_t *winenet_get_state_string(wnet_state_t state);

void winenet_xmit_data_processed(int ret);
void winenet_wait_xmit_event(void);
void winenet_get_last_beacon(wnet_beacon_t *last_beacon);

struct list_head *winenet_get_neighbours(void);
int winenet_get_my_index_and_listener(uint8_t *index, neighbour_desc_t *listener);
void winenet_dump_neighbours(void);
void winenet_dump_state(void);

void winenet_rx(sl_desc_t *sl_desc, transceiver_packet_t *packet);

void winenet_change_state(wnet_fsm_handle_t *fsm_handle, wnet_state_t new_state);
wnet_state_t winenet_get_state(wnet_fsm_handle_t *fsm_handle);
void winenet_start_fsm_task(char *name, wnet_fsm_handle_t *fsm_handle);
void winenet_init(void);

#endif /* WINENET_H */
