/*
 * Copyright (C) 2016-2018 Baptiste Delporte <bonel@bonel.net>
 * Copyright (C) 2018 Jean-Pierre Miceli <jean-pierre.miceli@heig-vd.ch>
 * Copyright (C) 2018-2021 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>

#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/sched/task.h>
#include <uapi/linux/sched/types.h>

#include <soo/soolink/soolink.h>

#include <soo/soolink/datalink/winenet.h>
#include <soo/soolink/datalink.h>
#include <soo/soolink/discovery.h>
#include <soo/soolink/transcoder.h>
#include <soo/soolink/transceiver.h>

#include <soo/core/device_access.h>

#include <soo/uapi/soo.h>
#include <soo/uapi/debug.h>
#include <soo/uapi/console.h>
#include <soo/uapi/soo.h>

/*
 * Winenet is implementing a so-called unibroad communication mode that is, a mode where a speaker sends
 * to his neighbours called listeners. These listeners form a group (of listeners). It may happen
 * that a listener appears as neighbour, but is not immediately part of a group.
 *
 * About the Winenet protocol:
 *
 * - When a new smart object (SOO) is showing up in a location where other SOOs are present, it first
 *   needs to ping the others to get know each other. A SOO is considered as "valid" when the ping procedure
 *   has been successfully performed.
 *
 * - As soon as the SOOs are valid to each other, they can start sending/receiving data with a speaker
 *   and listeners. The first speaker is the first in the ascending (agencyUID) list in the (valid)
 *   neighborhood. It does not exclude that two (or more) SOOs starts as speaker simultaneously, when several
 *   SOOs are showing up almost at the same time. However, the "speaker broadcast" process will allow
 *   to converge towards a nominal situation with only one speaker and several listeners.
 *
 * - When scaling with hidden nodes, the major point is the Discovery block which periodically broadcast (for real)
 *   beacons to the other SOOs so that they can get information about potential paired speakers, and decide what
 *   to do (of course, the idea is to preserve wireless interference between several neighbourhoods.
 *
 */

/* FSM state helpers */
static void change_state(wnet_state_t new_state);
static wnet_state_t get_state(void);
static char *wnet_str_state(void);

/*
 * Processing states to have a fine grained state of what we are currently doing.
 */
typedef enum {

	S_STANDBY,
	S_PING_REQUEST,
	S_GO_SPEAKER

} processing_state_t;

/* SOO environment specific to Winenet */
struct soo_winenet_env {

	/* Store the "first elected" speaker of a round */
	agencyUID_t first_speakerUID;

	volatile bool transmission_over;

	/* Handle used in the FSM */
	wnet_fsm_handle_t fsm_handle;

	struct mutex wnet_xmit_lock;

	/* Event used to track the receival of a Winenet beacon or a TX request */
	struct completion wnet_event;
	struct completion beacon_event;
	struct completion data_event;

	/* Internal SOOlink descriptor for handling beacons such as ping req/rsp/go_speaker/etc. */
	sl_desc_t *__sl_desc;

	wnet_tx_t wnet_tx;
	wnet_rx_t wnet_rx;

	/* Used to track transID in received packet */
	uint32_t last_transID;

	/* Management of the neighbourhood of SOOs. Used in the retry packet management. */
	struct list_head wnet_neighbours;

	/* Each call to winenet_tx will increment the transID counter */
	uint32_t sent_packet_transID;

	discovery_listener_t wnet_discovery_desc;

	/*
	 * Packet buffering for the n pkt/1 ACK strategy, in a circular buffer way.
	 * n packets form a frame.
	 * The n packets must be bufferized to be able to re-send them if a retry is necessary. As a packet
	 * is freed by the Sender once Winenet has processed it, the packet must be locally copied.
	 */
	transceiver_packet_t *buf_tx_pkt[WNET_N_PACKETS_IN_FRAME];
	transceiver_packet_t *buf_rx_pkt[WNET_N_PACKETS_IN_FRAME];

	/* Used for logging purpose to limit the log message when looping in a state. */
	wnet_state_t last_state;

	/* Message logging */
	char __beacon_str[80];

	wnet_neighbour_t *__current_speaker;

	/* Self reference to us */
	wnet_neighbour_t *ourself;

	processing_state_t processing_state;
};


/* Debugging strings */
/* All states are not used in unibroad mode like those related to collision management. */

static char *state_str[WNET_STATE_N] = {
	[WNET_STATE_IDLE] = "Idle",
	[WNET_STATE_SPEAKER] = "Speaker",
	[WNET_STATE_LISTENER] = "Listener",
};

static char *beacon_id_str[WNET_BEACON_N] = {
	[WNET_BEACON_GO_SPEAKER] = "GO_SPEAKER",
	[WNET_BEACON_ACKNOWLEDGMENT] = "ACKNOWLEDGMENT",
	[WNET_BEACON_BROADCAST_SPEAKER] = "BROADCAST_SPEAKER",
	[WNET_BEACON_PING] = "PING"
};

static char *ping_type_str[2] = {
	[WNET_PING_REQUEST] = "PING REQUEST",
	[WNET_PING_RESPONSE] = "PING RESPONSE"
};

static uint8_t invalid_str[] = "INVALID";

static int wait_for_ack(void);

/* Debugging functions */

char *winenet_get_state_str(wnet_state_t state) {
	uint32_t state_int = (uint32_t) state;

	if (unlikely(state_int >= WNET_STATE_N))
		return invalid_str;

	return state_str[state_int];
}

char *get_current_state_str(void) {
	return winenet_get_state_str(get_state());
}

char *winenet_get_beacon_id_str(wnet_beacon_id_t beacon_id) {
	uint32_t beacon_id_int = (uint32_t) beacon_id;

	return beacon_id_str[beacon_id_int];
}

char *beacon_str(wnet_beacon_t *beacon, agencyUID_t *uid) {
	char uid_str[80];
	int i;
	wnet_ping_t *ping_type;

	ping_type = (wnet_ping_t *) beacon->priv;

	sprintf(current_soo_winenet->__beacon_str, " Beacon %s (type: %s) / UID: ", winenet_get_beacon_id_str(beacon->id),
		((beacon->id == WNET_BEACON_PING) ? ping_type_str[(int) *ping_type] : "n/a" ));

	/* Display the agency UID with the fifth first bytes (enough) */
	for (i = 0 ; i < 5 ; i++) {
		sprintf(uid_str, "%02x ", ((char *) uid)[i]);
		strcat(current_soo_winenet->__beacon_str, uid_str);
	}

	return current_soo_winenet->__beacon_str;
}

/**
 * Allow the producer to be informed about potential problems or to
 * send a next packet.
 */
void winenet_xmit_data_processed(int ret) {

	current_soo_winenet->wnet_tx.ret = ret;

	/* Allow the producer to go further */
	complete(&current_soo_winenet->wnet_tx.xmit_event);

}

/**
 * Destroy the bufferized TX packets.
 * This function has to be called when a packet frame has been acknowledged, or if there is
 * an unexpected transition that requires the whole frame to be freed.
 */
static void clear_buf_tx_pkt(void) {
	uint32_t i;

	for (i = 0; i < WNET_N_PACKETS_IN_FRAME; i++)
		current_soo_winenet->buf_tx_pkt[i]->packet_type = TRANSCEIVER_PKT_NONE;
}

/**
 * Destroy the bufferized RX packets.
 * This function has to be called when a packet frame has been acknowledged, or if there is
 * an unexpected transition that requires the whole frame to be freed.
 */
static void clear_buf_rx_pkt(void) {
	uint32_t i;

	/* We assume wnet_rx_request_lock is acquired. */

	for (i = 0; i < WNET_N_PACKETS_IN_FRAME; i++)
		current_soo_winenet->buf_rx_pkt[i]->packet_type = TRANSCEIVER_PKT_NONE;
}

/*
 * Discard a transmission (sending packets).
 */
void discard_transmission(void) {
	/* Clear the TX pkt buffer */
	clear_buf_tx_pkt();

	/* Reset the TX trans ID */
	current_soo_winenet->sent_packet_transID = 0;

	winenet_xmit_data_processed(-EIO);
}

/*
 * next_neighbour() is just a convenient way to get the next neighbour after a given neighbour, taking into account
 * ourself (not considered). It processes the list in a circular way.
 * There is at least one (not valid) entry regarding ourself (where plugin is NULL).
 *
 * If argument pos is NULL, return the first valid neighbour if any.
 * Return NULL if there is no next neighbour anymore.
 *
 */
static wnet_neighbour_t *next_neighbour(wnet_neighbour_t *pos, bool valid) {
	wnet_neighbour_t *next = NULL;
	bool old;

	/* Sanity check */
	BUG_ON(list_empty(&current_soo_winenet->wnet_neighbours));

	old = neighbour_list_protection(true);

	if (pos == NULL) {
		pos = list_first_entry(&current_soo_winenet->wnet_neighbours, wnet_neighbour_t, list);

		/* circularity - pass the head of list */
		if (&pos->list == &current_soo_winenet->wnet_neighbours)
			pos = list_next_entry(pos, list);

		if (!valid || pos->valid) {
			neighbour_list_protection(old);
			return pos;
		}
	}

	next = pos;
	do {
		next = list_next_entry(next, list);

		/* Skip head of the list */
		if (&next->list == &current_soo_winenet->wnet_neighbours)
			next = list_next_entry(next, list);

		if ((next != pos) && (!valid || next->valid)) {
			neighbour_list_protection(old);
			return next;
		}

	} while (next != pos);

	neighbour_list_protection(old);

	return NULL;
}

/*
 * Same as the previous function, but the neighbour must be valid, i.e. the neighbour has successfully processed a ping request.
 */
static wnet_neighbour_t *next_valid_neighbour(wnet_neighbour_t *pos) {
	wnet_neighbour_t *__next;

	__next = next_neighbour(pos, true);

	return __next;
}

/* Find a neighbour by its agencyUID */
wnet_neighbour_t *find_neighbour(agencyUID_t *agencyUID) {
	wnet_neighbour_t *wnet_neighbour = NULL;
	bool old;

	old = neighbour_list_protection(true);

	list_for_each_entry(wnet_neighbour, &current_soo_winenet->wnet_neighbours, list) {

		if (!cmpUID(&wnet_neighbour->neighbour->agencyUID, agencyUID)) {
			neighbour_list_protection(old);

			return wnet_neighbour;
		}
	}

	neighbour_list_protection(old);

	return NULL;
}

/**
 * Send a Winenet beacon.
 * According to the kind of beacon, arg can be used to give a reference (assuming a known size) or
 * value of any type, opt for a integer.
 */
static void winenet_send_beacon(agencyUID_t *agencyUID, wnet_beacon_id_t beacon_id, uint8_t cause, void *priv, uint8_t priv_len) {
	transceiver_packet_t *transceiver_packet;
	wnet_beacon_t *beacon;

	/* Enforce the use of the a known SOOlink descriptor */
	BUG_ON(!current_soo_winenet->__sl_desc);

	transceiver_packet = (transceiver_packet_t *) kzalloc(sizeof(transceiver_packet_t) + sizeof(wnet_beacon_t) + priv_len, GFP_KERNEL);
	BUG_ON(!transceiver_packet);

	beacon = (wnet_beacon_t *) transceiver_packet->payload;

	beacon->id = beacon_id;
	beacon->cause = cause;

	beacon->priv_len = priv_len;
	memcpy(beacon->priv, priv, priv_len);

	transceiver_packet->packet_type = TRANSCEIVER_PKT_DATALINK;
	transceiver_packet->transID = 0;
	transceiver_packet->size = sizeof(wnet_beacon_t) + priv_len;

	memcpy(&current_soo_winenet->__sl_desc->agencyUID_to, agencyUID, SOO_AGENCY_UID_SIZE);

	soo_log("[soo:soolink:winenet:beacon] (state %s) Sending beacon to %s cause: %d\n",
		wnet_str_state(),
		beacon_str(beacon, &current_soo_winenet->__sl_desc->agencyUID_to), cause);

	__sender_tx(current_soo_winenet->__sl_desc, transceiver_packet);

	/* Release the outgoing packet */
	kfree(transceiver_packet);
}


/**
 * Add a new neighbour in our list. As Winenet is a Discovery listener,
 * this function is called when a neighbour appears.
 *
 * The adherence of a smart object in the neighborhood leads to a ping request/response
 * exchange between the two smart objects (us and the discovered one).
 * The smart object with the smallest agencyUID initiates the ping (request) and
 * the state is determined during this operation.
 */
static void winenet_add_neighbour(neighbour_desc_t *neighbour) {
	wnet_neighbour_t *wnet_neighbour;
	int ret;
	struct list_head *cur;
	wnet_neighbour_t *cur_neighbour;
	wnet_ping_t ping_type;

	/*
	 * Ping has been received correctly and will be processed by the neighbor.
	 * If something goes wrong, the Discovery will detect and remove it.
	 * At the moment, we do not perform other keep-alive event.
	 */

	wnet_neighbour = kzalloc(sizeof(wnet_neighbour_t), GFP_KERNEL);
	BUG_ON(!wnet_neighbour);

	wnet_neighbour->neighbour = neighbour;
	wnet_neighbour->last_transID = 0;

	soo_log("[soo:soolink:winenet:neighbour] Adding neighbour (our state is %s): ", get_current_state_str());
	soo_log_printlnUID(&neighbour->agencyUID);

	/*
	 * We use the same sorting strategy than the
	 * Discovery to be consistent.
	 */

	/* If the list is empty, add the neighbour (ourself) to it */
	if (list_empty(&current_soo_winenet->wnet_neighbours)) {

		current_soo_winenet->ourself = wnet_neighbour;

		list_add_tail(&wnet_neighbour->list, &current_soo_winenet->wnet_neighbours);

		change_state(WNET_STATE_IDLE);
		complete(&current_soo_winenet->wnet_event);

		return ;

	} else {

		/* Walk the list until we find the right place in ascending sort. */
		list_for_each(cur, &current_soo_winenet->wnet_neighbours) {

			cur_neighbour = list_entry(cur, wnet_neighbour_t, list);
			ret = cmpUID(&wnet_neighbour->neighbour->agencyUID, &cur_neighbour->neighbour->agencyUID);

			if (ret < 0) {

				/* The new neighbour has an agencyUID greater than the current, hence insert it after */
				list_add_tail(&wnet_neighbour->list, cur);
				break;
			}
		}

		/* All UIDs are less than the new one */
		if (cur == &current_soo_winenet->wnet_neighbours)
			list_add_tail(&wnet_neighbour->list, &current_soo_winenet->wnet_neighbours);
	}

	/* If we have the smaller agencyUID, we initiate the ping procedure. */
	if (cmpUID(&current_soo_winenet->ourself->neighbour->agencyUID, &neighbour->agencyUID) < 0) {

		/* Trigger a ping procedure */
		soo_log("[soo:soolink:winenet:ping] Sending PING_REQUEST to ");
		soo_log_printlnUID(&wnet_neighbour->neighbour->agencyUID);

		ping_type = WNET_PING_REQUEST;
		current_soo_winenet->processing_state = S_PING_REQUEST;

		winenet_send_beacon(&neighbour->agencyUID, WNET_BEACON_PING, 0, &ping_type, sizeof(wnet_ping_t));
	}

	winenet_dump_neighbours();
}

/**
 * Remove a neighbour from the neighbour list. As Winenet is a Discovery listener,
 * this function is called when a neighbour disappears.
 *
 */
static void winenet_remove_neighbour(neighbour_desc_t *neighbour) {
	wnet_neighbour_t *tmp, *wnet_neighbour = NULL;

	/* Could be called from the non-RT context at the beginning of the agency_core
	 * (selection of neighbourhood).
	 */

	soo_log("[soo:soolink:winenet:neighbour] Removing neighbour (our state is %s): ", get_current_state_str());
	soo_log_printlnUID(&neighbour->agencyUID);

	/* Sanity check to perform before removal.
	 * Check if the neighbour was our speaker and reset it if yes.
	 */
	if (current_soo_winenet->ourself->neighbour->priv && !cmpUID(current_soo_winenet->ourself->neighbour->priv, &neighbour->agencyUID))
		current_soo_winenet->ourself->neighbour->priv = NULL;

	list_for_each_entry_safe(wnet_neighbour, tmp, &current_soo_winenet->wnet_neighbours, list) {

		if (!cmpUID(&wnet_neighbour->neighbour->agencyUID, &neighbour->agencyUID)) {

			if (wnet_neighbour->neighbour->priv)
				kfree(wnet_neighbour->neighbour->priv);

			list_del(&wnet_neighbour->list);

			if (current_soo_winenet->__current_speaker == wnet_neighbour)
				current_soo_winenet->__current_speaker = NULL;

			kfree(wnet_neighbour);
			break;
		}
	}

	winenet_dump_neighbours();
}

/**
 * Perform update of neighbour information (private data) when a Iamasoo packet is received.
 *
 * Consistency rules:
 * - If we get a neighbour corresponding to our speaker and this neighbour has a priv which is
 *   null, then we discard our pairing and check for the SPEAKER state.
 *
 */
static void winenet_update_neighbour_priv(neighbour_desc_t *neighbour) {
	wnet_neighbour_t *wnet_neighbour, *wnet_neighbour_entry;
	wnet_ping_t ping_type;
	bool ok = false;

	/* First, we check if the neighbour is still in our (winenet) list of neighbours.
	 * If it is not the case, we (re-)add it into our list.
	 * Then, we examine the state IDLE.
	 */

	soo_log("[soo:soolink:winenet:neighbour] Updating neighbour (our state is %s and processing state is %d): ",
		get_current_state_str(), current_soo_winenet->processing_state);

	soo_log_printlnUID(&neighbour->agencyUID);

	winenet_dump_neighbours();

	list_for_each_entry(wnet_neighbour, &current_soo_winenet->wnet_neighbours, list) {

		if (!cmpUID(&wnet_neighbour->neighbour->agencyUID, &neighbour->agencyUID))
			break;
	}

	/* Must exist */
	BUG_ON(&wnet_neighbour->list == &current_soo_winenet->wnet_neighbours);

	/* This is a new neighbour. Proceed with the ping procedure. */
	if (!wnet_neighbour->valid) {

		if (current_soo_winenet->processing_state != S_PING_REQUEST) {

			/* Trigger a ping procedure */
			soo_log("[soo:soolink:winenet:ping] Sending PING_REQUEST to ");
			soo_log_printlnUID(&wnet_neighbour->neighbour->agencyUID);

			ping_type = WNET_PING_REQUEST;

			winenet_send_beacon(&neighbour->agencyUID, WNET_BEACON_PING, 0, &ping_type, sizeof(wnet_ping_t));
		}

		goto out;

	}

	if (current_soo_winenet->processing_state == S_STANDBY) {


		if (current_soo_winenet->ourself->neighbour->priv) {

			/* Consistency checking - Make sure we still have our speaker alive */

			/* Check if the beacon concerns our speaker and make sure it
			 * considers itself as speaker.
			 */

			if (!cmpUID(current_soo_winenet->ourself->neighbour->priv, &wnet_neighbour->neighbour->agencyUID)) {

				if (wnet_neighbour->neighbour->priv &&
						!cmpUID(&wnet_neighbour->neighbour->agencyUID, wnet_neighbour->neighbour->priv))
					ok = true;

			} else {


				/* Check if there is at least our speaker in the neighbourhood */
				list_for_each_entry(wnet_neighbour, &current_soo_winenet->wnet_neighbours, list)

					if (!cmpUID(current_soo_winenet->ourself->neighbour->priv, &wnet_neighbour->neighbour->agencyUID)) {
						ok = true;
					break;
				}
			}

			if (!ok)
				current_soo_winenet->ourself->neighbour->priv = NULL;
		}

	} else
		ok = true;

	/*
	 * Check if there is at least one speaker in our neighborhood, i.e. at least a neighbour with
	 * its <priv> (speaker) not NULL.
	 * If it not the case, we check if we are the smallest UID and change to SPEAKER.
	 */

	list_for_each_entry(wnet_neighbour_entry, &current_soo_winenet->wnet_neighbours, list)
	{
		if (wnet_neighbour_entry->neighbour->priv != NULL) {

			winenet_dump_neighbours();

			/* Everything works well :-) */
			return ;
		}
	}

	/* No speaker apparently found in our neighbourhood, hence we set ourself
	 * as speaker *only* if we are the first in the list.
	 */

	current_soo_winenet->ourself->neighbour->priv = &current_soo_winenet->ourself->neighbour->agencyUID;

	change_state(WNET_STATE_SPEAKER);
	complete(&current_soo_winenet->wnet_event);

out:
	winenet_dump_neighbours();
}

/**
 * Get information about private data for a Iamasoo packet before sending the beacon.
 */
static uint8_t winenet_get_neighbour_priv(neighbour_desc_t *neighbour) {

	/* Private data of us is up-to-date; it is our current speaker UID we are aware of. */

	/* Will be memcpy'd in the Iamasoo pkt */
	if (neighbour->priv) /* speakerUID */
		return SOO_AGENCY_UID_SIZE;
	else
		return 0;
}

/**
 * Reset the processed helper field of all neighbours
 */
void wnet_neighbours_processed_reset(void) {
	wnet_neighbour_t *cur;
	bool old;

	old = neighbour_list_protection(true);

	list_for_each_entry(cur, &current_soo_winenet->wnet_neighbours, list)
		cur->processed = false;

	neighbour_list_protection(old);
}

/**
 * Dump the active neighbour list.
 */
void winenet_dump_neighbours(void) {
	struct list_head *cur;
	wnet_neighbour_t *neighbour;
	uint32_t count = 0;

	soo_log("[soo:soolink:winenet:neighbour] ***** List of neighbours:\n");

	/* There is no neighbour in the list, I am alone */
	if (list_empty(&current_soo_winenet->wnet_neighbours)) {
		soo_log("[soo:soolink:winenet:neighbour] No neighbour\n");

		return;
	}

	list_for_each(cur, &current_soo_winenet->wnet_neighbours) {

		neighbour = list_entry(cur, wnet_neighbour_t, list);

		soo_log("[soo:soolink:winenet:neighbour] Neighbour %d (valid: %d): ", count+1, neighbour->valid);
		soo_log_printUID(&neighbour->neighbour->agencyUID);
		soo_log("  priv: ");
		soo_log_printlnUID(neighbour->neighbour->priv);

		count++;
	}
}

/**
 * Dump the current Winenet state.
 */
void winenet_dump_state(void) {
	lprintk("Winenet status: %s\n", state_str[get_state()]);
}

/*
 * Unset the beacon ID *before* sending the signal.
 */
void beacon_clear(void) {

	/* Sanity check */
	BUG_ON(!current_soo_winenet->wnet_rx.last_beacon);

	kfree(current_soo_winenet->wnet_rx.last_beacon);

	current_soo_winenet->wnet_rx.last_beacon = NULL;

	complete(&current_soo_winenet->beacon_event);
}


/*
 * Clear spurious ack which may arrive after a ack timeout.
 * We simply ignore it (but we need to clear the beacon event.
 */
bool clear_spurious_ack(void) {

	if (current_soo_winenet->wnet_rx.last_beacon && (current_soo_winenet->wnet_rx.last_beacon->id == WNET_BEACON_ACKNOWLEDGMENT)) {
		beacon_clear();
		return true;
	}

	return false;
}

/*
 * Check for a ping request/response at any time.
 * Returns false if we remain unchanged, otherwise the neighbour which is currently speaker
 * (according to the ping strategy).
 *
 * Returns true if a new neighbour is the new speaker. The state has changed to listener.
 */
void process_ping(void) {
	wnet_neighbour_t *pos;
	wnet_ping_t ping_type;
	agencyUID_t agencyUID_from;
	bool old;

	old = neighbour_list_protection(true);

	if (current_soo_winenet->wnet_rx.last_beacon && (current_soo_winenet->wnet_rx.last_beacon->id == WNET_BEACON_PING)) {

		memcpy(&ping_type, current_soo_winenet->wnet_rx.last_beacon->priv, sizeof(wnet_ping_t));

		if (ping_type == WNET_PING_REQUEST) {

			/* Because of the PING REQUEST strategy, we know that the other is less than us in all case, so
			 * we abort our sending as speaker.
			 */

			soo_log("[soo:soolink:winenet:ping] %s: (state %s) processing ping request... from ", __func__, get_current_state_str());
			soo_log_printlnUID(&current_soo_winenet->wnet_rx.sl_desc->agencyUID_from);

			pos = find_neighbour(&current_soo_winenet->wnet_rx.sl_desc->agencyUID_from);
			memcpy(&agencyUID_from, &current_soo_winenet->wnet_rx.sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE);

			beacon_clear();

			/* Already got by our Discovery? If no, he will have to re-send a ping request later. */
			if (pos) {
				soo_log("[soo:soolink:winenet:ping] %s: neighbour VALID. Its UID: ", __func__);
				soo_log_printlnUID(&agencyUID_from);

				pos->valid = true;

				/* Send a PING RESPONSE beacon
				 * But okay, we stay in our current state.
				 */
				soo_log("[soo:soolink:winenet:ping] Sending PING_RESPONSE to ");
				soo_log_printlnUID(&agencyUID_from);

				ping_type = WNET_PING_RESPONSE;

				winenet_send_beacon(&agencyUID_from, WNET_BEACON_PING, 0, &ping_type, sizeof(ping_type));
			}

		} else if (ping_type == WNET_PING_RESPONSE) {

			soo_log("[soo:soolink:winenet:ping] (state %s) processing ping response...\n", get_current_state_str());

			memcpy(&agencyUID_from, &current_soo_winenet->wnet_rx.sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE);
			beacon_clear();

			pos = find_neighbour(&agencyUID_from);

			/* Sanity check (we got a response to our request) */
			BUG_ON(!pos);

			pos->valid = true;

			current_soo_winenet->processing_state = S_STANDBY;
		}
	}

	neighbour_list_protection(old);
}

/*
 * Wait for an acknowledgment beacon.
 *
 * Return 0 the ack has been successfully received.
 * Return -1 in case of timeout
 * Return 1 in case of another beacon received instead of ack.
 */
static int wait_for_ack(void) {
	static unsigned long remaining = 0;
	int ret_ack = ACK_STATUS_TIMEOUT;

	/* Timeout of Tspeaker us */
	remaining = wait_for_completion_timeout(&current_soo_winenet->wnet_event,
			((remaining == 0) ? msecs_to_jiffies(WNET_TSPEAKER_ACK_MS) : remaining));

	if (remaining > ACK_STATUS_OK) {
		ret_ack = ACK_STATUS_BEACON;

		if (current_soo_winenet->wnet_rx.last_beacon && (current_soo_winenet->wnet_rx.last_beacon->id == WNET_BEACON_ACKNOWLEDGMENT)) {

			/* We also want to make sure that the received beacon is issued from the right sender */

			if (!current_soo_winenet->wnet_rx.last_beacon->priv_len ||
			    (*((uint32_t *) current_soo_winenet->wnet_rx.last_beacon->priv) == (current_soo_winenet->wnet_tx.transID & WNET_MAX_PACKET_TRANSID)))

					/* OK - We got a correct acknowledgment, we set the cause for further processing. */
					ret_ack = current_soo_winenet->wnet_rx.last_beacon->cause;

			remaining = 0;

			/* Event processed */
			beacon_clear();
		}

	} else {

		/* The timeout has expired */;

		soo_log("[soo:soolink:winenet:ack] !!!!! ACK timeout... will retry to ");
		soo_log_printlnUID(&current_soo_winenet->wnet_rx.sl_desc->agencyUID_to);

		ret_ack = ACK_STATUS_TIMEOUT;
	}

	return ret_ack;
}

/**
 * Contact the next speaker in our neighborhoud.
 * At this point, the neighbour protection must be disabled.
 * This function is not re-entrant; it is called only once at a time.
 */
void forward_next_speaker(void) {
	wnet_neighbour_t *next_speaker;
	int ack;
	int retry_count;
	bool old;

	/* The priv must remain to ourself until we got an ack on GO_SPEAKER */

	old = neighbour_list_protection(true);
again:

	/*
	 * Check if the current_speaker has been removed in the meanwhile.
	 * Used to guarantee that all neighbours have a chance to get speaker, even
	 * if the neighbour is not visible by all other neighbours (hidden node).
	 */
	if (current_soo_winenet->__current_speaker == NULL)
		current_soo_winenet->__current_speaker = current_soo_winenet->ourself;

	next_speaker = next_valid_neighbour(current_soo_winenet->__current_speaker);

	/* Okay, if next_speaker should be the same :-) because we have only this neighbour...
	 * this function will return NULL in this case... so we do a special treatment.
	 */

	if (!next_speaker)
		next_speaker = next_valid_neighbour(current_soo_winenet->ourself);

	if (!next_speaker) {

		/* Reset our speakerUID */
		current_soo_winenet->ourself->neighbour->priv = NULL;
		current_soo_winenet->__current_speaker = NULL;

		neighbour_list_protection(old);

		change_state(WNET_STATE_IDLE);
		return ;
	}

	current_soo_winenet->ourself->neighbour->priv = &next_speaker->neighbour->agencyUID;
	
	/* Well, the current speaker is known */
	current_soo_winenet->__current_speaker = next_speaker;

	retry_count = 0;
	do {
		current_soo_winenet->processing_state = S_GO_SPEAKER;

		/* Now send the beacon */
		winenet_send_beacon(&next_speaker->neighbour->agencyUID, WNET_BEACON_GO_SPEAKER, 0, NULL, 0);

retry_waitack:
		ack = wait_for_ack();

		if (ack != ACK_STATUS_OK) {

			/* Did we receive another beacon than ack ? */
			if (ack == ACK_STATUS_BEACON) {

				/* ping ? */
				process_ping();

				if (current_soo_winenet->wnet_rx.last_beacon &&
				    ((current_soo_winenet->wnet_rx.last_beacon->id == WNET_BEACON_GO_SPEAKER) ||
			            (current_soo_winenet->wnet_rx.last_beacon->id == WNET_BEACON_BROADCAST_SPEAKER)))
					beacon_clear();

				goto retry_waitack;

			} else if (ack == ACK_STATUS_ABORT) {

				soo_log("[soo:soolink:winenet:ack] got a STATUS ABORT\n");

				current_soo_winenet->processing_state = S_STANDBY;
				current_soo_winenet->ourself->neighbour->priv = NULL;

				goto out;

			} else {
				retry_count++;
				soo_log("[soo:soolink:winenet:ack] retry_count = %d\n", retry_count);
			}
		}

	} while ((ack != 0) && (retry_count <= WNET_RETRIES_MAX));

	if (ack != 0) {
		/*
		 * Well, it seems we have a bad guy as neighbour :-(
		 * Just invalidate it and pick-up a new speaker.
		 * Please note the the list managed by the Discovery is not impacted.
		 * If the neighbour is really down, it will disappear very soon by Discovery.
		 */

		next_speaker->valid = false;

		goto again;
	}

out:
	neighbour_list_protection(old);

	current_soo_winenet->processing_state = S_STANDBY;

	change_state(WNET_STATE_LISTENER);
}

/**
 * Broadcast to all neighbours that we are the new speaker.
 *
 * This will update the neighbourhood according to the ack we receive.
 * The neighbor must be protected during this operation to remain consistent
 * during the sending.
 *
 * Return false if we have been discarded as speaker.
 *
 */
bool speaker_broadcast(void) {
	wnet_neighbour_t *pos, *tmp;
	int ack;
	int retry_count;
	agencyUID_t agencyUID_from;
	bool old;

	old = neighbour_list_protection(true);

	wnet_neighbours_processed_reset();

	pos = next_valid_neighbour(NULL);

	while (pos && !pos->processed) {

		pos->processed = true;

		/* Send the beacon requiring an acknowledgement */
		retry_count = 0;
		do {

			/* Now send the beacon */
			winenet_send_beacon(&pos->neighbour->agencyUID, WNET_BEACON_BROADCAST_SPEAKER, 0, NULL, 0);
retry_waitack:
			ack = wait_for_ack();

			/* Did we receive another beacon than ack ? */
			if (ack == ACK_STATUS_BEACON) {

				process_ping();

				if ((current_soo_winenet->wnet_rx.last_beacon && (current_soo_winenet->wnet_rx.last_beacon->id == WNET_BEACON_GO_SPEAKER)) ||
				    (current_soo_winenet->wnet_rx.last_beacon && (current_soo_winenet->wnet_rx.last_beacon->id == WNET_BEACON_BROADCAST_SPEAKER)))
				{
					/* Event processed */
					memcpy(&agencyUID_from, &current_soo_winenet->wnet_rx.sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE);
					beacon_clear();

					winenet_send_beacon(&agencyUID_from, WNET_BEACON_ACKNOWLEDGMENT, ACK_STATUS_ABORT, NULL, 0);

					goto retry_waitack;
				}
			} else if (ack == ACK_STATUS_ABORT) {

				soo_log("[soo:soolink:winenet:ack] got a STATUS ABORT\n");

				current_soo_winenet->ourself->neighbour->priv = NULL;

				change_state(WNET_STATE_LISTENER);

				neighbour_list_protection(old);

				return false;

			} else if (ack == -1) {
				retry_count++;
				soo_log("[soo:soolink:winenet:ack] retry_count = %d\n", retry_count);
			}

		} while ((ack != ACK_STATUS_OK) && (retry_count <= WNET_RETRIES_MAX));

		/* Timeout ? */
		if (ack != 0) {

			/*
			 * Well, it seems we have a bad guy as neighbour :-(
			 * Just go on with another neighbour.
			 */

			tmp = pos;
			pos = next_valid_neighbour(pos);
			if (pos == tmp)
				pos = NULL;

			tmp->valid = false;

		} else
			pos = next_valid_neighbour(pos);

	};

	/* Alone ? */
	if (next_valid_neighbour(NULL) == NULL) {

		current_soo_winenet->ourself->neighbour->priv = NULL;

		change_state(WNET_STATE_IDLE);

		neighbour_list_protection(old);

		return false;
	}

	neighbour_list_protection(old);

	/* Completed */
	return true;
}


/** Start of FSM management **/

/*
 * How do we determine the initial speaker?
 * When we receive a first beacon from a neighbour, we look at its speaker UID which is stored in its private data.
 * If it is NULL, it means that either the smart object has just arrived like us or it was alone.
 *
 *   In this latter case (the simplest case), we compare the agencyUID and if we are the first, we put ourself as speaker, otherwise
 *   we get in the listener state.
 *
 *   If it just arrived like us, there might be a delay until it gets a speakerUID. So, in the meanwhile, the same strategy is considered as it was alone.
 *
 * If we get a beacon with a speakerUID not NULL, we need to consider if we are speaker or not; it the speakerUID we get is in our list, we
 * check if we are greater than it; if it the case we remain speaker. If not, we discard the transmission immediately. The neighbour will apply the
 * same strategy and will get (or stay) speaker if it is greater than us.
 * But, if the speakerUID does not appear in our list, we discard the transmission *if we are speaker* ONLY if the speakerUID is in the own neighbourhood of our neighboor.
 * In other cases, we are moving to listener state.
 *
 */

/*************************************************** WNET_STATE_INIT ****************************************************/

static void winenet_state_init(wnet_state_t old_state) {

	soo_log("[soo:soolink:winenet] Now in state INIT\n");

	/* Wait that Discovery inserted us into the list of neighbour. */
	wait_for_completion(&current_soo_winenet->wnet_event);
}

/**************************** WNET_STATE_IDLE *****************************/

/*
 * We are initially in this state until there is at least another smart object
 * in the neighbourhood.
 */
static void winenet_state_idle(wnet_state_t old_state) {
	wnet_neighbour_t *wnet_neighbour;
	wnet_ping_t ping_type;
	bool old;

	soo_log("[soo:soolink:winenet:state:idle] Now in state IDLE\n");

	if (current_soo_winenet->last_state != WNET_STATE_IDLE) {
		soo_log("[soo:soolink:winenet:state:idle] Smart object ");
		soo_log_printlnUID(get_my_agencyUID());
		soo_log(" -- Now in state IDLE\n");
		soo_log(" -- priv = "); soo_log_printlnUID(current_soo_winenet->ourself->neighbour->priv);

		current_soo_winenet->last_state = WNET_STATE_IDLE;
	}

retry:
	/* Waiting on a first neighbor at least. */
	wait_for_completion(&current_soo_winenet->wnet_event);

	old = neighbour_list_protection(true);

	if (clear_spurious_ack()) {
		neighbour_list_protection(old);
		goto retry;
	}

	/* Process beacon first */
	if (current_soo_winenet->wnet_rx.last_beacon && (current_soo_winenet->wnet_rx.last_beacon->id == WNET_BEACON_PING)) {

		memcpy(&ping_type, current_soo_winenet->wnet_rx.last_beacon->priv, sizeof(wnet_ping_t));

		wnet_neighbour = find_neighbour(&current_soo_winenet->wnet_rx.sl_desc->agencyUID_from);
		BUG_ON(!wnet_neighbour);

		beacon_clear();

		if (ping_type == WNET_PING_REQUEST) {

			/* Determine which of us is speaker/listener and set the appropriate. */

			soo_log("[soo:soolink:winenet:state:ping] We got a request. Send a response. Neighbour VALID / from ");
			soo_log_printlnUID(&wnet_neighbour->neighbour->agencyUID);

			/* Send a PING RESPONSE beacon */

			soo_log("[soo:soolink:winenet:ping] Sending PING_RESPONSE to ");
			soo_log_printlnUID(&wnet_neighbour->neighbour->agencyUID);

			ping_type = WNET_PING_RESPONSE;

			winenet_send_beacon(&wnet_neighbour->neighbour->agencyUID, WNET_BEACON_PING, 0, &ping_type, sizeof(wnet_ping_t));

		} else if (ping_type == WNET_PING_RESPONSE) {

			/* Determine which of us is speaker/listener and set the appropriate. */

			soo_log("[soo:soolink:winenet:state:ping] We got a RESPONSE. Neighbour is VALID / from ");
			soo_log_printlnUID(&wnet_neighbour->neighbour->agencyUID);

			current_soo_winenet->processing_state = S_STANDBY;

		} else
			BUG();

		wnet_neighbour->valid = true;

		change_state(WNET_STATE_LISTENER);

	} else {

		if (current_soo_winenet->wnet_rx.last_beacon)
			beacon_clear();

		/* Just ignore other beacons which will not be processed here. */
		neighbour_list_protection(old);
		goto retry;
	}

	neighbour_list_protection(old);
}

/**************************** WNET_STATE_SPEAKER *****************************/

static void winenet_state_speaker(wnet_state_t old_state) {
	int i, ack;
	int retry_count;
	wnet_neighbour_t *listener, *tmp;
	bool __broadcast_done = false;
	agencyUID_t agencyUID_from, agencyUID_to;
	bool first = true;
	bool old;

	soo_log("[soo:soolink:winenet:state:speaker] Now in state SPEAKER\n");

	if (current_soo_winenet->last_state != WNET_STATE_SPEAKER) {
		soo_log("[soo:soolink:winenet:state:speaker] Smart object ");
		soo_log_printlnUID(get_my_agencyUID());
		soo_log(" -- Now in state SPEAKER\n");

		current_soo_winenet->last_state = WNET_STATE_SPEAKER;
	}

	current_soo_winenet->ourself->neighbour->priv = &current_soo_winenet->ourself->neighbour->agencyUID;

	while (true) {

		/* We keep synchronized with our producer or be ready to process beacons. */

		/* As we enter in this state for the first time, we can process what is pending (beacon
		 * or data along the tx path).
		 */
		if (!first)
			wait_for_completion(&current_soo_winenet->wnet_event);

		old = neighbour_list_protection(true);

		first = false;

		if (clear_spurious_ack()) {
			neighbour_list_protection(old);
			continue;
		}

		/* Is it an end of transmission? */
		if (current_soo_winenet->transmission_over) {
			current_soo_winenet->transmission_over = false;

			/* Synchronize with the producer. */
			complete(&current_soo_winenet->wnet_tx.xmit_event);

			/* Send a go_speaker beacon to the next speaker. */
			neighbour_list_protection(old);
			forward_next_speaker();

			return;
		}

		if (current_soo_winenet->wnet_rx.last_beacon &&
		    ((current_soo_winenet->wnet_rx.last_beacon->id == WNET_BEACON_GO_SPEAKER) ||
		     (current_soo_winenet->wnet_rx.last_beacon->id == WNET_BEACON_BROADCAST_SPEAKER))) {

			memcpy(&agencyUID_from, &current_soo_winenet->wnet_rx.sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE);
			beacon_clear();

			winenet_send_beacon(&agencyUID_from, WNET_BEACON_ACKNOWLEDGMENT, ACK_STATUS_ABORT, NULL, 0);

			neighbour_list_protection(old);
			continue;
		}

		/* Check for ping */
		process_ping();

		/* Any data to send on this transmission? */
		if (!current_soo_winenet->wnet_tx.pending) {

			/* Send a go_speaker beacon to the next speaker. */
			forward_next_speaker();

			neighbour_list_protection(old);
			return ;
		}

		/* Now, we inform all neighbours that we are the new speaker.
		 *
		 * We ask an acknowledge for this beacon, and if a neighbour does
		 * not answer, we remove it from our neighbourhood.
		 *
		 * We do that only once and we do that here in the code, after the first waiting on wnet_event
		 * since such other waitings are done during the broadcast.
		 *
		 * Once a broadcast is fully achieved, we go to the next speaker (tranmission_over = true).
		 * So, there is no need to reset this bool (chaning state).
		 */

		if (!__broadcast_done) {

			if (!speaker_broadcast()) {
				neighbour_list_protection(old);
				return ;
			}
			__broadcast_done = true;
		}

		wnet_neighbours_processed_reset();

		/* Data can be sent out. Select a listener and proceed. */
		/* Get the first listener */
		listener = next_valid_neighbour(NULL);

		/* No more listener ? */
		if (listener == NULL) {

			discard_transmission();

			current_soo_winenet->ourself->neighbour->priv = NULL;

			change_state(WNET_STATE_IDLE);

			neighbour_list_protection(old);
			return ;
		}

		listener->processed = true;

		/* Set the destination */
		memcpy(&current_soo_winenet->wnet_tx.sl_desc->agencyUID_to, &listener->neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

		/* We have to transmit over all smart objects */
		/* Sending the frame for the first time (first listener) */

		for (i = 0; ((i < WNET_N_PACKETS_IN_FRAME) && (current_soo_winenet->buf_tx_pkt[i]->packet_type != TRANSCEIVER_PKT_NONE)); i++)
			__sender_tx(current_soo_winenet->wnet_tx.sl_desc, current_soo_winenet->buf_tx_pkt[i]);

		/* Now waiting for the ACK beacon */
		memcpy(&agencyUID_to, &current_soo_winenet->wnet_tx.sl_desc->agencyUID_to, SOO_AGENCY_UID_SIZE);

retry_ack1:
		ack = wait_for_ack();

		if (ack != 0) {

			if (current_soo_winenet->wnet_rx.last_beacon &&
			    ((current_soo_winenet->wnet_rx.last_beacon->id == WNET_BEACON_GO_SPEAKER) ||
			    (current_soo_winenet->wnet_rx.last_beacon->id == WNET_BEACON_BROADCAST_SPEAKER))) {

				memcpy(&agencyUID_from, &current_soo_winenet->wnet_rx.sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE);
				beacon_clear();

				winenet_send_beacon(&agencyUID_from, WNET_BEACON_ACKNOWLEDGMENT, ACK_STATUS_ABORT, NULL, 0);

				goto retry_ack1;
			}

			process_ping();

			retry_count = 0;
			do {

				/* Re-send the whole frame */
				for (i = 0; ((i < WNET_N_PACKETS_IN_FRAME) && (current_soo_winenet->buf_tx_pkt[i]->packet_type != TRANSCEIVER_PKT_NONE)); i++)
					__sender_tx(current_soo_winenet->wnet_tx.sl_desc, current_soo_winenet->buf_tx_pkt[i]);
retry_ack2:

				ack = wait_for_ack();

				if (current_soo_winenet->wnet_rx.last_beacon &&
				    ((current_soo_winenet->wnet_rx.last_beacon->id == WNET_BEACON_GO_SPEAKER) ||
				     (current_soo_winenet->wnet_rx.last_beacon->id == WNET_BEACON_BROADCAST_SPEAKER))) {

					memcpy(&agencyUID_from, &current_soo_winenet->wnet_rx.sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE);
					beacon_clear();

					winenet_send_beacon(&agencyUID_from, WNET_BEACON_ACKNOWLEDGMENT, ACK_STATUS_ABORT, NULL, 0);

					goto retry_ack2;
				}

				process_ping();

				if (ack == -1) {
					retry_count++;
					soo_log("[soo:soolink:winenet:ack] retry_count = %d\n", retry_count);
				}

			} while ((ack != 0) && (retry_count <= WNET_RETRIES_MAX));

			if (ack != 0) {
				/*
				 * Well, it seems we have a bad guy as neighbour :-(
				 * Just remove it and proceed with the next listener, i.e. lets proceed
				 * with the broadcast to other neighbours.
				 */

				tmp = listener;
				listener = next_valid_neighbour(listener);

				tmp->valid = false;

				if (listener == NULL) {

					discard_transmission();

					current_soo_winenet->ourself->neighbour->priv = NULL;

					change_state(WNET_STATE_IDLE);

					clear_buf_tx_pkt();
					winenet_xmit_data_processed(-EIO);

					neighbour_list_protection(old);
					return ;

				}
			} else
				/* Look for the next neighbour */
				listener = next_valid_neighbour(listener);
		} else
			/* Look for the next neighbour */
			listener = next_valid_neighbour(listener);

		while (listener && !listener->processed) {

			listener->processed = true;

			/* Set the destination */
			memcpy(&current_soo_winenet->wnet_tx.sl_desc->agencyUID_to, &listener->neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

			retry_count = 0;
			do {
				/* Re-send the whole frame */
				for (i = 0; ((i < WNET_N_PACKETS_IN_FRAME) && (current_soo_winenet->buf_tx_pkt[i]->packet_type != TRANSCEIVER_PKT_NONE)); i++)
					__sender_tx(current_soo_winenet->wnet_tx.sl_desc, current_soo_winenet->buf_tx_pkt[i]);
retry_ack3:

				ack = wait_for_ack();

				if (ack > 0) {

					/* Delayed (spurious) beacon */
					if (current_soo_winenet->wnet_rx.last_beacon &&
					    ((current_soo_winenet->wnet_rx.last_beacon->id == WNET_BEACON_GO_SPEAKER) ||
					     (current_soo_winenet->wnet_rx.last_beacon->id == WNET_BEACON_BROADCAST_SPEAKER))) {

						/*
						 * We tell the emitter to abort its process and to get listener.
						 */

						memcpy(&agencyUID_from, &current_soo_winenet->wnet_rx.sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE);

						beacon_clear();

						winenet_send_beacon(&agencyUID_from, WNET_BEACON_ACKNOWLEDGMENT, ACK_STATUS_ABORT, NULL, 0);

						goto retry_ack3;
					}

					process_ping();

				} else if (ack == -1) {
					retry_count++;
					soo_log("[soo:soolink:winenet:ack] retry_count = %d\n", retry_count);
				}

			} while ((ack != 0) && (retry_count <= WNET_RETRIES_MAX));

			if (ack != 0) {
				/*
				 * Well, it seems we have a bad guy as neighbour :-(
				 * Just remove it and proceed with the next listener, i.e. lets proceed
				 * with the broadcast to other neighbours.
				 */

				tmp = listener;
				listener = next_valid_neighbour(listener);

				tmp->valid = false;

				if (listener == NULL) {

					discard_transmission();

					current_soo_winenet->ourself->neighbour->priv = NULL;

					change_state(WNET_STATE_IDLE);

					clear_buf_tx_pkt();
					winenet_xmit_data_processed(-EIO);

					neighbour_list_protection(old);
					return ;
				}
			} else
				listener = next_valid_neighbour(listener);
		}

		/* We reach the end of the round of listeners. */

		clear_buf_tx_pkt();
		winenet_xmit_data_processed(0);

		neighbour_list_protection(old);
	}
}

/**************************** WNET_STATE_LISTENER *****************************/

static void winenet_state_listener(wnet_state_t old_state) {
	wnet_neighbour_t *wnet_neighbour;
	agencyUID_t agencyUID_from;
	bool first = true, old;

	soo_log("[soo:soolink:winenet:state:listener] Now in state LISTENER\n");

	if (current_soo_winenet->last_state != WNET_STATE_LISTENER) {
		soo_log("[soo:soolink:winenet:state:listener] Smart object ");
		soo_log_printlnUID(get_my_agencyUID());
		soo_log(" -- Now in state LISTENER\n");

		current_soo_winenet->last_state = WNET_STATE_LISTENER;
	}

	while (1) {
		if (!first)
			wait_for_completion(&current_soo_winenet->wnet_event);

		old = neighbour_list_protection(true);

		first = false;

		if (clear_spurious_ack()) {
			neighbour_list_protection(old);
			continue;
		}

		/* It may happen if the current speaker disappeared and we are now speaker. */
		if (get_state() == WNET_STATE_SPEAKER) {
			/* Go ahead in speaker state for active processing */

			neighbour_list_protection(old);
			return ;
		}

		/* Sanity check */
		BUG_ON(get_state() != WNET_STATE_LISTENER);

		if (next_valid_neighbour(NULL) == NULL) {

			/* Reset the speakerUID */
			current_soo_winenet->ourself->neighbour->priv = NULL;
			change_state(WNET_STATE_IDLE);

			neighbour_list_protection(old);
			return ;
		}

		/* Just check if we receive some ping beacons */
		process_ping();

		/*
		 * When a new SOO appears in the neighbourhood, it can see only us before the other and decides to become speaker
		 * and sends a BROADCAST_SPEAKER. The same thing may appear with a SOO sending a GO_SPEAKER.
		 * So, if we receive such beacons, we are checking if we are in the process of receiving a buffer and if it is
		 * the case, we do not acknowledge the beacon.
		 */
		if (current_soo_winenet->wnet_rx.last_beacon && (current_soo_winenet->wnet_rx.last_beacon->id == WNET_BEACON_GO_SPEAKER)) {

			memcpy(&agencyUID_from, &current_soo_winenet->wnet_rx.sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE);

			/* Event processed */
			beacon_clear();

			if (current_soo_winenet->ourself->neighbour->priv &&
			    cmpUID(current_soo_winenet->ourself->neighbour->priv, &current_soo_winenet->wnet_rx.sl_desc->agencyUID_from))

				/* Cause ACK_STATUS_ABORT means the sender will put its priv to NULL */
				winenet_send_beacon(&agencyUID_from, WNET_BEACON_ACKNOWLEDGMENT, ACK_STATUS_ABORT, NULL, 0);

			else {
				/* Our turn... */

				current_soo_winenet->ourself->neighbour->priv = &current_soo_winenet->ourself->neighbour->agencyUID;
				change_state(WNET_STATE_SPEAKER);

				winenet_send_beacon(&agencyUID_from, WNET_BEACON_ACKNOWLEDGMENT, 0, NULL, 0);
				neighbour_list_protection(old);
				return ;
			}
		}

		if (current_soo_winenet->wnet_rx.last_beacon && (current_soo_winenet->wnet_rx.last_beacon->id == WNET_BEACON_BROADCAST_SPEAKER))
		{

			memcpy(&agencyUID_from, &current_soo_winenet->wnet_rx.sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE);

			/* Event processed */
			beacon_clear();

			if (current_soo_winenet->ourself->neighbour->priv && cmpUID(current_soo_winenet->ourself->neighbour->priv, &current_soo_winenet->wnet_rx.sl_desc->agencyUID_from))

				winenet_send_beacon(&agencyUID_from, WNET_BEACON_ACKNOWLEDGMENT, ACK_STATUS_ABORT, NULL, 0);

			else {
				/* Pick it up */
				wnet_neighbour = find_neighbour(&current_soo_winenet->wnet_rx.sl_desc->agencyUID_from);
				BUG_ON(!wnet_neighbour);

				/* Now it is the new speaker. */
				current_soo_winenet->ourself->neighbour->priv = &wnet_neighbour->neighbour->agencyUID;

				/* We are ready to listen to this speaker. */
				winenet_send_beacon(&agencyUID_from, WNET_BEACON_ACKNOWLEDGMENT, 0, NULL, 0);

			}

		}

		neighbour_list_protection(old);
	}
}

/********************************* End of FSM management *************************************/

/* FSM function table */
static wnet_state_fn_t fsm_functions[WNET_STATE_N] = {
	[WNET_STATE_INIT] = winenet_state_init,

	[WNET_STATE_IDLE] = winenet_state_idle,
	[WNET_STATE_SPEAKER] = winenet_state_speaker,
	[WNET_STATE_LISTENER] = winenet_state_listener,
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

	soo_log("[soo:soolink:winenet:state] ***** Changing state from %s to %s\n", winenet_get_state_str(handle->state), winenet_get_state_str(new_state));

	handle->old_state = handle->state;
	handle->state = new_state;
}

/**
 * Change the state of Winenet.
 */
static void change_state(wnet_state_t new_state) {
	winenet_change_state(&current_soo_winenet->fsm_handle, new_state);
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
	return winenet_get_state(&current_soo_winenet->fsm_handle);
}

static char *wnet_str_state(void) {
	return winenet_get_state_str(get_state());
}

/**
 * Main Winenet routine that implements the FSM.
 * At the beginning, we are in the IDLE state.
 */
static int fsm_task_fn(void *args) {
	wnet_fsm_handle_t *handle = (wnet_fsm_handle_t *) args;
	wnet_state_fn_t *functions = handle->funcs;

	soo_log("[soo:soolink:winenet:state] Entering Winenet FSM task...\n");

	while (true)

		/* Call the proper state function */
		(*functions[handle->state])(handle->old_state);

	return 0;
}

/**
 * Start the Winenet FSM routine.
 * The FSM function table and the RTDM event are in the handle given as parameter.
 * This function has to be called from CPU #0.
 */
void winenet_start_fsm_task(char *name, wnet_fsm_handle_t *handle) {
	struct task_struct *t;

	handle->old_state = WNET_STATE_INIT;
	handle->state = WNET_STATE_INIT;

	t = kthread_create(fsm_task_fn, (void *) handle, "fsm_task");
	BUG_ON(!t);

	add_thread(current_soo, t->pid);

	wake_up_process(t);
}

/**
 * This function is called when a data packet or a Iamasoo beacon has to be sent.
 * The call is made by the Sender.
 */
static int winenet_tx(sl_desc_t *sl_desc, transceiver_packet_t *packet, bool completed) {
	int ret = 0;

	/* End of transmission ? */
	if (!packet) {

		/* Ok, go ahead with the next speaker */

		/* We are synchronized with the SPEAKER if it is still active, i.e.
		 * if there is still some valid neighbour.
		 */
		if (get_state() == WNET_STATE_SPEAKER) {

			current_soo_winenet->transmission_over = true;
			current_soo_winenet->wnet_tx.pending = false;

			complete(&current_soo_winenet->wnet_event);

			soo_log("[soo:soolink:winenet] %s waiting on xmit_event...\n", __func__);

			/* Wait until the FSM has processed the data. */
			wait_for_completion(&current_soo_winenet->wnet_tx.xmit_event);

			soo_log("[soo:soolink:winenet] %s Okay, ready to go.\n", __func__);
		}

		return 0;
	}

	if (unlikely(sl_desc->req_type == SL_REQ_DISCOVERY)) {

		/* Iamasoo beacons */
		packet->transID = 0xffffffff;

		__sender_tx(sl_desc, packet);

		return 0;
	}

	/* Only one producer can call winenet_tx at a time */
	mutex_lock(&current_soo_winenet->wnet_xmit_lock);

	packet->transID = current_soo_winenet->sent_packet_transID;

	/*
	 * If this is the last packet, set the WNET_LAST_PACKET bit in the transID.
	 * This is required to allow the receiver to identify the last packet of the
	 * frame (if the modulo of its trans ID is not equal to WNET_N_PACKETS_IN_FRAME - 1)
	 * and force it to send an ACK.
	 * And prepare the next transID.
	 */
	if (completed) {
		packet->transID |= WNET_LAST_PACKET;
		current_soo_winenet->sent_packet_transID = 0;
	} else
		current_soo_winenet->sent_packet_transID = (current_soo_winenet->sent_packet_transID + 1) % WNET_MAX_PACKET_TRANSID;

	/* Fill in the buffer */
	memcpy(current_soo_winenet->buf_tx_pkt[(packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME], packet, packet->size + sizeof(transceiver_packet_t));

	/* Look for a completed frame (max number of packets reached or completed) */
	if (current_soo_winenet->sent_packet_transID % WNET_N_PACKETS_IN_FRAME == 0) {

		/* Fill the TX request parameters */
		current_soo_winenet->wnet_tx.sl_desc = sl_desc;
		current_soo_winenet->wnet_tx.transID = packet->transID;

		/* Now, setting tx_pending to true will allow the speaker to send out */
		current_soo_winenet->wnet_tx.pending = true;

		complete(&current_soo_winenet->wnet_event);

		soo_log("[soo:soolink:winenet] %s Packet ready to be sent, waiting on xmit_event...\n", __func__);

		/* Wait until the packed has been sent out. */
		wait_for_completion(&current_soo_winenet->wnet_tx.xmit_event);

		soo_log("[soo:soolink:winenet] Okay, ready to go...\n");

		ret = current_soo_winenet->wnet_tx.ret;

		/* In case of failure, we re-init the transID sequence. */
		if (ret < 0)
			current_soo_winenet->sent_packet_transID = 0;
	}

	mutex_unlock(&current_soo_winenet->wnet_xmit_lock);

	return ret;
}


/**
 * This function is called when a packet is received. This can be a data packet to forward to a
 * consumer (typically the Decoder), a Iamasoo beacon to forward to the Discovery block or a
 * Datalink beacon to handle in Winenet.
 * The call is made by the Receiver.
 * The size refers to the whole transceiver packet.
 */
void winenet_rx(sl_desc_t *sl_desc, transceiver_packet_t *packet) {
	uint32_t i;
	uint32_t transID;

	current_soo_winenet->wnet_rx.sl_desc = sl_desc;
	current_soo_winenet->wnet_rx.transID = packet->transID;

	if (packet->packet_type == TRANSCEIVER_PKT_DATALINK) {

		current_soo_winenet->wnet_rx.last_beacon = kzalloc(packet->size, GFP_KERNEL);
		BUG_ON(!current_soo_winenet->wnet_rx.last_beacon);

		memcpy(current_soo_winenet->wnet_rx.last_beacon, packet->payload, packet->size);

		soo_log("[soo:soolink:winenet:beacon] (state %s) Receiving beacon from %s\n",
			wnet_str_state(),
			beacon_str(current_soo_winenet->wnet_rx.last_beacon, &current_soo_winenet->wnet_rx.sl_desc->agencyUID_from));

		/* Processed within the FSM directly */

		complete(&current_soo_winenet->wnet_event);

		/* Wait until the beacon has been processed by the FSM */
		wait_for_completion(&current_soo_winenet->beacon_event);

	}

	if (packet->packet_type == TRANSCEIVER_PKT_DATA) {

		/*
		 * We check if the received packet comes from an expected smart object (SOO).
		 * Case to be examined:
		 *
		 * If the SOO matches with our priv, this is our speaker and we accept packets.
		 *
		 * In addition, we can logically NOT receive a PKT_DATA from a smart object which would not have
		 * been paired (via PING) with us. Hence, current_soo_winenet->ourself->neighbour->priv can't be NULL.
		 */
		if (!current_soo_winenet->ourself->neighbour->priv ||
		    cmpUID(&current_soo_winenet->wnet_rx.sl_desc->agencyUID_from, current_soo_winenet->ourself->neighbour->priv)) {

			soo_log("[soo:soolink:winenet] Skipping SOO ");
			soo_log_printUID(&current_soo_winenet->wnet_rx.sl_desc->agencyUID_from);
			soo_log("    bound speaker: ");
			soo_log_printlnUID(current_soo_winenet->ourself->neighbour->priv);

			return ;
		}

		/*
		 * Data packets are processed immediately along the RX callpath (upper layers) and has nothing to
		 * do with the FSM. Actually, the speaker will wait for an acknowledgment beacon which will
		 * process by the FSM *after* all packets have been received (and hence forwarded to the upper layers).
		 *
		 * Check different things according the packet transID field.
		 * If the packet transID is inferior to the last transID. This may happen
		 * if we send the previous acknowledgment BUT it has not been received by the sender.
		 * In this case, the sender will ack-timeout and re-send the same frame. We skip the frame then,
		 * and we have to re-send a new acknowledgment.
		 */

		if (!packet->transID) {

			/*
			 * First packet of the frame: be ready to process the next packets of the frame.
			 * By default, at the beginning, we set the new_frame boolean to true. If any packet in the frame is missed,
			 * the boolean is set to false, meaning that the frame is invalid.
			 */
			clear_buf_rx_pkt();

		} else if ((packet->transID & WNET_MAX_PACKET_TRANSID) < current_soo_winenet->last_transID) {

			/* A packet we already received; might happen if a ACK has not been received by the speaker. */

			if (((packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME == WNET_N_PACKETS_IN_FRAME - 1) || (packet->transID & WNET_LAST_PACKET)) {
				transID = current_soo_winenet->wnet_rx.transID & WNET_MAX_PACKET_TRANSID;

				winenet_send_beacon(&current_soo_winenet->wnet_rx.sl_desc->agencyUID_from, WNET_BEACON_ACKNOWLEDGMENT, 0, &transID, sizeof(transID));
			}

			return ;


		} else if (packet->transID && (packet->transID & WNET_MAX_PACKET_TRANSID) != current_soo_winenet->last_transID + 1) {

			/* The packet chain is (temporarly) broken by ack timeout processing from the sender. */

			soo_log("[soo:soolink:winenet] Pkt chain broken: (last_transID=%d)/(packet->transID=%d)\n", current_soo_winenet->last_transID, packet->transID & WNET_MAX_PACKET_TRANSID);

			return ;
		}

		/* Copy the packet into the bufferized packet array */
		memcpy(current_soo_winenet->buf_rx_pkt[(packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME], packet, packet->size + sizeof(transceiver_packet_t));

		/* Save the last ID of the last received packet */
		current_soo_winenet->last_transID = (packet->transID & WNET_MAX_PACKET_TRANSID);

		/* If all the packets of the frame have been received, forward them to the upper layer */
		if (((packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME == WNET_N_PACKETS_IN_FRAME - 1) || (packet->transID & WNET_LAST_PACKET)) {

			for (i = 0; ((i < WNET_N_PACKETS_IN_FRAME) && (current_soo_winenet->buf_rx_pkt[i]->packet_type != TRANSCEIVER_PKT_NONE)); i++)
				receiver_rx(sl_desc, current_soo_winenet->buf_rx_pkt[i]);

			clear_buf_rx_pkt();

			/*
			 * Send an ACKNOWLEDGMENT beacon.
			 */
			transID = current_soo_winenet->wnet_rx.transID & WNET_MAX_PACKET_TRANSID;
			winenet_send_beacon(&current_soo_winenet->wnet_rx.sl_desc->agencyUID_from, WNET_BEACON_ACKNOWLEDGMENT, 0, &transID, sizeof(transID));

			if (packet->transID & WNET_LAST_PACKET)
				current_soo_winenet->ourself->neighbour->priv = NULL;
		}
	}
}

void winenet_cancel_rx(sl_desc_t *sl_desc) {
	current_soo_winenet->ourself->neighbour->priv = NULL;
}

/**
 * Callbacks of the Winenet protocol
 */
static datalink_proto_desc_t winenet_proto = {
	.tx_callback = winenet_tx,
	.rx_callback = winenet_rx,
	.rx_cancel_callback = winenet_cancel_rx,
};

/**
 * Initialization of Winenet.
 */
void winenet_init(void) {
	int i;

	lprintk("Winenet initializing...\n");

	current_soo->soo_winenet = kzalloc(sizeof(struct soo_winenet_env), GFP_KERNEL);
	BUG_ON(!current_soo->soo_winenet);

	current_soo_winenet->last_state = WNET_STATE_N;

	current_soo_winenet->processing_state = S_STANDBY;

	memcpy(&current_soo_winenet->first_speakerUID, get_my_agencyUID(), sizeof(agencyUID_t));

	INIT_LIST_HEAD(&current_soo_winenet->wnet_neighbours);

	init_completion(&current_soo_winenet->wnet_event);
	init_completion(&current_soo_winenet->wnet_tx.xmit_event);
	init_completion(&current_soo_winenet->beacon_event);
	init_completion(&current_soo_winenet->data_event);

	current_soo_winenet->wnet_tx.pending = false;
	current_soo_winenet->wnet_rx.last_beacon = NULL;
	current_soo_winenet->last_transID = 0;

	mutex_init(&current_soo_winenet->wnet_xmit_lock);

	current_soo_winenet->fsm_handle.funcs = fsm_functions;

	/*
	 * Allocate once all tx & rx buffers
	 */
	for (i = 0; i < WNET_N_PACKETS_IN_FRAME; i++) {
		current_soo_winenet->buf_tx_pkt[i] = (transceiver_packet_t *) kzalloc(SL_PACKET_PAYLOAD_MAX_SIZE + sizeof(transcoder_packet_t) + sizeof(transceiver_packet_t), GFP_KERNEL);
		BUG_ON(!current_soo_winenet->buf_tx_pkt[i]);

		current_soo_winenet->buf_tx_pkt[i]->packet_type = TRANSCEIVER_PKT_NONE;

		current_soo_winenet->buf_rx_pkt[i] = (transceiver_packet_t *) kzalloc(SL_PACKET_PAYLOAD_MAX_SIZE + sizeof(transcoder_packet_t) + sizeof(transceiver_packet_t), GFP_KERNEL);
		BUG_ON(!current_soo_winenet->buf_rx_pkt[i]);

		current_soo_winenet->buf_rx_pkt[i]->packet_type = TRANSCEIVER_PKT_NONE;
	}

	/* Internal SOOlink descriptor */
#if defined(CONFIG_SOOLINK_PLUGIN_WLAN)
	current_soo_winenet->__sl_desc = sl_register(SL_REQ_DATALINK, SL_IF_WLAN, SL_MODE_UNIBROAD);
#elif defined(CONFIG_SOOLINK_PLUGIN_ETHERNET)
	current_soo_winenet->__sl_desc = sl_register(SL_REQ_DATALINK, SL_IF_ETH, SL_MODE_UNIBROAD);
#elif defined(CONFIG_SOOLINK_PLUGIN_SIMULATION)
	current_soo_winenet->__sl_desc = sl_register(SL_REQ_DATALINK, SL_IF_SIM, SL_MODE_UNIBROAD);
#elif
#error !! Winenet SOOlink plugin undefined...
#endif /* !CONFIG_SOOLINK_PLUGIN_WLAN */

	BUG_ON(!current_soo_winenet->__sl_desc);

	datalink_register_protocol(SL_DL_PROTO_WINENET, &winenet_proto);

	current_soo_winenet->wnet_discovery_desc.add_neighbour_callback = winenet_add_neighbour;
	current_soo_winenet->wnet_discovery_desc.remove_neighbour_callback = winenet_remove_neighbour;
	current_soo_winenet->wnet_discovery_desc.update_neighbour_priv_callback = winenet_update_neighbour_priv;
	current_soo_winenet->wnet_discovery_desc.get_neighbour_priv_callback = winenet_get_neighbour_priv;

	/* Register with Discovery as Discovery listener */
	discovery_listener_register(&current_soo_winenet->wnet_discovery_desc);

	winenet_start_fsm_task("Wnet", &current_soo_winenet->fsm_handle);

}
