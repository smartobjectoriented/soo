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
#include <linux/random.h>
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

/* SOO environment specific to Winenet */
struct soo_winenet_env {

	/* Handle used in the FSM */
	wnet_fsm_handle_t fsm_handle;

	struct mutex wnet_xmit_lock;

	/* Event used to track the receival of a Winenet beacon or a TX request */
	struct completion wnet_event;
	struct completion data_event;

	/* Internal SOOlink descriptor for handling beacons such as ping req/rsp/go_speaker/etc. */
	sl_desc_t *__sl_desc;

	wnet_tx_t wnet_tx;
	wnet_rx_t wnet_rx;

	/* Pending (received) beacons */

	struct list_head pending_beacons;
	struct mutex pending_beacons_lock;

	/* Used to track transID in received packet */
	uint32_t expected_transID;

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

	/* Current target agencyUID to handle ack correctly */
	uint64_t current_targetUID;
};

typedef bool(*neighbour_fn_t)(wnet_neighbour_t *neighbour, void *arg);

/* Debugging strings */
/* All states are not used in unibroad mode like those related to collision management. */

static char *state_str[WNET_STATE_N] = {
	[WNET_STATE_IDLE] = "Idle",
	[WNET_STATE_SPEAKER] = "Speaker",
	[WNET_STATE_LISTENER] = "Listener",
};

static char *reqrsp_str[3] = {
	[WNET_NONE] = "NONE",
	[WNET_REQUEST] = "REQUEST",
	[WNET_RESPONSE] = "RESPONSE"

};

static uint8_t invalid_str[] = "INVALID";

/* Forward declarations */
wnet_reqrsp_t handle_neighbour_query(wnet_neighbour_t **wnet_neighbour);
static void change_state(wnet_state_t new_state);
static wnet_state_t get_state(void);
static char *wnet_str_state(void);
void neighbour_query(wnet_neighbour_t *wnet_neighbour, wnet_reqrsp_t reqrsp);

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

char *beacon_str(wnet_beacon_t *beacon, uint64_t uid) {
	char idstr[80];
	int i;
	uint8_t *c = (uint8_t *) &uid;
	wnet_reqrsp_t type;

	type = (wnet_reqrsp_t) beacon->cause;

	switch (beacon->id){
	case WNET_BEACON_GO_SPEAKER:
		strcpy(idstr, "GO_SPEAKER");
		break;
	case WNET_BEACON_ACKNOWLEDGMENT:
		strcpy(idstr, "ACKNOWLEDGMENT");
		break;
	case WNET_BEACON_BROADCAST_SPEAKER:
		strcpy(idstr, "BROADCAST_SPEAKER");
		break;
	case WNET_BEACON_PING:
		strcpy(idstr, "PING");
		break;
	case WNET_BEACON_QUERY_STATE:
		strcpy(idstr, "QUERY_STATE");
		break;
	}

	sprintf(current_soo_winenet->__beacon_str, " Beacon %s (type: %s) / UID: ", idstr,
		((beacon->id == WNET_BEACON_PING) ? reqrsp_str[type] :
		((beacon->id == WNET_BEACON_QUERY_STATE ? reqrsp_str[type] : "n/a" ))));

	/* Display the agency UID with the fifth first bytes (enough) */
	for (i = 0; i < 8 ; i++) {
		sprintf(idstr, "%02x ", *(c+7-i));
		strcat(current_soo_winenet->__beacon_str, idstr);
	}

	return current_soo_winenet->__beacon_str;
}

extern struct mutex soo_log_lock;
void wnet_trace(char *format, ...) {
	char info[50], uid[50];
	va_list va;
	char *c;
	char buf[CONSOLEIO_BUFFER_SIZE];
	char state_str[15], tmpstr[4];
	int i;

	mutex_lock(&soo_log_lock);

	va_start(va, format);
	vsnprintf(buf, CONSOLEIO_BUFFER_SIZE, format, va);
	va_end(va);

	if (current_soo_winenet->ourself && current_soo_winenet->ourself->paired_speaker) {

		strcpy(uid, "");
		c = (uint8_t *) &current_soo_winenet->ourself->paired_speaker;
		for (i = 0 ; i < 8 ; i++) {
			sprintf(tmpstr, "%02x ", *(c+7-i));
			strcat(uid, tmpstr);
		}
		uid[strlen(uid)-1] = 0;

	} else
		strcpy(uid, "U");

	if (get_current_state_str()) {
		state_str[0] = get_current_state_str()[0];
		state_str[1] = 0;
	} else
		strcpy(state_str, "N");

	sprintf(info, "%s-%s-%s", current_soo->name, state_str, uid);

	__soo_log(info, buf);

	mutex_unlock(&soo_log_lock);
}

/**
 * Allow the producer to be informed about potential problems or to
 * send a next packet.
 */
void winenet_xmit_data_processed(int ret) {

	current_soo_winenet->wnet_tx.ret = ret;

	if (ret < 0) {
		current_soo_winenet->wnet_tx.pending = TX_NO_DATA;
		current_soo_winenet->wnet_tx.ret = ret;
	}

	if (ret == TX_DATA_COMPLETED) {
		current_soo_winenet->wnet_tx.ret = 0;
		current_soo_winenet->wnet_tx.pending = TX_NO_DATA;
	}

	if (ret == TX_DATA_IN_PROGRESS) {
		current_soo_winenet->wnet_tx.pending = TX_DATA_IN_PROGRESS;
		current_soo_winenet->wnet_tx.ret = 0;
	}

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

/**
 * This function is a convenient way to get the next neighbour after a given neighbour, taking into account
 * ourself (not considered). It processes the list in a circular way.
 * There is at least one (not valid) entry regarding ourself (where plugin is NULL).
 *
 * @param pos	We want the neighbour after the one pointed by <pos>
 * @param valid If we want only neighbours who are fully paired (ping req/resp achieved)
 * @param paired_with_us If we want a neighhour paired with us
 * @return NULL if there is no neighbour anymore.
 */
static wnet_neighbour_t *next_neighbour(wnet_neighbour_t *pos, bool valid, bool paired_with_us) {
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

		if ((!valid || pos->valid) &&
			(!paired_with_us || (pos->paired_speaker == current_soo_winenet->ourself->neighbour->agencyUID))) {

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

		if ((next != pos) &&
			((!valid || next->valid) &&
			(!paired_with_us || (next->paired_speaker == current_soo_winenet->ourself->neighbour->agencyUID)))) {

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
static wnet_neighbour_t *next_valid_neighbour(wnet_neighbour_t *pos, bool paired_with_us) {
	wnet_neighbour_t *__next;

	__next = next_neighbour(pos, true, paired_with_us);

	return __next;
}

/**
 *
 * Perform an action on all neighbours which are valid if required (or not).
 *
 * @param valid		Consider only valid neighbours
 * @param neighbour_fn	Action to be executed on each neighbour
 * @param arg		Any possible argument
 * @return  		Number of neighbour which has been processed
 */
static int iterate_on_neighbours(bool valid, neighbour_fn_t neighbour_fn, void *arg) {
	wnet_neighbour_t *pos = NULL;
	bool old;
	int count = 0;

	/* Sanity check */
	BUG_ON(list_empty(&current_soo_winenet->wnet_neighbours));

	old = neighbour_list_protection(true);

	list_for_each_entry(pos, &current_soo_winenet->wnet_neighbours, list) {

		if (!valid || pos->valid) {
			count++; /* One neighbour being processed */

			soo_log_printlnUID(pos->neighbour->agencyUID);
			if (!neighbour_fn(pos, arg))
				break;
		}
	}

	neighbour_list_protection(old);

	return count;
}

/* Find a neighbour by its agencyUID */
wnet_neighbour_t *find_neighbour(uint64_t agencyUID) {
	wnet_neighbour_t *wnet_neighbour = NULL;
	bool old;

	old = neighbour_list_protection(true);

	list_for_each_entry(wnet_neighbour, &current_soo_winenet->wnet_neighbours, list) {

		if (wnet_neighbour->neighbour->agencyUID == agencyUID) {
			neighbour_list_protection(old);

			return wnet_neighbour;
		}
	}

	neighbour_list_protection(old);

	return NULL;
}

/**
 * Update the state of a specific neighbour based on neighbour_state_t
 *
 * @param wnet_neighbour
 * @param neighbour_state
 */
void update_current_neighbour_state(wnet_neighbour_t *wnet_neighbour, neighbour_state_t *neighbour_state) {

	wnet_neighbour->paired_speaker = neighbour_state->paired_speaker;
	wnet_neighbour->randnr = neighbour_state->randnr;
}

/**
 * Retrieve the state of a neighbour to fill in the neighbour_state_t
 *
 * @param wnet_neighbour
 * @param neighbour_state
 */
void retrieve_current_neighbour_state(wnet_neighbour_t *wnet_neighbour, neighbour_state_t *neighbour_state) {

	neighbour_state->paired_speaker = wnet_neighbour->paired_speaker;
	neighbour_state->randnr = wnet_neighbour->randnr;
	neighbour_state->pkt_data = false;
}

/**
 * Send a Winenet beacon.
 *
 * Our state is also propagated along all beacons.
 *
 * According to the kind of beacon, arg can be used to give a reference (assuming a known size) or
 * value of any type, opt for a integer.
 */
static void ____winenet_send_beacon(uint64_t agencyUID, int beacon_id, uint8_t cause, neighbour_state_t *neighbour_state) {
	transceiver_packet_t *transceiver_packet;
	wnet_beacon_t *beacon;

	/* Enforce the use of the a known SOOlink descriptor */
	BUG_ON(!current_soo_winenet->__sl_desc);

	transceiver_packet = (transceiver_packet_t *) kzalloc(sizeof(transceiver_packet_t) + sizeof(wnet_beacon_t) +
		sizeof(neighbour_state_t), GFP_KERNEL);

	BUG_ON(!transceiver_packet);

	beacon = (wnet_beacon_t *) transceiver_packet->payload;

	beacon->id = beacon_id;
	beacon->cause = cause;

	beacon->priv_len = sizeof(neighbour_state_t);
	memcpy(beacon->priv, neighbour_state, beacon->priv_len);

	transceiver_packet->packet_type = TRANSCEIVER_PKT_DATALINK;
	transceiver_packet->transID = 0;
	transceiver_packet->size = sizeof(wnet_beacon_t) + beacon->priv_len;

	current_soo_winenet->__sl_desc->agencyUID_to =  agencyUID;

	wnet_trace("[soo:soolink:winenet:beacon] (state %s) SENDING beacon to %s cause: %d\n",
		wnet_str_state(),
		beacon_str(beacon, current_soo_winenet->__sl_desc->agencyUID_to), cause);

	__sender_tx(current_soo_winenet->__sl_desc, transceiver_packet);

	/* Release the outgoing packet */
	kfree(transceiver_packet);
}

static void __winenet_send_beacon(uint64_t agencyUID, int beacon_id, uint8_t cause) {
	neighbour_state_t neighbour_state;

	retrieve_current_neighbour_state(current_soo_winenet->ourself, &neighbour_state);

	____winenet_send_beacon(agencyUID, beacon_id, cause, &neighbour_state);
}

static void winenet_send_beacon(uint64_t agencyUID, int beacon_id, uint8_t cause) {

	current_soo_winenet->current_targetUID = agencyUID;

	__winenet_send_beacon(agencyUID, beacon_id, cause);
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
	struct list_head *cur;
	wnet_neighbour_t *cur_neighbour;

	/*
	 * Ping has been received correctly and will be processed by the neighbor.
	 * If something goes wrong, the Discovery will detect and remove it.
	 * At the moment, we do not perform other keep-alive event.
	 */

	wnet_neighbour = kzalloc(sizeof(wnet_neighbour_t), GFP_KERNEL);
	BUG_ON(!wnet_neighbour);

	wnet_neighbour->neighbour = neighbour;

	wnet_trace("[soo:soolink:winenet:neighbour] Adding neighbour (our state is %s): ", get_current_state_str());
	soo_log_printlnUID(neighbour->agencyUID);

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

			if (wnet_neighbour->neighbour->agencyUID > cur_neighbour->neighbour->agencyUID) {

				/* The new neighbour has an agencyUID greater than the current, hence insert it after */
				list_add_tail(&wnet_neighbour->list, cur);
				break;
			}
		}

		/* All UIDs are less than the new one */
		if (cur == &current_soo_winenet->wnet_neighbours)
			list_add_tail(&wnet_neighbour->list, &current_soo_winenet->wnet_neighbours);
	}

	/* If we have an agencyUID greater than the neighbour, we send PING_REQUEST */
	if (current_soo_winenet->ourself->neighbour->agencyUID > wnet_neighbour->neighbour->agencyUID) {

		/* Trigger a ping procedure */
		wnet_trace("[soo:soolink:winenet:ping] Sending PING_REQUEST to ");
		soo_log_printlnUID(wnet_neighbour->neighbour->agencyUID);

		__winenet_send_beacon(wnet_neighbour->neighbour->agencyUID, WNET_BEACON_PING, WNET_REQUEST);
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

	wnet_trace("[soo:soolink:winenet:neighbour] Removing neighbour (our state is %s): ", get_current_state_str());
	soo_log_printlnUID(neighbour->agencyUID);

	/* Sanity check to perform before removal.
	 * Check if the neighbour was our speaker and reset it if yes.
	 */
	if (current_soo_winenet->ourself->paired_speaker == neighbour->agencyUID)
		current_soo_winenet->ourself->paired_speaker = 0;

	list_for_each_entry_safe(wnet_neighbour, tmp, &current_soo_winenet->wnet_neighbours, list) {

		if (wnet_neighbour->neighbour->agencyUID == neighbour->agencyUID) {

			list_del(&wnet_neighbour->list);

			if (current_soo_winenet->__current_speaker == wnet_neighbour)
				current_soo_winenet->__current_speaker = 0;

			wnet_neighbour->paired_speaker = 0;

			kfree(wnet_neighbour);
			break;
		}
	}

	/* Allow to examine if we are concerned with this removal */
	complete(&current_soo_winenet->wnet_event);

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
static void winenet_update_neighbour(neighbour_desc_t *neighbour) {
	wnet_neighbour_t *wnet_neighbour;

	/* First, we check if the neighbour is still in our (winenet) list of neighbours.
	 * If it is not the case, we (re-)add it into our list.
	 * Then, we examine the state IDLE.
	 */

	wnet_trace("[soo:soolink:winenet:neighbour] Updating neighbour (our state is %s) ", get_current_state_str());

	soo_log_printlnUID(neighbour->agencyUID);

	winenet_dump_neighbours();

	list_for_each_entry(wnet_neighbour, &current_soo_winenet->wnet_neighbours, list) {

		if (wnet_neighbour->neighbour->agencyUID == neighbour->agencyUID)
			break;
	}

	/* Must exist */
	BUG_ON(&wnet_neighbour->list == &current_soo_winenet->wnet_neighbours);

	if (!wnet_neighbour->valid) {
		/* Trigger a ping procedure */
		wnet_trace("[soo:soolink:winenet:ping] Sending PING_REQUEST to ");
		soo_log_printlnUID(wnet_neighbour->neighbour->agencyUID);

		__winenet_send_beacon(wnet_neighbour->neighbour->agencyUID, WNET_BEACON_PING, WNET_REQUEST);
	}
}

/**
 * Unpair a neighbour if paired with us.
 * Called from the Speaker state (s_)
 *
 * @param neighbour 	Neighbour to be checked
 * @param arg		(not used)
 */
static bool s_unpair_neighbour(wnet_neighbour_t *neighbour, void *arg) {

	wnet_trace("[soo:soolink:winenet:beacon] Now unpairing neighbour ");
	soo_log_printlnUID(neighbour->neighbour->agencyUID);

	/* Send beacon for unpairing */
	if (neighbour->paired_speaker == current_soo_winenet->ourself->neighbour->agencyUID)
		__winenet_send_beacon(neighbour->neighbour->agencyUID, WNET_BEACON_BROADCAST_SPEAKER, ACK_STATUS_ABORT);

	return true;
}

/**
 * Dump the active neighbour list.
 */
void winenet_dump_neighbours(void) {
	struct list_head *cur;
	wnet_neighbour_t *neighbour;
	uint32_t count = 0;

	wnet_trace("[soo:soolink:winenet:neighbour] ***** List of neighbours:\n");

	/* There is no neighbour in the list, I am alone */
	if (list_empty(&current_soo_winenet->wnet_neighbours)) {
		wnet_trace("[soo:soolink:winenet:neighbour] No neighbour\n");

		return;
	}

	list_for_each(cur, &current_soo_winenet->wnet_neighbours) {

		neighbour = list_entry(cur, wnet_neighbour_t, list);

		wnet_trace("[soo:soolink:winenet:neighbour] Neighbour %d (valid: %d): ", count+1, neighbour->valid);
		soo_log_printUID(neighbour->neighbour->agencyUID);
		wnet_trace("  paired speaker: ");
		soo_log_printlnUID(neighbour->paired_speaker);

		count++;
	}
}

/**
 * Dump the current Winenet state.
 */
void winenet_dump_state(void) {
	lprintk("Winenet status: %s\n", state_str[get_state()]);
}

/**
 * Get the first available beacon matching with <beacon_id>.
 * If beacon_id == WNET_BEACON_N, any beacon is considered.
 *
 * @param beacon_id to be picked up (WNET_BEACON_N means any beacon)
 * @return found beacon or NULL
 */
static pending_beacon_t *next_beacon(int beacon_id_mask) {
	pending_beacon_t *beacon = NULL;

	mutex_lock(&current_soo_winenet->pending_beacons_lock);

	if (list_empty(&current_soo_winenet->pending_beacons))
		goto out;

	list_for_each_entry(beacon, &current_soo_winenet->pending_beacons, list) {
		if (beacon->this->id & beacon_id_mask)
			goto out; /* Found beacon */
	}

	beacon = NULL; /* End of list */
out:
	if (beacon) {
		wnet_trace("[soo:soolink:winenet:beacon] Processing next beacon: %s", beacon_str(beacon->this, current_soo_winenet->ourself->neighbour->agencyUID));
		wnet_trace(" from ");
		soo_log_printlnUID(beacon->agencyUID_from);
	}
	mutex_unlock(&current_soo_winenet->pending_beacons_lock);

	return beacon;
}

/**
 * Remove a specific beacon from the pending beacon list.
 *
 * @param beacon	Beacon to be removed (and to be freed)
 */
static void beacon_del(pending_beacon_t *beacon) {

	pending_beacon_t *pos, *tmp;

	mutex_lock(&current_soo_winenet->pending_beacons_lock);

	list_for_each_entry_safe(pos, tmp, &current_soo_winenet->pending_beacons, list) {

		if (pos == beacon) {

			list_del(&pos->list);

			kfree(pos->this);
			kfree(pos);

			break;
		}
	}

	mutex_unlock(&current_soo_winenet->pending_beacons_lock);
}

/*
 * Check for a ping request/response at any time.
 * Returns false if we remain unchanged, otherwise the neighbour which is currently speaker
 * (according to the ping strategy).
 *
 * Returns true if a new neighbour is the new speaker. The state has changed to listener.
 */
wnet_reqrsp_t process_ping(void) {
	wnet_neighbour_t *pos;
	wnet_reqrsp_t ping_type;
	bool old;
	pending_beacon_t *beacon;

	old = neighbour_list_protection(true);

	ping_type = WNET_NONE;

	if ((beacon = next_beacon(WNET_BEACON_PING))) {

		ping_type = beacon->this->cause;

		if (ping_type == WNET_REQUEST) {

			/* Because of the PING REQUEST strategy, we know that the other is less than us in all case, so
			 * we abort our sending as speaker.
			 */

			wnet_trace("[soo:soolink:winenet:ping] %s: (state %s) processing ping request... from ", __func__, get_current_state_str());
			soo_log_printlnUID(beacon->agencyUID_from);

			pos = find_neighbour(beacon->agencyUID_from);

			/* Already got by our Discovery? If no, he will have to re-send a ping request later. */
			if (pos) {
				wnet_trace("[soo:soolink:winenet:ping] %s: neighbour VALID. Its UID: ", __func__);
				soo_log_printlnUID(beacon->agencyUID_from);

				pos->valid = true;

				/* Send a PING RESPONSE beacon
				 * But okay, we stay in our current state.
				 */
				wnet_trace("[soo:soolink:winenet:ping] Sending PING_RESPONSE to ");
				soo_log_printlnUID(beacon->agencyUID_from);

				__winenet_send_beacon(beacon->agencyUID_from, WNET_BEACON_PING, WNET_RESPONSE);
			}

			beacon_del(beacon);

		} else if (ping_type == WNET_RESPONSE) {

			wnet_trace("[soo:soolink:winenet:ping] (state %s) processing ping response...\n", get_current_state_str());

			pos = find_neighbour(beacon->agencyUID_from);

			/* Sanity check (we got a response to our request) */
			BUG_ON(!pos);

			pos->valid = true;

			beacon_del(beacon);
		}
	}

	neighbour_list_protection(old);

	return ping_type;
}

/*
 * Clear spurious ack which may arrive after a ack timeout.
 * We simply ignore it (but we need to clear the beacon event.
 */
void clear_spurious_ack(void) {
	pending_beacon_t *beacon;

	while ((beacon = next_beacon(WNET_BEACON_ACKNOWLEDGMENT)))
		beacon_del(beacon);
}

/**
 * Send ACKNOWLEDGMENT beacon with the corresponding status
 *
 * @param agencyUID
 * @param status
 * @param pkt_data
 */
void wnet_send_ack(uint64_t agencyUID, int status, bool pkt_data) {
	neighbour_state_t neighbour_state;

	retrieve_current_neighbour_state(current_soo_winenet->ourself, &neighbour_state);

	/* Additional field to complete for PKT_DATA */
	if (pkt_data) {

		neighbour_state.pkt_data = true;
		neighbour_state.transID = current_soo_winenet->wnet_rx.transID & WNET_MAX_PACKET_TRANSID;

	}

	____winenet_send_beacon(agencyUID, WNET_BEACON_ACKNOWLEDGMENT, status, &neighbour_state);
}

/**
 * Wait for an acknowledgment beacon. Various events may happen during this while:
 * either the corresponding is received before the timeout, and ACK_STATUS_OK is returned,
 * or a timeout expires (ACK_STATUS_TIMEOUT). If another beacon received from the sender who
 * should send the ACK, the beacon simply remains in the pending beacon list.
 *
 * @return 0 the ack has been successfully received.
 * 	  -1 in case of timeout
 * 	   1 in case of another beacon received instead of ack
 */
static wnet_ack_status_t wait_for_ack(void) {
	unsigned long remaining = msecs_to_jiffies(WNET_ACK_TIMEOUT_MS);
	wnet_ack_status_t ret_ack = ACK_STATUS_TIMEOUT;
	wnet_neighbour_t *pos;
	neighbour_state_t neighbour_state;
	pending_beacon_t *beacon;

retry_waitack:

	/* Wait on the acknowledgement during a certain time (timeout) */
	remaining = wait_for_completion_timeout(&current_soo_winenet->wnet_event, remaining);

	if (remaining > 0) {

		/* Make sure we have not been woken up by a simple complete on our wnet_event completion */
		while ((beacon = next_beacon(WNET_BEACON_ACKNOWLEDGMENT))) {

			if (current_soo_winenet->current_targetUID == beacon->agencyUID_from) {

				/* Update the neighbour state. */
				pos = find_neighbour(beacon->agencyUID_from);
				if (pos) {
					memcpy(&neighbour_state, beacon->this->priv, sizeof(neighbour_state_t));

					/* Update the neighbour state */
					update_current_neighbour_state(pos, &neighbour_state);
				}

				switch (beacon->this->cause) {
				case ACK_STATUS_ABORT:

					ret_ack = ACK_STATUS_ABORT;
					break;

				case ACK_STATUS_OK:

					/* If we wait for an ack regarding PKT_DATA, we ensure that the ack concerns the right frame. */
					if (!neighbour_state.pkt_data ||
						(neighbour_state.transID == (current_soo_winenet->wnet_tx.transID & WNET_MAX_PACKET_TRANSID)))
					{
						/* OK - We got a correct acknowledgment, we set the cause for further processing. */
						ret_ack = ACK_STATUS_OK;
					} else {
						/* A spurious ack probably... */
						beacon_del(beacon);
						goto retry_waitack;
					}
					break;

				default:
					/* Should not happen... */
					wnet_trace("*** Invalid ACK beacon cause %d...\n", beacon->this->cause);
					BUG();
				}

				/* Event processed */
				beacon_del(beacon);
				goto out;

			} else
				/* Considered as spurious ack */
				beacon_del(beacon);
		}

		/* If we are waiting for an ACK on GO_SPEAKER or BROADCAST_SPEAKER, we abort
		 * all incoming beacons of these types.
		 */

		if (!neighbour_state.pkt_data) {
			while ((beacon = next_beacon(WNET_BEACON_GO_SPEAKER | WNET_BEACON_BROADCAST_SPEAKER))) {
				pos = find_neighbour(beacon->agencyUID_from);

				if (pos) {
					if (current_soo_winenet->ourself->randnr > pos->randnr)
						wnet_send_ack(beacon->agencyUID_from, ACK_STATUS_ABORT, false);
				} else
					wnet_send_ack(beacon->agencyUID_from, ACK_STATUS_ABORT, false);

				beacon_del(beacon);
			}
		}

		/* The beacon remains in the pending beacon list */
		goto retry_waitack;

	} else {

		/* The timeout has expired (remaining == 0) */;

		wnet_trace("[soo:soolink:winenet:ack] !!!!! ACK timeout... will retry to ");
		soo_log_printlnUID(current_soo_winenet->current_targetUID);

		ret_ack = ACK_STATUS_TIMEOUT;
	}

out:
	return ret_ack;
}

/**
 * Sending a QUERY_STATE request or response with our current state including
 * the random number for helping in case of contention.
 * If we receive the same request from our target, we examine the randnr value
 * to determine which is aborting the transaction. If we have a higher value,
 * we abort the target smart object and re-initiate the request, otherwise
 * we leave without other effect.
 *
 * @param reqrsp
 * @param wnet_neighbour
 */
void neighbour_query(wnet_neighbour_t *wnet_neighbour, wnet_reqrsp_t reqrsp) {
	bool old;

	wnet_trace("[soo:soolink:winenet:query] Sending a QUERY_STATE (%s) to ", ((reqrsp == WNET_REQUEST) ? "REQUEST" : "RESPONSE"));
	soo_log_printlnUID(wnet_neighbour->neighbour->agencyUID);

	/* Ask the general state for this neighbour */
	old = neighbour_list_protection(true);

	/* Ask the general state for this neighbour */
	__winenet_send_beacon(wnet_neighbour->neighbour->agencyUID, WNET_BEACON_QUERY_STATE, reqrsp);

	neighbour_list_protection(old);
}

/**
 * We got a QUERY_STATE beacon and we need to examine the query: in case of request, we simply
 * return WNET_REQUEST with associate neighbour, otherwise we update the information about
 * the neighbour state and return the WNET_RESPONSE with associate (updated) neighbour.
 *
 * @param wnet_neighbour  	Contain the updated neighbour (in response)
 * @return wnet_reqrsp_t	REQUEST, RESPONSE or NONE if not a valid processing
 */
wnet_reqrsp_t handle_neighbour_query(wnet_neighbour_t **wnet_neighbour) {
	bool old;
	wnet_neighbour_t *pos;
	int cause;
	pending_beacon_t *beacon;

	if (wnet_neighbour)
		*wnet_neighbour = NULL;

	old = neighbour_list_protection(true);

	if ((beacon = next_beacon(WNET_BEACON_QUERY_STATE))) {

		pos = find_neighbour(beacon->agencyUID_from);
		if (pos && wnet_neighbour)
			*wnet_neighbour = pos;

		cause = beacon->this->cause;

		beacon_del(beacon);
		neighbour_list_protection(old);

		return cause;
	}

	neighbour_list_protection(old);

	return WNET_NONE;
}

/**
 * General check on the arrival of beacons which may happen during *stable* states.
 *
 * @return true if a beacon as been processed, false otherwise.
 */
static void check_for_query_or_ping(void) {
	wnet_neighbour_t *wnet_neighbour;
	wnet_reqrsp_t reqrsp;

	reqrsp = handle_neighbour_query(&wnet_neighbour);

	if (reqrsp != WNET_NONE) {

		if ((reqrsp == WNET_REQUEST) && wnet_neighbour)
			neighbour_query(wnet_neighbour, WNET_RESPONSE);

	}

	/* Check for ping */
	process_ping();
}

/**
 * A general function to check incoming beacons and to perform related housekeeping.
 *
 * This function is called only from the SPEAKER state.
 *
 */
static void s_check_for_beacons(void) {
	pending_beacon_t *beacon;

	/* We check the presence of beacon since this function is called from the FSM states, not
	 * necessary after receiving a ACK_STATUS_BEACON.
	 */
	if ((beacon = next_beacon(WNET_BEACON_GO_SPEAKER | WNET_BEACON_BROADCAST_SPEAKER))) {

		wnet_send_ack(beacon->agencyUID_from, ACK_STATUS_ABORT, false);
		beacon_del(beacon);

	}

	check_for_query_or_ping();
}


/**
 * Contact the next speaker in our neighborhoud.
 * At this point, the neighbour protection must be disabled.
 * This function is not re-entrant; it is called only once at a time.
 */
void s_go_next_speaker(void) {
	wnet_neighbour_t *next_speaker;
	wnet_ack_status_t ack;
	int retry_count;
	bool old;

	old = neighbour_list_protection(true);

	current_soo_winenet->ourself->paired_speaker = 0;

next_speaker:

	/*
	 * Check if the current_speaker has been removed in the meanwhile.
	 * Used to guarantee that all neighbours have a chance to get speaker, even
	 * if the neighbour is not visible by all other neighbours (hidden node).
	 */
	if (current_soo_winenet->__current_speaker == NULL)
		current_soo_winenet->__current_speaker = current_soo_winenet->ourself;

	next_speaker = next_valid_neighbour(current_soo_winenet->__current_speaker, false);

	/* Okay, if next_speaker should be the same :-) because we have only this neighbour...
	 * this function will return NULL in this case... so we do a special treatment.
	 */

	if (!next_speaker)
		next_speaker = next_valid_neighbour(current_soo_winenet->ourself, false);

	if (!next_speaker) {

		/* Reset our speakerUID */
		current_soo_winenet->__current_speaker = 0;
		current_soo_winenet->ourself->paired_speaker = 0;

		neighbour_list_protection(old);

		change_state(WNET_STATE_IDLE);
		return ;
	}

	/* Well, the current speaker is known */
	current_soo_winenet->__current_speaker = next_speaker;

	/* Okay, we pair ourself to this speaker */
	current_soo_winenet->ourself->paired_speaker = next_speaker->neighbour->agencyUID;

	retry_count = 0;
	do {
		/* Now send the beacon */
		winenet_send_beacon(next_speaker->neighbour->agencyUID, WNET_BEACON_GO_SPEAKER, 0);

		ack = wait_for_ack();

		if (ack == ACK_STATUS_ABORT) {
			wnet_trace("[soo:soolink:winenet:ack] got a STATUS ABORT on GO_SPEAKER\n");

			current_soo_winenet->ourself->paired_speaker = 0;
			goto out;
		}

		if (ack == ACK_STATUS_TIMEOUT) {
			retry_count++;
			wnet_trace("[soo:soolink:winenet:ack] retry_count = %d\n", retry_count);
		}

	} while ((ack != ACK_STATUS_OK) && (retry_count <= WNET_RETRIES_MAX));

	if (ack == ACK_STATUS_TIMEOUT) {
		/*
		 * Well, it seems we have a bad guy as neighbour :-(
		 * Just invalidate it and pick-up a new speaker.
		 * Please note the the list managed by the Discovery is not impacted.
		 * If the neighbour is really down, it will disappear very soon by Discovery.
		 */

		next_speaker->valid = false;

		goto next_speaker;
	}

out:
	neighbour_list_protection(old);

	change_state(WNET_STATE_LISTENER);
}

/**
 * Send a BROADCAST_SPEAKER to a neighbour and check for a valid acknowledgment.
 *
 * @param 	neighbour
 * @param 	arg
 * @return	true or false according to the necessity to proceed with the iterator.
 */
bool s_broadcast_speaker(wnet_neighbour_t *neighbour, void *arg) {
	wnet_ack_status_t ack;
	int retry_count;

	if (neighbour->neighbour->agencyUID == current_soo_winenet->ourself->neighbour->agencyUID)
		return true;

	/* Send the beacon requiring an acknowledgement */
	retry_count = 0;
	do {
		/* Now send the beacon */
		winenet_send_beacon(neighbour->neighbour->agencyUID, WNET_BEACON_BROADCAST_SPEAKER, 0);

		ack = wait_for_ack();

		if (ack == ACK_STATUS_ABORT) {

			wnet_trace("[soo:soolink:winenet:ack] got a STATUS ABORT\n");

			/* Unpair all listener paired so far. */
			iterate_on_neighbours(true, s_unpair_neighbour, NULL);

			current_soo_winenet->ourself->paired_speaker = 0;

			change_state(WNET_STATE_LISTENER);

			return false;
		}

		if (ack == ACK_STATUS_TIMEOUT) {
			retry_count++;
			wnet_trace("[soo:soolink:winenet:ack] retry_count = %d\n", retry_count);
		}


	} while ((ack != ACK_STATUS_OK) && (retry_count <= WNET_RETRIES_MAX));

	/* Timeout ? */
	if (ack == ACK_STATUS_TIMEOUT) {

		/*
		 * Well, it seems we have a bad guy as neighbour :-(
		 * Just go on with another neighbour.
		 */

		neighbour->valid = false;

		if (next_valid_neighbour(NULL, false) == NULL) {

			current_soo_winenet->ourself->paired_speaker = 0;

			change_state(WNET_STATE_IDLE);

			return false;
		}
	}

	/* Continue iterating */
	return true;
}

/**
 * Function called by the iterator to perform sending of a frame to a specific neighbour.
 * Called from Speaker state (s_).
 *
 * @param neighbour to which the frame has to be sent out.
 * @param arg
 * @return boolean to indicated if the iterator must pursue or not.
 */
bool s_frame_tx(wnet_neighbour_t *neighbour, void *arg) {
	int i;
	wnet_ack_status_t ack;
	int retry_count;

	/* Must be paired with us */
	if (neighbour->paired_speaker != current_soo_winenet->ourself->neighbour->agencyUID)
		return true; /* Skip this neighbour and continue iterating...*/

	/* Set the destination */
	current_soo_winenet->wnet_tx.sl_desc->agencyUID_to = neighbour->neighbour->agencyUID;
	current_soo_winenet->current_targetUID = neighbour->neighbour->agencyUID;

	wnet_trace("[soo:soolink:winenet] ---------------------------------------------------Transmitting frame to ");
	soo_log_printlnUID(neighbour->neighbour->agencyUID);

	/* We have to transmit over all smart objects */
	/* Sending the frame for the first time (first listener) */

	for (i = 0; ((i < WNET_N_PACKETS_IN_FRAME) && (current_soo_winenet->buf_tx_pkt[i]->packet_type != TRANSCEIVER_PKT_NONE)); i++)
		__sender_tx(current_soo_winenet->wnet_tx.sl_desc, current_soo_winenet->buf_tx_pkt[i]);

	/* Each frame must be acknowledged by the receiver. */
	ack = wait_for_ack();

	if ((ack == ACK_STATUS_TIMEOUT) || (ack == ACK_STATUS_ABORT)) {

		retry_count = 0;
		do {
			/* Re-send the whole frame */
			for (i = 0; ((i < WNET_N_PACKETS_IN_FRAME) && (current_soo_winenet->buf_tx_pkt[i]->packet_type != TRANSCEIVER_PKT_NONE)); i++)
				__sender_tx(current_soo_winenet->wnet_tx.sl_desc, current_soo_winenet->buf_tx_pkt[i]);

			ack = wait_for_ack();

			if (ack == ACK_STATUS_TIMEOUT) {
				retry_count++;
				wnet_trace("[soo:soolink:winenet:ack] retry_count = %d\n", retry_count);
			}

		} while ((ack != ACK_STATUS_OK) && (retry_count <= WNET_RETRIES_MAX));

		if (ack == ACK_STATUS_TIMEOUT) {
			/*
			 * Well, it seems we have a bad guy as neighbour :-(
			 * Just remove it and proceed with the next listener, i.e. lets proceed
			 * with the broadcast to other neighbours.
			 */

			neighbour->valid = false;
		}
	}

	return true;
}

/** Start of FSM management **/


/*************************************************** WNET_STATE_INIT ****************************************************/

static void winenet_state_init(wnet_state_t old_state) {

	wnet_trace("[soo:soolink:winenet] Now in state INIT\n");

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
	bool old;
	pending_beacon_t *beacon;

	wnet_trace("[soo:soolink:winenet:state:idle] Now in state IDLE\n");

	if (current_soo_winenet->last_state != WNET_STATE_IDLE) {
		wnet_trace("[soo:soolink:winenet:state:idle] Smart object ");
		soo_log_printlnUID(current_soo->agencyUID);
		wnet_trace(" -- Now in state IDLE\n");
		wnet_trace(" -- paired speaker = "); soo_log_printlnUID(current_soo_winenet->ourself->paired_speaker);

		current_soo_winenet->last_state = WNET_STATE_IDLE;
	}

retry:

	if (list_empty(&current_soo_winenet->pending_beacons))
		/* Waiting on a first neighbor at least. */
		wait_for_completion(&current_soo_winenet->wnet_event);

	old = neighbour_list_protection(true);

	if (process_ping() != WNET_NONE) {

		wnet_neighbour = next_neighbour(NULL, true, false);
		BUG_ON(!wnet_neighbour);

		current_soo_winenet->ourself->randnr = get_random_int();

		/* Query our first neighbour to see the global state, i.e. if it is necessary to
		 * determine a speaker.
		 */
		neighbour_query(wnet_neighbour, WNET_REQUEST);

		change_state(WNET_STATE_LISTENER);

	} else {

		if ((beacon = next_beacon(WNET_BEACON_ANY)))
			beacon_del(beacon);

		/* Just ignore other beacons which will not be processed here. */
		neighbour_list_protection(old);
		goto retry;
	}

	neighbour_list_protection(old);
}

/**************************** WNET_STATE_SPEAKER *****************************/

static void winenet_state_speaker(wnet_state_t old_state) {
	int count;
	bool __broadcast_done = false;
	bool first = true;
	bool old;

	wnet_trace("[soo:soolink:winenet:state:speaker] Now in state SPEAKER\n");

	if (current_soo_winenet->last_state != WNET_STATE_SPEAKER) {
		wnet_trace("[soo:soolink:winenet:state:speaker] Smart object ");
		soo_log_printlnUID(current_soo->agencyUID);
		wnet_trace(" -- Now in state SPEAKER\n");

		current_soo_winenet->last_state = WNET_STATE_SPEAKER;
	}

	BUG_ON(current_soo_winenet->ourself->paired_speaker != current_soo_winenet->ourself->neighbour->agencyUID);

	while (true) {

		/* We keep synchronized with our producer or be ready to process beacons. */

		/* As we enter in this state for the first time, we can process what is pending (beacon
		 * or data along the tx path).
		 */
		if (!first && list_empty(&current_soo_winenet->pending_beacons))
			wait_for_completion(&current_soo_winenet->wnet_event);

		old = neighbour_list_protection(true);

		first = false;

		clear_spurious_ack();

		/* Is it an end of transmission? */
		if (current_soo_winenet->wnet_tx.pending == TX_DATA_COMPLETED) {

			iterate_on_neighbours(true, s_unpair_neighbour, NULL);

			s_go_next_speaker();

			neighbour_list_protection(old);

			winenet_xmit_data_processed(TX_DATA_COMPLETED);

			return ;
		}

		s_check_for_beacons();

		/* Any data to send on this transmission? */
		if (current_soo_winenet->wnet_tx.pending == TX_NO_DATA) {

			iterate_on_neighbours(true, s_unpair_neighbour, NULL);

			/* Send a go_speaker beacon to the next speaker. */
			s_go_next_speaker();

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

			iterate_on_neighbours(true, s_broadcast_speaker, NULL);

			if (get_state() != WNET_STATE_SPEAKER) {
				neighbour_list_protection(old);
				return ;
			}

			__broadcast_done = true;
		}

		if (current_soo_winenet->wnet_tx.pending == TX_DATA_READY) {

			/* Proceed with sending the frame to all neighbours */

			count = iterate_on_neighbours(true, s_frame_tx, NULL);
			if (!count) {

				/* Reset the TX trans ID */
				current_soo_winenet->sent_packet_transID = 0;

				current_soo_winenet->ourself->paired_speaker = 0;

				change_state(WNET_STATE_IDLE);

				clear_buf_tx_pkt();
				winenet_xmit_data_processed(-EIO);

				neighbour_list_protection(old);
				return ;

			}

			/* End of sending */
			clear_buf_tx_pkt();
			winenet_xmit_data_processed(TX_DATA_IN_PROGRESS);

		}

		neighbour_list_protection(old);
	}
}

/**************************** WNET_STATE_LISTENER *****************************/

static void winenet_state_listener(wnet_state_t old_state) {
	wnet_neighbour_t *wnet_neighbour;
	bool first = true, old;
	wnet_reqrsp_t reqrsp;
	pending_beacon_t *beacon;
	unsigned long remaining = WNET_LISTENER_TIMEOUT_MS;

	wnet_trace("[soo:soolink:winenet:state:listener] Now in state LISTENER\n");

	if (current_soo_winenet->last_state != WNET_STATE_LISTENER) {
		wnet_trace("[soo:soolink:winenet:state:listener] Smart object ");
		soo_log_printlnUID(current_soo->agencyUID);
		wnet_trace(" -- Now in state LISTENER\n");

		current_soo_winenet->last_state = WNET_STATE_LISTENER;
	}

	while (true) {

		if (!first && list_empty(&current_soo_winenet->pending_beacons))
			remaining = wait_for_completion_timeout(&current_soo_winenet->wnet_event, msecs_to_jiffies(WNET_LISTENER_TIMEOUT_MS));

		/* Timeout? */
		if (remaining == 0) {

			wnet_trace("[soo:soolink:winenet:beacon] ----------------------- Listener state out by TIMEOUT \n");

			/* Trigger the sending of a QUERY_REQUEST to get the state of our neighbour */
			if (current_soo_winenet->ourself->paired_speaker)
				wnet_neighbour = find_neighbour(current_soo_winenet->ourself->paired_speaker);
			else
				wnet_neighbour = next_valid_neighbour(NULL, false);

			if (wnet_neighbour)
				neighbour_query(wnet_neighbour, WNET_REQUEST);

			remaining = WNET_LISTENER_TIMEOUT_MS;

			continue;
		}

		old = neighbour_list_protection(true);

		first = false;

		clear_spurious_ack();

		/* Check if we are alone? */
		if (next_valid_neighbour(NULL, false) == NULL) {

			/* Reset the speakerUID */
			current_soo_winenet->ourself->paired_speaker = 0;
			change_state(WNET_STATE_IDLE);

			neighbour_list_protection(old);
			return ;
		}

		/* Handle the QUERY_STATE request or response if any. */

		reqrsp = handle_neighbour_query(&wnet_neighbour);

		if (reqrsp != WNET_NONE) {

			/*
			 * We become SPEAKER in the following conditions:
			 *
			 *   - We have NO paired speaker *AND*
			 *
			 *     - our randnr is greater than the neighbour's one *AND*
			 *
			 *       - the neighbour has no paired speaker *OR*
			 *       - the neighbour has a paired_speaker *AND* this paired_speaker is ourself.
			 *
			 * We release our paired speaker this one is not speaker itself (or if it is not paired anymore).
			 *
			 * All other cases are considered as "normal".
			 */

			if (wnet_neighbour) {

				if (!current_soo_winenet->ourself->paired_speaker &&
				     (current_soo_winenet->ourself->randnr > wnet_neighbour->randnr) &&
				       (!wnet_neighbour->paired_speaker ||
				        (wnet_neighbour->paired_speaker == current_soo_winenet->ourself->neighbour->agencyUID))) {

						current_soo_winenet->ourself->paired_speaker = current_soo_winenet->ourself->neighbour->agencyUID;

						if (reqrsp == WNET_REQUEST)
							neighbour_query(wnet_neighbour, WNET_RESPONSE);

						change_state(WNET_STATE_SPEAKER);

						neighbour_list_protection(old);

						return ;
				}

				/* We performed the test only if the response comes from our speaker. */
				if ((current_soo_winenet->ourself->paired_speaker == wnet_neighbour->neighbour->agencyUID) &&
					(!wnet_neighbour->paired_speaker || (wnet_neighbour->paired_speaker != wnet_neighbour->neighbour->agencyUID))) {

					current_soo_winenet->ourself->paired_speaker = 0;
				}
				if (reqrsp == WNET_REQUEST)
					neighbour_query(wnet_neighbour, WNET_RESPONSE);
			}
		}

		/* A ping request may happen when we are listening. */
		process_ping();

		/*
		 * When a new SOO appears in the neighbourhood, it can consider ourself as alone and may become speaker;
		 * it then sends a BROADCAST_SPEAKER. The same thing may appear with a SOO sending a GO_SPEAKER.
		 * So, if we receive such beacons, we are checking if we are in the process of receiving a buffer and if it is
		 * the case, we do not acknowledge the beacon.
		 */
		if ((beacon = next_beacon(WNET_BEACON_GO_SPEAKER))) {

			if (current_soo_winenet->ourself->paired_speaker &&
			    (current_soo_winenet->ourself->paired_speaker != beacon->agencyUID_from)) {

				wnet_send_ack(beacon->agencyUID_from, ACK_STATUS_ABORT, false);

				beacon_del(beacon);
				goto listener_cont;

			}

			/* Our turn... */

			current_soo_winenet->ourself->paired_speaker = current_soo_winenet->ourself->neighbour->agencyUID;

			wnet_send_ack(beacon->agencyUID_from, ACK_STATUS_OK, false);

			change_state(WNET_STATE_SPEAKER);

			beacon_del(beacon);
			neighbour_list_protection(old);

			return ;

		}

		if ((beacon = next_beacon(WNET_BEACON_BROADCAST_SPEAKER))) {

			wnet_neighbour = find_neighbour(beacon->agencyUID_from);
			if (!wnet_neighbour) {

				/* Should normally never happen, but who knows... */
				beacon_del(beacon);
				goto listener_cont;
			}

			/* Unpairing? */
			if (beacon->this->cause == ACK_STATUS_ABORT) {
				current_soo_winenet->ourself->paired_speaker = 0;

				beacon_del(beacon);
				goto listener_cont;
			}

			if (current_soo_winenet->ourself->paired_speaker &&
			    (beacon->agencyUID_from != current_soo_winenet->ourself->paired_speaker)) {

				wnet_send_ack(beacon->agencyUID_from, ACK_STATUS_ABORT, false);

				beacon_del(beacon);
				goto listener_cont;
			}

			/* Now it is the new speaker. */
			current_soo_winenet->ourself->paired_speaker = wnet_neighbour->neighbour->agencyUID;

			/* We are ready to listen to this speaker. */
			wnet_send_ack(beacon->agencyUID_from, ACK_STATUS_OK, false);

			beacon_del(beacon);
		}

listener_cont:
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

	wnet_trace("[soo:soolink:winenet:state] ***** Changing state from %s to %s\n", winenet_get_state_str(handle->state), winenet_get_state_str(new_state));

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

	wnet_trace("[soo:soolink:winenet:state] Entering Winenet FSM task...\n");

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

			current_soo_winenet->wnet_tx.pending = TX_DATA_COMPLETED;

			complete(&current_soo_winenet->wnet_event);

			wnet_trace("[soo:soolink:winenet] DATA COMPLETED %s waiting on xmit_event...\n", __func__);

			/* Wait until the FSM has processed the data. */
			wait_for_completion(&current_soo_winenet->wnet_tx.xmit_event);

			wnet_trace("[soo:soolink:winenet] %s Okay, COMPLETED processed, ready to go ahead...\n", __func__);
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
	memcpy(current_soo_winenet->buf_tx_pkt[(packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME],
		packet, packet->size + sizeof(transceiver_packet_t));

	/* Look for a completed frame (max number of packets reached or completed) */
	if (current_soo_winenet->sent_packet_transID % WNET_N_PACKETS_IN_FRAME == 0) {

		/* Fill the TX request parameters */
		current_soo_winenet->wnet_tx.sl_desc = sl_desc;
		current_soo_winenet->wnet_tx.transID = packet->transID;

		/* Now, setting tx_pending to true will allow the speaker to send out */
		current_soo_winenet->wnet_tx.pending = TX_DATA_READY;

		complete(&current_soo_winenet->wnet_event);

		wnet_trace("[soo:soolink:winenet] %s DATA Packet ready to be sent, waiting on xmit_event...\n", __func__);

		/* Wait until the packed has been sent out. */
		wait_for_completion(&current_soo_winenet->wnet_tx.xmit_event);

		wnet_trace("[soo:soolink:winenet] DATA processed, ready to go ahead...\n");

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
	pending_beacon_t *pending_beacon;
	neighbour_state_t neighbour_state;
	wnet_neighbour_t *neighbour;

	current_soo_winenet->wnet_rx.sl_desc = sl_desc;
	current_soo_winenet->wnet_rx.transID = packet->transID;

	if (packet->packet_type == TRANSCEIVER_PKT_DATALINK) {

		mutex_lock(&current_soo_winenet->pending_beacons_lock);

		pending_beacon = kzalloc(sizeof(pending_beacon_t), GFP_KERNEL);
		BUG_ON(!pending_beacon);

		pending_beacon->this = kzalloc(packet->size, GFP_KERNEL);
		BUG_ON(!pending_beacon->this);

		memcpy(pending_beacon->this, packet->payload, packet->size);
		pending_beacon->agencyUID_from = current_soo_winenet->wnet_rx.sl_desc->agencyUID_from;

		wnet_trace("[soo:soolink:winenet:beacon] (state %s) Receiving beacon from %s cause %d\n",
			wnet_str_state(), beacon_str(pending_beacon->this, pending_beacon->agencyUID_from), pending_beacon->this->cause);

		/* Add this beacon to the tail of pending beacons and raise up the event */
		list_add_tail(&pending_beacon->list, &current_soo_winenet->pending_beacons);

		/* Update the neighbour state */

		neighbour = find_neighbour(pending_beacon->agencyUID_from);
		if (neighbour) {
			memcpy(&neighbour_state, pending_beacon->this->priv, sizeof(neighbour_state_t));

			/* Update the neighbour state */
			update_current_neighbour_state(neighbour, &neighbour_state);
		}

		mutex_unlock(&current_soo_winenet->pending_beacons_lock);

		complete(&current_soo_winenet->wnet_event);
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
		if (current_soo_winenet->wnet_rx.sl_desc->agencyUID_from != current_soo_winenet->ourself->paired_speaker) {

			wnet_trace("[soo:soolink:winenet] Skipping SOO ");
			soo_log_printUID(current_soo_winenet->wnet_rx.sl_desc->agencyUID_from);
			wnet_trace("    paired speaker: ");
			soo_log_printlnUID(current_soo_winenet->ourself->paired_speaker);

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

		/* First frame ? */
		if (packet->transID == 0)
			current_soo_winenet->expected_transID = 0;

		if ((packet->transID & WNET_MAX_PACKET_TRANSID) < current_soo_winenet->expected_transID) {

			/* A packet we already received; might happen if a ACK has not been received by the speaker. */

			if (((packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME == WNET_N_PACKETS_IN_FRAME - 1) || (packet->transID & WNET_LAST_PACKET))
				wnet_send_ack(current_soo_winenet->wnet_rx.sl_desc->agencyUID_from, ACK_STATUS_OK, true);
			else {
				wnet_send_ack(current_soo_winenet->wnet_rx.sl_desc->agencyUID_from, ACK_STATUS_ABORT, true);

				/* Reset to the beginning of the frame */
				current_soo_winenet->expected_transID = (current_soo_winenet->expected_transID / WNET_N_PACKETS_IN_FRAME) * WNET_N_PACKETS_IN_FRAME;
			}

			return ;


		} else if (packet->transID && ((packet->transID & WNET_MAX_PACKET_TRANSID) != current_soo_winenet->expected_transID)) {

			/* The packet chain is broken (bad transmission)  */
			wnet_send_ack(current_soo_winenet->wnet_rx.sl_desc->agencyUID_from, ACK_STATUS_ABORT, true);

			/* Reset to the beginning of the frame */
			current_soo_winenet->expected_transID = (current_soo_winenet->expected_transID / WNET_N_PACKETS_IN_FRAME) * WNET_N_PACKETS_IN_FRAME;


			return ;
		}

		/* Copy the packet into the bufferized packet array */
		memcpy(current_soo_winenet->buf_rx_pkt[(packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME], packet, packet->size + sizeof(transceiver_packet_t));

		/* Save the last ID of the last received packet */
		current_soo_winenet->expected_transID = (packet->transID & WNET_MAX_PACKET_TRANSID) + 1;

		/* Time to rethink to a better way... ? */
		BUG_ON(current_soo_winenet->expected_transID == WNET_MAX_PACKET_TRANSID);

		/* If all the packets of the frame have been received, forward them to the upper layer */
		if (((packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME == WNET_N_PACKETS_IN_FRAME - 1) || (packet->transID & WNET_LAST_PACKET)) {

			for (i = 0; ((i < WNET_N_PACKETS_IN_FRAME) && (current_soo_winenet->buf_rx_pkt[i]->packet_type != TRANSCEIVER_PKT_NONE)); i++)
				receiver_rx(sl_desc, current_soo_winenet->buf_rx_pkt[i]);

			clear_buf_rx_pkt();

			/*
			 * Send an ACKNOWLEDGMENT beacon.
			 */
			wnet_send_ack(current_soo_winenet->wnet_rx.sl_desc->agencyUID_from, ACK_STATUS_OK, true);
		}
	}
}

void winenet_cancel_rx(sl_desc_t *sl_desc) {
	current_soo_winenet->ourself->paired_speaker = 0;
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

	INIT_LIST_HEAD(&current_soo_winenet->wnet_neighbours);
	INIT_LIST_HEAD(&current_soo_winenet->pending_beacons);

	init_completion(&current_soo_winenet->wnet_event);
	init_completion(&current_soo_winenet->wnet_tx.xmit_event);
	init_completion(&current_soo_winenet->data_event);

	current_soo_winenet->wnet_tx.pending = TX_NO_DATA;
	current_soo_winenet->expected_transID = 0;

	mutex_init(&current_soo_winenet->wnet_xmit_lock);
	mutex_init(&current_soo_winenet->pending_beacons_lock);

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
#else
#error !! Winenet SOOlink plugin undefined...
#endif /* !CONFIG_SOOLINK_PLUGIN_WLAN */

	BUG_ON(!current_soo_winenet->__sl_desc);

	datalink_register_protocol(SL_DL_PROTO_WINENET, &winenet_proto);

	current_soo_winenet->wnet_discovery_desc.add_neighbour_callback = winenet_add_neighbour;
	current_soo_winenet->wnet_discovery_desc.remove_neighbour_callback = winenet_remove_neighbour;
	current_soo_winenet->wnet_discovery_desc.update_neighbour_callback = winenet_update_neighbour;

	/* Register with Discovery as Discovery listener */
	discovery_listener_register(&current_soo_winenet->wnet_discovery_desc);

	winenet_start_fsm_task("Wnet", &current_soo_winenet->fsm_handle);

}
