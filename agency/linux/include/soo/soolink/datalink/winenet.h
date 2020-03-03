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
#define WNET_RETRIES_MAX			6

/* Conversion from us to ns */
#define WNET_TIME_US_TO_NS(x)			((x) * 1000ull)

#define WNET_MAX_PACKET_TRANSID			0xffffff
#define WNET_LAST_PACKET			(1 << 24)

/* Number of bufferized packets in a frame for the n pkt / 1 ACK strategy */
#define WNET_N_PACKETS_IN_FRAME			8

/* Express in microsecs */
#define WNET_MIN_DRAND		1000
#define WNET_MAX_DRAND		2000
/*f
 * As the DCM send thread is non-RT, we choose a higher value to TSpeakerXMIT to give it a
 * chance to catch the TX request end before the expiration of the timeout.
 * The Tlistener timeout is adapted accordingly.
 */
#if defined(WNET_CONFIG_NETSTREAM)

/* Timeouts are expressed in us */

#if !defined(CONFIG_ARCH_VEXPRESS)

#define WNET_TSPEAKER_ACK			2000
#define WNET_TCHAIN_DELAY			5500
#define WNET_TIFS				180
#define WNET_TPROPA				350
#define WNET_TMARGIN				200

#else

#define WNET_TSPEAKER_ACK			15000
#define WNET_TCHAIN_DELAY			55000
#define WNET_TIFS				1800
#define WNET_TPROPA				3500
#define WNET_TMARGIN				2000

#endif /* !CONFIG_ARCH_VEXPRESS */

/* n is the number of Smart Objects in the ecosystem */
#define WNET_TLISTENER(n_soodios)	(n_soodios * WNET_TCHAIN_DELAY)

#elif defined(WNET_CONFIG_UNIBROAD)

/* TSPEAKER_XMIT must remain higher than the propagation cycle (currently, it is at 100 ms, so 5x more) */
#define WNET_TSPEAKER_XMIT			MILLISECS(500)
#define WNET_TSPEAKER_ACK			MILLISECS(200)

/* TLISTENER_LONG is used when the smart object belongs to a group, and for one iteration more (right after leaving a group) */
#define WNET_TLISTENER_LONG			MILLISECS(10000)

/* TLISTENER_SHORT is used when the smart object does not receive anything any longer. */
#define WNET_TLISTENER_SHORT			MILLISECS(1000)

#define WNET_TMEDIUM_FREE			MICROSECS(1200)

#endif /* CONFIG_NETSTREAM, CONFIG_UNIBROAD, CONFIG_BROADCAST, mutually exclusive */

typedef enum {
	WNET_STATE_IDLE				= 0,
	WNET_STATE_SPEAKER_CANDIDATE,
	WNET_STATE_SPEAKER,
	WNET_STATE_SPEAKER_WAIT_ACK,
	WNET_STATE_SPEAKER_RETRY,
	WNET_STATE_SPEAKER_SUSPENDED,
	WNET_STATE_LISTENER,
	WNET_STATE_LISTENER_COLLISION,
	WNET_STATE_N
} wnet_state_t;

typedef enum {
	WNET_BEACON_REQUEST_TO_SEND		= 0,
	WNET_BEACON_ACKNOWLEDGMENT		= 1,
	WNET_BEACON_TRANSMISSION_COMPLETED	= 2,
	WNET_BEACON_COLLISION_DETECTED		= 3,
	WNET_BEACON_ABORT			= 4,
	WNET_BEACON_RESUME			= 5,
	WNET_BEACON_SPEAKER_UNPAIR		= 6,
	WNET_BEACON_TAKE_OWNERSHIP		= 7,

	WNET_BEACON_REQUEST_TO_SEND_NETSTREAM	= 8,
	WNET_BEACON_TRANSMISSION_COMPLETED_NETSTREAM	= 9,

	WNET_BEACON_N
} wnet_beacon_id_t;

typedef struct {

	uint8_t			id;
	uint32_t		transID;

	union {
		struct {
			agencyUID_t speakerUID;
			agencyUID_t listenerUID;
		} medium_request;

		struct {
			agencyUID_t speakerUID;
			agencyUID_t listenerUID;
		} acknowledgment;

		struct {
			agencyUID_t speakerUID;
			agencyUID_t nextSpeakerUID;
		} transmission_completed;

		struct {
			agencyUID_t unused;
			agencyUID_t originatingListenerUID;
		} collision_detected;

		struct {
			agencyUID_t speakerUID;
			agencyUID_t originatingListenerUID;
		} abort;

		struct {
			agencyUID_t speakerUID;
			agencyUID_t originatingListenerUID;
		} resume;

		struct {
			agencyUID_t speakerUID;
			agencyUID_t listenerUID;
		} medium_request_netstream;

		struct {
			agencyUID_t speakerUID;
			agencyUID_t listenerUID;
		} transmission_completed_netstream;

		struct {
			agencyUID_t speakerUID;
			agencyUID_t listenerUID;
		} dummy;
	} u;

} wnet_beacon_t;

typedef struct {
	sl_desc_t		*sl_desc;
	plugin_desc_t		*plugin_desc;

	/* TX/XMIT path */
	bool			tx_pending;
	transceiver_packet_t	*tx_packet;
	size_t			tx_size;
	uint32_t		tx_transID;
	bool			tx_completed;
	int			tx_ret;
	rtdm_event_t		xmit_event;

	/* To determine it the smart object if it is our turn for sending. */
	bool			active_speaker;

	/* To know if the requester has finished sending its contents (true if yes). */
	bool 			transmission_over;

	/* RX path */
	bool			data_received;
	bool			rx_completed;
	uint32_t		rx_transID;

	/* Last received beacon */
	wnet_beacon_t		last_beacon;
} wnet_tx_rx_data_t;

typedef struct {
	struct list_head 	list;

	neighbour_desc_t	*neighbour;
	bool			listener_in_group;
	uint32_t		last_transID;
} wnet_neighbour_t;

typedef void (*wnet_state_fn_t)(wnet_state_t old_state);

typedef struct {
	wnet_state_fn_t	*funcs;
	rtdm_task_t	task;
	rtdm_event_t	event;
	wnet_state_t	old_state;
	wnet_state_t	state;
} wnet_fsm_handle_t;

uint8_t *winenet_get_state_string(wnet_state_t state);
uint8_t *winenet_get_beacon_id_string(wnet_beacon_id_t beacon_id);

void winenet_send_beacon(wnet_beacon_id_t beacon_id,
				agencyUID_t *dest_agencyUID,
				agencyUID_t *agencyUID1, agencyUID_t *agencyUID2,
				uint32_t opt);

void winenet_copy_tx_rx_data(wnet_tx_rx_data_t *tx_rx_data);
void winenet_xmit_data_processed(int ret);
void winenet_wait_xmit_event(void);
void winenet_get_last_beacon(wnet_beacon_t *last_beacon);
void winenet_tx_rx_data_protection(bool protect);
wnet_tx_rx_data_t *winenet_get_tx_rx_data(void);

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
