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

//#define VERBOSE
//#define VERBOSE_DISCOVERY
//#define VERBOSE_STATE

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
#include <linux/mutex.h>
#include <linux/kthread.h>

#include <soo/soolink/soolink.h>

#include <soo/debug/bandwidth.h>

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

/* Store the "first elected" speaker of a round */
static agencyUID_t first_speakerUID;

static bool transmission_over = false;

/* Handle used in the FSM */
static wnet_fsm_handle_t fsm_handle;

static struct mutex wnet_xmit_lock;

/* Event used to track the receival of a Winenet beacon or a TX request */
//static rtdm_event_t wnet_event;
//static rtdm_event_t beacon_event;
//static rtdm_event_t data_event;

static struct completion wnet_event;
static struct completion beacon_event;
static struct completion data_event;


/* Internal SOOlink descriptor for handling beacons such as ping req/rsp/go_speaker/etc. */
static sl_desc_t *__sl_desc;

static wnet_tx_t wnet_tx;
static wnet_rx_t wnet_rx;

/* Management of the neighbourhood of SOOs. Used in the retry packet management. */
static struct list_head wnet_neighbours;
//static rtdm_mutex_t neighbour_list_lock;
static struct mutex neighbour_list_lock;

/* Each call to winenet_tx will increment the transID counter */
static uint32_t sent_packet_transID = 0;

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

#ifdef VERBOSE_STATE
static wnet_state_t last_state = WNET_STATE_N;
#endif

static int wait_for_ack(wnet_beacon_t *beacon);

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

	if (unlikely((beacon_id_int >= WNET_BEACON_N)))
		return invalid_str;

	return beacon_id_str[beacon_id_int];
}

static char __beacon_str[80];
char *beacon_str(wnet_beacon_t *beacon, agencyUID_t *uid) {
	char uid_str[80];
	int i;

	sprintf(__beacon_str, " Beacon %s (type: %s) / UID: ",
			winenet_get_beacon_id_str(beacon->id),
			((beacon->id == WNET_BEACON_PING) ? ping_type_str[(int) beacon->priv] : "n/a" ));

	/* Display the agency UID with the fourth first bytes (enough) */
	for (i = 0 ; i < 4 ; i++) {
		sprintf(uid_str, "%02x ", ((char *) uid)[i]);
		strcat(__beacon_str, uid_str);
	}

	strcat(__beacon_str, uid_str);

	return __beacon_str;
}

/**
 * Allow the producer to be informed about potential problems or to
 * send a next packet.
 */
void winenet_xmit_data_processed(int ret) {

	wnet_tx.pending = false;
	wnet_tx.ret = ret;

	if (ret < 0)
		wnet_tx.completed = true;

	/* Allow the producer to go further */
	//rtdm_event_signal(&wnet_tx.xmit_event);
	complete(&wnet_tx.xmit_event);

}

/**
 * Destroy the bufferized TX packets.
 * This function has to be called when a packet frame has been acknowledged, or if there is
 * an unexpected transition that requires the whole frame to be freed.
 */
static void clear_buf_tx_pkt(void) {
	uint32_t i;

	for (i = 0; i < WNET_N_PACKETS_IN_FRAME; i++) {
		if (buf_tx_pkt[i] != NULL) {
			kfree(buf_tx_pkt[i]);
			buf_tx_pkt[i] = NULL;
		}
	}
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

	mutex_lock(&neighbour_list_lock);

	/* Look for the remote SOO in the list */
	list_for_each(cur, &wnet_neighbours) {

		/* The agency UID is already known */
		neighbour_cur = list_entry(cur, wnet_neighbour_t, list);
		if (!cmpUID(&neighbour_cur->neighbour->agencyUID, agencyUID_from)) {

			if (((packet->transID & WNET_MAX_PACKET_TRANSID) > neighbour_cur->last_transID) ||
				((packet->transID & WNET_MAX_PACKET_TRANSID) == 0)) {
				neighbour_cur->last_transID = packet->transID & WNET_MAX_PACKET_TRANSID;

				mutex_unlock(&neighbour_list_lock);
				return false;
			}
			else {
				/* The packet has already been received */
				DBG("Agency UID found: ");
				DBG_BUFFER(&neighbour_cur->neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

				mutex_unlock(&neighbour_list_lock);
				return true;
			}
		}
	}

	/* This is an unknown agency UID. Will be integrated in the neighbour list very soon. */

	mutex_unlock(&neighbour_list_lock);

	return false;
}

/*
 * Get a reference to us in the neighbour list.
 */
static wnet_neighbour_t *ourself(void) {
	struct list_head *cur;
	static wnet_neighbour_t *wnet_neighbour = NULL;

	/* Spare time... */
	if (wnet_neighbour)
		return wnet_neighbour;

	mutex_lock(&neighbour_list_lock);

	list_for_each(cur, &wnet_neighbours) {
		wnet_neighbour = list_entry(cur, wnet_neighbour_t, list);

		if (!wnet_neighbour->neighbour->plugin) {
			mutex_unlock(&neighbour_list_lock);
			return wnet_neighbour;
		}
	}

	/* Not found? Abnormal situation... */
	BUG();
}

/*
 * Discard a transmission (sending packets).
 */
void discard_transmission(void) {
	/* Clear the TX pkt buffer */
	clear_buf_tx_pkt();

	/* Reset the TX trans ID */
	sent_packet_transID = 0;

	winenet_xmit_data_processed(-EIO);
}

/*
 * next_neighbour() is just a convenient way to get the next neighbour after a given neighbour, taking into account
 * ourself (not considered). It processes the list in a circular way.
 * There is at least one (not valid) entry regarding ourself (where plugin is NULL too).
 *
 * If argument pos is NULL, return the first valid neighbour if any.
 * Return NULL if there is no next neighbour anymore.
 *
 */
static wnet_neighbour_t *next_neighbour(wnet_neighbour_t *pos, bool valid) {
	wnet_neighbour_t *next = NULL;

	/* Sanity check */
	BUG_ON(list_empty(&wnet_neighbours));

	if (pos == NULL) {
		pos = list_first_entry(&wnet_neighbours, wnet_neighbour_t, list);

		/* circularity - pass the head of list */
		if (&pos->list == &wnet_neighbours)
			pos = list_next_entry(pos, list);

		if (!valid || pos->valid)
			return pos;
	}

	next = pos;
	do {
		next = list_next_entry(next, list);

		/* Skip head of the list */
		if (&next->list == &wnet_neighbours)
			next = list_next_entry(next, list);

		if ((next != pos) && (!valid || next->valid))
			return next;

	} while (next != pos);

	return NULL;
}

/*
 * Same as the previous function, but the neighbour must be valid, i.e. to have successfully processed a ping request.
 */

static wnet_neighbour_t *next_valid_neighbour(wnet_neighbour_t *pos) {
	return next_neighbour(pos, true);
}

/* Find a neighbour by its agencyUID */
wnet_neighbour_t *find_neighbour(agencyUID_t *agencyUID) {
	struct list_head *cur;
	static wnet_neighbour_t *wnet_neighbour = NULL;

	mutex_lock(&neighbour_list_lock);

	list_for_each(cur, &wnet_neighbours) {
		wnet_neighbour = list_entry(cur, wnet_neighbour_t, list);

		if (!cmpUID(&wnet_neighbour->neighbour->agencyUID, agencyUID)) {
			mutex_unlock(&neighbour_list_lock);
			return wnet_neighbour;
		}
	}
	mutex_unlock(&neighbour_list_lock);

	return NULL;
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
	wnet_beacon_t beacon;

	/* Ping has been received correctly and will be processed by the neighbor.
	 * If something goes wrong, the Discovery will detect and remove it.
	 * At the moment, we do not perform other keep-alive event.
	 */

	mutex_lock(&neighbour_list_lock);

	wnet_neighbour = kzalloc(sizeof(wnet_neighbour_t), GFP_ATOMIC);
	BUG_ON(!wnet_neighbour);

	wnet_neighbour->neighbour = neighbour;
	wnet_neighbour->last_transID = 0;

#ifdef VERBOSE_DISCOVERY /* Debug */
	lprintk("******************** Adding neighbour (our state is %s): ", get_current_state_str());
	printlnUID(&neighbour->agencyUID);
#endif

	/*
	 * We use the same sorting strategy than the
	 * Discovery to be consistent.
	 */

	/* If the list is empty, add the neighbour to it */
	if (list_empty(&wnet_neighbours)) {
		list_add_tail(&wnet_neighbour->list, &wnet_neighbours);

		change_state(WNET_STATE_IDLE);
		//rtdm_event_signal(&wnet_event);
		complete(&wnet_event);

		mutex_unlock(&neighbour_list_lock);

		return ;

	} else {

		/* Walk the list until we find the right place in ascending sort. */
		list_for_each(cur, &wnet_neighbours) {

			cur_neighbour = list_entry(cur, wnet_neighbour_t, list);
			ret = cmpUID(&wnet_neighbour->neighbour->agencyUID, &cur_neighbour->neighbour->agencyUID);

			if (ret < 0) {
				/* The new neighbour has an agencyUID greater than the current, hence insert it after */
				list_add_tail(&wnet_neighbour->list, cur);
				break;
			}
		}

		/* All UIDs are less than the new one */
		if (cur == &wnet_neighbours)
			list_add_tail(&wnet_neighbour->list, &wnet_neighbours);
	}

	mutex_unlock(&neighbour_list_lock);

	/* If we have the smaller agencyUID, we initiate the ping procedure. */
	if (cmpUID(&ourself()->neighbour->agencyUID, &neighbour->agencyUID) < 0)

		/* Trigger a ping procedure */
		winenet_send_beacon(&beacon, WNET_BEACON_PING, &neighbour->agencyUID, (void *) WNET_PING_REQUEST);

#ifdef VERBOSE_DISCOVERY
	winenet_dump_neighbours();
#endif

}

/**
 * Remove a neighbour from the neighbour list. As Winenet is a Discovery listener,
 * this function is called when a neighbour disappears.
 *
 * The neighbour might be already disappeared from our list, for example in case where some acknowledgment beacon
 * is missing, the neighbour is removed from this list (but not from the list managed by the Discovery).
 */
static void winenet_remove_neighbour(neighbour_desc_t *neighbour) {
	struct list_head *cur, *tmp;
	wnet_neighbour_t *next = NULL, *wnet_neighbour = NULL;

	/* Could be called from the non-RT context at the beginning of the agency_core
	 * (selection of neighbourhood).
	 */

#ifdef VERBOSE_DISCOVERY
	lprintk("******************** Removing neighbour (our state is %s): ", get_current_state_str());
	printlnUID(&neighbour->agencyUID);
#endif

	if (smp_processor_id() == AGENCY_RT_CPU)
		mutex_lock(&neighbour_list_lock);

	list_for_each_safe(cur, tmp, &wnet_neighbours) {
		wnet_neighbour = list_entry(cur, wnet_neighbour_t, list);
		if (!cmpUID(&wnet_neighbour->neighbour->agencyUID, &neighbour->agencyUID)) {

			/* Take the next, and check if we are at the end of the list */
			next = next_neighbour(wnet_neighbour, false);

			list_del(cur);
			kfree(wnet_neighbour);
			break;
		}
	}

	/* We found the corresponding wnet_neighbour */
	if (get_state() == WNET_STATE_LISTENER) {
		if (next) {

			if (next == ourself()) {

				/* We are the new speaker */
				ourself()->neighbour->priv = &ourself()->neighbour->agencyUID;

				change_state(WNET_STATE_SPEAKER);
				//rtdm_event_signal(&wnet_event); /* Get out of listener state */
				complete(&wnet_event);
			}

		} else
			//rtdm_event_signal(&wnet_event); /* Get out of listener state */
			complete(&wnet_event);

	}

	if (smp_processor_id() == AGENCY_RT_CPU)
		mutex_unlock(&neighbour_list_lock);

#ifdef VERBOSE_DISCOVERY
	winenet_dump_neighbours();
#endif
}

/**
 * Perform update of neighbour information (private data) when a Iamasoo packet is received.
 */
static void winenet_update_neighbour_priv(neighbour_desc_t *neighbour) {

	wnet_neighbour_t *wnet_neighbour;
	wnet_beacon_t beacon;

	/* First, we check if the neighbour is still in our (winenet) list of neighbours.
	 * If it is not the case, we (re-)add it into our list.
	 * Then, we examine the state IDLE.
	 */

#ifdef VERBOSE_DISCOVERY
	lprintk("******************** Updating neighbour (our state is %s): ", get_current_state_str());
	printlnUID(&neighbour->agencyUID);
#endif

	wnet_neighbour = find_neighbour(&neighbour->agencyUID);
	if (!wnet_neighbour)
		/* Try to re-add this neighbour if something went wrong... */
		winenet_add_neighbour(neighbour);
	else {
		/* This is a new neighbour. Proceed with the ping procedure. */
		if (!wnet_neighbour->valid) {

			/* If we have the smaller agencyUID, we initiate the ping procedure. */
			if (cmpUID(&ourself()->neighbour->agencyUID, &neighbour->agencyUID) < 0)

				/* Trigger a ping procedure */
				winenet_send_beacon(&beacon, WNET_BEACON_PING, &neighbour->agencyUID, (void *) WNET_PING_REQUEST);
		}
	}
#ifdef VERBOSE_DISCOVERY
	winenet_dump_neighbours();
#endif

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
	struct list_head *cur;
	wnet_neighbour_t *neighbour;

	list_for_each(cur, &wnet_neighbours) {

		neighbour = list_entry(cur, wnet_neighbour_t, list);

		neighbour->processed = false;
	}
}

/**
 * Dump the active neighbour list.
 */
void winenet_dump_neighbours(void) {
	struct list_head *cur;
	wnet_neighbour_t *neighbour;
	uint32_t count = 0;

	mutex_lock(&neighbour_list_lock);

	lprintk("***** List of neighbours:\n");

	/* There is no neighbour in the list, I am alone */
	if (list_empty(&wnet_neighbours)) {
		lprintk("No neighbour\n");

		mutex_unlock(&neighbour_list_lock);
		return;
	}

	list_for_each(cur, &wnet_neighbours) {

		neighbour = list_entry(cur, wnet_neighbour_t, list);

		lprintk("- Neighbour %d (valid: %d): ", count+1, neighbour->valid);
		printlnUID(&neighbour->neighbour->agencyUID);
		count++;
	}

	mutex_unlock(&neighbour_list_lock);
}

/**
 * Dump the current Winenet state.
 */
void winenet_dump_state(void) {
	lprintk("Winenet status: %s\n", state_str[get_state()]);
}

static discovery_listener_t wnet_discovery_desc = {
	.add_neighbour_callback = winenet_add_neighbour,
	.remove_neighbour_callback = winenet_remove_neighbour,
	.update_neighbour_priv_callback = winenet_update_neighbour_priv,
	.get_neighbour_priv_callback = winenet_get_neighbour_priv
};

/*
 * Unset the beacon ID *before* sending the signal.
 */
void beacon_clear(void) {

	/* Sanity check */
	BUG_ON(wnet_rx.last_beacon.id == WNET_BEACON_N);

	wnet_rx.last_beacon.id = WNET_BEACON_N;
	memcpy(&wnet_rx.sl_desc->agencyUID_from, get_null_agencyUID(), SOO_AGENCY_UID_SIZE);

	//rtdm_event_signal(&beacon_event);
	complete(&beacon_event);
}

/**
 * Send a Winenet beacon.
 * According to the kind of beacon, arg can be used to give a reference (assuming a known size) or
 * value of any type, opt for a integer.
 */
void winenet_send_beacon(wnet_beacon_t *outgoing_beacon, wnet_beacon_id_t beacon_id, agencyUID_t *dest_agencyUID, void *arg) {
	transceiver_packet_t *transceiver_packet;
	void *outgoing_packet = NULL;

	outgoing_beacon->id = beacon_id;

	memcpy(outgoing_beacon->agencyUID, dest_agencyUID, SOO_AGENCY_UID_SIZE);

	outgoing_beacon->priv = arg;

	/* Enforce the use of the a known SOOlink descriptor */
	BUG_ON(!__sl_desc);

	transceiver_packet = (transceiver_packet_t *) kmalloc(sizeof(transceiver_packet_t) + sizeof(wnet_beacon_t), GFP_ATOMIC);

	transceiver_packet->packet_type = TRANSCEIVER_PKT_DATALINK;
	transceiver_packet->transID = 0;

	memcpy(transceiver_packet->payload, outgoing_beacon, sizeof(wnet_beacon_t));

	outgoing_packet = (void *) transceiver_packet;

	memcpy(&__sl_desc->agencyUID_to, dest_agencyUID, SOO_AGENCY_UID_SIZE);

#ifdef VERBOSE
	lprintk("### Sending beacon %s\n", beacon_str(outgoing_beacon, &__sl_desc->agencyUID_to));
#endif
	__sender_tx(__sl_desc, outgoing_packet, sizeof(wnet_beacon_t), 0);

	/* Release the outgoing packet */
	if (outgoing_packet)
		kfree(outgoing_packet);
}

/*
 * Clear spurious ack which may arrive after a ack timeout.
 * We simply ignore it (but we need to clear the beacon event.
 */
bool clear_spurious_ack(void) {

	if (wnet_rx.last_beacon.id == WNET_BEACON_ACKNOWLEDGMENT) {
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
bool process_ping_is_speaker(void) {
	wnet_neighbour_t *pos;
	wnet_beacon_t beacon;

	if (wnet_rx.last_beacon.id == WNET_BEACON_PING) {

		if ((int) wnet_rx.last_beacon.priv == WNET_PING_REQUEST) {

			/* Because of the PING REQUEST strategy, we know that the other is less than us in all case, so
			 * we abort our sending as speaker and we
			 */
#ifdef VERBOSE_DISCOVERY
			lprintk("## %s: (state %s) processing ping request... from ", __func__, get_current_state_str());
			printlnUID(&wnet_rx.sl_desc->agencyUID_from);
#endif
			pos = find_neighbour(&wnet_rx.sl_desc->agencyUID_from);

			/* Already got by our Discovery? If no, he will have to re-send a ping request later. */
			if (pos) {
#ifdef VERBOSE_DISCOVERY
				lprintk("## %s: neighbour VALID. Its UID: ", __func__);
				printlnUID(&wnet_rx.sl_desc->agencyUID_from);
#endif
				pos->valid = true;

				/* This case needs refinement. Indeed, if the neighbour has a speakerUID, but not belonging to
				 * our neighbourhood, we have to do something...
				 * At the moment, lets consider only the case that the neighbour may be speaker itself.
				 */

				if (pos->neighbour->priv && !cmpUID(&pos->neighbour->agencyUID, pos->neighbour->priv)) {
					lprintk("## %s: neighbour is speaker apparently...its agencyUID: ", __func__);
					printlnUID(&wnet_rx.sl_desc->agencyUID_from);

					ourself()->neighbour->priv = pos->neighbour->priv;

					change_state(WNET_STATE_LISTENER);

					beacon_clear();

					/* Send a PING RESPONSE beacon
					 * We are now attached to the new speaker. If it had time to get IDLE,
					 * the response will put it in speaker state.
					 */
					winenet_send_beacon(&beacon, WNET_BEACON_PING, &pos->neighbour->agencyUID, (void *) WNET_PING_RESPONSE);

					return true;
				}

				/* Send a PING RESPONSE beacon
				 * But okay, we stay in our current state.
				 */
				winenet_send_beacon(&beacon, WNET_BEACON_PING, &pos->neighbour->agencyUID, (void *) WNET_PING_RESPONSE);
			}
		}

		if ((int) wnet_rx.last_beacon.priv == WNET_PING_RESPONSE) {
#ifdef VERBOSE_DISCOVERY
			lprintk("## %s: (state %s) processing ping response...\n", __func__, get_current_state_str());
#endif

			pos = find_neighbour(&wnet_rx.sl_desc->agencyUID_from);

			/* Sanity check (we got a response to our request) */
			BUG_ON(!pos);

			pos->valid = true;
		}

		beacon_clear();
	}

	return NULL;
}

/*
 * Wait for an acknowledgment beacon.
 *
 * Return 0 the ack has been successfully received.
 * Return -1 in case of timeout
 * Return 1 in case of another beacon received instead of ack.
 */
static int wait_for_ack(wnet_beacon_t *beacon) {
	int ret;
	int ret_ack = -1;

	/* Timeout of Tspeaker us */
	//ret = rtdm_event_timedwait(&wnet_event, tspeaker_ack, NULL);
	ret = wait_for_completion_timeout(&wnet_event, msecs_to_jiffies(WNET_TSPEAKER_ACK_MS));

	if (ret > 0) {
		ret_ack = 1;

		if (wnet_rx.last_beacon.id == WNET_BEACON_ACKNOWLEDGMENT) {

			/* We also want to make sure that the received beacon is issued from the right sender */
			if (!cmpUID(&wnet_rx.sl_desc->agencyUID_from, (agencyUID_t *) &beacon->agencyUID)) {

				if (!wnet_rx.last_beacon.priv || ((uint32_t) wnet_rx.last_beacon.priv == (wnet_tx.transID & WNET_MAX_PACKET_TRANSID)))

					/* OK - We got a correct acknowledgment. */
					ret_ack = 0;
			}

			/* Event processed */
			beacon_clear();
		}

	} else {

		/* The timeout has expired */;
//#ifdef VERBOSE
		lprintk("######## ACK timeout... will retry to ");
		printlnUID((agencyUID_t *) &beacon->agencyUID);
//#endif
		ret_ack = -1;
	}

	return ret_ack;
}

/*
 * Contact the next speaker in our neighborhoud.
 * At this point, the neighbour protection must be disabled.
 * This function is not re-entrant; it is called only once at a time.
 */
void forward_next_speaker(void) {
	wnet_neighbour_t *next_speaker;
	int ack;
	int retry_count;
	wnet_beacon_t beacon;

	neighbour_list_protection(true);

again:
	next_speaker = next_valid_neighbour(ourself());
	if (!next_speaker) {
		/* Reset our speakerUID */
		ourself()->neighbour->priv = NULL;

		neighbour_list_protection(false);

		change_state(WNET_STATE_IDLE);
		return ;
	}

	retry_count = 0;
	do {
		/* Now send the beacon */
		winenet_send_beacon(&beacon, WNET_BEACON_GO_SPEAKER, &next_speaker->neighbour->agencyUID, NULL);

retry_waitack:
		ack = wait_for_ack(&beacon);

		if (ack != 0) {
			/* Did we receive another beacon than ack ? */
			if (ack > 0) {

				/* ping ? */
				if (process_ping_is_speaker()) {

					neighbour_list_protection(false);
					return ;
				}

				if ((wnet_rx.last_beacon.id == WNET_BEACON_GO_SPEAKER) || (wnet_rx.last_beacon.id == WNET_BEACON_BROADCAST_SPEAKER))
					beacon_clear();

				goto retry_waitack;

			} else
				retry_count++;
		}

	} while ((ack != 0) && (retry_count <= WNET_RETRIES_MAX));

	if (ack != 0) {
		/*
		 * Well, it seems we have a bad guy as neighbour :-(
		 * Just remove it and pick-up a new speaker.
		 * Please note the the list managed by the Discovery is not impacted.
		 * If the neighbour is really down, it will disappear very soon.
		 */

		winenet_remove_neighbour(next_speaker->neighbour);

		goto again;
	}

	/* Set our new speakerUID */
	ourself()->neighbour->priv = &next_speaker->neighbour->agencyUID;

	neighbour_list_protection(false);

	change_state(WNET_STATE_LISTENER);
}

/*
 * Broadcast to all neighbours that we are the new speaker.
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
	wnet_beacon_t beacon;

	wnet_neighbours_processed_reset();

	pos = next_valid_neighbour(NULL);

	while (pos && !pos->processed) {

		pos->processed = true;

		/* Send the beacon requiring an acknowledgement */
		retry_count = 0;
		do {
			/* Now send the beacon */
			winenet_send_beacon(&beacon, WNET_BEACON_BROADCAST_SPEAKER, &pos->neighbour->agencyUID, NULL);
retry_waitack:
			ack = wait_for_ack(&beacon);

			if (ack != 0) {
				/* Did we receive another beacon than ack ? */
				if (ack > 0) {

					if (process_ping_is_speaker())
						/* Aborting the speaker... */
						return false;

					if (wnet_rx.last_beacon.id == WNET_BEACON_GO_SPEAKER) {
						wnet_beacon_t ack_beacon;

						winenet_send_beacon(&ack_beacon, WNET_BEACON_ACKNOWLEDGMENT, (agencyUID_t *) wnet_rx.last_beacon.agencyUID, NULL);

						/* Event processed */
						beacon_clear();
					}

					if (wnet_rx.last_beacon.id == WNET_BEACON_BROADCAST_SPEAKER)
						beacon_clear();

					goto retry_waitack;

				} else
					retry_count++;
			}

		} while ((ack != 0) && (retry_count <= WNET_RETRIES_MAX));

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

			winenet_remove_neighbour(tmp->neighbour);
		} else
			pos = next_valid_neighbour(pos);

	};

	/* Alone ? */
	if (next_valid_neighbour(NULL) == NULL) {
		ourself()->neighbour->priv = NULL;

		change_state(WNET_STATE_IDLE);
		return false;
	}

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

/**************************** WNET_STATE_INIT *****************************/

static void winenet_state_init(wnet_state_t old_state) {

	DBG("Init\n");

	/* Wait that Discovery inserted us into the list of neighbour. */
	//rtdm_event_wait(&wnet_event);
	wait_for_completion(&wnet_event);
}

/**************************** WNET_STATE_IDLE *****************************/

/*
 * We are initially in this state until there is at least another smart object
 * in the neighbourhood.
 */
static void winenet_state_idle(wnet_state_t old_state) {
	wnet_neighbour_t *wnet_neighbour;
	wnet_beacon_t beacon;

	DBG("Idle\n");

#ifdef VERBOSE_STATE
	if (last_state != WNET_STATE_IDLE) {
		lprintk("!!! Smart object ");
		printlnUID(get_my_agencyUID());
		lprintk(" -- Now in state IDLE\n");

		last_state = WNET_STATE_IDLE;
	}
#endif

	/* Sanity check */
	BUG_ON(ourself()->neighbour->priv != NULL);

retry:
	/* Waiting on a first neighbor at least. */
	//rtdm_event_wait(&wnet_event);
	wait_for_completion(&wnet_event);

	if (clear_spurious_ack())
		goto retry;

	/* Process beacon first */
	if (wnet_rx.last_beacon.id == WNET_BEACON_PING) {

		wnet_neighbour = find_neighbour(&wnet_rx.sl_desc->agencyUID_from);
		BUG_ON(!wnet_neighbour);

		if ((uint32_t) wnet_rx.last_beacon.priv == WNET_PING_REQUEST) {

			/* Determine which of us is speaker/listener and set the appropriate. */
#ifdef VERBOSE_DISCOVERY
			lprintk("### OK, we got a request. Send a response. Neighbour VALID.\n");
#endif
			wnet_neighbour->valid = true;

			if (!wnet_neighbour->neighbour->priv) {
				if (cmpUID(&ourself()->neighbour->agencyUID, &wnet_neighbour->neighbour->agencyUID) < 0) {

					ourself()->neighbour->priv = &ourself()->neighbour->agencyUID;
					change_state(WNET_STATE_SPEAKER);
					//rtdm_event_signal(&wnet_event); /* Proceed immediately */
					complete(&wnet_event);

				} else {

					ourself()->neighbour->priv = &wnet_neighbour->neighbour->agencyUID;
					change_state(WNET_STATE_LISTENER);

				}

			} else {
				ourself()->neighbour->priv = wnet_neighbour->neighbour->priv;
				change_state(WNET_STATE_LISTENER);
			}

			/* Send a PING RESPONSE beacon */
			winenet_send_beacon(&beacon, WNET_BEACON_PING, &wnet_neighbour->neighbour->agencyUID, (void *) WNET_PING_RESPONSE);

		} else if ((uint32_t ) wnet_rx.last_beacon.priv == WNET_PING_RESPONSE) {
			/* Determine which of us is speaker/listener and set the appropriate. */
#ifdef VERBOSE_DISCOVERY
			lprintk("### OK, we got a RESPONSE. Neighbour is VALID. ");
			printlnUID(&wnet_rx.sl_desc->agencyUID_from);
#endif
			wnet_neighbour->valid = true;

			if (!wnet_neighbour->neighbour->priv || !cmpUID(wnet_neighbour->neighbour->priv, &ourself()->neighbour->agencyUID)) {

				if (cmpUID(&ourself()->neighbour->agencyUID, &wnet_neighbour->neighbour->agencyUID) < 0) {

					ourself()->neighbour->priv = &ourself()->neighbour->agencyUID;
					change_state(WNET_STATE_SPEAKER);
					//rtdm_event_signal(&wnet_event); /* Proceed immediately */
					complete(&wnet_event);

				} else {
					ourself()->neighbour->priv = &wnet_neighbour->neighbour->agencyUID;
					change_state(WNET_STATE_LISTENER);
				}
			} else {
				ourself()->neighbour->priv = wnet_neighbour->neighbour->priv;
				change_state(WNET_STATE_LISTENER);
			}

		}
		beacon_clear();

	} else {

		/* Well, at this point, we should not receive any other beacon in the IDLE state.
		 * However, we can have the wnet_event been signaled during the chage of state
		 * SPEAKER to IDLE. Typically, if a send fails and leads to the removal of all neighbours,
		 * the sender can fail, but a send complete could happen (case of discovery test code)
		 * and this will lead to signaling wnet_event. We hence skip such occurences here.
		 */
		if (wnet_rx.last_beacon.id != WNET_BEACON_N) {
			lprintk("### %s: failure in idle state, received unexpected beacon: %d from ", __func__, wnet_rx.last_beacon.id);
			printlnUID(&wnet_rx.sl_desc->agencyUID_from);
			BUG();
		} else
			goto retry;
	}

}

/**************************** WNET_STATE_SPEAKER *****************************/

static void winenet_state_speaker(wnet_state_t old_state) {
	int i, ack;
	int retry_count;
	wnet_neighbour_t *listener, *tmp;
	bool __broadcast_done = false;
	wnet_beacon_t beacon;

	DBG("Speaker\n");

#ifdef VERBOSE_STATE
	if (last_state != WNET_STATE_SPEAKER) {
		lprintk("!!! Smart object ");
		printUID(get_my_agencyUID());
		lprintk(" -- Now in state SPEAKER\n");

		last_state = WNET_STATE_SPEAKER;
	}
#endif

	neighbour_list_protection(true);

	while (true) {

		/* We keep synchronized with our producer or be ready to process beacons. */

		//rtdm_event_wait(&wnet_event);
		wait_for_completion(&wnet_event);

		if (clear_spurious_ack())
			continue;

		/* Delayed (spurious) beacon */
		if (wnet_rx.last_beacon.id == WNET_BEACON_GO_SPEAKER) {
			winenet_send_beacon(&beacon, WNET_BEACON_ACKNOWLEDGMENT, &wnet_rx.sl_desc->agencyUID_from, NULL);

			beacon_clear();
			continue;
		}

		if (wnet_rx.last_beacon.id == WNET_BEACON_BROADCAST_SPEAKER) {

			/* We ignore it since we already received a GO_SPEAKER. If this beacon comes from
			 * new neighbors, the send/ack flow operations will lead to abort one or the other
			 * and the system will get stable along new a new discovery add/remove & ping procedure.
			 */

			beacon_clear();

			continue;
		}

		/* Check for ping */
		if (process_ping_is_speaker()) {

			discard_transmission();

			neighbour_list_protection(false);
			return ;
		}

		/* Any data to send on this transmission? */
		if (!wnet_tx.pending) {

			/* Is it an end of transmission? */
			if (transmission_over) {
				transmission_over = false;

				/* Synchronize with the producer. */
				//rtdm_event_signal(&wnet_tx.xmit_event);
				complete(&wnet_tx.xmit_event);
			}

			/* Send a go_speaker beacon to the next speaker. */
			neighbour_list_protection(false);
			forward_next_speaker();

			return ;
		}
		/* Now, we inform all neighbours that we are the new speaker.
		 * We ask an acknowledge for this beacon, and if a neighbour does
		 * not answer, we remove it from our neighbourhood.
		 * We do that only once and we do that here in the code, after the first waiting on wnet_event
		 * since such other waitings are done during the broadcast.
		 */

		if (!__broadcast_done && wnet_tx.pending) {

			if (!speaker_broadcast()) {
				neighbour_list_protection(false);
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

			ourself()->neighbour->priv = NULL;

			neighbour_list_protection(false);

			change_state(WNET_STATE_IDLE);
			return ;
		}

		listener->processed = true;

		/* We have to transmit over all smart objects */
		/* Sending packet of the frame for the first time (first listener) */

		/* Copy the packet into the bufferized packet array */
		if (buf_tx_pkt[(wnet_tx.packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME]) {
			DBG("TX buffer already populated: %d, %d\n", wnet_tx.packet->transID, (wnet_tx.packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME);

			clear_buf_tx_pkt();
		}

		buf_tx_pkt[(wnet_tx.packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME] = (transceiver_packet_t *) kmalloc(wnet_tx.packet->size + sizeof(transceiver_packet_t), GFP_ATOMIC);
		memcpy(buf_tx_pkt[(wnet_tx.packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME], wnet_tx.packet, wnet_tx.packet->size + sizeof(transceiver_packet_t));

		/* Set the destination */
		memcpy(&wnet_tx.sl_desc->agencyUID_to, &listener->neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

#ifdef VERBOSE
		lprintk("## send data to: ");
		printlnUID(&wnet_tx.sl_desc->agencyUID_to);
#endif

		/* Propagate the data packet to the lower layers */
		__sender_tx(wnet_tx.sl_desc, wnet_tx.packet, wnet_tx.size, 0);

		if (((wnet_tx.packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME == WNET_N_PACKETS_IN_FRAME-1) || (wnet_tx.packet->transID & WNET_LAST_PACKET))
		{
			memcpy(&beacon.agencyUID, &wnet_tx.sl_desc->agencyUID_to, SOO_AGENCY_UID_SIZE);

			/* Now waiting for the ACK beacon */
retry_ack1:
			ack = wait_for_ack(&beacon);

			if (ack != 0) {

				/* Delayed (spurious) beacon */
				if (wnet_rx.last_beacon.id == WNET_BEACON_GO_SPEAKER) {
					winenet_send_beacon(&beacon, WNET_BEACON_ACKNOWLEDGMENT, &wnet_rx.sl_desc->agencyUID_from, NULL);

					beacon_clear();
					continue;
				}
				if (wnet_rx.last_beacon.id == WNET_BEACON_BROADCAST_SPEAKER) {

					beacon_clear();
					goto retry_ack1;
				}

				if (process_ping_is_speaker()) {

					discard_transmission();

					neighbour_list_protection(false);
					return ;
				}

				retry_count = 0;
				do {

					for (i = 0; ((i < WNET_N_PACKETS_IN_FRAME) && (buf_tx_pkt[i] != NULL)); i++) {
						__sender_tx(wnet_tx.sl_desc, buf_tx_pkt[i], buf_tx_pkt[i]->size, 0);



retry_ack2:
					ack = wait_for_ack(&beacon);

					/* Delayed (spurious) beacon */
					if (wnet_rx.last_beacon.id == WNET_BEACON_GO_SPEAKER) {
						winenet_send_beacon(&beacon, WNET_BEACON_ACKNOWLEDGMENT, &wnet_rx.sl_desc->agencyUID_from, NULL);

						beacon_clear();
						continue;
					}
					if (wnet_rx.last_beacon.id == WNET_BEACON_BROADCAST_SPEAKER) {

						beacon_clear();
						goto retry_ack2;
					}

					if (process_ping_is_speaker()) {

						discard_transmission();

						neighbour_list_protection(false);
						return ;
					}

					if (ack == -1)
						retry_count++;

				} while ((ack != 0) && (retry_count < WNET_RETRIES_MAX));

				if (ack != 0) {
					/*
					 * Well, it seems we have a bad guy as neighbour :-(
					 * Just remove it and proceed with the next listener, i.e. lets proceed
					 * with the broadcast to other neighbours.
					 */

					tmp = listener;
					listener = next_valid_neighbour(listener);
					winenet_remove_neighbour(tmp->neighbour);

					if (listener == NULL) {

						discard_transmission();

						ourself()->neighbour->priv = NULL;

						neighbour_list_protection(false);

						change_state(WNET_STATE_IDLE);

						return ;

					}
				}
			}

			/* Look for the next neighbour */
			listener = next_valid_neighbour(listener);
		} else {

			/* Allow the producer to go further, as the frame is not complete yet */
			winenet_xmit_data_processed(0);

			continue;
		}

		while (listener && !listener->processed) {

			listener->processed = true;

			/* Set the destination */
			memcpy(&wnet_tx.sl_desc->agencyUID_to, &listener->neighbour->agencyUID, SOO_AGENCY_UID_SIZE);
			memcpy(&beacon.agencyUID, &wnet_tx.sl_desc->agencyUID_to, SOO_AGENCY_UID_SIZE);

			retry_count = 0;
			do {
				for (i = 0; ((i < WNET_N_PACKETS_IN_FRAME) && (buf_tx_pkt[i] != NULL)); i++)
					__sender_tx(wnet_tx.sl_desc, buf_tx_pkt[i], buf_tx_pkt[i]->size, 0);
retry_ack3:
				ack = wait_for_ack(&beacon);

				if (ack > 0) {

					/* Delayed (spurious) beacon */
					if ((wnet_rx.last_beacon.id == WNET_BEACON_GO_SPEAKER) || (wnet_rx.last_beacon.id == WNET_BEACON_BROADCAST_SPEAKER)) {

						/* We ignore it since we already received a GO_SPEAKER. If these beacons are due
						 * to some new neighbours, the send/ack flow operations will lead to abort one or the other
						 * and the system will get stable along new a new discovery add/remove & ping procedure.
						 */

						beacon_clear();
						goto retry_ack3;
					}

					if (process_ping_is_speaker()) {
						discard_transmission();

						neighbour_list_protection(false);
						return ;
					}

				} else
					retry_count++;

			} while ((ack != 0) && (retry_count < WNET_RETRIES_MAX));

			if (ack != 0) {
				/*
				 * Well, it seems we have a bad guy as neighbour :-(
				 * Just remove it and proceed with the next listener, i.e. lets proceed
				 * with the broadcast to other neighbours.
				 */

				tmp = listener;
				listener = next_valid_neighbour(listener);

				winenet_remove_neighbour(tmp->neighbour);

				if (listener == NULL) {

					discard_transmission();
					ourself()->neighbour->priv = NULL;

					change_state(WNET_STATE_IDLE);

					clear_buf_tx_pkt();
					winenet_xmit_data_processed(-EIO);
					neighbour_list_protection(false);

					return ;
				}
			} else
				listener = next_valid_neighbour(listener);
		}

		/* We reach the end of the round of listeners. */

		clear_buf_tx_pkt();
		winenet_xmit_data_processed(0);
	}
}

/**************************** WNET_STATE_LISTENER *****************************/

static void winenet_state_listener(wnet_state_t old_state) {
	wnet_neighbour_t *wnet_neighbour;
	wnet_beacon_t beacon;

	DBG("Listener.\n");

#ifdef VERBOSE_STATE
	if (last_state != WNET_STATE_LISTENER) {
		lprintk("!!! Smart object ");
		printUID(get_my_agencyUID());
		lprintk(" -- Now in state LISTENER\n");

		last_state = WNET_STATE_LISTENER;
	}
#endif

	while (1) {
		//rtdm_event_wait(&wnet_event);
		wait_for_completion(&wnet_event);

		if (clear_spurious_ack())
			continue;

		/* It may happen if the current speaker disappeared and we are now speaker. */
		if (get_state() == WNET_STATE_SPEAKER) {
			//rtdm_event_signal(&wnet_event); /* Go ahead in speaker state for active processing */
			complete(&wnet_event);
			return ;
		}

		/* Sanity check */
		BUG_ON(get_state() != WNET_STATE_LISTENER);

		if (next_valid_neighbour(NULL) == NULL) {

			/* Reset the speakerUID */
			ourself()->neighbour->priv = NULL;
			change_state(WNET_STATE_IDLE);

			return ;
		}

		/* Just check if we receive some ping beacons */
		process_ping_is_speaker();

		if (wnet_rx.last_beacon.id == WNET_BEACON_GO_SPEAKER) {

			/* Our turn... */
			ourself()->neighbour->priv = &ourself()->neighbour->agencyUID;
			change_state(WNET_STATE_SPEAKER);
			//rtdm_event_signal(&wnet_event); /* Proceed immediately */
			complete(&wnet_event);

			/* We can respond to our promoter :-) */
			winenet_send_beacon(&beacon, WNET_BEACON_ACKNOWLEDGMENT, &wnet_rx.sl_desc->agencyUID_from, NULL);

			/* Event processed */
			beacon_clear();

			return ;
		}

		if (wnet_rx.last_beacon.id == WNET_BEACON_BROADCAST_SPEAKER) {

			wnet_neighbour = find_neighbour(&wnet_rx.sl_desc->agencyUID_from);
			BUG_ON(!wnet_neighbour);

			/* Check if we this neighbour is known yet. */

			ourself()->neighbour->priv = &wnet_neighbour->neighbour->agencyUID;

			/* We are ready to listen to this speaker. */
			winenet_send_beacon(&beacon, WNET_BEACON_ACKNOWLEDGMENT, &wnet_rx.sl_desc->agencyUID_from, NULL);

			/* Event processed */
			beacon_clear();
		}

		if (wnet_rx.data_received) {
			/* Data has been received. Reset the proper flag. */

			wnet_rx.data_received = false;

			/*
			 * Send an ACKNOWLEDGMENT beacon.
			 */

			winenet_send_beacon(&beacon, WNET_BEACON_ACKNOWLEDGMENT, &wnet_rx.sl_desc->agencyUID_from, (void *) (wnet_rx.transID & WNET_MAX_PACKET_TRANSID));

			//rtdm_event_signal(&data_event);
			complete(&data_event);
		}

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

	DBG(" ** change_state: %s -> %s              UID: \n", winenet_get_state_str(get_state()), winenet_get_state_str(new_state));
	DBG_BUFFER(get_my_agencyUID(), SOO_AGENCY_UID_SIZE);

	handle->old_state = handle->state;
	handle->state = new_state;

	//rtdm_event_signal(&handle->event);
	complete(&handle->event);
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
static int fsm_task_fn(void *args) {
	wnet_fsm_handle_t *handle = (wnet_fsm_handle_t *) args;
	wnet_state_fn_t *functions = handle->funcs;
	//rtdm_event_t *event = &handle->event;
	struct completion *event = &handle->event;

	DBG("Entering Winenet FSM task...\n");

	while (true) {
		DBG("Got the wnet_state_event signal.\n");

		/* Call the proper state function */
		(*functions[handle->state])(handle->old_state);

		//rtdm_event_wait(event);
		wait_for_completion(event);
	}

	return 0;
}

/**
 * Start the Winenet FSM routine.
 * The FSM function table and the RTDM event are in the handle given as parameter.
 * This function has to be called from CPU #0.
 */
void winenet_start_fsm_task(char *name, wnet_fsm_handle_t *handle) {

	handle->old_state = WNET_STATE_INIT;
	handle->state = WNET_STATE_INIT;

	//rtdm_task_init(&handle->task, name, fsm_task_fn, (void *) handle, WINENET_TASK_PRIO, 0);
	kthread_run(fsm_task_fn, (void *) handle, "fsm_task");
}

/**
 * This function is called when a data packet or a Iamasoo beacon has to be sent.
 * The call is made by the Sender.
 */
static int winenet_tx(sl_desc_t *sl_desc, void *packet_ptr, size_t size, bool completed) {
	transceiver_packet_t *packet;
	int ret;

	/* End of transmission ? */
	if (!packet_ptr) {
		/* Ok, go ahead with the next speaker */

		/* tx_pending will remain to false */

		/* We are synchronized with the SPEAKER if it is still active, i.e.
		 * if there is still some valid neighbour.
		 */
		if (get_state() == WNET_STATE_SPEAKER) {

			transmission_over = true;

			//rtdm_event_signal(&wnet_event);
			complete(&wnet_event);

			/* Wait until the FSM has processed the data. */
			//rtdm_event_wait(&wnet_tx.xmit_event);
			wait_for_completion(&wnet_tx.xmit_event);
		}

		return 0;
	}

	packet = (transceiver_packet_t *) packet_ptr;

	if (unlikely(sl_desc->req_type == SL_REQ_DISCOVERY)) {

		/* Iamasoo beacons */
		packet->transID = 0xffffffff;

		__sender_tx(sl_desc, packet, size, 0);

		return 0;
	}

	/* Only one producer can call winenet_tx at a time */
	mutex_lock(&wnet_xmit_lock);

	packet->transID = sent_packet_transID;

	/*
	 * If this is the last packet, set the WNET_LAST_PACKET bit in the transID.
	 * This is required to allow the receiver to identify the last packet of the
	 * frame (if the modulo of its trans ID is not equal to WNET_N_PACKETS_IN_FRAME - 1)
	 * and force it to send an ACK.
	 */
	if (completed)
		packet->transID |= WNET_LAST_PACKET;

	/* Fill the TX request parameters */

	wnet_tx.sl_desc = sl_desc;
	wnet_tx.packet = packet;
	wnet_tx.size = size;
	wnet_tx.transID = packet->transID;
	wnet_tx.completed = completed;

	/* Now, setting tx_pending to true will allow the speaker to send out */
	wnet_tx.pending = true;

	/*
	 * Prepare the next transID.
	 * If the complete flag is set, the next value will be 0.
	 */
	if (!completed)
		sent_packet_transID = (sent_packet_transID + 1) % WNET_MAX_PACKET_TRANSID;
	else
		sent_packet_transID = 0;

	if (wnet_tx.transID > 0)
		//rtdm_event_signal(&wnet_event);
		complete(&wnet_event);

	/* Wait until the packed has been sent out. */
	//rtdm_event_wait(&wnet_tx.xmit_event);
	wait_for_completion(&wnet_tx.xmit_event);

	/* completed ? */
	if (wnet_tx.completed)
		/* Reset the TX trans ID */
		sent_packet_transID = 0;

	ret = wnet_tx.ret;

	mutex_unlock(&wnet_xmit_lock);

	return ret;
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
	static bool all_packets_received = false;
	static uint32_t last_transID;
	uint32_t i;
	static bool got_data = false;

	packet = (transceiver_packet_t *) packet_ptr;

#ifdef VERBOSE
	lprintk("** receiving: agencyUID_to: ");
	printlnUID(&sl_desc->agencyUID_to);

	lprintk("**            agencyUID_from: ");
	printlnUID(&sl_desc->agencyUID_from);
#endif

	wnet_rx.sl_desc = sl_desc;

	if (packet->packet_type == TRANSCEIVER_PKT_DATALINK) {

		memcpy(&wnet_rx.last_beacon, packet->payload, sizeof(wnet_beacon_t));

#ifdef VERBOSE
		lprintk("### Receiving beacon %s\n", beacon_str(&wnet_rx.last_beacon, &wnet_rx.sl_desc->agencyUID_from));
#endif

		/* Processed within the FSM directly */
		//rtdm_event_signal(&wnet_event);
		complete(&wnet_event);

		/* Wait until the beacon has been processed by the FSM */
		//rtdm_event_wait(&beacon_event);
		wait_for_completion(&beacon_event);
	}

	if (packet->packet_type == TRANSCEIVER_PKT_DATA) {

#ifdef VERBOSE
		if (!got_data) {
			lprintk("### Receiving data from ");
			printlnUID(&wnet_rx.sl_desc->agencyUID_from);
			got_data = true;
		}
#endif

		/* Skip the data which are not for us. */
		/* We can logically NOT receive a PKT_DATA from a smart object which would not have
		 * been paired (via PING) with us. Hence, ourself()->neighbour->priv can't be NULL.
		 */
		
		if (cmpUID(&wnet_rx.sl_desc->agencyUID_from, ourself()->neighbour->priv)) {
			lprintk("### SKIPPED ");
			printUID(&wnet_rx.sl_desc->agencyUID_from);
			lprintk("    bound speaker: ");
			printlnUID(ourself()->neighbour->priv);

			return ;
		}

		/*
		 * Data packets are processed immediately along the RX callpath (upper layers) and has nothing to
		 * do with the FSM. Actually, the speaker will wait for an acknowledgment beacon which will
		 * process by the FSM *after* all packets have been received (and hence forwarded to the upper layers).
		 */

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

		/* Save the last ID of the last received packet */
		last_transID = (packet->transID & WNET_MAX_PACKET_TRANSID);

		if (((packet->transID & WNET_MAX_PACKET_TRANSID) % WNET_N_PACKETS_IN_FRAME == WNET_N_PACKETS_IN_FRAME - 1) || (packet->transID & WNET_LAST_PACKET)) {
			/* If all the packets of the frame have been received, forward them to the upper layer */

			if (all_packets_received) {

				for (i = 0; ((i < WNET_N_PACKETS_IN_FRAME) && (buf_rx_pkt[i] != NULL)); i++)
					if ((buf_rx_pkt[i]->packet_type == TRANSCEIVER_PKT_DATA) && !packet_already_received(buf_rx_pkt[i], &sl_desc->agencyUID_from))
						receiver_rx(sl_desc, plugin_desc, buf_rx_pkt[i], buf_rx_pkt[i]->size);

				clear_buf_rx_pkt();

				wnet_rx.sl_desc = sl_desc;
				wnet_rx.transID = packet->transID;
				wnet_rx.data_received = true;

				got_data = false;

				//rtdm_event_signal(&wnet_event);
				complete(&wnet_event);

				/* Wait until the beacon has been processed by the FSM */
				//rtdm_event_wait(&data_event);
				complete(&data_event);
			}
		}
	}
}

/**
 * Callbacks of the Winenet protocol
 */
static datalink_proto_desc_t winenet_proto = {
	.tx_callback = winenet_tx,
	.rx_callback = winenet_rx,
};

/**
 * Initialization of Winenet.
 */
void winenet_init(void) {
	DBG("Winenet initialization\n");

	memcpy(&first_speakerUID, get_my_agencyUID(), sizeof(agencyUID_t));

	INIT_LIST_HEAD(&wnet_neighbours);

	//rtdm_event_init(&wnet_event, 0);
	//rtdm_event_init(&wnet_tx.xmit_event, 0);
	//rtdm_event_init(&beacon_event, 0);
	//rtdm_event_init(&data_event, 0);
	init_completion(&wnet_event);
	init_completion(&wnet_tx.xmit_event);
	init_completion(&beacon_event);
	init_completion(&data_event);

	wnet_tx.pending = false;
	wnet_rx.data_received = false;
	wnet_rx.last_beacon.id = WNET_BEACON_N;

	mutex_init(&wnet_xmit_lock);
	mutex_init(&neighbour_list_lock);

	//rtdm_event_init(&fsm_handle.event, 0);
	init_completion(&fsm_handle.event);
	fsm_handle.funcs = fsm_functions;

	/* Internal SOOlink descriptor */
#if defined(CONFIG_SOOLINK_PLUGIN_WLAN)
	__sl_desc = sl_register(SL_REQ_DATALINK, SL_IF_WLAN, SL_MODE_UNIBROAD);
#else /* CONFIG_SOOLINK_PLUGIN_WLAN */
	__sl_desc = sl_register(SL_REQ_DATALINK, SL_IF_ETH, SL_MODE_UNIBROAD);
#endif /* !CONFIG_SOOLINK_PLUGIN_WLAN */

	BUG_ON(!__sl_desc);

	datalink_register_protocol(SL_DL_PROTO_WINENET, &winenet_proto);

	/* Register with Discovery as Discovery listener */
	discovery_listener_register(&wnet_discovery_desc);

	winenet_start_fsm_task("Wnet", &fsm_handle);

}
