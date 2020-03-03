/*
 * Copyright (C) 2016-2018 Baptiste Delporte <bonel@bonel.net>
 * Copyright (C) 2018 Jean-Pierre Miceli <jean-pierre.miceli@heig-vd.ch>
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
		force_print("(" fmt ")\n", ##__VA_ARGS__); \
	} while (0)
#else
#define WNET_SHORT_DBG(fmt, ...)
#endif

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/time.h>

#include <soolink/soolink.h>

#define WNET_CONFIG_BROADCAST		1
#include <soolink/datalink/winenet.h>
#include <soolink/datalink.h>
#include <soolink/sender.h>
#include <soolink/receiver.h>
#include <soolink/discovery.h>

#include <soo/core/device_access.h>

#include <xenomai/rtdm/driver.h>

#include <rtdm/soo.h>

#include <virtshare/debug.h>
#include <virtshare/console.h>
#include <virtshare/soo.h>

static void winenet_change_state(wnet_state_t new_state);

static agencyUID_t speakerUID;
static agencyUID_t prevSpeakerUID;
static agencyUID_t wrongSpeakerUID;
static agencyUID_t listenerUID;
static agencyUID_t activeListenerUID;
static agencyUID_t nominatedActiveListenerUID;

static rtdm_task_t wnet_task;

static wnet_state_t wnet_old_state = WNET_STATE_IDLE;
static wnet_state_t wnet_state = WNET_STATE_IDLE;

static rtdm_mutex_t wnet_xmit_lock;

/* Events used to allow the producer to go further */
static rtdm_event_t wnet_xmit_event;

/* Event used to track the receival of a Winenet beacon or a TX request */
static rtdm_event_t wnet_event;

/* Event used in the FSM */
static rtdm_event_t wnet_state_event;

/* Medium monitoring to detect if the medium is free */
static bool wnet_isMonitoringMedium = false;
static rtdm_event_t wnet_medium_free_event;
static rtdm_mutex_t wnet_medium_free_lock;

static wnet_tx_rx_data_t wnet_tx_rx_data;
static rtdm_mutex_t wnet_tx_rx_data_lock;

/* Management of the neighbourhood of SOOs. Used in the retry packet management. */
static struct list_head wnet_neighbours;
static rtdm_mutex_t neighbour_list_lock;

/* Handling the unibroad group of smart objects */
static wnet_neighbour_t *unibroad_current_listener = NULL;
static wnet_neighbour_t *unibroad_initial_listener = NULL;

/* Each call to winenet_tx will increment the transID counter */
static uint32_t sent_packet_transID = 0;

/* Data packet/Medium request beacon retry management */
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

static uint8_t *beacon_id_string[0x10] = {
	[WNET_BEACON_ID_MEDIUM_REQUEST]			= "MEDIUM REQUEST",
	[WNET_BEACON_ID_ACKNOWLEDGMENT]			= "ACKNOWLEDGMENT",
	[WNET_BEACON_ID_TRANSMISSION_COMPLETED]		= "TRANSMISSION COMPLETED",
	[WNET_BEACON_ID_COLLISION_DETECTED]		= "COLLISION DETECTED",
	[WNET_BEACON_ID_ABORT]				= "ABORT",
	[WNET_BEACON_ID_RESUME]				= "RESUME"
};

static uint8_t invalid_string[] = "INVALID";

/* Debugging functions */

uint8_t *get_state_string(wnet_state_t state) {
	uint32_t state_int = (uint32_t) state;

	if (unlikely(state_int >= WNET_STATE_N))
		return invalid_string;

	return state_string[state_int];
}

uint8_t *get_beacon_id_string(wnet_beacon_id_t beacon_id) {
	uint32_t beacon_id_int = (uint32_t) beacon_id;

	if (unlikely((beacon_id_int >= WNET_BEACON_ID_N)))
		return invalid_string;

	return beacon_id_string[beacon_id_int];
}


/*
 * Update the active listener.
 */
static bool isActiveListener(trans_mode_t trans_mode) {

	if (trans_mode == SL_MODE_UNIBROAD)
		return true;
	else
		return (!memcmp(&nominatedActiveListenerUID, get_my_agencyUID(), SOO_AGENCY_UID_SIZE));
}

/**
 * Take a snapshot of the TX request parameters.
 */
static void get_tx_rx_data(wnet_tx_rx_data_t *tx_rx_data) {
	rtdm_mutex_lock(&wnet_tx_rx_data_lock);
	memcpy(tx_rx_data, &wnet_tx_rx_data, sizeof(wnet_tx_rx_data_t));
	rtdm_mutex_unlock(&wnet_tx_rx_data_lock);
}

/**
 * Allow the producer to be informed about potential problems or to
 * send a next packet.
 */
static void xmit_data_processed(int ret) {
	rtdm_mutex_lock(&wnet_tx_rx_data_lock);

	if (!wnet_tx_rx_data.tx_pending) {
		/* There is no TX request. There is no need to make any ACK. */
		rtdm_mutex_unlock(&wnet_tx_rx_data_lock);
		return ;
	}

	wnet_tx_rx_data.tx_pending = false;
	wnet_tx_rx_data.tx_ret = ret;
	rtdm_mutex_unlock(&wnet_tx_rx_data_lock);

	/* Allow the producer to go further */
	rtdm_event_signal(&wnet_xmit_event);
}

/**
 * Destroy the bufferized TX packets.
 * This function has to be called when a packet frame has been acknowledged, or if there is
 * an unexpected transition that requires the whole frame to be freed.
 */
static void clear_buf_tx_pkt(void) {
	uint32_t i;

	rtdm_mutex_lock(&wnet_tx_rx_data_lock);

	for (i = 0; i < WNET_N_PACKETS_IN_FRAME; i++) {
		if (buf_tx_pkt[i] != NULL) {
			kfree(buf_tx_pkt[i]);
			buf_tx_pkt[i] = NULL;
		}
	}

	rtdm_mutex_unlock(&wnet_tx_rx_data_lock);
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
static void get_last_beacon(wnet_beacon_t *last_beacon) {
	rtdm_mutex_lock(&wnet_tx_rx_data_lock);

	memcpy(last_beacon, &wnet_tx_rx_data.last_beacon, sizeof(wnet_beacon_t));
	wnet_tx_rx_data.last_beacon.id = WNET_BEACON_ID_N;
	rtdm_mutex_unlock(&wnet_tx_rx_data_lock);
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
	static bool first_neighbour = true;
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

		unibroad_current_listener = wnet_neighbour;
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

			/* We update the global references */
			if (unibroad_current_listener == wnet_neighbour)
				unibroad_current_listener = ((next == &wnet_neighbours) ? NULL : list_entry(next, wnet_neighbour_t, list));

			if (unibroad_initial_listener == wnet_neighbour)
				unibroad_initial_listener = ((next == &wnet_neighbours) ? NULL : list_entry(next, wnet_neighbour_t, list));

			list_del(cur);
			kfree(wnet_neighbour);
		}
	}

	if (smp_processor_id() == AGENCY_RT_CPU)
		rtdm_mutex_unlock(&neighbour_list_lock);

	DBG("*** REMOVing new neighbour\n");

}

/**
 * Get the next neighbour for the unibroad mode transmission.
 */
static inline wnet_neighbour_t *unibroad_next_listener(wnet_neighbour_t *from) {
	struct list_head *cur;

	rtdm_mutex_lock(&neighbour_list_lock);

	if (from == NULL)
		cur = &unibroad_current_listener->list;
	else
		cur = &from->list;

	cur = cur->next;
	if (cur == &wnet_neighbours)
		cur = wnet_neighbours.next;

	rtdm_mutex_unlock(&neighbour_list_lock);

	return list_entry(cur, wnet_neighbour_t, list);
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

		wnet_neighbour->listener_in_group = false;
	}

	rtdm_mutex_unlock(&neighbour_list_lock);
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

static discovery_listener_t wnet_discovery_desc = {
	.add_neighbour_callback = winenet_add_neighbour,
	.remove_neighbour_callback = winenet_remove_neighbour
};

/*
 * The function returns a neighbour selected from the neighbour list by choosing the neighbour
 * which is just after the given one.
 * If prev_agencyUID is NULL, we return the first entry of the list.
 *
 * We return the pointer to the chosen neighbour. Returns NULL if no neighbour available.
 */
static wnet_neighbour_t *winenet_next_neighbour_rr(agencyUID_t *prev_agencyUID, agencyUID_t *next_agencyUID) {
	struct list_head *cur;
	wnet_neighbour_t *wnet_neighbour;

	/* There is no neighbour in the list, I am alone */
	if (list_empty(&wnet_neighbours))
		return NULL;

	rtdm_mutex_lock(&neighbour_list_lock);

	if (prev_agencyUID != NULL)
		/* Iterate over the neighbours to find the one whose agency UID is just after the "old" one */
		list_for_each(cur, &wnet_neighbours) {
			wnet_neighbour = list_entry(cur, wnet_neighbour_t, list);

			if (!memcmp(prev_agencyUID, &wnet_neighbour->neighbour->agencyUID, SOO_AGENCY_UID_SIZE))
				/* The prev agency UID has been found */
				break;
		} else
			cur = &wnet_neighbours; /* Prepare the next condition */

	/* We reached the end of the list and were not able to find the agencyUID; maybe it disappeared in between. */
	if (cur == &wnet_neighbours) {
		/* Get the first item of the list */
		wnet_neighbour = list_entry(wnet_neighbours.next, wnet_neighbour_t, list);
	} else {
		cur = wnet_neighbour->list.next;
		if (cur == &wnet_neighbours)
			cur = cur->next;

		wnet_neighbour = list_entry(cur, wnet_neighbour_t, list);
	}

	memcpy(next_agencyUID, &wnet_neighbour->neighbour->agencyUID, sizeof(agencyUID_t));

	rtdm_mutex_unlock(&neighbour_list_lock);

	return wnet_neighbour;
}

/**
 * Send a Winenet beacon.
 * The purpose of agencyUID1, agencyUID2 and the optional field depends on the beacon type.
 */
static void winenet_send_beacon(wnet_beacon_id_t beacon_id,
				agencyUID_t *dest_agencyUID,
				agencyUID_t *agencyUID1, agencyUID_t *agencyUID2,
				uint32_t opt) {
	wnet_tx_rx_data_t tx_rx_data;
	wnet_beacon_t outgoing_beacon;
	transceiver_packet_t *outgoing_packet = (transceiver_packet_t *) kmalloc(sizeof(transceiver_packet_t) + sizeof(wnet_beacon_t), GFP_ATOMIC);

	DBG("Send beacon %s to ", get_beacon_id_string(beacon_id));
	if (!dest_agencyUID)
		DBG("(broadcast)\n");
	else
		DBG_BUFFER(dest_agencyUID, SOO_AGENCY_UID_SIZE);

	get_tx_rx_data(&tx_rx_data);

	outgoing_packet->packet_type = TRANSCEIVER_PKT_DATALINK;
	outgoing_packet->transID = 0;

	outgoing_beacon.id = beacon_id;
	outgoing_beacon.transID = 0;

	switch (beacon_id) {
	case WNET_BEACON_ID_MEDIUM_REQUEST:
		outgoing_beacon.transID = listeners_in_group();

		memcpy(&outgoing_beacon.u.medium_request.speakerUID, agencyUID1, SOO_AGENCY_UID_SIZE);
		memcpy(&outgoing_beacon.u.medium_request.listenerUID, agencyUID2, SOO_AGENCY_UID_SIZE);

		break;

	case WNET_BEACON_ID_ACKNOWLEDGMENT:
		/* opt is the TX request trans ID */
		outgoing_beacon.transID = opt;

		memcpy(&outgoing_beacon.u.acknowledgment.speakerUID, agencyUID1, SOO_AGENCY_UID_SIZE);
		memcpy(&outgoing_beacon.u.acknowledgment.listenerUID, agencyUID2, SOO_AGENCY_UID_SIZE);

		break;

	case WNET_BEACON_ID_TRANSMISSION_COMPLETED:
		outgoing_beacon.transID = 0;

		memcpy(&outgoing_beacon.u.transmission_completed.speakerUID, agencyUID1, SOO_AGENCY_UID_SIZE);
		memcpy(&outgoing_beacon.u.transmission_completed.nextSpeakerUID, agencyUID2, SOO_AGENCY_UID_SIZE);

		break;

	case WNET_BEACON_ID_COLLISION_DETECTED:
		outgoing_beacon.transID = 0;

		memset(&outgoing_beacon.u.collision_detected.unused, 0, SOO_AGENCY_UID_SIZE);
		memcpy(&outgoing_beacon.u.collision_detected.originatingListenerUID, agencyUID2, SOO_AGENCY_UID_SIZE);

		break;

	case WNET_BEACON_ID_ABORT:
		outgoing_beacon.transID = 0;

		memcpy(&outgoing_beacon.u.abort.speakerUID, agencyUID1, SOO_AGENCY_UID_SIZE);
		memcpy(&outgoing_beacon.u.abort.originatingListenerUID, agencyUID2, SOO_AGENCY_UID_SIZE);

		break;

	case WNET_BEACON_ID_RESUME:
		outgoing_beacon.transID = 0;

		memcpy(&outgoing_beacon.u.resume.speakerUID, agencyUID1, SOO_AGENCY_UID_SIZE);
		memcpy(&outgoing_beacon.u.resume.originatingListenerUID, agencyUID2, SOO_AGENCY_UID_SIZE);

		break;

	default:
		lprintk("Invalid beacon ID\n");
		BUG();
	}

	memcpy(outgoing_packet->payload, &outgoing_beacon, sizeof(wnet_beacon_t));

	/* Enforce the use of the a known Soolink descriptor */
	if (likely(tx_rx_data.sl_desc)) {
		if (dest_agencyUID != NULL)
			memcpy(&tx_rx_data.sl_desc->agencyUID_to, dest_agencyUID, SOO_AGENCY_UID_SIZE);
		else
			memcpy(&tx_rx_data.sl_desc->agencyUID_to, get_null_agencyUID(), SOO_AGENCY_UID_SIZE);

		sender_tx(tx_rx_data.sl_desc, outgoing_packet, sizeof(wnet_beacon_t), 0);
	}

	/* Release the outgoing packet */
	kfree(outgoing_packet);
}

/**
 * Monitors the medium during a fixed delay to know if the medium is free, or in use.
 */
static bool is_medium_free(void) {
	nanosecs_rel_t dfree = WNET_TIME_US_TO_NS(WNET_TMEDIUM_FREE);
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

/**************************** Winenet FSM *****************************/

/**
 * IDLE state.
 */
static void winenet_state_idle(wnet_state_t old_state) {
	wnet_tx_rx_data_t tx_rx_data;
	wnet_beacon_t last_beacon;
	bool tx_request_pending;
	nanosecs_rel_t tmedium_free = WNET_TIME_US_TO_NS(WNET_TMEDIUM_FREE);

	DBG("Idle\n");

	while (1) {
		/*
		 * The IDLE state has no timeout to avoid overloading with useless activity.
		 * If we need to check for pending TX, the previous state has to send an event to
		 * wake the thread and to check the situation.
		 */

		rtdm_event_wait(&wnet_event);

		get_tx_rx_data(&tx_rx_data);
		get_last_beacon(&last_beacon);

		rtdm_mutex_lock(&wnet_tx_request_pending_lock);
		tx_request_pending = wnet_tx_request_pending;
		rtdm_mutex_unlock(&wnet_tx_request_pending_lock);

		/* Reset the peer agencyUID. sl_desc should not be NULL. */
		memcpy(&tx_rx_data.sl_desc->agencyUID_to, get_null_agencyUID(), SOO_AGENCY_UID_SIZE);

		/* We give the priority to event issued from a potential speaker. Indeed, the medium has been requested
		 * by another smart object in this case, therefore we do not attempt to send our stuff now.
		 */
		if (last_beacon.id == WNET_BEACON_ID_MEDIUM_REQUEST) {
			/* Medium request beacon received */

			/* Save the agency UID of the current Speaker */
			memcpy(&speakerUID, &last_beacon.u.medium_request.speakerUID, SOO_AGENCY_UID_SIZE);

			/* Save the nominated active Listener UID */

			/* In case of SL_MODE_UNIBROAD, this listener which receives the beacon IS the active listener (the
			 * other smart objects do not hear about anything (underlying unicast mode).
			 */

			DBG("5: Medium request beacon received -> Listener\n");
			WNET_SHORT_DBG("5");

			if (tx_rx_data.sl_desc->trans_mode == SL_MODE_UNIBROAD) {
				memcpy(&nominatedActiveListenerUID, get_my_agencyUID(), SOO_AGENCY_UID_SIZE);
				winenet_send_beacon(WNET_BEACON_ID_ACKNOWLEDGMENT, &speakerUID, &speakerUID, get_my_agencyUID(), 0);

			} else {
				memcpy(&nominatedActiveListenerUID, &last_beacon.u.medium_request.listenerUID, SOO_AGENCY_UID_SIZE);

				if (isActiveListener(tx_rx_data.sl_desc->trans_mode))
					winenet_send_beacon(WNET_BEACON_ID_ACKNOWLEDGMENT, NULL, &speakerUID, get_my_agencyUID(), 0);
			}

			winenet_change_state(WNET_STATE_LISTENER);

			return ;
		}

		/* unibroad: if we are joining a group, we will take part of the turnover by the speaker. Be ready to listen. */
		if ((tx_rx_data.sl_desc->trans_mode == SL_MODE_UNIBROAD) && tx_rx_data.data_received) {

			winenet_change_state(WNET_STATE_LISTENER);
			rtdm_event_signal(&wnet_event);

			return ;
		}

		/* React to a TX request or a pending data XMIT request */
		while (tx_request_pending || tx_rx_data.tx_pending) {

#if 0
			if (tx_rx_data.sl_desc->trans_mode == SL_MODE_UNIBROAD)
				memcpy(&speakerUID, get_my_agencyUID(), SOO_AGENCY_UID_SIZE);
#endif

			if (!is_medium_free())
				/* We suspend ourself during a certain time */
				rtdm_task_sleep(tmedium_free);

			/* The medium is free */
			DBG("1: Data TX request and free medium -> Speaker candidate\n");
			WNET_SHORT_DBG("1");

			/* Transition 1: Data TX request and free medium */
			winenet_change_state(WNET_STATE_SPEAKER_CANDIDATE);

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
	nanosecs_rel_t drand = WNET_TIME_US_TO_NS((get_random_int() % WNET_MAX_DRAND) + WNET_MIN_DRAND);
	bool tx_request_pending;
	int ret;

	DBG("Speaker candidate\n");

	while (1) {
		/* Delay of Drand us */
		ret = rtdm_event_timedwait(&wnet_event, drand, NULL);

		get_last_beacon(&last_beacon);
		get_tx_rx_data(&tx_rx_data);

		rtdm_mutex_lock(&wnet_tx_request_pending_lock);
		tx_request_pending = wnet_tx_request_pending;
		rtdm_mutex_unlock(&wnet_tx_request_pending_lock);

		if (ret == 0) {
			/* Data has been received or a Winenet beacon has been received */

			if (tx_rx_data.data_received) {
				/* Data packet received */

				rtdm_mutex_lock(&wnet_tx_rx_data_lock);
				wnet_tx_rx_data.data_received = false;
				rtdm_mutex_unlock(&wnet_tx_rx_data_lock);

				DBG("3: Data packet received -> Listener\n");
				WNET_SHORT_DBG("3a");

				winenet_change_state(WNET_STATE_LISTENER);

				return ;
			}

			if (last_beacon.id == WNET_BEACON_ID_MEDIUM_REQUEST) {
				/* Medium request beacon received */

				/* Save the agency UID of the current Speaker */
				memcpy(&speakerUID, &last_beacon.u.medium_request.speakerUID, SOO_AGENCY_UID_SIZE);

				/* Save the nominated active Listener UID */
				memcpy(&nominatedActiveListenerUID, &last_beacon.u.medium_request.listenerUID, SOO_AGENCY_UID_SIZE);

				DBG("3: Medium request beacon received -> Listener\n");
				WNET_SHORT_DBG("3b");

				if (tx_rx_data.sl_desc->trans_mode == SL_MODE_UNIBROAD)
					winenet_send_beacon(WNET_BEACON_ID_ACKNOWLEDGMENT, &speakerUID, &speakerUID, get_my_agencyUID(), 0);
				else {
					/* Is this Smart Object the Active Listener? */
					if (isActiveListener(tx_rx_data.sl_desc->trans_mode))
						winenet_send_beacon(WNET_BEACON_ID_ACKNOWLEDGMENT, NULL, &speakerUID, get_my_agencyUID(), 0);
				}

				winenet_change_state(WNET_STATE_LISTENER);

				return ;

			} else {

				/* Winenet beacon different from Medium request received */

				DBG("3: Winenet beacon different from Medium request received -> Listener\n");
				WNET_SHORT_DBG("3c");

				winenet_change_state(WNET_STATE_LISTENER);

				return ;
			}
		}
		else if (ret == -ETIMEDOUT) {
			/* The timeout has expired and no Medium request beacon has been received: we can now become Speaker */

			if (tx_request_pending) {
				rtdm_mutex_lock(&wnet_tx_request_pending_lock);
				wnet_tx_request_pending = false;
				rtdm_mutex_unlock(&wnet_tx_request_pending_lock);

				rtdm_event_signal(&wnet_tx_request_event);
			}

			neighbour_list_protection(true);

			/* Get the next active listener */
			/* We keep a reference to the initial listener for unibroad mode; without interest in other modes.*/
			if (unibroad_current_listener != NULL)
				unibroad_initial_listener = winenet_next_neighbour_rr(&unibroad_current_listener->neighbour->agencyUID, &activeListenerUID);
			unibroad_current_listener = unibroad_initial_listener;

			/* Are we alone in the neighbourhood? */
			if (!unibroad_initial_listener) {
				xmit_data_processed(-ENONET);

				DBG("4: No neighbour -> Idle\n");
				WNET_SHORT_DBG("4c");

				neighbour_list_protection(false);
				winenet_change_state(WNET_STATE_IDLE);

				return ;
			}

			DBG("Active Listener: ");
			DBG_BUFFER(&activeListenerUID, SOO_AGENCY_UID_SIZE);
#if 0
			memcpy(&speakerUID, get_my_agencyUID(), SOO_AGENCY_UID_SIZE);
#endif
			medium_request_pending = true;

			DBG("2: Drand delay expired -> Speaker\n");
			WNET_SHORT_DBG("2");

			/* Transition 2: Drand delay expired */
			winenet_change_state(WNET_STATE_SPEAKER);

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

static void give_relay_to_next_speaker(sl_desc_t *sl_desc) {
	agencyUID_t nextSpeakerUID;

	/* Clear the TX pkt buffer */
	clear_buf_tx_pkt();

	/* Reset the TX trans ID */
	sent_packet_transID = 0;

	if (!winenet_next_neighbour_rr(&prevSpeakerUID, &nextSpeakerUID)) {
		xmit_data_processed(0);
		retry_count = 0;

		/* There is no neighbour. Go to the IDLE state. */
		DBG("?: No neighbour -> Idle\n");
		WNET_SHORT_DBG("?");

		neighbour_list_protection(false);

		winenet_change_state(WNET_STATE_IDLE);

		return ;
	}
	memcpy(&prevSpeakerUID, &nextSpeakerUID, SOO_AGENCY_UID_SIZE);

	if (sl_desc->trans_mode == SL_MODE_UNIBROAD) {

		lprintk("*************** NEXT speaker: "); lprintk_buffer(&nextSpeakerUID, SOO_AGENCY_UID_SIZE);

		/* Now, transmit the frame to all listeners. */
		do {
			unibroad_current_listener = unibroad_next_listener(NULL);
			BUG_ON(!memcmp(unibroad_current_listener, get_null_agencyUID(), SOO_AGENCY_UID_SIZE));

			if (unibroad_current_listener->listener_in_group) {
				/* Update the active listener UID we are transmitting with. */
				memcpy(&activeListenerUID, &unibroad_current_listener->neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

				winenet_send_beacon(WNET_BEACON_ID_TRANSMISSION_COMPLETED, &unibroad_current_listener->neighbour->agencyUID, get_my_agencyUID(), &nextSpeakerUID, 0);

				unibroad_current_listener->listener_in_group = false;
			}

		} while (unibroad_current_listener != unibroad_initial_listener);
	} else
		winenet_send_beacon(WNET_BEACON_ID_TRANSMISSION_COMPLETED, NULL, get_my_agencyUID(), &nextSpeakerUID, 0);

	DBG("Next Speaker: ");
	DBG_BUFFER(&nextSpeakerUID, SOO_AGENCY_UID_SIZE);

	retry_count = 0;

	DBG("Acknowledgment beacon received and matching Speaker/Listener UID and medium release requested and matching trans ID -> Listener\n");
	WNET_SHORT_DBG("20b");

	memcpy(&speakerUID, get_null_agencyUID(), SOO_AGENCY_UID_SIZE);

	neighbour_list_protection(false);

	winenet_change_state(WNET_STATE_LISTENER);

	return ;

}

/**
 * SPEAKER state.
 * A data packet is being sent in this state.
 */
static void winenet_state_speaker(wnet_state_t old_state) {
	wnet_tx_rx_data_t tx_rx_data;
	wnet_beacon_t last_beacon;
	nanosecs_rel_t tspeaker_xmit = WNET_TIME_US_TO_NS(WNET_TSPEAKER_XMIT);
	int ret, i;

	DBG("Speaker\n");

	while (1) {

		/* Timeout of Tspeaker us */
		ret = rtdm_event_timedwait(&wnet_event, tspeaker_xmit, NULL);

		get_tx_rx_data(&tx_rx_data);
		get_last_beacon(&last_beacon);

		/*
		 * There are three cases in which we have to enter in Speaker wait ACK state:
		 * - This SOO was in Speaker candidate state and it has decided to become Speaker. There is a
		 *   medium request in progress, provoked by the TX request.
		 * - This is the last packet of a frame (n pkt/1 ACK strategy). Wait for an ACK only for the
		 *   last packet of the frame.
		 * - This is the last packet of the block (completed is true). Wait for an ACK for this packet.
		 */

		if (ret == 0) {
			if (medium_request_pending) {
				/* A Medium request beacon is being sent */

				if (tx_rx_data.sl_desc->trans_mode == SL_MODE_UNIBROAD) {

					/* Get the next neighbour from the list (the initial is processed at the end). */
					unibroad_current_listener = unibroad_next_listener(NULL);

					/* Update the active listener UID we are transmitting with. */
					memcpy(&activeListenerUID, &unibroad_current_listener->neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

					winenet_send_beacon(WNET_BEACON_ID_MEDIUM_REQUEST, &activeListenerUID, get_my_agencyUID(), &activeListenerUID, 0);
				} else
					winenet_send_beacon(WNET_BEACON_ID_MEDIUM_REQUEST, NULL, get_my_agencyUID(), &activeListenerUID, 0);

				DBG("12: Medium request beacon sent -> Speaker wait ACK\n");
				WNET_SHORT_DBG("12a");

				winenet_change_state(WNET_STATE_SPEAKER_WAIT_ACK);
				return ;
			}

			if (tx_rx_data.tx_pending) {
				/* Data is being sent */

				rtdm_mutex_lock(&wnet_tx_rx_data_lock);
				if (tx_rx_data.sl_desc->trans_mode == SL_MODE_UNIBROAD) {
					/* Set the destination */
					memcpy(&tx_rx_data.sl_desc->agencyUID_to, &unibroad_current_listener->neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

					/* unibroad mode: we have to transmit over all smart objects */
					if ((unibroad_current_listener != unibroad_initial_listener) &&
							unibroad_current_listener->listener_in_group) {

						for (i = 0; ((i < WNET_N_PACKETS_IN_FRAME) && (buf_tx_pkt[i] != NULL)); i++)
							sender_tx(tx_rx_data.sl_desc, buf_tx_pkt[i], buf_tx_pkt[i]->size, 0);

						DBG("12: Data sent -> Speaker wait ACK\n");

						rtdm_mutex_unlock(&wnet_tx_rx_data_lock);

						/* Transition 12: Data sent */
						winenet_change_state(WNET_STATE_SPEAKER_WAIT_ACK);

						return ;
					}
				}

				/* Copy the packet into the bufferized packet array */
				if (unlikely(buf_tx_pkt[(tx_rx_data.tx_packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME])) {
					DBG("TX buffer already populated: %d, %d\n", tx_rx_data.tx_packet->transID, (tx_rx_data.tx_packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME);

					clear_buf_tx_pkt();
				}

				buf_tx_pkt[(tx_rx_data.tx_packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME] = (transceiver_packet_t *) kmalloc(tx_rx_data.tx_packet->size + sizeof(transceiver_packet_t), GFP_ATOMIC);
				memcpy(buf_tx_pkt[(tx_rx_data.tx_packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME], tx_rx_data.tx_packet, tx_rx_data.tx_packet->size + sizeof(transceiver_packet_t));

				rtdm_mutex_unlock(&wnet_tx_rx_data_lock);

				/* Propagate the data packet to the lower layers */
				sender_tx(tx_rx_data.sl_desc, tx_rx_data.tx_packet, tx_rx_data.tx_size, 0);

				if (((tx_rx_data.tx_packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME == WNET_N_PACKETS_IN_FRAME-1) || (tx_rx_data.tx_packet->transID & WNET_LAST_PACKET)) {

					DBG("12: Data sent -> Speaker wait ACK\n");
					/* Be careful when debugging! This transition induces high verbosity. */
					//WNET_SHORT_DBG("12b");

					/* Transition 12: Data sent */
					winenet_change_state(WNET_STATE_SPEAKER_WAIT_ACK);

					return ;

				} else {

					/* Allow the producer to go further, as the frame is not complete yet */
					xmit_data_processed(0);

					continue;
				}
			} else {
				give_relay_to_next_speaker(tx_rx_data.sl_desc);

				return ;
			}

		} else if (ret == -ETIMEDOUT) {

			/* Clear the TX pkt buffer */
			clear_buf_tx_pkt();

			/* Reset the TX trans ID */
			sent_packet_transID = 0;

			xmit_data_processed(-ETIMEDOUT);

			DBG("6: Speaker XMIT timeout -> Idle\n");
			WNET_SHORT_DBG("6");

			neighbour_list_protection(false);

			/* Transition 6: Tspeaker timeout */
			winenet_change_state(WNET_STATE_IDLE);

			return ;

		} else {

			/* Unexpected! */
			lprintk("Unexpected ret value: %d\n", ret);
			BUG();

			return ;
		}
	}
}

/**
 * SPEAKER Wait ACK state.
 */
static void winenet_state_speaker_wait_ack(wnet_state_t old_state) {
	wnet_tx_rx_data_t tx_rx_data;
	wnet_beacon_t last_beacon;
	nanosecs_rel_t tspeaker_ack = WNET_TIME_US_TO_NS(WNET_TSPEAKER_ACK);
	agencyUID_t nextSpeakerUID;
	int ret;

	DBG("Speaker wait ACK\n");

	while (1) {
		/* Timeout of Tspeaker us */
		ret = rtdm_event_timedwait(&wnet_event, tspeaker_ack, NULL);

		get_tx_rx_data(&tx_rx_data);
		get_last_beacon(&last_beacon);

		if (ret == 0) {
			if (medium_request_pending) {
				if ((last_beacon.id == WNET_BEACON_ID_ACKNOWLEDGMENT) &&
					(!memcmp(&last_beacon.u.acknowledgment.speakerUID, get_my_agencyUID(), SOO_AGENCY_UID_SIZE)) &&
					(!memcmp(&last_beacon.u.acknowledgment.listenerUID, &activeListenerUID, SOO_AGENCY_UID_SIZE))) {

					retry_count = 0;

					if (tx_rx_data.sl_desc->trans_mode == SL_MODE_UNIBROAD) {
						unibroad_current_listener->listener_in_group = true;

						/* Check if we have finished the turn of neighbours. */
						if (unibroad_current_listener == unibroad_initial_listener)
							medium_request_pending = false;
					} else
						medium_request_pending = false;

					DBG("13: Acknowledgment beacon received and matching Speaker/Listener UID and no medium release requested and matching trans ID -> Speaker\n");
					WNET_SHORT_DBG("13a");

					memcpy(&speakerUID, get_my_agencyUID(), SOO_AGENCY_UID_SIZE);

					/* Transition 13: Acknowledgment beacon received and matching Speaker/Listener UID and no medium release requested and matching trans ID */
					winenet_change_state(WNET_STATE_SPEAKER);

					/* As we are going back to the Speaker state, we send a signal to process the TX request */
					rtdm_event_signal(&wnet_event);

					return ;
				}
			}

			if ((last_beacon.id == WNET_BEACON_ID_ACKNOWLEDGMENT) &&
				(!memcmp(&last_beacon.u.acknowledgment.speakerUID, get_my_agencyUID(), SOO_AGENCY_UID_SIZE)) &&
				(!memcmp(&last_beacon.u.acknowledgment.listenerUID, &activeListenerUID, SOO_AGENCY_UID_SIZE)) &&
				(last_beacon.transID == (tx_rx_data.tx_transID & WNET_MAX_PACKET_TRANSID)) &&
				(tx_rx_data.tx_completed)) {

				/* Unibroad mode: now proceed with the transmission of the frame to other SOOs in the neighbours. */
				if (tx_rx_data.sl_desc->trans_mode == SL_MODE_UNIBROAD) {

					/* Now, transmit the frame to the next listener. */
					do {
						unibroad_current_listener = unibroad_next_listener(NULL);
					} while (!unibroad_current_listener->listener_in_group && (unibroad_current_listener != unibroad_initial_listener));

					/* Update the active listener UID we are transmitting with. */
					memcpy(&activeListenerUID, &unibroad_current_listener->neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

					if (unibroad_current_listener != unibroad_initial_listener) {

						/* Now proceed with the transmission to other neighbours */
						winenet_change_state(WNET_STATE_SPEAKER);

						rtdm_event_signal(&wnet_event);

						return ;

					}

					/* unibroad turn over: unibroad_current_listener == unibroad_initial_listener */
				}

				/* Clear the TX pkt buffer */
				clear_buf_tx_pkt();

				/* Reset the TX trans ID */
				sent_packet_transID = 0;

				if (!winenet_next_neighbour_rr(&prevSpeakerUID, &nextSpeakerUID)) {
					xmit_data_processed(0);
					retry_count = 0;

					/* There is no neighbour. Go to the IDLE state. */
					DBG("?: No neighbour -> Idle\n");
					WNET_SHORT_DBG("?");

					neighbour_list_protection(false);

					winenet_change_state(WNET_STATE_IDLE);

					return ;
				}
				memcpy(&prevSpeakerUID, &nextSpeakerUID, SOO_AGENCY_UID_SIZE);

				if (tx_rx_data.sl_desc->trans_mode == SL_MODE_UNIBROAD) {

					lprintk("*************** NEXT speaker: "); lprintk_buffer(&nextSpeakerUID, SOO_AGENCY_UID_SIZE);

					/* Now, transmit the frame to all listeners. */
					do {
						unibroad_current_listener = unibroad_next_listener(NULL);
						BUG_ON(!memcmp(unibroad_current_listener, get_null_agencyUID(), SOO_AGENCY_UID_SIZE));

						if (unibroad_current_listener->listener_in_group) {
							/* Update the active listener UID we are transmitting with. */
							memcpy(&activeListenerUID, &unibroad_current_listener->neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

							winenet_send_beacon(WNET_BEACON_ID_TRANSMISSION_COMPLETED, &unibroad_current_listener->neighbour->agencyUID, get_my_agencyUID(), &nextSpeakerUID, 0);

							unibroad_current_listener->listener_in_group = false;
						}

					} while (unibroad_current_listener != unibroad_initial_listener);
				} else
					winenet_send_beacon(WNET_BEACON_ID_TRANSMISSION_COMPLETED, NULL, get_my_agencyUID(), &nextSpeakerUID, 0);

				DBG("Next Speaker: ");
				DBG_BUFFER(&nextSpeakerUID, SOO_AGENCY_UID_SIZE);

				xmit_data_processed(0);
				retry_count = 0;

				DBG("Acknowledgment beacon received and matching Speaker/Listener UID and medium release requested and matching trans ID -> Listener\n");
				WNET_SHORT_DBG("20b");

				memcpy(&speakerUID, &nextSpeakerUID, SOO_AGENCY_UID_SIZE);
				//memcpy(&speakerUID, get_null_agencyUID(), SOO_AGENCY_UID_SIZE);

				neighbour_list_protection(false);

				winenet_change_state(WNET_STATE_LISTENER);

				return ;
			}

			if ((last_beacon.id == WNET_BEACON_ID_ACKNOWLEDGMENT) &&
				(!memcmp(&last_beacon.u.acknowledgment.speakerUID, get_my_agencyUID(), SOO_AGENCY_UID_SIZE)) &&
				(!memcmp(&last_beacon.u.acknowledgment.listenerUID, &activeListenerUID, SOO_AGENCY_UID_SIZE)) &&
				(last_beacon.transID == (tx_rx_data.tx_transID & WNET_MAX_PACKET_TRANSID))) {

				if (tx_rx_data.sl_desc->trans_mode == SL_MODE_UNIBROAD) {

					/* Now, transmit the frame to the next listener. */
					unibroad_current_listener = unibroad_next_listener(NULL);

					/* Update the active listener UID we are transmitting with. */
					memcpy(&activeListenerUID, &unibroad_current_listener->neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

					if (unibroad_current_listener != unibroad_initial_listener) {
						winenet_change_state(WNET_STATE_SPEAKER);

						rtdm_event_signal(&wnet_event);
						return ;

					 }
				}

				/* Clear the TX pkt buffer */
				clear_buf_tx_pkt();
				xmit_data_processed(0);

				retry_count = 0;

				DBG("13: Acknowledgment beacon received and matching Speaker/Listener UID and no medium release requested and matching trans ID, data -> Speaker\n");
				/* Be careful when debugging! This transition induces high verbosity. */
				//WNET_SHORT_DBG("13b");

				/* Transition 13: Acknowledgment beacon received and matching Speaker/Listener UID and no medium release requested and matching trans ID, data */
				winenet_change_state(WNET_STATE_SPEAKER);

				return ;
			}

			if (last_beacon.id == WNET_BEACON_ID_COLLISION_DETECTED) {
				DBG("14: Collision detected beacon received -> Speaker suspended\n");
				WNET_SHORT_DBG("14");

				/* Transition 14: Collision detected beacon received */
				winenet_change_state(WNET_STATE_SPEAKER_SUSPENDED);

				return ;
			}

			if (last_beacon.id == WNET_BEACON_ID_MEDIUM_REQUEST) {
				/* Medium request beacon received */

				if ((tx_rx_data.sl_desc->trans_mode == SL_MODE_UNIBROAD) && (listeners_in_group() > last_beacon.transID)) {

					winenet_send_beacon(WNET_BEACON_ID_MEDIUM_REQUEST, &activeListenerUID, get_my_agencyUID(), &activeListenerUID, 0);
					continue;
				}

				/* Clear the TX pkt buffer */
				clear_buf_tx_pkt();

				/* Reset the TX trans ID */
				sent_packet_transID = 0;

				/* Since we are becoming a listener, we reject any TX attempt for the moment. */
				xmit_data_processed(-EIO);

				retry_count = 0;

				/* Save the agency UID of the current Speaker */
				memcpy(&speakerUID, &last_beacon.u.medium_request.speakerUID, SOO_AGENCY_UID_SIZE);

				/* Save the nominated active Listener UID */
				memcpy(&nominatedActiveListenerUID, &last_beacon.u.medium_request.listenerUID, SOO_AGENCY_UID_SIZE);

				/* Remove all neighbours from the group */
				listeners_reset();

				DBG("Beacon: "); DBG_BUFFER(&last_beacon.u.dummy.speakerUID, SOO_AGENCY_UID_SIZE);
				DBG("Expected: "); DBG_BUFFER(&speakerUID, SOO_AGENCY_UID_SIZE);

				DBG("20: Medium request beacon received -> Listener\n");
				WNET_SHORT_DBG("20a");

				/* Is this Smart Object the Active Listener? */
				if (isActiveListener(tx_rx_data.sl_desc->trans_mode)) {
					if (tx_rx_data.sl_desc->trans_mode == SL_MODE_UNIBROAD)
						winenet_send_beacon(WNET_BEACON_ID_ACKNOWLEDGMENT, &unibroad_current_listener->neighbour->agencyUID, &speakerUID, get_my_agencyUID(), 0);
					else
						winenet_send_beacon(WNET_BEACON_ID_ACKNOWLEDGMENT, NULL, &speakerUID, get_my_agencyUID(), 0);
				}
				neighbour_list_protection(false);
				winenet_change_state(WNET_STATE_LISTENER);

				return ;
			}

			if (last_beacon.id == WNET_BEACON_ID_TRANSMISSION_COMPLETED) {
				/* Transmission completed beacon received */

				/* Clear the TX pkt buffer */
				clear_buf_tx_pkt();

				/* Reset the TX trans ID */
				sent_packet_transID = 0;

				xmit_data_processed(-EIO);
				retry_count = 0;

				DBG("Beacon: "); DBG_BUFFER(&last_beacon.u.dummy.speakerUID, SOO_AGENCY_UID_SIZE);
				DBG("Expected: "); DBG_BUFFER(&speakerUID, SOO_AGENCY_UID_SIZE);

				DBG("20: Transmission completed beacon received -> Listener\n");
				WNET_SHORT_DBG("20c");

				memcpy(&speakerUID, get_null_agencyUID(), SOO_AGENCY_UID_SIZE);

				neighbour_list_protection(false);

				winenet_change_state(WNET_STATE_LISTENER);

				return ;
			}

		}
		else if (ret == -ETIMEDOUT) {
			/* The timeout has expired */
			DBG("15: No Acknowledgment beacon received and Tspeaker timeout expired -> Speaker send retry\n");
			WNET_SHORT_DBG("15");

			/* Transition 15: No Acknowledgment beacon received and Tspeaker timeout expired */
			winenet_change_state(WNET_STATE_SPEAKER_RETRY);
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
	agencyUID_t nextSpeakerUID;

	DBG("Speaker retry %d\n", retry_count);

	get_tx_rx_data(&tx_rx_data);

	if (retry_count == WNET_RETRIES_MAX) {

		if (tx_rx_data.sl_desc->trans_mode == SL_MODE_UNIBROAD) {

			unibroad_current_listener->listener_in_group = false;

			if (medium_request_pending) {

				/* Is it the initial which fails to answer the request? In this case, we re-position the initial to a valid
				 * listener. Do not forget, if it is the initial, we have already questioned the other listeners.
				 */

				if (unibroad_current_listener == unibroad_initial_listener) {

					xmit_data_processed(-EIO);
					retry_count = 0;
					medium_request_pending = false;
					memcpy(&speakerUID, get_null_agencyUID(), SOO_AGENCY_UID_SIZE);

					/* Clear the TX pkt buffer */
					clear_buf_tx_pkt();

					/* Reset the TX trans ID */
					sent_packet_transID = 0;

					neighbour_list_protection(false);
					winenet_change_state(WNET_STATE_IDLE);

					return ;

				} else {
					/* Here, we pursue our turn of listeners */

					unibroad_current_listener = unibroad_next_listener(NULL);

					winenet_change_state(WNET_STATE_SPEAKER);
					rtdm_event_signal(&wnet_event);
				}


			} else { 	/* Other case than medium request */

				if (listeners_in_group() == 0) {
					xmit_data_processed(-EIO);
					retry_count = 0;
					medium_request_pending = false;
					memcpy(&speakerUID, get_null_agencyUID(), SOO_AGENCY_UID_SIZE);

					/* Clear the TX pkt buffer */
					clear_buf_tx_pkt();

					/* Reset the TX trans ID */
					sent_packet_transID = 0;

					neighbour_list_protection(false);
					winenet_change_state(WNET_STATE_IDLE);

					return ;

				} else {

					/* Is it the initial which fails to acknowledge? In this case, we re-position the initial to a valid
					 * listener.
					 */
					if (unibroad_current_listener == unibroad_initial_listener) {

						while (!unibroad_current_listener->listener_in_group)
							unibroad_current_listener = unibroad_next_listener(NULL);
						unibroad_initial_listener = unibroad_current_listener;

					} else {
						/* Here, we pursue our turn of listeners */

						unibroad_current_listener = unibroad_next_listener(NULL);
						winenet_change_state(WNET_STATE_SPEAKER);
						rtdm_event_signal(&wnet_event);
					}
				}
			}
		}

		/* Clear the TX pkt buffer */
		clear_buf_tx_pkt();

		/* Reset the TX trans ID */
		sent_packet_transID = 0;

		if (!winenet_next_neighbour_rr(&prevSpeakerUID, &nextSpeakerUID)) {
			/* There is no neighbour. Go to the IDLE state. */

			xmit_data_processed(-EIO);
			retry_count = 0;

			DBG("?: No neighbour -> Idle\n");
			WNET_SHORT_DBG("?");

			neighbour_list_protection(false);
			winenet_change_state(WNET_STATE_IDLE);

			return ;
		}

		memcpy(&prevSpeakerUID, &nextSpeakerUID, SOO_AGENCY_UID_SIZE);

		DBG("Next Speaker: ");
		DBG_BUFFER(&nextSpeakerUID, SOO_AGENCY_UID_SIZE);

		if (tx_rx_data.sl_desc->trans_mode == SL_MODE_UNIBROAD) {
			winenet_send_beacon(WNET_BEACON_ID_TRANSMISSION_COMPLETED, &nextSpeakerUID, get_my_agencyUID(), &nextSpeakerUID, 0);

			/* Set the new agencyUID - As soon as we get some */
			memcpy(&tx_rx_data.sl_desc->agencyUID_to, get_null_agencyUID(), SOO_AGENCY_UID_SIZE);
			memcpy(&speakerUID, get_null_agencyUID(), SOO_AGENCY_UID_SIZE);
		} else
			winenet_send_beacon(WNET_BEACON_ID_TRANSMISSION_COMPLETED, NULL, get_my_agencyUID(), &nextSpeakerUID, 0);

		xmit_data_processed(-EIO);
		retry_count = 0;

		DBG("Retry data/Medium request beacon sent and maximal number of retries reached -> Listener\n");
		WNET_SHORT_DBG("19");
		neighbour_list_protection(false);

		winenet_change_state(WNET_STATE_LISTENER);

		return ;
	}

	if (medium_request_pending) {
		/* A Medium request has been sent and we are waiting for the ACK */

		if (tx_rx_data.sl_desc->trans_mode == SL_MODE_UNIBROAD)
			/* Set the destination */
			winenet_send_beacon(WNET_BEACON_ID_MEDIUM_REQUEST, &unibroad_current_listener->neighbour->agencyUID,
				get_my_agencyUID(), &activeListenerUID, 0);
		else
			winenet_send_beacon(WNET_BEACON_ID_MEDIUM_REQUEST, NULL, get_my_agencyUID(), &activeListenerUID, 0);
	} else {
		/* A data frame is being processed */

		rtdm_mutex_lock(&wnet_tx_rx_data_lock);

		if (tx_rx_data.sl_desc->trans_mode == SL_MODE_UNIBROAD)
			/* Set the destination */
			memcpy(&tx_rx_data.sl_desc->agencyUID_to, &unibroad_current_listener->neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

		/* Re-send all the packets of the frame */
		for (i = 0; ((i < WNET_N_PACKETS_IN_FRAME) && (buf_tx_pkt[i] != NULL)); i++)
			sender_tx(tx_rx_data.sl_desc, buf_tx_pkt[i], buf_tx_pkt[i]->size, 0);

		rtdm_mutex_unlock(&wnet_tx_rx_data_lock);
	}

	retry_count++;

	DBG("18: Retry data/Medium request beacon sent and maximal number of retries not reached\n");
	WNET_SHORT_DBG("18");

	/* Transition 18: Retry data/Medium request beacon sent and maximal number of retries not reached */
	winenet_change_state(WNET_STATE_SPEAKER_WAIT_ACK);
}

/**
 * SPEAKER Suspended state.
 */
static void winenet_state_speaker_suspended(wnet_state_t old_state) {
	wnet_beacon_t last_beacon;
	wnet_tx_rx_data_t tx_rx_data;
	nanosecs_rel_t tcollision = WNET_TIME_US_TO_NS(WNET_TCOLLISION);
	int ret;

	DBG("Speaker suspended\n");

	while (1) {
		/* Timeout of Tcollision us */
		ret = rtdm_event_timedwait(&wnet_event, tcollision, NULL);

		get_tx_rx_data(&tx_rx_data);
		get_last_beacon(&last_beacon);

		if (ret == 0) {
			if (last_beacon.id == WNET_BEACON_ID_ABORT) {
				/*
				 * Abort beacon received. This Speaker must stop sending anything.
				 * Go to LISTENER state.
				 */

				/* Clear the TX pkt buffer */
				clear_buf_tx_pkt();

				/* Reset the TX trans ID */
				sent_packet_transID = 0;

				xmit_data_processed(0);
				retry_count = 0;

				DBG("11: Abort beacon received -> Listener\n");
				WNET_SHORT_DBG("11a");

				neighbour_list_protection(false);

				if (tx_rx_data.sl_desc->trans_mode == SL_MODE_UNIBROAD) {
					/* Reset the peer agencyUID */
					memcpy(&tx_rx_data.sl_desc->agencyUID_to, get_null_agencyUID(), SOO_AGENCY_UID_SIZE);

					memcpy(&speakerUID, get_null_agencyUID(), SOO_AGENCY_UID_SIZE);
				}

				winenet_change_state(WNET_STATE_LISTENER);

				return ;
			}

			if (last_beacon.id == WNET_BEACON_ID_RESUME) {
				/* Resume beacon received. This Speaker is allowed to resume sending data. */

				DBG("17: Resume beacon received -> Speaker\n");
				WNET_SHORT_DBG("17");

				/* Transition 17: Resume beacon received */
				winenet_change_state(WNET_STATE_SPEAKER);

				return ;
			}

			if (((last_beacon.id == WNET_BEACON_ID_ACKNOWLEDGMENT) &&
					(memcmp(&last_beacon.u.acknowledgment.speakerUID, &speakerUID, SOO_AGENCY_UID_SIZE))) ||
				((last_beacon.id == WNET_BEACON_ID_TRANSMISSION_COMPLETED) &&
					(memcmp(&last_beacon.u.transmission_completed.speakerUID, &speakerUID, SOO_AGENCY_UID_SIZE)))) {
				/*
				 * Acknowledgment of Transmission completed beacon received from another Speaker.
				 * Go back to IDLE state.
				 */

				/* Clear the TX pkt buffer */
				clear_buf_tx_pkt();

				/* Reset the TX trans ID */
				sent_packet_transID = 0;

				xmit_data_processed(0);
				retry_count = 0;

				DBG("Beacon: "); DBG_BUFFER(&last_beacon.u.dummy.speakerUID, SOO_AGENCY_UID_SIZE);
				DBG("Expected: "); DBG_BUFFER(&speakerUID, SOO_AGENCY_UID_SIZE);

				DBG("11: Beacon received from another Speaker -> Idle\n");
				WNET_SHORT_DBG("11b");

				memcpy(&speakerUID, get_null_agencyUID(), SOO_AGENCY_UID_SIZE);

				neighbour_list_protection(false);

				winenet_change_state(WNET_STATE_LISTENER);

				return ;
			}

			if ((last_beacon.id == WNET_BEACON_ID_MEDIUM_REQUEST) &&
				(memcmp(&last_beacon.u.medium_request.speakerUID, &speakerUID, SOO_AGENCY_UID_SIZE))) {

				/*
				 * Medium request beacon received from another Speaker.
				 * Go to LISTENER state.
				 */

				/* Clear the TX pkt buffer */
				clear_buf_tx_pkt();

				/* Reset the TX trans ID */
				sent_packet_transID = 0;

				xmit_data_processed(0);
				retry_count = 0;

				DBG("Beacon: "); DBG_BUFFER(&last_beacon.u.dummy.speakerUID, SOO_AGENCY_UID_SIZE);
				DBG("Expected: "); DBG_BUFFER(&speakerUID, SOO_AGENCY_UID_SIZE);

				DBG("11: Medium request beacon received from another Speaker -> Listener\n");
				WNET_SHORT_DBG("11c");

				/* Save the agency UID of the current Speaker */
				memcpy(&speakerUID, &last_beacon.u.medium_request.speakerUID, SOO_AGENCY_UID_SIZE);

				/* Save the nominated active Listener UID */
				memcpy(&nominatedActiveListenerUID, &last_beacon.u.medium_request.listenerUID, SOO_AGENCY_UID_SIZE);

				/* Is this Smart Object the Active Listener? */
				if (isActiveListener(tx_rx_data.sl_desc->trans_mode)) {
					if (tx_rx_data.sl_desc->trans_mode == SL_MODE_UNIBROAD)
						winenet_send_beacon(WNET_BEACON_ID_ACKNOWLEDGMENT, &unibroad_current_listener->neighbour->agencyUID,
							&speakerUID, get_my_agencyUID(), 0);
					else
						winenet_send_beacon(WNET_BEACON_ID_ACKNOWLEDGMENT, NULL, &speakerUID, get_my_agencyUID(), 0);
				}

				neighbour_list_protection(false);

				winenet_change_state(WNET_STATE_LISTENER);

				return ;
			}
		} else if (ret == -ETIMEDOUT) {
			/* Clear the TX pkt buffer */
			clear_buf_tx_pkt();

			/* Reset the TX trans ID */
			sent_packet_transID = 0;

			xmit_data_processed(0);
			retry_count = 0;

			DBG("16: Tcollision timeout expired -> Idle\n");
			WNET_SHORT_DBG("16");

			memcpy(&speakerUID, get_null_agencyUID(), SOO_AGENCY_UID_SIZE);

			neighbour_list_protection(false);

			/* Transition 16: Tcollision timeout expired */
			winenet_change_state(WNET_STATE_IDLE);

			return ;
		} else {
			/* Unexpected! */
			lprintk("Unexpected ret value: %d\n", ret);
			BUG();
		}
	}
}

/**
 * LISTENER state.
 */
static void winenet_state_listener(wnet_state_t old_state) {
	wnet_tx_rx_data_t tx_rx_data;
	wnet_beacon_t last_beacon;
	nanosecs_rel_t tlistener = WNET_TIME_US_TO_NS(WNET_TLISTENER);
	bool tx_request_pending;
	int ret;

	DBG("Listener.\n");

	while (1) {
		if (memcmp(&speakerUID, get_null_agencyUID(), SOO_AGENCY_UID_SIZE)) {
			lprintk("### Now paired with "); lprintk_buffer(&speakerUID, SOO_AGENCY_UID_SIZE);

	} else {
		lprintk("### NOOT PAIRED YET\n");

		winenet_change_state(WNET_STATE_IDLE);
		rtdm_event_signal(&wnet_event);
		return ;

	}
		/* Timeout of Tlistener us */
		ret = rtdm_event_timedwait(&wnet_event, tlistener, NULL);

		neighbour_list_protection(true);

		get_tx_rx_data(&tx_rx_data);
		get_last_beacon(&last_beacon);

		rtdm_mutex_lock(&wnet_tx_request_pending_lock);
		tx_request_pending = wnet_tx_request_pending;
		rtdm_mutex_unlock(&wnet_tx_request_pending_lock);

		if (ret == 0) {

			/*
			 * unibroad mode: we can be in the Listener state *without" any (paired) speaker; we have to reject all beacons which
			 * are addressed to us except the medium request from a speaker, in this case.
			 * Scenario: if another speaker tried to reach us with some repeated medium request, and we were not available at this time,
			 * we could received a TRANSMISSION_COMPLETED
			 */

			if (tx_rx_data.data_received) {
				/* Data has been received. Reset the proper flag. */

				rtdm_mutex_lock(&wnet_tx_rx_data_lock);
				wnet_tx_rx_data.data_received = false;
				rtdm_mutex_unlock(&wnet_tx_rx_data_lock);

				/*
				 * Send an ACKNOWLEDGMENT beacon only for data packets, and if this Smart Object is
				 * the Active Listener.
				 * The speakerUID has been retrieved during the medium request *or* during the receival of a
				 * transmission completed beacon; it is the SOO who is speaking now.
				 *
				 */

				if (tx_rx_data.sl_desc->trans_mode == SL_MODE_UNIBROAD) {

					if (!memcmp(&speakerUID, get_null_agencyUID(), SOO_AGENCY_UID_SIZE))
						memcpy(&speakerUID, &tx_rx_data.sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE);

					winenet_send_beacon(WNET_BEACON_ID_ACKNOWLEDGMENT, &speakerUID, &speakerUID, get_my_agencyUID(),
							tx_rx_data.rx_transID & WNET_MAX_PACKET_TRANSID);

				} else {
					if (isActiveListener(tx_rx_data.sl_desc->trans_mode))
						winenet_send_beacon(WNET_BEACON_ID_ACKNOWLEDGMENT, NULL, &speakerUID, get_my_agencyUID(), tx_rx_data.rx_transID & WNET_MAX_PACKET_TRANSID);
				}

				neighbour_list_protection(false);
				continue;
			}

			if (last_beacon.id == WNET_BEACON_ID_MEDIUM_REQUEST) {
				/* Medium request beacon received */

				/* Save the agency UID of the current Speaker. */

				/* unibroad mode: when a listener receives a medium request, it means that it will belong to a group (a speaker and
				 * several listeners).
				 */
				memcpy(&speakerUID, &last_beacon.u.medium_request.speakerUID, SOO_AGENCY_UID_SIZE);

				/* Save the nominated active Listener UID */
				memcpy(&nominatedActiveListenerUID, &last_beacon.u.medium_request.listenerUID, SOO_AGENCY_UID_SIZE);

				DBG("21: Medium request beacon received -> Listener\n");
				WNET_SHORT_DBG("21a");

				if (tx_rx_data.sl_desc->trans_mode == SL_MODE_UNIBROAD)
					winenet_send_beacon(WNET_BEACON_ID_ACKNOWLEDGMENT, &speakerUID, &speakerUID, get_my_agencyUID(), 0);
				else
					/* Is this Smart Object the Active Listener? */
					if (isActiveListener(tx_rx_data.sl_desc->trans_mode))
						winenet_send_beacon(WNET_BEACON_ID_ACKNOWLEDGMENT, NULL, &speakerUID, get_my_agencyUID(), 0);

				neighbour_list_protection(false);
				continue;
			}

			if (((last_beacon.id == WNET_BEACON_ID_ACKNOWLEDGMENT) &&
					(memcmp(&last_beacon.u.acknowledgment.speakerUID, &speakerUID, SOO_AGENCY_UID_SIZE) &&
					!((tx_rx_data.sl_desc->trans_mode == SL_MODE_UNIBROAD) && !memcmp(&speakerUID, get_null_agencyUID(), SOO_AGENCY_UID_SIZE)))) ||

				((last_beacon.id == WNET_BEACON_ID_TRANSMISSION_COMPLETED) &&
					memcmp(&last_beacon.u.transmission_completed.speakerUID, &speakerUID, SOO_AGENCY_UID_SIZE) &&
					!((tx_rx_data.sl_desc->trans_mode == SL_MODE_UNIBROAD) && !memcmp(&speakerUID, get_null_agencyUID(), SOO_AGENCY_UID_SIZE)))) {
				/* An Acknowledgment or Transmission completed beacon specifying an unexpected Speaker has been received */

				DBG("9: Acknowledgment or Transmission completed beacon received from an unexpected Speaker -> Collision\n");
				WNET_SHORT_DBG("9");

				/* Transition 9: Acknowledgment or Transmission completed beacon received from an unexpected Speaker */
				winenet_change_state(WNET_STATE_LISTENER_COLLISION);

				return ;
			}

			if (last_beacon.id == WNET_BEACON_ID_TRANSMISSION_COMPLETED) {
				if (!memcmp(&last_beacon.u.transmission_completed.nextSpeakerUID, get_my_agencyUID(), SOO_AGENCY_UID_SIZE)) {
					/* This SOO is nominated Next Speaker */
					DBG("I'm nominated Next Speaker !!\n");

					/* Save the prev Speaker agency UID */
					memcpy(&prevSpeakerUID, &last_beacon.u.transmission_completed.speakerUID, SOO_AGENCY_UID_SIZE);

					/* Will be updated when necessary (see below) */
					memcpy(&speakerUID, get_null_agencyUID(), SOO_AGENCY_UID_SIZE);

					if (tx_request_pending) {
						rtdm_mutex_lock(&wnet_tx_request_pending_lock);
						wnet_tx_request_pending = false;
						rtdm_mutex_unlock(&wnet_tx_request_pending_lock);

						rtdm_event_signal(&wnet_tx_request_event);

						/* Get the next active listener */
						unibroad_initial_listener = winenet_next_neighbour_rr(&prevSpeakerUID, &activeListenerUID);
						unibroad_current_listener = unibroad_initial_listener;
						if (!unibroad_current_listener) {
							DBG("8b: No neighbour -> Idle\n");
							WNET_SHORT_DBG("8b");
							neighbour_list_protection(false);

							winenet_change_state(WNET_STATE_IDLE);

							return ;
						}

						DBG("Active Listener: ");
						DBG_BUFFER(&activeListenerUID, SOO_AGENCY_UID_SIZE);
						memcpy(&speakerUID, get_my_agencyUID(), SOO_AGENCY_UID_SIZE);

						/* Reset the peer agencyUID */
						memcpy(&tx_rx_data.sl_desc->agencyUID_to, get_null_agencyUID(), SOO_AGENCY_UID_SIZE);

						medium_request_pending = true;

						DBG("8: Transmission completed beacon received and Next Speaker and TX request pending -> Speaker\n");
						WNET_SHORT_DBG("8");

						/* Transition 8: Transmission completed beacon received and Next Speaker and TX request pending */
						winenet_change_state(WNET_STATE_SPEAKER);

						/* We keep the neighborhood protected. */

						/* Inform the speaker that there is something to send */
						rtdm_event_signal(&wnet_event);

						return ;

					} else {

						DBG("21: Transmission completed beacon received and Next Speaker and no TX request pending -> Idle\n");
						WNET_SHORT_DBG("21b");

						/* Transmission completed beacon received and Next Speaker and no TX request pending, therefore
						 * we move to IDLE. */

						neighbour_list_protection(false);

						winenet_change_state(WNET_STATE_IDLE);

						return ;
					}

				} else {

					/* Transmission completed beacon received and not Next Speaker */

					/* This SOO is not nominated next Speaker */

					DBG("21: Transmission completed beacon received and not Next Speaker -> Listener\n");
					WNET_SHORT_DBG("21c");


					/* Update the speaker so that the listener state will wait to be contacted */
					//memcpy(&tx_rx_data.sl_desc->agencyUID_to, get_null_agencyUID(), SOO_AGENCY_UID_SIZE);

					memcpy(&tx_rx_data.sl_desc->agencyUID_to, &last_beacon.u.transmission_completed.nextSpeakerUID, SOO_AGENCY_UID_SIZE);
					memcpy(&speakerUID, &last_beacon.u.transmission_completed.nextSpeakerUID, SOO_AGENCY_UID_SIZE);

					//memcpy(&speakerUID, get_null_agencyUID(), SOO_AGENCY_UID_SIZE);

					neighbour_list_protection(false);
					continue;
				}
			}


		} else if (ret == -ETIMEDOUT) {
			/* The timeout has expired */
			DBG("7: Tlistener timeout expired -> Idle\n");
			WNET_SHORT_DBG("7");

			memcpy(&speakerUID, get_null_agencyUID(), SOO_AGENCY_UID_SIZE);

			neighbour_list_protection(false);

			/* Transition 7: Tlistener timeout expired */
			winenet_change_state(WNET_STATE_IDLE);

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

/**
 * LISTENER Collision state.
 */
static void winenet_state_listener_collision(wnet_state_t old_state) {
	nanosecs_rel_t drand = WNET_TIME_US_TO_NS((get_random_int() % WNET_MAX_DRAND) + WNET_MIN_DRAND);
	wnet_beacon_t last_beacon;
	wnet_tx_rx_data_t tx_rx_data;
	int ret;

	DBG("Listener collision\n");

	while (1) {
		/* Delay of Drand us */
		ret = rtdm_event_timedwait(&wnet_event, drand, NULL);

		get_last_beacon(&last_beacon);
		get_tx_rx_data(&tx_rx_data);

		if (ret == 0) {
			if ((last_beacon.id == WNET_BEACON_ID_COLLISION_DETECTED) ||
				(last_beacon.id == WNET_BEACON_ID_ABORT) ||
				(last_beacon.id == WNET_BEACON_ID_RESUME)) {
				/* A collision management beacon has been received */

				DBG("10: Collision management beacon received -> Listener\n");
				WNET_SHORT_DBG("10a");

				neighbour_list_protection(false);
				/* Transition 10: Winenet beacon received */
				winenet_change_state(WNET_STATE_LISTENER);

				return ;
			}
			else
				continue;

		} else if (ret == -ETIMEDOUT) {

			DBG("10: Drand delay expired -> Listener\n");
			WNET_SHORT_DBG("10b");

			if (tx_rx_data.sl_desc->trans_mode == SL_MODE_UNIBROAD) {
				winenet_send_beacon(WNET_BEACON_ID_COLLISION_DETECTED, &wrongSpeakerUID, get_null_agencyUID(), get_my_agencyUID(), 0);
				winenet_send_beacon(WNET_BEACON_ID_COLLISION_DETECTED, &speakerUID, get_null_agencyUID(), get_my_agencyUID(), 0);
			} else
				/* The first agency UID in the Collision detected beacon is not used */
				winenet_send_beacon(WNET_BEACON_ID_COLLISION_DETECTED, NULL, get_null_agencyUID(), get_my_agencyUID(), 0);

			if (tx_rx_data.sl_desc->trans_mode == SL_MODE_UNIBROAD)
				winenet_send_beacon(WNET_BEACON_ID_ABORT, &wrongSpeakerUID, &wrongSpeakerUID, get_my_agencyUID(), 0);
			else
				/* Abort the wrong Speaker */
				winenet_send_beacon(WNET_BEACON_ID_ABORT, NULL, &wrongSpeakerUID, get_my_agencyUID(), 0);

			if (tx_rx_data.sl_desc->trans_mode == SL_MODE_UNIBROAD)
				winenet_send_beacon(WNET_BEACON_ID_RESUME, &speakerUID, &speakerUID, get_my_agencyUID(), 0);
			else
				/* Resume the expected Speaker */
				winenet_send_beacon(WNET_BEACON_ID_RESUME, NULL, &speakerUID, get_my_agencyUID(), 0);

			neighbour_list_protection(false);

			/* Transition 10: Drand delay expired */
			winenet_change_state(WNET_STATE_LISTENER);

			return ;
		} else {
			/* Unexpected! */
			lprintk("Unexpected ret value: %d\n", ret);
			BUG();
		}
	}
}

/* FSM management */

/* FSM function table */
static wnet_state_fn_t wnet_functions[WNET_STATE_N] = {
	[WNET_STATE_IDLE]				= winenet_state_idle,
	[WNET_STATE_SPEAKER_CANDIDATE]			= winenet_state_speaker_candidate,
	[WNET_STATE_SPEAKER]				= winenet_state_speaker,
	[WNET_STATE_SPEAKER_WAIT_ACK]			= winenet_state_speaker_wait_ack,
	[WNET_STATE_SPEAKER_RETRY]			= winenet_state_speaker_retry,
	[WNET_STATE_SPEAKER_SUSPENDED]			= winenet_state_speaker_suspended,
	[WNET_STATE_LISTENER]				= winenet_state_listener,
	[WNET_STATE_LISTENER_COLLISION]			= winenet_state_listener_collision
};

/**
 * Change the state of Winenet.
 */
static void winenet_change_state(wnet_state_t new_state) {
	if (unlikely((wnet_state < 0) || (wnet_state >= WNET_STATE_N) ||
			(new_state < 0) || (new_state >= WNET_STATE_N))) {
		lprintk("Invalid state: %d -> %d\n", wnet_state, new_state);
		BUG();
	}

	lprintk(" ** winenet_change_state: %s -> %s              UID: \n", get_state_string(wnet_state), get_state_string(new_state));
	DBG_BUFFER(get_my_agencyUID(), SOO_AGENCY_UID_SIZE);

	wnet_old_state = wnet_state;
	wnet_state = new_state;

	rtdm_event_signal(&wnet_state_event);
}

/**
 * Main Winenet routine that implements the FSM.
 * At the beginning, we are in the IDLE state.
 */
static void winenet_fn(void *args) {
	while (1) {
		rtdm_event_wait(&wnet_state_event);

		/* Call the proper state function */
		(*wnet_functions[wnet_state])(wnet_old_state);
	}
}

/**
 * This function is called when a data packet or a Iamasoo beacon has to be sent.
 * The call is made by the Sender.
 */
static int winenet_xmit(sl_desc_t *sl_desc, transceiver_packet_t *packet, size_t size, bool completed) {
	int ret;

	/* Only one producer can call winenet_tx at a time */
	rtdm_mutex_lock(&wnet_xmit_lock);

	if (unlikely(sl_desc->req_type == SL_REQ_DISCOVERY)) {
		/* Iamasoo beacons bypass Winenet */
		packet->transID = 0xffffffff;
		sender_tx(sl_desc, packet, size, 0);

		rtdm_mutex_unlock(&wnet_xmit_lock);

		return 0;
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

	rtdm_mutex_lock(&wnet_tx_rx_data_lock);

	/* Fill the TX request parameters */

	wnet_tx_rx_data.tx_pending = true;
	wnet_tx_rx_data.sl_desc = sl_desc;
	wnet_tx_rx_data.tx_packet = packet;
	wnet_tx_rx_data.tx_size = size;
	wnet_tx_rx_data.tx_transID = packet->transID;
	wnet_tx_rx_data.tx_completed = completed;

	rtdm_mutex_unlock(&wnet_tx_rx_data_lock);

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
	rtdm_event_wait(&wnet_xmit_event);

	rtdm_mutex_lock(&wnet_tx_rx_data_lock);
	ret = wnet_tx_rx_data.tx_ret;
	rtdm_mutex_unlock(&wnet_tx_rx_data_lock);

	rtdm_mutex_unlock(&wnet_xmit_lock);

	return ret;
}

/**
 * This function is called when a TX request is being issued.
 * The call is made by the Sender.
 */
static int winenet_request_xmit(sl_desc_t *sl_desc) {
	DBG("** winenet_request_xmit\n");

	rtdm_mutex_lock(&wnet_tx_request_pending_lock);
	wnet_tx_request_pending = true;

	wnet_tx_rx_data.sl_desc = sl_desc;

	rtdm_mutex_unlock(&wnet_tx_request_pending_lock);

	/* Wake the Winenet task to process the TX request if it is in IDLE state */
	if (wnet_state == WNET_STATE_IDLE)
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
static void winenet_rx(sl_desc_t *sl_desc, plugin_desc_t *plugin_desc, transceiver_packet_t *packet, size_t size) {
	bool isMonitoringMedium;
	static bool all_packets_received = false;
	static uint32_t last_transID;
	uint32_t i;

	if (unlikely((sl_desc->req_type == SL_REQ_DISCOVERY))) {
		/* Iamasoo beacons bypass Winenet */
		receiver_rx(sl_desc, plugin_desc, packet, size);

		return ;
	}

	//DBG("** receiving: agencyUID_to: "); DBG_BUFFER(&sl_desc->agencyUID_to, SOO_AGENCY_UID_SIZE);
	//DBG("**            agencyUID_from: "); DBG_BUFFER(&sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE);

	/*
	 *
	 * Update the TX_RX data with sl_desc.
	 * This is useful when there is no XMIT request: the sl_desc should not be NULL.
	 */

	rtdm_mutex_lock(&wnet_tx_rx_data_lock);
	wnet_tx_rx_data.sl_desc = sl_desc;
	rtdm_mutex_unlock(&wnet_tx_rx_data_lock);

	rtdm_mutex_lock(&wnet_medium_free_lock);
	isMonitoringMedium = wnet_isMonitoringMedium;
	rtdm_mutex_unlock(&wnet_medium_free_lock);

	/* If the free medium detection is in progress, send an event to make the function return false */
	if (isMonitoringMedium)
		rtdm_event_signal(&wnet_medium_free_event);

	/* unibroad mode: if we are waiting for packets from a speaker, we simply ignore packets incoming from other
	 * possible speakers (multiple groups with hidden nodes).
	 * -> speakerUID is used to detected if the SOO is paired with another (unibroad)
	 */

	if (sl_desc->trans_mode == SL_MODE_UNIBROAD)
		if (memcmp(&speakerUID, get_null_agencyUID(), SOO_AGENCY_UID_SIZE) &&
				memcmp(&sl_desc->agencyUID_to, get_null_agencyUID(), SOO_AGENCY_UID_SIZE))
			if (memcmp(&sl_desc->agencyUID_from, &sl_desc->agencyUID_to, SOO_AGENCY_UID_SIZE))
				return ; /* Skip it */

	if ((packet->packet_type == TRANSCEIVER_PKT_DATA) && (wnet_state != WNET_STATE_LISTENER)) {

		rtdm_mutex_lock(&wnet_tx_rx_data_lock);
		wnet_tx_rx_data.sl_desc = sl_desc;
		wnet_tx_rx_data.rx_transID = packet->transID;
		wnet_tx_rx_data.data_received = true;
		rtdm_mutex_unlock(&wnet_tx_rx_data_lock);

		rtdm_event_signal(&wnet_event);
	}

	if ((packet->packet_type == TRANSCEIVER_PKT_DATA) && (wnet_state == WNET_STATE_LISTENER)) {
		/* Data packet */

		rtdm_mutex_lock(&wnet_tx_rx_data_lock);

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

		buf_rx_pkt[(packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME] = (transceiver_packet_t *) kmalloc(packet->size + sizeof(transceiver_packet_t), GFP_ATOMIC);
		memcpy(buf_rx_pkt[(packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME], packet, packet->size + sizeof(transceiver_packet_t));

		rtdm_mutex_unlock(&wnet_tx_rx_data_lock);

		/* Save the last ID of the last received packet */
		last_transID = (packet->transID & WNET_MAX_PACKET_TRANSID);

		if (((packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME == WNET_N_PACKETS_IN_FRAME - 1) || (packet->transID & WNET_LAST_PACKET)) {
			/* If all the packets of the frame have been received, forward them to the upper layer */

			if (all_packets_received) {
				rtdm_mutex_lock(&wnet_tx_rx_data_lock);

				for (i = 0; ((i < WNET_N_PACKETS_IN_FRAME) && (buf_rx_pkt[i] != NULL)); i++)
					if ((buf_rx_pkt[i]->packet_type == TRANSCEIVER_PKT_DATA) && !packet_already_received(buf_rx_pkt[i], &sl_desc->agencyUID_from))
						receiver_rx(sl_desc, plugin_desc, buf_rx_pkt[i], buf_rx_pkt[i]->size);

				clear_buf_rx_pkt();

				wnet_tx_rx_data.sl_desc = sl_desc;
				wnet_tx_rx_data.rx_transID = packet->transID;
				wnet_tx_rx_data.data_received = true;

				rtdm_mutex_unlock(&wnet_tx_rx_data_lock);

				rtdm_event_signal(&wnet_event);
			}
		}
	}

	if (packet->packet_type == TRANSCEIVER_PKT_DATALINK) {
		/* Datalink packet (typically, a Winenet beacon) */

		rtdm_mutex_lock(&wnet_tx_rx_data_lock);
		memcpy(&wnet_tx_rx_data.last_beacon, packet->payload, sizeof(wnet_beacon_t));
		rtdm_mutex_unlock(&wnet_tx_rx_data_lock);

		DBG("Beacon recv: %s from ", get_beacon_id_string(wnet_tx_rx_data.last_beacon.id)); DBG_BUFFER(&sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE);

		rtdm_event_signal(&wnet_event);
	}
}

/**
 * Callbacks of the Winenet protocol
 */
static datalink_proto_desc_t winenet_proto = {
	.xmit_callback = winenet_xmit,
	.rx_callback = winenet_rx,
	.request_xmit_callback = winenet_request_xmit
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
	memset(&listenerUID, 0, sizeof(agencyUID_t));
	memset(&prevSpeakerUID, 0, sizeof(agencyUID_t));

	INIT_LIST_HEAD(&wnet_neighbours);

	rtdm_event_init(&wnet_event, 0);
	rtdm_event_init(&wnet_state_event, 0);
	rtdm_event_init(&wnet_medium_free_event, 0);
	rtdm_event_init(&wnet_xmit_event, 0);
	rtdm_event_init(&wnet_tx_request_event, 0);

	wnet_tx_rx_data.tx_pending = false;
	wnet_tx_rx_data.data_received = false;
	wnet_tx_rx_data.last_beacon.id = WNET_BEACON_ID_N;

	rtdm_mutex_init(&wnet_tx_rx_data_lock);
	rtdm_mutex_init(&wnet_xmit_lock);
	rtdm_mutex_init(&wnet_medium_free_lock);
	rtdm_mutex_init(&wnet_tx_request_pending_lock);

	rtdm_mutex_init(&neighbour_list_lock);

	rtdm_task_init(&wnet_task, "Winenet", winenet_fn, NULL, RTDM_WINENET_TASK_PRIO, 0);

	winenet_register();

	/* Register with Discovery as Discovery listener */
	discovery_listener_register(&wnet_discovery_desc);

	rtdm_event_signal(&wnet_state_event);
}
