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

#if 0
#define DEBUG
#endif

/* Winenet debugging with short messages. Typically, this is used to print transition numbers. */
#if 0
#define WNET_SHORT_DBG(fmt, ...) \
	do { \
		force_print("(" fmt ")", ##__VA_ARGS__); \
	} while (0)
#else
#define WNET_SHORT_DBG(fmt, ...)
#endif

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/time.h>

#include <soo/soolink/soolink.h>

#define WNET_CONFIG_UNIBROAD 	1

#include <soo/soolink/datalink/winenet.h>
#include <soo/soolink/datalink.h>
#include <soo/soolink/sender.h>
#include <soo/soolink/receiver.h>
#include <soo/soolink/discovery.h>

#include <soo/core/device_access.h>

#include <xenomai/rtdm/driver.h>

#include <soo/uapi/soo.h>

#include <soo/uapi/debug.h>
#include <soo/uapi/console.h>
#include <soo/uapi/soo.h>

/*
 * Winenet is implementing a so-called unibroad communication mode that is, a mode where a speaker sends
 * to his neighbours called listeners. These listeners form a group (of listeners). It may happen
 * that a listener appears as neighbour, but is not immediately part of a group.
 * Furthermore, the speaker resets the group so that the next speaker will contact his neighbours
 * to grasp them in order to form a new group.
 */

/* FSM state helpers */
static void change_state(wnet_state_t new_state);
static wnet_state_t get_state(void);

/* Used to maitain the remote peer SOO */
static agencyUID_t speakerUID;

/* Keep a reference to the last_speaker, i.e. the last speaker which sent us data. */
static agencyUID_t last_speakerUID;

/* Used to manage the initial and current listener */
static bool first_neighbour = true;

/* Once we sent out a complete frame to the initial listener, we broadcast the same frame
 * to the other listeners of the group. So, at the beginning, we have do not start "broadcasting"
 * to the others.
 */
static bool broadcast_to_listeners = false;

/* Handle used in the FSM */
static wnet_fsm_handle_t fsm_handle;

static rtdm_mutex_t wnet_xmit_lock;
static rtdm_mutex_t sender_lock;

/* Event used to track the receival of a Winenet beacon or a TX request */
static rtdm_event_t wnet_event;

/* Medium monitoring to detect if the medium is free */
static bool wnet_isMonitoringMedium = false;
static rtdm_event_t wnet_medium_free_event;
static rtdm_mutex_t wnet_medium_free_lock;

static wnet_tx_rx_data_t __tx_rx_data;

rtdm_mutex_t tx_rx_data_lock;

/* Management of the neighbourhood of SOOs. Used in the retry packet management. */
static struct list_head wnet_neighbours;
static rtdm_mutex_t neighbour_list_lock;

/* To handle the neighbours */
static wnet_neighbour_t *current_listener = NULL;
static wnet_neighbour_t *initial_listener = NULL;

/* Each call to winenet_tx will increment the transID counter */
static uint32_t sent_packet_transID = 0;

/* Data packet/RTS beacon retry management */
static uint32_t retry_count = 0;
static bool medium_request_pending = false;

/* TX request */
static rtdm_mutex_t wnet_tx_request_pending_lock;
static bool wnet_tx_request_pending = false;
static rtdm_event_t wnet_tx_request_event;

/*
 * Packet buffering for the n pkt/1 ACK strategy, in a circular buffer way.
 * n packets form a frame.
 * The n packets must be bufferized to be able to re-send them if a retry is necessary. As a packet
 * is freed by the Sender once Winenet has processed it, the packet must be locally copied.
 */
static transceiver_packet_t *buf_tx_pkt[WNET_N_PACKETS_IN_FRAME] = { NULL };
static transceiver_packet_t *buf_rx_pkt[WNET_N_PACKETS_IN_FRAME] = { NULL };

/* Debugging strings */
/* All states are not used in unibroad mode like those related to collision management. */

static uint8_t *state_string[WNET_STATE_N] = {
	[WNET_STATE_IDLE]				= "Idle",
	[WNET_STATE_SPEAKER_CANDIDATE]			= "Speaker candidate",
	[WNET_STATE_SPEAKER]				= "Speaker",
	[WNET_STATE_SPEAKER_WAIT_ACK]			= "Speaker wait ACK",
	[WNET_STATE_SPEAKER_RETRY]			= "Speaker send retry",
	[WNET_STATE_SPEAKER_SUSPENDED]			= "Speaker suspended",
	[WNET_STATE_LISTENER]				= "Listener",
	[WNET_STATE_LISTENER_COLLISION]			= "Listener collision"
};

static uint8_t *beacon_id_string[WNET_BEACON_N] = {
	[WNET_BEACON_REQUEST_TO_SEND]			= "REQUEST_TO_SEND",
	[WNET_BEACON_ACKNOWLEDGMENT]			= "ACKNOWLEDGMENT",
	[WNET_BEACON_TRANSMISSION_COMPLETED]		= "TRANSMISSION COMPLETED",
	[WNET_BEACON_COLLISION_DETECTED]		= "COLLISION DETECTED",
	[WNET_BEACON_ABORT]				= "ABORT",
	[WNET_BEACON_RESUME]				= "RESUME",
	[WNET_BEACON_SPEAKER_UNPAIR]			= "UNPAIR",
	[WNET_BEACON_TAKE_OWNERSHIP]			= "TAKE_OWNERSHIP",

	[WNET_BEACON_REQUEST_TO_SEND_NETSTREAM]		= "REQUEST TO SEND NETSTREAM",
	[WNET_BEACON_TRANSMISSION_COMPLETED_NETSTREAM]	= "TRANSMISSION COMPLETED NETSTREAM",

};

static uint8_t invalid_string[] = "INVALID";

/* Debugging functions */

uint8_t *winenet_get_state_string(wnet_state_t state) {
	uint32_t state_int = (uint32_t) state;

	if (unlikely(state_int >= WNET_STATE_N))
		return invalid_string;

	return state_string[state_int];
}

uint8_t *winenet_get_beacon_id_string(wnet_beacon_id_t beacon_id) {
	uint32_t beacon_id_int = (uint32_t) beacon_id;

	if (unlikely((beacon_id_int >= WNET_BEACON_N)))
		return invalid_string;

	return beacon_id_string[beacon_id_int];
}

/**
 * Take a snapshot of the TX request parameters.
 */
void winenet_copy_tx_rx_data(wnet_tx_rx_data_t *dest) {
	rtdm_mutex_lock(&tx_rx_data_lock);
	memcpy(dest, &__tx_rx_data, sizeof(wnet_tx_rx_data_t));
	rtdm_mutex_unlock(&tx_rx_data_lock);
}

/**
 * Allow the producer to be informed about potential problems or to
 * send a next packet.
 */
void winenet_xmit_data_processed(int ret) {
	rtdm_mutex_lock(&tx_rx_data_lock);

	if (!__tx_rx_data.tx_pending) {
		/* There is no TX request. There is no need to make any ACK. */
		rtdm_mutex_unlock(&tx_rx_data_lock);
		return ;
	}

	__tx_rx_data.tx_pending = false;
	__tx_rx_data.tx_ret = ret;
	rtdm_mutex_unlock(&tx_rx_data_lock);

	/* Allow the producer to go further */
	rtdm_event_signal(&__tx_rx_data.xmit_event);
}

/**
 * Make a producer wait for a feedback after a XMIT operation.
 */
void winenet_wait_xmit_event(void) {
	rtdm_event_wait(&__tx_rx_data.xmit_event);
}

/**
 * Destroy the bufferized TX packets.
 * This function has to be called when a packet frame has been acknowledged, or if there is
 * an unexpected transition that requires the whole frame to be freed.
 */
static void clear_buf_tx_pkt(void) {
	uint32_t i;

	rtdm_mutex_lock(&tx_rx_data_lock);

	for (i = 0; i < WNET_N_PACKETS_IN_FRAME; i++) {
		if (buf_tx_pkt[i] != NULL) {
			kfree(buf_tx_pkt[i]);
			buf_tx_pkt[i] = NULL;
		}
	}

	rtdm_mutex_unlock(&tx_rx_data_lock);
}

/**
 * Destroy the bufferized RX packets.
 * This function has to be called when a packet frame has been acknowledged, or if there is
 * an unexpected transition that requires the whole frame to be freed.
 */
static void clear_buf_rx_pkt(void) {
	uint32_t i;

	/* We assume wnet_rx_request_lock is acquired. */

	for (i = 0; i < WNET_N_PACKETS_IN_FRAME; i++) {
		if (buf_rx_pkt[i] != NULL) {
			kfree(buf_rx_pkt[i]);
			buf_rx_pkt[i] = NULL;
		}
	}
}

/**
 * Take a snapshot of the last received Winenet beacon.
 * The beacon is consumed and it cannot be used anymore.
 */
void winenet_get_last_beacon(wnet_beacon_t *last_beacon) {
	rtdm_mutex_lock(&tx_rx_data_lock);
	memcpy(last_beacon, &__tx_rx_data.last_beacon, sizeof(wnet_beacon_t));
	__tx_rx_data.last_beacon.id = WNET_BEACON_N;
	rtdm_mutex_unlock(&tx_rx_data_lock);
}

/**
 * Get a pointer to the TX/RX data.
 * The protection must be active when writing into it or reading it.
 */
wnet_tx_rx_data_t *winenet_get_tx_rx_data(void) {
	return &__tx_rx_data;
}

/**
 * Check if the incoming packet has a trans ID which is greater than the last detected trans ID from
 * this particular agency UID. This function is used to detect the duplicate packets in case of retry.
 * There are three cases:
 * - A packet with the trans ID given as parameter has already been received. We return true.
 *   The packet is discarded and not transmitted to the upper layer (sender requester).
 * - The incoming packet trans ID is greater than the last seen trans ID. We return false.
 *   The packet is transmitted to the upper layer (sender requester).
 * - The particular case of a packet whose trans ID is 0 indicates that a new block is being processed.
 *   A trans ID equal to 0 will forward the packet to the upper layer.
 */
static bool packet_already_received(transceiver_packet_t *packet, agencyUID_t *agencyUID_from) {
	struct list_head *cur;
	wnet_neighbour_t *neighbour_cur;

	rtdm_mutex_lock(&neighbour_list_lock);

	/* Look for the remote SOO in the list */
	list_for_each(cur, &wnet_neighbours) {

		/* The agency UID is already known */
		neighbour_cur = list_entry(cur, wnet_neighbour_t, list);
		if (!memcmp(&neighbour_cur->neighbour->agencyUID, agencyUID_from, SOO_AGENCY_UID_SIZE)) {

			if (((packet->transID & WNET_MAX_PACKET_TRANSID) > neighbour_cur->last_transID) ||
				((packet->transID & WNET_MAX_PACKET_TRANSID) == 0)) {
				neighbour_cur->last_transID = packet->transID & WNET_MAX_PACKET_TRANSID;

				rtdm_mutex_unlock(&neighbour_list_lock);
				return false;
			}
			else {
				/* The packet has already been received */
				DBG("Agency UID found: ");
				DBG_BUFFER(&neighbour_cur->neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

				rtdm_mutex_unlock(&neighbour_list_lock);
				return true;
			}
		}
	}

	/* This is an unknown agency UID. Will be integrated in the neighbour list very soon. */

	rtdm_mutex_unlock(&neighbour_list_lock);

	return false;
}

/**
 * Add a new neighbour in our list. As Winenet is a Discovery listener,
 * this function is called when a neighbour appears.
 */
static void winenet_add_neighbour(neighbour_desc_t *neighbour) {
	wnet_neighbour_t *wnet_neighbour;
	int position_prev, position_cur = 0;
	struct list_head *cur;
	wnet_neighbour_t *cur_neighbour, *prev_neighbour;

	rtdm_mutex_lock(&neighbour_list_lock);

	wnet_neighbour = kmalloc(sizeof(wnet_neighbour_t), GFP_ATOMIC);
	if (!wnet_neighbour)
		BUG();

	wnet_neighbour->neighbour = neighbour;
	wnet_neighbour->last_transID = 0;
	wnet_neighbour->listener_in_group = false;

	DBG("*** ADDing new neighbour\n");

	if (first_neighbour) {

		current_listener = wnet_neighbour;
		initial_listener = wnet_neighbour;

		first_neighbour = false;
		list_add_tail(&wnet_neighbour->list, &wnet_neighbours);

	}  else {
		
		/*
		 * We use the same sorting strategy than the
		 * Discovery to be consistent.
 		 */		

		list_for_each(cur, &wnet_neighbours) {
			position_prev = position_cur;
			prev_neighbour = cur_neighbour;

			cur_neighbour = list_entry(cur, wnet_neighbour_t, list);
			position_cur = memcmp(&cur_neighbour->neighbour->agencyUID, &neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

			if (position_cur > 0) {
				/* Head of the list */
				if (position_prev == 0)
					list_add(&wnet_neighbour->list, &wnet_neighbours);
				/* The neighbour must be inserted between two neighbours in the list  */
				else if (position_prev < 0)
					list_add(&wnet_neighbour->list, &prev_neighbour->list);
			}
		}

		/* Tail of the list */
		if (position_cur < 0)
			list_add_tail(&wnet_neighbour->list, &wnet_neighbours);
	}

	rtdm_mutex_unlock(&neighbour_list_lock);
}

/**
 * Remove a neighbour from the neighbour list. As Winenet is a Discovery listener,
 * this function is called when a neighbour disappears.
 */
static void winenet_remove_neighbour(neighbour_desc_t *neighbour) {
	struct list_head *cur, *tmp, *next;
	wnet_neighbour_t *wnet_neighbour;

	/* Could be called from the non-RT context at the beginning of the agency_core
	 * (selection of neighbourhood).
	 */
	if (smp_processor_id() == AGENCY_RT_CPU)
		rtdm_mutex_lock(&neighbour_list_lock);

	list_for_each_safe(cur, tmp, &wnet_neighbours) {
		wnet_neighbour = list_entry(cur, wnet_neighbour_t, list);
		if (!memcmp(&wnet_neighbour->neighbour->agencyUID, &neighbour->agencyUID, SOO_AGENCY_UID_SIZE)) {

			/* Take the next, and check if we are at the end of the list */
			next = cur->next;
			if (next == &wnet_neighbours)
				next = wnet_neighbours.next;

			/* We update the global references. We check if the list will be empty after deletion. */
			if (current_listener == wnet_neighbour)
				current_listener = ((next == &wnet_neighbour->list) ? NULL : list_entry(next, wnet_neighbour_t, list));

			if (initial_listener == wnet_neighbour)
				initial_listener = ((next == &wnet_neighbour->list) ? NULL : list_entry(next, wnet_neighbour_t, list));

			/* If our prev speaker *is* this wnet_neighbour, we arbitrary choose the current listener as the
			 * next speaker.
			 */

			list_del(cur);
			kfree(wnet_neighbour);
		}
	}

	if (list_empty(&wnet_neighbours))
		first_neighbour = true; /* For initializing current_listener correctly */

	if (smp_processor_id() == AGENCY_RT_CPU)
		rtdm_mutex_unlock(&neighbour_list_lock);

	DBG("*** REMOVing new neighbour\n");

}

struct list_head *winenet_get_neighbours(void) {
	return &wnet_neighbours;
}

/**
 * Get the index of this SOO in the ecosystem and its Listener, that is, the next
 * immediate neighbour for the netstream round robin mode transmission. As the
 * neighbourhood is frozen when streaming is on, it is necessary to call this
 * function only once.
 * The number of neighbours is returned on success.
 * A negative value is returned if the neighbourhood is not available yet.
 */
int winenet_get_my_index_and_listener(uint8_t *index, neighbour_desc_t *listener) {
	struct list_head *cur;
	int position_prev, position_cur = 0;
	wnet_neighbour_t *first_neighbour, *cur_neighbour;
	uint32_t count = 0, n_neighbours = 0;

	rtdm_mutex_lock(&neighbour_list_lock);

	if (list_empty(&wnet_neighbours)) {
		rtdm_mutex_unlock(&neighbour_list_lock);
		return -ENONET;
	}

	list_for_each(cur, &wnet_neighbours)
		n_neighbours++;

	list_for_each(cur, &wnet_neighbours) {
		position_prev = position_cur;

		cur_neighbour = list_entry(cur, wnet_neighbour_t, list);
		position_cur = memcmp(&cur_neighbour->neighbour->agencyUID, get_my_agencyUID(), SOO_AGENCY_UID_SIZE);

		if (position_cur > 0) {
			/* Head of the list */
			if (position_prev == 0) {
				/* This SOO is the first Speaker */
				*index = 0;
				memcpy(listener, cur_neighbour->neighbour, sizeof(neighbour_desc_t));

				rtdm_mutex_unlock(&neighbour_list_lock);
				return n_neighbours;
			} else if (position_prev < 0) {
				/* This SOO is between two neighbours in the list */
				*index = count;
				memcpy(listener, cur_neighbour->neighbour, sizeof(neighbour_desc_t));

				rtdm_mutex_unlock(&neighbour_list_lock);
				return n_neighbours;
			}
		}

		count++;
	}

	/* Tail of the list */
	if (position_cur < 0) {
		*index = count;
		first_neighbour = list_first_entry(&wnet_neighbours, wnet_neighbour_t, list);
		memcpy(listener, first_neighbour->neighbour, sizeof(neighbour_desc_t));

		rtdm_mutex_unlock(&neighbour_list_lock);
		return n_neighbours;
	}

	/* This should be never reached */
	rtdm_mutex_unlock(&neighbour_list_lock);
	BUG();
	return -ENOENT;
}

/**
 * Dump the active neighbour list.
 */
void winenet_dump_neighbours(void) {
	struct list_head *cur;
	wnet_neighbour_t *neighbour;
	uint32_t count = 0;

	rtdm_mutex_lock(&neighbour_list_lock);

	/* There is no neighbour in the list, I am alone */
	if (list_empty(&wnet_neighbours)) {
		lprintk("No neighbour\n");
		rtdm_mutex_unlock(&neighbour_list_lock);
		return;
	}

	list_for_each(cur, &wnet_neighbours) {

		neighbour = list_entry(cur, wnet_neighbour_t, list);

		lprintk("- Neighbour %d: ", count+1);
		lprintk_buffer(&neighbour->neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

		count++;
	}

	rtdm_mutex_unlock(&neighbour_list_lock);
}

/**
 * Dump the current Winenet state.
 */
void winenet_dump_state(void) {
	lprintk("Winenet status: %s\n", state_string[get_state()]);
}

/**
 * Get the next neighbour for the unibroad mode transmission.
 */
static inline wnet_neighbour_t *next_listener(wnet_neighbour_t *from) {
	struct list_head *cur;

	rtdm_mutex_lock(&neighbour_list_lock);

	if (from == NULL)
		cur = &current_listener->list;
	else
		cur = &from->list;

	cur = cur->next;
	if (cur == &wnet_neighbours)
		cur = wnet_neighbours.next;

	rtdm_mutex_unlock(&neighbour_list_lock);

	return list_entry(cur, wnet_neighbour_t, list);
}

/*
 * Returns the number of listener participants within the group.
 */
int listeners_in_group(void) {
	int i = 0;
	struct list_head *cur;
	wnet_neighbour_t *wnet_neighbour;

	rtdm_mutex_lock(&neighbour_list_lock);

	list_for_each(cur, &wnet_neighbours) {
		wnet_neighbour = list_entry(cur, wnet_neighbour_t, list);
		if (wnet_neighbour->listener_in_group)
			i++;
	}

	rtdm_mutex_unlock(&neighbour_list_lock);

	return i;
}

/*
 * Returns the number of listener participants within the group.
 * Particular case: if agencyUID is the NULL agencyUID, return the first neighbour.
 */
wnet_neighbour_t *find_neighbour(agencyUID_t *agencyUID) {
	struct list_head *cur;
	wnet_neighbour_t *wnet_neighbour;

	rtdm_mutex_lock(&neighbour_list_lock);

	if (!memcmp(agencyUID, get_null_agencyUID(), SOO_AGENCY_UID_SIZE))
		return (wnet_neighbour_t *) &wnet_neighbours.next;

	list_for_each(cur, &wnet_neighbours) {
		wnet_neighbour = list_entry(cur, wnet_neighbour_t, list);

		if (!memcmp(&wnet_neighbour->neighbour->agencyUID, agencyUID, SOO_AGENCY_UID_SIZE)) {
			rtdm_mutex_unlock(&neighbour_list_lock);
			return wnet_neighbour;
		}
	}

	rtdm_mutex_unlock(&neighbour_list_lock);

	return NULL;
}

static discovery_listener_t wnet_discovery_desc = {
	.add_neighbour_callback = winenet_add_neighbour,
	.remove_neighbour_callback = winenet_remove_neighbour
};

/**
 * Send a Winenet beacon.
 * The purpose of agencyUID1, agencyUID2 and the optional field depends on the beacon type.
 */
void winenet_send_beacon(wnet_beacon_id_t beacon_id,
			agencyUID_t *dest_agencyUID,
			agencyUID_t *agencyUID1, agencyUID_t *agencyUID2,
			uint32_t opt) {
	wnet_tx_rx_data_t tx_rx_data;
	wnet_beacon_t outgoing_beacon;
	transceiver_packet_t *transceiver_packet;
	netstream_transceiver_packet_t *netstream_transceiver_packet;
	void *outgoing_packet = NULL;

	DBG("Send beacon %s to ", winenet_get_beacon_id_string(beacon_id));
	if (!dest_agencyUID)
		DBG("(broadcast)\n");
	else
		DBG_BUFFER(dest_agencyUID, SOO_AGENCY_UID_SIZE);

	winenet_copy_tx_rx_data(&tx_rx_data);

	outgoing_beacon.id = beacon_id;
	outgoing_beacon.transID = 0;

	switch (beacon_id) {
	case WNET_BEACON_REQUEST_TO_SEND:
		outgoing_beacon.transID = listeners_in_group();

		break;

	case WNET_BEACON_ACKNOWLEDGMENT:
		/* opt is the TX request trans ID */
		outgoing_beacon.transID = opt;

		memcpy(&outgoing_beacon.u.acknowledgment.speakerUID, agencyUID1, SOO_AGENCY_UID_SIZE);
		memcpy(&outgoing_beacon.u.acknowledgment.listenerUID, agencyUID2, SOO_AGENCY_UID_SIZE);

		break;

	case WNET_BEACON_TRANSMISSION_COMPLETED:
		outgoing_beacon.transID = 0;

		memcpy(&outgoing_beacon.u.transmission_completed.speakerUID, agencyUID1, SOO_AGENCY_UID_SIZE);
		memcpy(&outgoing_beacon.u.transmission_completed.nextSpeakerUID, agencyUID2, SOO_AGENCY_UID_SIZE);
		break;

	case WNET_BEACON_COLLISION_DETECTED:
		outgoing_beacon.transID = 0;

		memset(&outgoing_beacon.u.collision_detected.unused, 0, SOO_AGENCY_UID_SIZE);
		memcpy(&outgoing_beacon.u.collision_detected.originatingListenerUID, agencyUID2, SOO_AGENCY_UID_SIZE);

		break;

	case WNET_BEACON_SPEAKER_UNPAIR:
	case WNET_BEACON_TAKE_OWNERSHIP:
		break;

	case WNET_BEACON_REQUEST_TO_SEND_NETSTREAM:
		memcpy(&outgoing_beacon.u.medium_request_netstream.speakerUID, agencyUID1, SOO_AGENCY_UID_SIZE);
		memcpy(&outgoing_beacon.u.medium_request_netstream.listenerUID, agencyUID2, SOO_AGENCY_UID_SIZE);

		/* Store the packet size in the transID field, given by the opt parameter */
		outgoing_beacon.transID = opt;

		break;

	case WNET_BEACON_TRANSMISSION_COMPLETED_NETSTREAM:
		memcpy(&outgoing_beacon.u.transmission_completed_netstream.speakerUID, agencyUID1, SOO_AGENCY_UID_SIZE);
		memcpy(&outgoing_beacon.u.transmission_completed_netstream.listenerUID, agencyUID2, SOO_AGENCY_UID_SIZE);

		/* Store the packet size in the transID field, given by the opt parameter */
		outgoing_beacon.transID = opt;

		break;


	default:
		lprintk("Invalid beacon ID\n");
		BUG();
	}

	/* Enforce the use of the a known Soolink descriptor */
	if (likely(tx_rx_data.sl_desc)) {
		if (tx_rx_data.sl_desc->trans_mode == SL_MODE_NETSTREAM) {
			netstream_transceiver_packet = (netstream_transceiver_packet_t *) kmalloc(sizeof(netstream_transceiver_packet_t) + sizeof(wnet_beacon_t), GFP_ATOMIC);

			netstream_transceiver_packet->packet_type = TRANSCEIVER_PKT_DATALINK;
			netstream_transceiver_packet->peerID = 0;

			memcpy(netstream_transceiver_packet->payload, &outgoing_beacon, sizeof(wnet_beacon_t));

			outgoing_packet = (void *) netstream_transceiver_packet;
		} else {
			transceiver_packet = (transceiver_packet_t *) kmalloc(sizeof(transceiver_packet_t) + sizeof(wnet_beacon_t), GFP_ATOMIC);

			transceiver_packet->packet_type = TRANSCEIVER_PKT_DATALINK;
			transceiver_packet->transID = 0;

			memcpy(transceiver_packet->payload, &outgoing_beacon, sizeof(wnet_beacon_t));

			outgoing_packet = (void *) transceiver_packet;
		}

		rtdm_mutex_lock(&tx_rx_data_lock);
		if (dest_agencyUID != NULL)
			memcpy(&winenet_get_tx_rx_data()->sl_desc->agencyUID_to, dest_agencyUID, SOO_AGENCY_UID_SIZE);
		else
			memcpy(&winenet_get_tx_rx_data()->sl_desc->agencyUID_to, get_null_agencyUID(), SOO_AGENCY_UID_SIZE);
		rtdm_mutex_unlock(&tx_rx_data_lock);

		rtdm_mutex_lock(&sender_lock);
		sender_tx(tx_rx_data.sl_desc, outgoing_packet, sizeof(wnet_beacon_t), 0);
		rtdm_mutex_unlock(&sender_lock);
	}

	/* Release the outgoing packet */
	if (outgoing_packet)
		kfree(outgoing_packet);
}

/*
 * Reset the group of participants.
 */
void listeners_reset(void) {
	struct list_head *cur;
	wnet_neighbour_t *wnet_neighbour;

	rtdm_mutex_lock(&neighbour_list_lock);

	list_for_each(cur, &wnet_neighbours) {
		wnet_neighbour = list_entry(cur, wnet_neighbour_t, list);
		if (wnet_neighbour->listener_in_group) {

			winenet_send_beacon(WNET_BEACON_SPEAKER_UNPAIR, &wnet_neighbour->neighbour->agencyUID, NULL, NULL, 0);

			wnet_neighbour->listener_in_group = false;
		}

	}

	rtdm_mutex_unlock(&neighbour_list_lock);
}

/**
 * Monitors the medium during a fixed delay to know if the medium is free, or in use.
 */
static bool is_medium_free(void) {
	nanosecs_rel_t dfree = WNET_TMEDIUM_FREE;
	int ret;

	rtdm_mutex_lock(&wnet_medium_free_lock);
	wnet_isMonitoringMedium = true;
	rtdm_mutex_unlock(&wnet_medium_free_lock);

	ret = rtdm_event_timedwait(&wnet_medium_free_event, dfree, NULL);

	rtdm_mutex_lock(&wnet_medium_free_lock);
	wnet_isMonitoringMedium = false;
	rtdm_mutex_unlock(&wnet_medium_free_lock);

	if (ret == 0) {
		/*
		 * We have been unlocked by an event signal, meaning that a packet has been received:
		 * the medium is not free
		 */
		DBG("Medium not free\n");

		return false;
	}
	else if (ret == -ETIMEDOUT) {
		/* The timeout has expired and no packet has been received: the medium is free */
		DBG("Medium free timeout\n");

		return true;
	}
	else {
		/* Unexpected! */
		lprintk("Unexpected ret value: %d\n", ret);
		BUG();
	}

	return false;
}

/*
 * Unpairing a listener with its speaker.
 */
void listener_unpair(void) {
	memcpy(&speakerUID, get_null_agencyUID(), SOO_AGENCY_UID_SIZE);
}

/*
 * Contact the next speaker in our neighborhoud.
 * At this point, the neighbour protection must be disabled.
 * This function is not re-entrant; it is called only once at a time.
 */
void forward_next_speaker(void) {
	static agencyUID_t next_speakerUID = { .id = {0} };
	wnet_neighbour_t *last_speaker, *next_speaker;

	neighbour_list_protection(true);

	if (list_empty(&wnet_neighbours)) {
		neighbour_list_protection(false);

		change_state(WNET_STATE_IDLE);
		return ;
	}

	/* Get the last_speaker */
	last_speaker = find_neighbour(&last_speakerUID);
	next_speaker = find_neighbour(&next_speakerUID);

	if (next_speaker == NULL)
		next_speaker = next_listener(last_speaker);
	else {
		next_speaker = next_listener(next_speaker);
		if (next_speaker == last_speaker)
			next_speaker = next_listener(next_speaker);
	}

	winenet_send_beacon(WNET_BEACON_TRANSMISSION_COMPLETED, &next_speaker->neighbour->agencyUID, get_my_agencyUID(),
			&next_speaker->neighbour->agencyUID, 0);

	memcpy(&next_speakerUID, &next_speaker->neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

	neighbour_list_protection(false);
	change_state(WNET_STATE_LISTENER);
}


/************************************************* Winenet FSM *****************************************************/

/**
 * IDLE state.
 */
static void winenet_state_idle(wnet_state_t old_state) {
	wnet_tx_rx_data_t tx_rx_data;
	wnet_beacon_t last_beacon;
	bool tx_request_pending;
	nanosecs_rel_t tmedium_free = WNET_TMEDIUM_FREE;

	DBG("Idle\n");

	while (1) {
		/*
		 * The IDLE state has no timeout to avoid overloading with useless activity.
		 * If we need to check for pending TX, the previous state has to send an event to
		 * wake the thread and to check the situation.
		 */

		__tx_rx_data.active_speaker = true;
		rtdm_event_wait(&wnet_event);
		__tx_rx_data.active_speaker = false;

		winenet_copy_tx_rx_data(&tx_rx_data);
		winenet_get_last_beacon(&last_beacon);

		rtdm_mutex_lock(&wnet_tx_request_pending_lock);
		tx_request_pending = wnet_tx_request_pending;
		rtdm_mutex_unlock(&wnet_tx_request_pending_lock);

		/* Reset the peer agencyUID. sl_desc should not be NULL. */
		memcpy(&winenet_get_tx_rx_data()->sl_desc->agencyUID_to, get_null_agencyUID(), SOO_AGENCY_UID_SIZE);

		/* Preserve the speaker for a next speaker selection */
		memcpy(&last_speakerUID, &tx_rx_data.sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE);

		/* We give the priority to event issued from a potential speaker. Indeed, the medium has been requested
		 * by another smart object in this case, therefore we do not attempt to send our stuff now.
		 */
		if (last_beacon.id == WNET_BEACON_REQUEST_TO_SEND) {
			/* RTS beacon received */

			/* Save the agency UID of the current Speaker */
			memcpy(&speakerUID, &tx_rx_data.sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE);

			/* Save the nominated active Listener UID */

			/* In case of SL_MODE_UNIBROAD, this listener which receives the beacon IS the active listener (the
			 * other smart objects do not hear about anything (underlying unicast mode).
			 */

			winenet_send_beacon(WNET_BEACON_ACKNOWLEDGMENT, &speakerUID, &speakerUID, get_my_agencyUID(), 0);

			change_state(WNET_STATE_LISTENER);

			return ;
		}

		/* React to a TX request or a pending data XMIT request */
		while (tx_request_pending || tx_rx_data.tx_pending) {

			if (!is_medium_free())
				/* We suspend ourself during a certain time */
				rtdm_task_sleep(tmedium_free);

			/* Data TX request and free medium */
			change_state(WNET_STATE_SPEAKER_CANDIDATE);

			return ;
		}

		/* React to a transmission completed beacon used to inform that we would be
		 * asked to be a speaker. This happens if a listener is becoming speaker, but with nothing to transmit.
		 * So it directly gives hand to us.
		 */
		if (last_beacon.id == WNET_BEACON_TRANSMISSION_COMPLETED) {

			forward_next_speaker();
			return ;
		}


	}
}

/* SPEAKER states */

/**
 * SPEAKER Candidate state.
 */
static void winenet_state_speaker_candidate(wnet_state_t old_state) {
	wnet_tx_rx_data_t tx_rx_data;
	wnet_beacon_t last_beacon;
	nanosecs_rel_t drand = MICROSECS((get_random_int() % WNET_MAX_DRAND) + WNET_MIN_DRAND);
	bool tx_request_pending;
	int ret;

	DBG("Speaker candidate\n");

	while (1) {
		/* Delay of Drand microseconds */
		ret = rtdm_event_timedwait(&wnet_event, drand, NULL);

		winenet_get_last_beacon(&last_beacon);
		winenet_copy_tx_rx_data(&tx_rx_data);

		rtdm_mutex_lock(&wnet_tx_request_pending_lock);
		tx_request_pending = wnet_tx_request_pending;
		rtdm_mutex_unlock(&wnet_tx_request_pending_lock);

		if (ret == 0) {

			if (last_beacon.id == WNET_BEACON_REQUEST_TO_SEND) {
				/* RTS beacon received */

				/* Paired with this new speaker */
				memcpy(&speakerUID, &tx_rx_data.sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE);

				winenet_send_beacon(WNET_BEACON_ACKNOWLEDGMENT, &speakerUID, &speakerUID, get_my_agencyUID(), 0);

				change_state(WNET_STATE_LISTENER);

				return ;

			} else {

				/* Winenet beacon different from RTS received. This branch
				 * may occur if the listener reached its timeout while the speaker
				 * is still sending a transmission_completed for example.
				 * So in the meanwhile, we might have something to transmit, and
				 * ask for being speaker.
				 */

				change_state(WNET_STATE_LISTENER);

				return ;
			}
		} else if (ret == -ETIMEDOUT) {
			/* The timeout has expired and no RTS beacon has been received: we can now become Speaker */

			if (tx_request_pending) {
				rtdm_mutex_lock(&wnet_tx_request_pending_lock);
				wnet_tx_request_pending = false;
				rtdm_mutex_unlock(&wnet_tx_request_pending_lock);

				rtdm_event_signal(&wnet_tx_request_event);
			}

			neighbour_list_protection(true);

			/* Are we alone in the neighbourhood? */
			if (!initial_listener) {
				winenet_xmit_data_processed(-ENONET);

				/* Clear the TX pkt buffer */
				clear_buf_tx_pkt();

				/* Reset the TX trans ID */
				sent_packet_transID = 0;

				DBG("4: No neighbour -> Idle\n");
				WNET_SHORT_DBG("4c");

				neighbour_list_protection(false);
				change_state(WNET_STATE_IDLE);

				return ;
			}
			current_listener = initial_listener;

			DBG("Current listener: ");
			DBG_BUFFER(&current_listener->neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

			medium_request_pending = true;

			/* Transition 2: Drand delay expired */
			change_state(WNET_STATE_SPEAKER);

			/* Inform the speaker that there is something to send */
			rtdm_event_signal(&wnet_event);

			return ;

		} else {

			/* Unexpected! */
			lprintk("Unexpected ret value: %d\n", ret);
			BUG();
		}
	}
}

/**
 * SPEAKER state.
 * A data packet is being sent in this state.
 */
static void winenet_state_speaker(wnet_state_t old_state) {
	wnet_tx_rx_data_t tx_rx_data;
	wnet_beacon_t last_beacon;
	bool tx_request_pending;
	static bool select_listener = true;
	nanosecs_rel_t tspeaker_xmit = WNET_TSPEAKER_XMIT;
	int ret, i;

	DBG("Speaker\n");

	while (1) {

		/* Timeout of Tspeaker us */

		__tx_rx_data.active_speaker = true;
		ret = rtdm_event_timedwait(&wnet_event, tspeaker_xmit, NULL);
		__tx_rx_data.active_speaker = false;

		winenet_copy_tx_rx_data(&tx_rx_data);
		winenet_get_last_beacon(&last_beacon);

		rtdm_mutex_lock(&wnet_tx_request_pending_lock);
		tx_request_pending = wnet_tx_request_pending;
		rtdm_mutex_unlock(&wnet_tx_request_pending_lock);

		/* We might receive some delayed ack beacon. */

		if (ret == 0) {

			if (__tx_rx_data.transmission_over) {
				listener_unpair();

				neighbour_list_protection(false);
				forward_next_speaker();

				__tx_rx_data.transmission_over = false;

				return ;
			}

			if (medium_request_pending) {
				memcpy(&speakerUID, get_null_agencyUID(), SOO_AGENCY_UID_SIZE);

				/* A RTS beacon is being sent */

				/* Get the next neighbour from the list (the initial listener is processed at the end).
				 * The neighbour is not paired yet (it might be paired during the wait ack).
				 */
				do {
					current_listener = next_listener(NULL);
				} while (current_listener->listener_in_group && (current_listener != initial_listener));

				if (!current_listener->listener_in_group) {

					winenet_send_beacon(WNET_BEACON_REQUEST_TO_SEND, &current_listener->neighbour->agencyUID, NULL, NULL, 0);

					change_state(WNET_STATE_SPEAKER_WAIT_ACK);

				} else {
					/* All neighbours are in the group. */

					medium_request_pending = false;

					change_state(WNET_STATE_SPEAKER);
					rtdm_event_signal(&wnet_event);
				}
				return ;
			}

			if (tx_rx_data.tx_pending) {
				/* Data is being sent */

				/* Set the speakerUID in the speaker; from now on, we receive only packets from our group. */
				memcpy(&speakerUID, get_my_agencyUID(), SOO_AGENCY_UID_SIZE);
				if (select_listener) {
					do {
						current_listener = next_listener(NULL);
					} while (!current_listener->listener_in_group && (current_listener != initial_listener));
					select_listener = false;
				}

				if (!current_listener->listener_in_group) {
					/* No more listener in the group... weired but could happen. */

					/* Clear the TX pkt buffer */
					clear_buf_tx_pkt();

					/* Reset the TX trans ID */
					sent_packet_transID = 0;

					winenet_xmit_data_processed(-ENONET);

					neighbour_list_protection(false);

					/* Transition 6: Tspeaker timeout */
					change_state(WNET_STATE_IDLE);

					return ;
				}

				rtdm_mutex_lock(&tx_rx_data_lock);

				/* We have to transmit over all smart objects */
				if (broadcast_to_listeners) {
					/* Set the destination */
					memcpy(&winenet_get_tx_rx_data()->sl_desc->agencyUID_to, &current_listener->neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

					rtdm_mutex_lock(&sender_lock);
					for (i = 0; ((i < WNET_N_PACKETS_IN_FRAME) && (buf_tx_pkt[i] != NULL)); i++)
						sender_tx(tx_rx_data.sl_desc, buf_tx_pkt[i], buf_tx_pkt[i]->size, 0);
					rtdm_mutex_unlock(&sender_lock);

					rtdm_mutex_unlock(&tx_rx_data_lock);

					select_listener = true;

					/* Transition 12: Data sent */
					change_state(WNET_STATE_SPEAKER_WAIT_ACK);

					return ;
				}

				/* Sending packet of the frame for the first time (first listener) */

				/* Copy the packet into the bufferized packet array */
				if (unlikely(buf_tx_pkt[(tx_rx_data.tx_packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME])) {
					DBG("TX buffer already populated: %d, %d\n", tx_rx_data.tx_packet->transID, (tx_rx_data.tx_packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME);

					clear_buf_tx_pkt();
				}

				buf_tx_pkt[(tx_rx_data.tx_packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME] = (transceiver_packet_t *) kmalloc(tx_rx_data.tx_packet->size + sizeof(transceiver_packet_t), GFP_ATOMIC);
				memcpy(buf_tx_pkt[(tx_rx_data.tx_packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME], tx_rx_data.tx_packet, tx_rx_data.tx_packet->size + sizeof(transceiver_packet_t));

				rtdm_mutex_unlock(&tx_rx_data_lock);

				/* Set the destination */
				memcpy(&winenet_get_tx_rx_data()->sl_desc->agencyUID_to, &current_listener->neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

				/* Propagate the data packet to the lower layers */
				rtdm_mutex_lock(&sender_lock);
				sender_tx(tx_rx_data.sl_desc, tx_rx_data.tx_packet, tx_rx_data.tx_size, 0);
				rtdm_mutex_unlock(&sender_lock);

				if (((tx_rx_data.tx_packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME == WNET_N_PACKETS_IN_FRAME-1) || (tx_rx_data.tx_packet->transID & WNET_LAST_PACKET)) {

					/* Prepare to pick a new listener */
					select_listener = true;

					/* Transition 12: Data sent */
					change_state(WNET_STATE_SPEAKER_WAIT_ACK);

					return ;

				} else {

					/* Allow the producer to go further, as the frame is not complete yet */
					winenet_xmit_data_processed(0);

					continue;
				}

			}

			if (tx_request_pending) {
				rtdm_mutex_lock(&wnet_tx_request_pending_lock);
				wnet_tx_request_pending = false;
				rtdm_mutex_unlock(&wnet_tx_request_pending_lock);

				rtdm_event_signal(&wnet_tx_request_event);

				/* Get the first listener */

				current_listener = initial_listener;

				if (!current_listener) {
					neighbour_list_protection(false);

					change_state(WNET_STATE_IDLE);

					return ;
				}

				DBG("Current listener: ");
				DBG_BUFFER(&current_listener->neighbour->agencyUID, SOO_AGENCY_UID_SIZE);
				memcpy(&speakerUID, get_my_agencyUID(), SOO_AGENCY_UID_SIZE);

				rtdm_mutex_lock(&tx_rx_data_lock);

				/* Reset the peer agencyUID */
				memcpy(&winenet_get_tx_rx_data()->sl_desc->agencyUID_to, get_null_agencyUID(), SOO_AGENCY_UID_SIZE);
				rtdm_mutex_unlock(&tx_rx_data_lock);

				medium_request_pending = true;

				/* Transmission completed beacon received and Next Speaker and TX request pending */
				/* We keep the neighborhood protected. */
			}

		} else if (ret == -ETIMEDOUT) {

			/* Clear the TX pkt buffer */
			clear_buf_tx_pkt();

			/* Reset the TX trans ID */
			sent_packet_transID = 0;

			winenet_xmit_data_processed(-ETIMEDOUT);

			neighbour_list_protection(false);
			forward_next_speaker();

			return ;

		} else {

			/* Unexpected! */
			lprintk("Unexpected ret value: %d\n", ret);
			BUG();

		}
	}
}

/**
 * SPEAKER Wait ACK state.
 */
static void winenet_state_speaker_wait_ack(wnet_state_t old_state) {
	wnet_tx_rx_data_t tx_rx_data;
	wnet_beacon_t last_beacon;
	nanosecs_rel_t tspeaker_ack = WNET_TSPEAKER_ACK;
	int ret;

	DBG("Speaker wait ACK\n");

	while (1) {

		DBG("Speaker UID: "); DBG_BUFFER(&speakerUID, SOO_AGENCY_UID_SIZE);
		/* Timeout of Tspeaker us */
		ret = rtdm_event_timedwait(&wnet_event, tspeaker_ack, NULL);

		winenet_copy_tx_rx_data(&tx_rx_data);
		winenet_get_last_beacon(&last_beacon);

		/*
		 * There are three cases in which we have to enter in Speaker wait ACK state:
		 * - This SOO was in Speaker candidate state and it has decided to become Speaker. There is a
		 *   RTS in progress, provoked by the TX request.
		 * - This is the last packet of a frame (n pkt/1 ACK strategy). Wait for an ACK only for the
		 *   last packet of the frame.
		 * - This is the last packet of the block (completed is true). Wait for an ACK for this packet.
		 */

		if (ret == 0) {
			if (medium_request_pending) {
				if ((last_beacon.id == WNET_BEACON_ACKNOWLEDGMENT) &&
				    !memcmp(&current_listener->neighbour->agencyUID, &last_beacon.u.acknowledgment.listenerUID, SOO_AGENCY_UID_SIZE)) {

					retry_count = 0;

					/* Put the listener in our group */
					current_listener->listener_in_group = true;

					/* Check if we have finished the turn of neighbours. */
					if (current_listener == initial_listener)
						medium_request_pending = false;

					/* Transition 13: Acknowledgment beacon received and matching Speaker/Listener UID and no medium release requested and matching trans ID */
					change_state(WNET_STATE_SPEAKER);

					/* As we are going back to the Speaker state, we send a signal to process the TX request */
					rtdm_event_signal(&wnet_event);

					return ;
				}
			} else {

				if ((last_beacon.id == WNET_BEACON_ACKNOWLEDGMENT) && (last_beacon.transID == (tx_rx_data.tx_transID & WNET_MAX_PACKET_TRANSID)) &&
					(tx_rx_data.tx_completed)) {

					if (current_listener != initial_listener) {
						/* Now, transmit the frame to the next listener. */

						broadcast_to_listeners = true;

						change_state(WNET_STATE_SPEAKER);
						rtdm_event_signal(&wnet_event);

						return ;
					}

					broadcast_to_listeners = false;

					/* Clear the TX pkt buffer */
					clear_buf_tx_pkt();

					/* Reset the TX trans ID */
					sent_packet_transID = 0;

					/* We will send the transmission completed beacon to a listener belonging to the group. */
					if (listeners_in_group() == 0) {
						winenet_xmit_data_processed(0);
						retry_count = 0;

						/* There is no neighbour. Go to the IDLE state. */
						DBG("?: No neighbour -> Idle\n");

						neighbour_list_protection(false);

						change_state(WNET_STATE_IDLE);

						return ;
					}

					/* Now, transmit the frame to all listeners. */
					do {
						current_listener = next_listener(NULL);
						BUG_ON(!memcmp(current_listener, get_null_agencyUID(), SOO_AGENCY_UID_SIZE));

						if (current_listener->listener_in_group) {
							winenet_send_beacon(WNET_BEACON_TRANSMISSION_COMPLETED, &current_listener->neighbour->agencyUID, get_my_agencyUID(),
									get_null_agencyUID(), 0);

							current_listener->listener_in_group = false;
						}

					} while (current_listener != initial_listener);

					broadcast_to_listeners = false;

					winenet_xmit_data_processed(0);
					retry_count = 0;

					/* Return to the SPEAKER state so that the requester can pursue with additional transmission.
					 * The end of transmission will be carried out with a NULL value as data buffer.
					 */
					change_state(WNET_STATE_SPEAKER);

					return ;

				} else if ((last_beacon.id == WNET_BEACON_ACKNOWLEDGMENT) && (last_beacon.transID == (tx_rx_data.tx_transID & WNET_MAX_PACKET_TRANSID))) {

					if (current_listener != initial_listener) {
						/* Now, transmit the frame to the next listener. */

						broadcast_to_listeners = true;

						change_state(WNET_STATE_SPEAKER);

						rtdm_event_signal(&wnet_event);
						return ;
					}

					broadcast_to_listeners = false;

					/* Clear the TX pkt buffer */
					clear_buf_tx_pkt();
					winenet_xmit_data_processed(0);

					retry_count = 0;

					/* Acknowledgment beacon received and matching Speaker/Listener UID and no RTS and matching trans ID, data */
					change_state(WNET_STATE_SPEAKER);
					rtdm_event_signal(&wnet_event);

					return ;
				}
			}

			/* Possible with medium_request_pending */
			if (last_beacon.id == WNET_BEACON_TAKE_OWNERSHIP) {

				/* Need to unpair all paired listeners. */
				listeners_reset();

				/* Clear the TX pkt buffer */
				clear_buf_tx_pkt();

				/* Reset the TX trans ID */
				sent_packet_transID = 0;

				/* Since we are becoming a listener, we reject any TX attempt for the moment. */
				winenet_xmit_data_processed(-EIO);

				retry_count = 0;

				/* Save the agency UID of the current Speaker */
				memcpy(&speakerUID, &tx_rx_data.sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE);

				/* Send the ack to the initial RTS from the speaker. */
				winenet_send_beacon(WNET_BEACON_ACKNOWLEDGMENT, &speakerUID, &speakerUID, get_my_agencyUID(), 0);

				neighbour_list_protection(false);

				change_state(WNET_STATE_LISTENER);

				return ;

			}

			if (last_beacon.id == WNET_BEACON_REQUEST_TO_SEND) {
				/* RTS beacon received */

				/* If we receive a RTS from a listener of the group, we simply discard it. */
				if (memcmp(&speakerUID, get_null_agencyUID(), SOO_AGENCY_UID_SIZE)) {
					current_listener->listener_in_group = false;

					if (current_listener == initial_listener) {

						retry_count = 0;
						medium_request_pending = false;

						if (listeners_in_group() == 0) {
							winenet_xmit_data_processed(-EIO);
							listener_unpair();

							/* Clear the TX pkt buffer */
							clear_buf_tx_pkt();

							/* Reset the TX trans ID */
							sent_packet_transID = 0;

							neighbour_list_protection(false);
							change_state(WNET_STATE_IDLE);

							return ;
						}

						while (!current_listener->listener_in_group)
							current_listener = next_listener(NULL);

						/* Set the initial listener as a valid one of the group. */
						initial_listener = current_listener;
					}

					/* Here, we pursue our turn of listeners */

					change_state(WNET_STATE_SPEAKER);
					rtdm_event_signal(&wnet_event);

					return ;
				}
				DBG(" ** Number of listeners in group : %d\n", listeners_in_group());
				if (listeners_in_group() > last_beacon.transID) {
					DBG(" ** Taking the lead, sending take_ownership beancon...\n");

					winenet_send_beacon(WNET_BEACON_TAKE_OWNERSHIP, &tx_rx_data.sl_desc->agencyUID_from, NULL, NULL, 0);

					/* We reposition our current_listener on the first one since we need to
					 * walk again all neighbours with the potential new ones.
					 * The walk will start after getting the acknowledgment from the listener which gets the take_ownership beacon.
					 */

					current_listener = initial_listener;

				} else {
					DBG(" ** Unpairing all listeners...\n");
					/* Need to unpair all paired listeners and associate them to the new speaker. */
					listeners_reset();

					/* Clear the TX pkt buffer */
					clear_buf_tx_pkt();

					/* Reset the TX trans ID */
					sent_packet_transID = 0;

					/* Since we are becoming a listener, we reject any TX attempt for the moment. */
					winenet_xmit_data_processed(-EIO);

					retry_count = 0;

					/* Save the agency UID of the current Speaker */
					memcpy(&speakerUID, &tx_rx_data.sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE);

					/* Send back an acknowledgment */
					winenet_send_beacon(WNET_BEACON_ACKNOWLEDGMENT, &tx_rx_data.sl_desc->agencyUID_from, &speakerUID, get_my_agencyUID(), 0);

					neighbour_list_protection(false);
					change_state(WNET_STATE_LISTENER);

					return ;
				}
			}

		} else if (ret == -ETIMEDOUT) {

			/* The timeout has expired */
			DBG("15: No Acknowledgment beacon received and Tspeaker timeout expired -> Speaker send retry\n");
			WNET_SHORT_DBG("15");

			/* Transition 15: No Acknowledgment beacon received and Tspeaker timeout expired */
			change_state(WNET_STATE_SPEAKER_RETRY);

			return ;

		} else {
			/* Unexpected! */
			lprintk("Unexpected ret value: %d\n", ret);
			BUG();
		}
	}
}

/**
 * SPEAKER retry state.
 */
static void winenet_state_speaker_retry(wnet_state_t old_state) {
	uint8_t i;
	wnet_tx_rx_data_t tx_rx_data;

	DBG("Speaker retry %d\n", retry_count);

	winenet_copy_tx_rx_data(&tx_rx_data);

	if (retry_count == WNET_RETRIES_MAX) {

		current_listener->listener_in_group = false;

		if ((listeners_in_group() == 0) && (!medium_request_pending || (current_listener == initial_listener))) {
			winenet_xmit_data_processed(-EIO);
			retry_count = 0;

			broadcast_to_listeners = false;
			medium_request_pending = false;

			listener_unpair();

			/* Clear the TX pkt buffer */
			clear_buf_tx_pkt();

			/* Reset the TX trans ID */
			sent_packet_transID = 0;

			neighbour_list_protection(false);
			change_state(WNET_STATE_IDLE);

			return ;
		}

		if (medium_request_pending) {

			/* If current_listener == initial_listener, it means a full turn of neighbours has been done. */
			if (current_listener == initial_listener) {

				retry_count = 0;
				medium_request_pending = false;

				while (!current_listener->listener_in_group)
					current_listener  = next_listener(NULL);

				/* Set the initial listener as a valid one of the group. */
				initial_listener = current_listener;
			}
		} else {
			/* Select another (valid) listener in this group. */

			do {
				current_listener = next_listener(NULL);
			} while (!current_listener->listener_in_group);

			if (!initial_listener->listener_in_group)
				initial_listener = current_listener;
		}

		change_state(WNET_STATE_SPEAKER);
		rtdm_event_signal(&wnet_event);

		return ;
	}
	if (medium_request_pending) {
		retry_count++;
		winenet_send_beacon(WNET_BEACON_REQUEST_TO_SEND, &current_listener->neighbour->agencyUID, NULL, NULL, 0);

		change_state(WNET_STATE_SPEAKER_WAIT_ACK);

		return ;
	}
	/* A data frame is being processed */

	rtdm_mutex_lock(&tx_rx_data_lock);

	/* Set the destination */
	memcpy(&winenet_get_tx_rx_data()->sl_desc->agencyUID_to, &current_listener->neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

	/* Re-send all the packets of the frame */
	rtdm_mutex_lock(&sender_lock);
	for (i = 0; ((i < WNET_N_PACKETS_IN_FRAME) && (buf_tx_pkt[i] != NULL)); i++)
		sender_tx(tx_rx_data.sl_desc, buf_tx_pkt[i], buf_tx_pkt[i]->size, 0);
	rtdm_mutex_unlock(&sender_lock);

	rtdm_mutex_unlock(&tx_rx_data_lock);

	retry_count++;

	/* Retry data/RTS beacon sent and maximal number of retries not reached */
	change_state(WNET_STATE_SPEAKER_WAIT_ACK);
}

/******************************************************************* Listener **************************************/

/**
 * LISTENER state.
 */
static void winenet_state_listener(wnet_state_t old_state) {
	wnet_tx_rx_data_t tx_rx_data;
	wnet_beacon_t last_beacon;
	nanosecs_rel_t tlistener;
	bool tx_request_pending, speakerUID_known;
	int ret;
	static bool just_unpaired = false;

	DBG("Listener.\n");

	while (1) {

		DBG("Listener, paired with speaker: "); DBG_BUFFER(&speakerUID, SOO_AGENCY_UID_SIZE);

		/* Check if the current speaker UID is known */
		speakerUID_known = (memcmp(&speakerUID, get_null_agencyUID(), SOO_AGENCY_UID_SIZE) != 0);

		/* Timeout of Tlistener us */
		if (speakerUID_known || just_unpaired) {
			tlistener = WNET_TLISTENER_LONG;

			/* Will be set to true when listener_unpaired() is called */
			just_unpaired = false;
		} else
			tlistener = WNET_TLISTENER_SHORT;

		ret = rtdm_event_timedwait(&wnet_event, tlistener, NULL);

		neighbour_list_protection(true);

		winenet_copy_tx_rx_data(&tx_rx_data);
		winenet_get_last_beacon(&last_beacon);

		rtdm_mutex_lock(&wnet_tx_request_pending_lock);
		tx_request_pending = wnet_tx_request_pending;
		rtdm_mutex_unlock(&wnet_tx_request_pending_lock);

		/* Preserve the speaker for a next speaker selection */
		memcpy(&last_speakerUID, &tx_rx_data.sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE);

		/*
		 * We can be in the Listener state *without* any (paired) speaker; we have to reject all beacons which
		 * are addressed to us except the RTS from a speaker, in this case.
		 * Scenario: if another speaker tried to reach us with some repeated RTS, and we were not available at this time,
		 * we could received a TRANSMISSION_COMPLETED
		 */

		if (ret == 0) {

			if (speakerUID_known && (tx_rx_data.data_received)) {
				/* Data has been received. Reset the proper flag. */

				rtdm_mutex_lock(&tx_rx_data_lock);
				winenet_get_tx_rx_data()->data_received = false;
				rtdm_mutex_unlock(&tx_rx_data_lock);

				/*
				 * Send an ACKNOWLEDGMENT beacon only for data packets, and if this Smart Object is
				 * the Active Listener.
				 * The speakerUID has been retrieved during the RTS *or* during the receival of a
				 * transmission completed beacon; it is the SOO who is speaking now.
				 *
				 */

				BUG_ON(!memcmp(&speakerUID, get_null_agencyUID(), SOO_AGENCY_UID_SIZE));

				winenet_send_beacon(WNET_BEACON_ACKNOWLEDGMENT, &speakerUID, &speakerUID, get_my_agencyUID(),
						tx_rx_data.rx_transID & WNET_MAX_PACKET_TRANSID);

				neighbour_list_protection(false);
				continue;
			}

			/* Only a RTS can be processed with an unknown speaker UID */
			if (last_beacon.id == WNET_BEACON_REQUEST_TO_SEND) {
				/* RTS beacon received */

				/* Save the agency UID of the current Speaker.
				 * When a listener receives a RTS, it means that it will belong to a group (a speaker and
				 * several listeners).
				 */

				memcpy(&speakerUID, &tx_rx_data.sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE);

				winenet_send_beacon(WNET_BEACON_ACKNOWLEDGMENT, &speakerUID, &speakerUID, get_my_agencyUID(), 0);

				neighbour_list_protection(false);
				continue;
			}

			if ((last_beacon.id == WNET_BEACON_ACKNOWLEDGMENT) && !speakerUID_known) {
				/* Simply ignore it, the remote SOO was in a medium_request process, but we took over the lead */
				neighbour_list_protection(false);
				continue;
			}

			/* Got the end of transmission of a list of packets (typically a block) */
			if (last_beacon.id == WNET_BEACON_TRANSMISSION_COMPLETED) {

				/* Unpair with the speaker */
				listener_unpair();
				just_unpaired = true;

				if (!memcmp(&last_beacon.u.transmission_completed.nextSpeakerUID, get_my_agencyUID(), SOO_AGENCY_UID_SIZE)) {

					/* This SOO is nominated Next Speaker */
					DBG("I'm nominated Next Speaker !! : "); DBG_BUFFER(get_my_agencyUID(), SOO_AGENCY_UID_SIZE);

					change_state(WNET_STATE_SPEAKER);
					rtdm_event_signal(&wnet_event);

					return ;

				} else {

					/* Transmission completed beacon received and not Next Speaker */

					neighbour_list_protection(false);
					continue;
				}
			}


		} else if (ret == -ETIMEDOUT) {
			/* The timeout has expired */

			memcpy(&speakerUID, get_null_agencyUID(), SOO_AGENCY_UID_SIZE);

			neighbour_list_protection(false);

			/* Transition 7: Tlistener timeout expired */
			change_state(WNET_STATE_IDLE);

			/* Give the opportunity to check if there are some TX pending request or xmit */
			rtdm_event_signal(&wnet_event);

			return ;
		}
		else {
			/* Unexpected! */
			lprintk("Unexpected ret value: %d\n", ret);
			BUG();
		}

		neighbour_list_protection(false);

	}
}

/* FSM management */

/* FSM function table */
static wnet_state_fn_t fsm_functions[WNET_STATE_N] = {
	[WNET_STATE_IDLE]				= winenet_state_idle,
	[WNET_STATE_SPEAKER_CANDIDATE]			= winenet_state_speaker_candidate,
	[WNET_STATE_SPEAKER]				= winenet_state_speaker,
	[WNET_STATE_SPEAKER_WAIT_ACK]			= winenet_state_speaker_wait_ack,
	[WNET_STATE_SPEAKER_RETRY]			= winenet_state_speaker_retry,
	[WNET_STATE_LISTENER]				= winenet_state_listener,
};

/**
 * Change the state of Winenet. Generic function.
 */
void winenet_change_state(wnet_fsm_handle_t *handle, wnet_state_t new_state) {
	if (unlikely((handle->state < 0) || (handle->state >= WNET_STATE_N) ||
			(new_state < 0) || (new_state >= WNET_STATE_N))) {
		lprintk("Invalid state: %d -> %d\n", handle->state, new_state);
		BUG();
	}

	DBG(" ** change_state: %s -> %s              UID: \n", winenet_get_state_string(get_state()), winenet_get_state_string(new_state));
	DBG_BUFFER(get_my_agencyUID(), SOO_AGENCY_UID_SIZE);

	handle->old_state = handle->state;
	handle->state = new_state;

	rtdm_event_signal(&handle->event);
}

/**
 * Change the state of Winenet.
 */
static void change_state(wnet_state_t new_state) {
	winenet_change_state(&fsm_handle, new_state);
}

/**
 * Get the state of Winenet. Generic function.
 */
wnet_state_t winenet_get_state(wnet_fsm_handle_t *handle) {
	return handle->state;
}

/**
 * Get the state of Winenet.
 */
static wnet_state_t get_state(void) {
	return winenet_get_state(&fsm_handle);
}

/**
 * Main Winenet routine that implements the FSM.
 * At the beginning, we are in the IDLE state.
 */
static void fsm_task_fn(void *args) {
	wnet_fsm_handle_t *handle = (wnet_fsm_handle_t *) args;
	wnet_state_fn_t *functions = handle->funcs;
	rtdm_event_t *event = &handle->event;

	DBG("Entering Winenet FSM task...\n");

	while (true) {
		DBG("Got the wnet_state_event signal.\n");

		/* Call the proper state function */
		(*functions[handle->state])(handle->old_state);

		rtdm_event_wait(event);
	}
}

/**
 * Start the Winenet FSM routine.
 * The FSM function table and the RTDM event are in the handle given as parameter.
 * This function has to be called from CPU #0.
 */
void winenet_start_fsm_task(char *name, wnet_fsm_handle_t *handle) {
	handle->old_state = WNET_STATE_IDLE;
	handle->state = WNET_STATE_IDLE;

	rtdm_task_init(&handle->task, name, fsm_task_fn, (void *) handle, WINENET_TASK_PRIO, 0);
}

/**
 * This function is called when a data packet or a Iamasoo beacon has to be sent.
 * The call is made by the Sender.
 */
static int winenet_xmit(sl_desc_t *sl_desc, void *packet_ptr, size_t size, bool completed) {
	transceiver_packet_t *packet;
	int ret;

	/* End of transmission ? */
	if (!packet_ptr) {
		__tx_rx_data.transmission_over = true;

		/* Ok, go ahead with the next speaker */
		if (get_state() == WNET_STATE_SPEAKER)
			rtdm_event_signal(&wnet_event);

		return 0;
	}

	/* Only one producer can call winenet_tx at a time */
	rtdm_mutex_lock(&wnet_xmit_lock);

	packet = (transceiver_packet_t *) packet_ptr;

	if (unlikely(sl_desc->req_type == SL_REQ_DISCOVERY)) {
		/* Iamasoo beacons bypass Winenet */
		packet->transID = 0xffffffff;

		rtdm_mutex_lock(&sender_lock);
		sender_tx(sl_desc, packet, size, 0);
		rtdm_mutex_unlock(&sender_lock);

		rtdm_mutex_unlock(&wnet_xmit_lock);

		return 0;
	}

	/* If the transmission mode is SL_MODE_NETSTREAM, redirect to the proper callback */
	if (sl_desc->trans_mode == SL_MODE_NETSTREAM) {
		ret = winenet_netstream_xmit(sl_desc, packet_ptr, size, completed);
		rtdm_mutex_unlock(&wnet_xmit_lock);
		return ret;
	}

	packet->transID = sent_packet_transID;

	/*
	 * If this is the last packet, set the WNET_LAST_PACKET bit in the transID.
	 * This is required to allow the receiver to identify the last packet of the
	 * frame (if the modulo of its trans ID is not equal to WNET_N_PACKETS_IN_FRAME - 1)
	 * and force it to send an ACK.
	 */
	if (completed)
		packet->transID |= WNET_LAST_PACKET;

	rtdm_mutex_lock(&tx_rx_data_lock);

	/* Fill the TX request parameters */

	__tx_rx_data.tx_pending = true;
	__tx_rx_data.sl_desc = sl_desc;
	__tx_rx_data.tx_packet = packet;
	__tx_rx_data.tx_size = size;
	__tx_rx_data.tx_transID = packet->transID;
	__tx_rx_data.tx_completed = completed;

	rtdm_mutex_unlock(&tx_rx_data_lock);

	/*
	 * Prepare the next transID.
	 * If the complete flag is set, the next value will be 0.
	 */
	if (!completed)
		sent_packet_transID = (sent_packet_transID + 1) % WNET_MAX_PACKET_TRANSID;
	else
		sent_packet_transID = 0;

	/* Wake the Winenet task to process the TX request */
	rtdm_event_signal(&wnet_event);

	/* Wait for the packet to be processed */
	winenet_wait_xmit_event();

	rtdm_mutex_lock(&tx_rx_data_lock);
	ret = __tx_rx_data.tx_ret;
	rtdm_mutex_unlock(&tx_rx_data_lock);

	rtdm_mutex_unlock(&wnet_xmit_lock);

	return ret;
}

/**
 * This function is called when a TX request is being issued.
 * The call is made by the Sender.
 */
static int winenet_request_xmit(sl_desc_t *sl_desc) {
	/* If the transmission mode is SL_MODE_NETSTREAM, redirect to the proper callback */
	if (sl_desc->trans_mode == SL_MODE_NETSTREAM)
		return winenet_netstream_request_xmit(sl_desc);

	DBG("** winenet_request_xmit\n");

	rtdm_mutex_lock(&tx_rx_data_lock);
	__tx_rx_data.sl_desc = sl_desc;
	rtdm_mutex_unlock(&tx_rx_data_lock);

	rtdm_mutex_lock(&wnet_tx_request_pending_lock);
	wnet_tx_request_pending = true;
	rtdm_mutex_unlock(&wnet_tx_request_pending_lock);

	/* Wake the Winenet task to process the TX request if it is in IDLE state */
	if ((get_state() == WNET_STATE_IDLE) || (get_state() == WNET_STATE_SPEAKER))
		rtdm_event_signal(&wnet_event);

	/* Wait for the TX request to be processed */
	rtdm_event_wait(&wnet_tx_request_event);

	return 0;
}

/**
 * This function is called when a packet is received. This can be a data packet to forward to a
 * consumer (typically the Decoder), a Iamasoo beacon to forward to the Discovery block or a
 * Datalink beacon to handle in Winenet.
 * The call is made by the Receiver.
 * The size refers to the whole transceiver packet.
 */
void winenet_rx(sl_desc_t *sl_desc, plugin_desc_t *plugin_desc, void *packet_ptr, size_t size) {
	transceiver_packet_t *packet, *new_packet;
	bool isMonitoringMedium;
	static bool all_packets_received = false;
	static uint32_t last_transID;
	uint32_t i;

	if (unlikely((sl_desc->req_type == SL_REQ_DISCOVERY))) {
		/* Iamasoo beacons bypass Winenet */
		receiver_rx(sl_desc, plugin_desc, packet_ptr, size);

		return ;
	}

	/* If the transmission mode is SL_MODE_NETSTREAM, redirect to the proper callback */
	if (sl_desc->trans_mode == SL_MODE_NETSTREAM) {
		winenet_netstream_rx(sl_desc, plugin_desc, packet_ptr, size);
		return ;
	}

	packet = (transceiver_packet_t *) packet_ptr;

	DBG("** receiving: agencyUID_to: "); DBG_BUFFER(&sl_desc->agencyUID_to, SOO_AGENCY_UID_SIZE);
	DBG("**            agencyUID_from: "); DBG_BUFFER(&sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE);
	DBG("**            speakerUID: "); DBG_BUFFER(&speakerUID, SOO_AGENCY_UID_SIZE);

	/*
	 *
	 * Update the TX_RX data with sl_desc.
	 * This is useful when there is no XMIT request: the sl_desc should not be NULL.
	 */

	rtdm_mutex_lock(&tx_rx_data_lock);
	__tx_rx_data.sl_desc = sl_desc;
	rtdm_mutex_unlock(&tx_rx_data_lock);

	rtdm_mutex_lock(&wnet_medium_free_lock);
	isMonitoringMedium = wnet_isMonitoringMedium;
	rtdm_mutex_unlock(&wnet_medium_free_lock);

	/* If the free medium detection is in progress, send an event to make the function return false */
	if (isMonitoringMedium)
		rtdm_event_signal(&wnet_medium_free_event);

	if (packet->packet_type == TRANSCEIVER_PKT_DATALINK) {

		rtdm_mutex_lock(&tx_rx_data_lock);
		memcpy(&__tx_rx_data.last_beacon, packet->payload, sizeof(wnet_beacon_t));
		rtdm_mutex_unlock(&tx_rx_data_lock);

		if (__tx_rx_data.last_beacon.id == WNET_BEACON_SPEAKER_UNPAIR)
			memcpy(&speakerUID, get_null_agencyUID(), SOO_AGENCY_UID_SIZE);

	}

	/* unibroad mode: if we are waiting for packets from a speaker, we simply ignore packets incoming from other
	 * possible speakers (multiple groups with hidden nodes).
	 * -> speakerUID is used to detected if the SOO is paired with another (unibroad)
	 */

	if (memcmp(&speakerUID, get_null_agencyUID(), SOO_AGENCY_UID_SIZE))
		if (memcmp(&sl_desc->agencyUID_from, &sl_desc->agencyUID_to, SOO_AGENCY_UID_SIZE))
			return ; /* Skip it */

	if ((packet->packet_type == TRANSCEIVER_PKT_DATA) && (get_state() != WNET_STATE_LISTENER)) {
		rtdm_mutex_lock(&tx_rx_data_lock);
		__tx_rx_data.sl_desc = sl_desc;
		__tx_rx_data.rx_transID = packet->transID;
		__tx_rx_data.data_received = true;
		rtdm_mutex_unlock(&tx_rx_data_lock);

		rtdm_event_signal(&wnet_event);
	}

	if ((packet->packet_type == TRANSCEIVER_PKT_DATA) && (get_state() == WNET_STATE_LISTENER)) {
		/* Data packet */

		rtdm_mutex_lock(&tx_rx_data_lock);

		if ((packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME == 0) {
			/*
			 * First packet of the frame: be ready to process the next packets of the frame.
			 * By default, at the beginning, we set the all_packets_received boolean to true. If any packet in the frame is missed,
			 * the boolean is set to false, meaning that the frame is invalid.
			 */
			all_packets_received = true;
			last_transID = (packet->transID & 0xffffff);

			clear_buf_rx_pkt();
		}

		if ((packet->transID & WNET_MAX_PACKET_TRANSID) - last_transID > 1) {
			/* We missed a packet. This frame is discarded. */
			DBG("Pkt missed: %d/%d\n", last_transID, packet->transID & WNET_MAX_PACKET_TRANSID);
			all_packets_received = false;

			clear_buf_rx_pkt();
		}

		/* Copy the packet into the bufferized packet array */
		if (buf_rx_pkt[(packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME]) {
			/*
			 * An incoming packet has to be placed in a buffer which is already populated.
			 * This means that some packets at the beginning of the frame have been missed. The whole
			 * frame has to be discarded.
			 */

			DBG("RX buffer already populated: %d, %d\n", packet->transID, (packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME);

			all_packets_received = false;

			clear_buf_rx_pkt();
		}

		new_packet = (transceiver_packet_t *) kmalloc(packet->size + sizeof(transceiver_packet_t), GFP_ATOMIC);
		BUG_ON(!new_packet);

		buf_rx_pkt[(packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME] = new_packet;

		memcpy(buf_rx_pkt[(packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME], packet, packet->size + sizeof(transceiver_packet_t));

		rtdm_mutex_unlock(&tx_rx_data_lock);

		/* Save the last ID of the last received packet */
		last_transID = (packet->transID & WNET_MAX_PACKET_TRANSID);

		if (((packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME == WNET_N_PACKETS_IN_FRAME - 1) || (packet->transID & WNET_LAST_PACKET)) {
			/* If all the packets of the frame have been received, forward them to the upper layer */

			if (all_packets_received) {
				rtdm_mutex_lock(&tx_rx_data_lock);

				for (i = 0; ((i < WNET_N_PACKETS_IN_FRAME) && (buf_rx_pkt[i] != NULL)); i++)
					if ((buf_rx_pkt[i]->packet_type == TRANSCEIVER_PKT_DATA) && !packet_already_received(buf_rx_pkt[i], &sl_desc->agencyUID_from))
						receiver_rx(sl_desc, plugin_desc, buf_rx_pkt[i], buf_rx_pkt[i]->size);

				clear_buf_rx_pkt();

				__tx_rx_data.sl_desc = sl_desc;
				__tx_rx_data.rx_transID = packet->transID;
				__tx_rx_data.data_received = true;

				rtdm_mutex_unlock(&tx_rx_data_lock);

				rtdm_event_signal(&wnet_event);
			}
		}
	}

	if (packet->packet_type == TRANSCEIVER_PKT_DATALINK) {
		/* Datalink packet (typically, a Winenet beacon) */

		DBG0("Beacon recv\n");

		rtdm_event_signal(&wnet_event);
	}
}

static bool winenet_ready_to_send(sl_desc_t *sl_desc) {
	return __tx_rx_data.active_speaker;
}

/**
 * Callbacks of the Winenet protocol
 */
static datalink_proto_desc_t winenet_proto = {
	.ready_to_send = winenet_ready_to_send,
	.xmit_callback = winenet_xmit,
	.rx_callback = winenet_rx,
	.request_xmit_callback = winenet_request_xmit,
};

/**
 * Register the Winenet protocol with the Datalink subsystem. The protocol is associated
 * to the SL_PROTO_WINENET ID.
 */
static void winenet_register(void) {
	datalink_register_protocol(SL_DL_PROTO_WINENET, &winenet_proto);
}

/**
 * Initialization of Winenet.
 */
void winenet_init(void) {
	DBG("Winenet initialization\n");

	memset(&speakerUID, 0, sizeof(agencyUID_t));

	INIT_LIST_HEAD(&wnet_neighbours);

	rtdm_event_init(&wnet_event, 0);
	rtdm_event_init(&wnet_medium_free_event, 0);
	rtdm_event_init(&__tx_rx_data.xmit_event, 0);
	rtdm_event_init(&wnet_tx_request_event, 0);

	__tx_rx_data.tx_pending = false;
	__tx_rx_data.data_received = false;
	__tx_rx_data.last_beacon.id = WNET_BEACON_N;
	__tx_rx_data.active_speaker = false;
	__tx_rx_data.transmission_over = false;

	rtdm_mutex_init(&tx_rx_data_lock);
	rtdm_mutex_init(&wnet_xmit_lock);
	rtdm_mutex_init(&wnet_medium_free_lock);
	rtdm_mutex_init(&wnet_tx_request_pending_lock);
	rtdm_mutex_init(&sender_lock);

	rtdm_mutex_init(&neighbour_list_lock);

	rtdm_event_init(&fsm_handle.event, 0);
	fsm_handle.funcs = fsm_functions;

	winenet_register();

	/* Register with Discovery as Discovery listener */
	discovery_listener_register(&wnet_discovery_desc);

	winenet_start_fsm_task("Wnet", &fsm_handle);

	/* Initialize the Winenet netstream extension */
	winenet_netstream_init();
}
