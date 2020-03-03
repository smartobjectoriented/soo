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

/*
 * Winenet netstream mode.
 *
 * This Winenet extension is devoted to the SL_MODE_NETSTREAM transmission mode, devoted to low latency streaming
 * and messaging.
 *
 * Winenet netstream is implementing a so-called netstream round robin communication mode in which a stream is
 * transmitted using a packet propagated along a chain, in a cyclic way. The Speaker sends the packet to its
 * immmediate neighbour (Listener), which is the next SOO in the chain, then the Listener becomes Speaker and
 * sends the packet to the next Listener, and so on. Within the packet, each SOO has a dedicated subpacket.
 * It is responsible of the update of its contents.
 *
 * -----
 *
 * All Winenet states are not necessarily used in netstream mode, especially those related to medium sensing.
 * Netstream is using the following states:
 * - WNET_STATE_IDLE: the stream is disabled.
 * - WNET_STATE_SPEAKER: the SOO is about to send a packet. It waits for the packet to be updated by the requester.
 *                       Once it is done, it sends the packet.
 * - WNET_STATE_SPEAKER_WAIT_ACK: the SOO is the first Speaker and waiting for ACKs from the other SOOs, during the
 *                                stream initialization.
 * - WNET_STATE_LISTENER: the SOO is receiving a packet, or waiting for one to come. It also re-sends the
 *                        packet if a transmission issue allegedly occurred.
 *
 * All beacons are not necessarily used in netstream mode, especially those related to medium sensing.
 * Netstream is using the following beacons:
 * - WNET_BEACON_REQUEST_TO_SEND_NETSTREAM: stream initialization (first Speaker)
 * - WNET_BEACON_TRANSMISSION_COMPLETED_NETSTREAM: stream termination (first Speaker)
 * - ACKNOWLEDGE (not first Speaker)
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

#if 1
#define WNET_CONFIG_NETSTREAM		1
#endif

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/time.h>

#include <soo/soolink/soolink.h>
#include <soo/soolink/datalink/winenet.h>
#include <soo/soolink/datalink.h>
#include <soo/soolink/sender.h>
#include <soo/soolink/receiver.h>
#include <soo/soolink/discovery.h>
#include <soo/soolink/transceiver.h>

#include <soo/core/device_access.h>

#include <xenomai/rtdm/driver.h>

#include <soo/uapi/soo.h>

#include <soo/uapi/debug.h>
#include <soo/uapi/console.h>
#include <soo/uapi/soo.h>
#include <soo/debug/dbgvar.h>
#include <soo/debug/time.h>

/* Enable this to inspect Listener timeouts */
#if 0
/*
 * Debugging functionality that adds a Listener timeout counter in the packet.
 * In this mode, the packet payload contains the following elements:
 * - n_soodios * uint32_t: Number of Listener timeouts
 * - n_soodios * uint32_t: Number of received+forwarded packets.
 */
#define WNET_DEBUG_INFO_IN_PKT		1
#endif

/* FSM helpers */
static void change_state(wnet_state_t new_state);
static wnet_state_t get_state(void);

/* Handle used in the FSM */
static wnet_fsm_handle_t fsm_handle;

/* Reference to the immediate neighbour of this SOO */
static neighbour_desc_t my_listener;

/* Number of neighbours */
static uint32_t n_neighbours = 0;

/* Number of SOOs in the ecosystem */
static uint32_t n_soos = 0;

/*
 * Index of the SOO in the chain.
 * 0 stands for the first Speaker.
 * A value of 0xff means that the index has not been set yet.
 */
static uint8_t my_index = 0xff;

/* Data used during Request To Send and Transmission completed operations */
static bool request_to_send_pending = false;
static struct list_head *request_to_send_neighbour;
static bool transmission_completed_pending = false;
static struct list_head *transmission_completed_neighbour;

/* Boolean used by the first Speaker to send the first burst */
static bool send_first_burst = false;

extern rtdm_mutex_t tx_rx_data_lock;

/*
 * Packet buffering
 *
 * There are two packet buffers:
 * - One packet buffer is allocated by the requester and the information related to it is provided
 *   by the requester during the stream init operation.
 * - One packet buffer is allocated on-the-fly by Winenet if the stream init operation has not been
 *   done yet.
 *
 * The number of packets in the burst should match the number of SOOs in the ecosystem.
 * All packets in the burst must have the same size. We cannot pre-allocate the burst
 * buffer as the number of SOOs is unknown.
 */

/*
 * Packet buffering if the requester has made an allocation.
 * If pkt_req_size is 0 and pkt_req_burst_data is NULL, this means that the stream
 * init operation has not been done yet. In this case, pkt_tmp and pkt_list_tmp
 * are used instead.
 */
static size_t pkt_req_size = 0;
static netstream_transceiver_packet_t *pkt_req_data = NULL;

/*
 * Packet buffering if no buffer has been provided by the requester yet.
 */
static size_t pkt_tmp_size = 0;
static netstream_transceiver_packet_t *pkt_tmp_data = NULL;

/*
 * Pointer to the proper packet buffer and associated size:
 * - pkt_req_* if the stream init operation has been done
 * - pkt_tmp_* if the stream init operation has not been done yet
 */
static size_t pkt_size = 0;
static netstream_transceiver_packet_t *pkt_data = NULL;

/* Lock associated to the packet buffering data */
static rtdm_mutex_t pkt_lock;

/* Event used to track the receival of a TX request or a beacon */
static rtdm_event_t wnet_event;

/***** Lib *****/

static void reset_pkt_data(void) {
	rtdm_mutex_lock(&pkt_lock);

	if (pkt_tmp_data)
		kfree(pkt_tmp_data);

	my_index = 0xff;
	request_to_send_pending = false;
	transmission_completed_pending = false;
	send_first_burst = false;
	pkt_req_size = 0;
	pkt_req_data = NULL;
	pkt_tmp_size = 0;
	pkt_tmp_data = NULL;
	pkt_size = 0;
	pkt_data = NULL;

	rtdm_mutex_unlock(&pkt_lock);
}

/***** TX and RX management *****/

/**
 * The request XMIT callback is, in netstream mode, dedicated to two operations:
 * - the stream init operation
 * - the stream termination operation
 *
 * Stream init:
 * - If this SOO is the first Speaker, it initiates the chain by sending a REQUEST TO SEND NETSTREAM
 *   beacon to all neighbours then becomes Speaker.
 * - If this SOO is not the first Speaker, it becomes Listener when a REQUEST TO SEND NETSTREAM
 *   beacon is received. It forwards the packet to the Listener but does not forward it to the upper
 *   layers until the stream init is performed.
 *
 * Stream termination:
 * If this SOO is the first Speaker, it sends a TRASMISSION COMPLETED NETSTREAM beacon to all neighbours
 * then becomes Idle.
 */
int winenet_netstream_request_xmit(sl_desc_t *sl_desc) {
	uint8_t index;
	neighbour_desc_t listener;
	int ret;

	if ((sl_desc->incoming_block) && (sl_desc->incoming_block_size)) {
		/* Stream init */

		DBG("Stream init\n");

		/* Disable Discovery */
		discovery_disable();

		if (my_index == 0xff) {
			/*
			 * The SOO does not know if it is the first Speaker.
			 * The first Speaker might not have been started yet.
			 */

			if ((ret = winenet_get_my_index_and_listener(&index, &listener)) < 0)
				return ret;

			n_neighbours = ret;
			n_soos = n_neighbours + 1;

			my_index = index;
			memcpy(&my_listener, &listener, sizeof(neighbour_desc_t));

			DBG("Speaker index: %d\n", my_index);
			DBG("My Listener agency UID: "); DBG_BUFFER(&my_listener.agencyUID, SOO_AGENCY_UID_SIZE);
		}

		if (my_index == 0) {
			/* This SOO is the first Speaker */

			/* Initialize the iterator used during the medium request procedure */
			request_to_send_neighbour = winenet_get_neighbours()->next;

			pkt_req_size = sl_desc->incoming_block_size;
			pkt_req_data = sl_desc->incoming_block;
			pkt_req_data->packet_type = TRANSCEIVER_PKT_DATA;

			rtdm_mutex_lock(&pkt_lock);
			pkt_size = pkt_req_size;
			pkt_data = pkt_req_data;
			rtdm_mutex_unlock(&pkt_lock);

			rtdm_mutex_lock(&tx_rx_data_lock);
			/* Retrieve the sl_desc used for data and beacons */
			winenet_get_tx_rx_data()->sl_desc = sl_desc;

			/* Set the destination */
			memcpy(&sl_desc->agencyUID_to, &my_listener.agencyUID, SOO_AGENCY_UID_SIZE);
			rtdm_mutex_unlock(&tx_rx_data_lock);

			DBG("Speaker index: %d\n", my_index);

			request_to_send_pending = true;
			send_first_burst = true;

			rtdm_event_signal(&wnet_event);
		} else {
			/* This SOO is not the first Speaker */

			DBG("Speaker index: %d\n", my_index);

			rtdm_mutex_lock(&pkt_lock);

			if (pkt_tmp_data)
				kfree(pkt_tmp_data);

			pkt_req_size = sl_desc->incoming_block_size;
			pkt_req_data = sl_desc->incoming_block;
			pkt_req_data->packet_type = TRANSCEIVER_PKT_DATA;

			pkt_size = pkt_req_size;
			pkt_data = pkt_req_data;
			rtdm_mutex_unlock(&pkt_lock);
		}

	} else if ((!sl_desc->incoming_block) && (!sl_desc->incoming_block_size)) {

		/* Stream termination */

		DBG("Stream termination, Speaker %d\n", my_index);

		if (my_index == 0) {
			/* This SOO is the first Speaker */

			/* Initialize the iterator used during the transmission completed procedure */
			transmission_completed_neighbour = winenet_get_neighbours()->next;

			transmission_completed_pending = true;

			/* The next time this SOO becomes Speaker, it will perform the stream termination */
		} else {
			DBG("Stream termination: Not first Speaker, operation not permitted\n");
		}

	}

	return 0;
}

/**
 * This function is called when a data packet or a Iamasoo beacon has to be sent.
 * The call is performed by the Sender.
 * In netstream mode:
 * - The packet parameter is a direct pointer to data. It must belong to a packet within the packet buffer.
 * - The size parameter and completed flag are ignored. (The size has been set during the stream init operation).
 */
int winenet_netstream_xmit(sl_desc_t *sl_desc, void *packet, size_t size, bool completed) {
	wnet_tx_rx_data_t tx_rx_data;

	rtdm_mutex_lock(&pkt_lock);

	/* A XMIT can be performed only if the requester has allocated the packet buffer */
	if (unlikely(!pkt_req_data)) {
		rtdm_mutex_unlock(&pkt_lock);
		return -EPERM;
	}

	winenet_copy_tx_rx_data(&tx_rx_data);

	rtdm_mutex_lock(&tx_rx_data_lock);
	memcpy(&winenet_get_tx_rx_data()->sl_desc->agencyUID_to, &my_listener.agencyUID, SOO_AGENCY_UID_SIZE);
	rtdm_mutex_unlock(&tx_rx_data_lock);

	/* Send the whole burst to the Listener, that is, the next immediate neighbour */
	if (pkt_data) {
		DBG("> SEND: "); DBG_BUFFER(pkt_data, sizeof(netstream_transceiver_packet_t) + pkt_size);

		sender_tx(tx_rx_data.sl_desc, pkt_data, pkt_size, 0);
	}
	rtdm_mutex_unlock(&pkt_lock);

	rtdm_mutex_lock(&tx_rx_data_lock);
	winenet_get_tx_rx_data()->tx_completed = true;
	rtdm_mutex_unlock(&tx_rx_data_lock);

	rtdm_event_signal(&wnet_event);

	return 0;
}

/**
 * This function is called when a packet is received. This can be a data packet to forward to a
 * consumer, a Iamasoo beacon to forward to the Discovery block or a Datalink beacon to handle in Winenet.
 * The call is performed by the Receiver.
 * - The packet parameter is a pointer to a netstream transceiver packet.
 * - The size refers to the whole netstream transceiver packet.
 */
void winenet_netstream_rx(sl_desc_t *sl_desc, plugin_desc_t *plugin_desc, void *packet, size_t size) {
	netstream_transceiver_packet_t *netstream_transceiver_packet = (netstream_transceiver_packet_t *) packet;

	DBG("RX: type=%d, state=%d\n", netstream_transceiver_packet->packet_type, get_state());

	if (netstream_transceiver_packet->packet_type == TRANSCEIVER_PKT_DATALINK) {
		rtdm_mutex_lock(&tx_rx_data_lock);
		winenet_get_tx_rx_data()->sl_desc = sl_desc;
		memcpy(&winenet_get_tx_rx_data()->last_beacon, netstream_transceiver_packet->payload, sizeof(wnet_beacon_t));
		rtdm_mutex_unlock(&tx_rx_data_lock);

		rtdm_event_signal(&wnet_event);

		return ;
	}

	if ((netstream_transceiver_packet->packet_type == TRANSCEIVER_PKT_DATA) &&
		(get_state() != WNET_STATE_IDLE) &&
		(get_state() != WNET_STATE_SPEAKER_WAIT_ACK)) {
		/*
		 * A received packet cannot be forwarded to the consumer if Winenet is in Idle or Speaker wait ACK state.
		 * If Winenet is in Listener state, this is the nominal situation.
		 * If Winenet is in Speaker state, this means that a packet has been lost and the chain will be rearmed by
		 * a Listener timeout coming from the current Listener.
		 */

		rtdm_mutex_lock(&pkt_lock);
		if (pkt_data) {
			/* Save the trans ID of the last received packet */
			rtdm_mutex_lock(&tx_rx_data_lock);
			winenet_get_tx_rx_data()->rx_transID = netstream_transceiver_packet->peerID;
			rtdm_mutex_unlock(&tx_rx_data_lock);

			memcpy(pkt_data->payload, netstream_transceiver_packet->payload, size - sizeof(netstream_transceiver_packet_t));

			/*
			 * A recv can be performed only if the requester has allocated the packet buffer, that is,
			 * the stream init operation has been done.
			 */
			if (likely(pkt_req_data)) {
				/* Forward the recv event to the requester. Do not care about the size. */
				receiver_rx(sl_desc, plugin_desc, pkt_req_data, 0);
			}

			/* Go to Speaker state */
			rtdm_mutex_lock(&tx_rx_data_lock);
			winenet_get_tx_rx_data()->plugin_desc = plugin_desc;
			winenet_get_tx_rx_data()->rx_completed = true;
			rtdm_mutex_unlock(&tx_rx_data_lock);

			DBG("RX: rx_completed\n");

			rtdm_event_signal(&wnet_event);
		}
		rtdm_mutex_unlock(&pkt_lock);

		DBG("< RECV: "); DBG_BUFFER(netstream_transceiver_packet, size);
	}
}

/***** FSM *****/

/**
 * IDLE state.
 */
static void wnet_state_idle(wnet_state_t old_state) {
	uint8_t index;
	neighbour_desc_t listener;
	wnet_tx_rx_data_t tx_rx_data;
	wnet_beacon_t last_beacon;
	int ret;

	DBG("Idle\n");

	while (1) {

		rtdm_event_wait(&wnet_event);

		winenet_copy_tx_rx_data(&tx_rx_data);
		winenet_get_last_beacon(&last_beacon);

		if (last_beacon.id == WNET_BEACON_REQUEST_TO_SEND_NETSTREAM) {
			DBG("Request To Send Netstream beacon received from "); DBG_BUFFER(&tx_rx_data.sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE);

			/* Ask the transcoder to deactivate the Discovery */
			discovery_disable();

			if ((ret = winenet_get_my_index_and_listener(&index, &listener)) < 0)
				continue;

			n_neighbours = ret;
			n_soos = n_neighbours + 1;

			my_index = index;
			memcpy(&my_listener, &listener, sizeof(neighbour_desc_t));

			DBG("Speaker index: %d\n", my_index);
			DBG("My Listener agency UID: "); DBG_BUFFER(&my_listener.agencyUID, SOO_AGENCY_UID_SIZE);

			/* Allocate the temporary buffers only if the stream init operation has not been done */
			if (!pkt_data) {
				DBG("Allocate wnet_pkt_tmp: size=%d\n", last_beacon.transID);

				/*
				 * Allocate a temporary packet buffer, as we did not get any buffer from the requester.
				 * The packet size is in the transID field of the REQUEST TO SEND NETSTREAM beacon.
				 */
				pkt_tmp_data = (netstream_transceiver_packet_t *) kzalloc(sizeof(netstream_transceiver_packet_t) + last_beacon.transID, GFP_ATOMIC);
				pkt_tmp_size = last_beacon.transID;
				pkt_tmp_data->packet_type = TRANSCEIVER_PKT_DATA;

				rtdm_mutex_lock(&pkt_lock);
				pkt_size = pkt_tmp_size;
				pkt_data = pkt_tmp_data;
				rtdm_mutex_unlock(&pkt_lock);
			}

			rtdm_mutex_lock(&tx_rx_data_lock);
			/* Set the destination */
			memcpy(&winenet_get_tx_rx_data()->sl_desc->agencyUID_to, &my_listener.agencyUID, SOO_AGENCY_UID_SIZE);
			rtdm_mutex_unlock(&tx_rx_data_lock);

			DBG("Send ACK to: "); DBG_BUFFER(&last_beacon.u.medium_request_netstream.speakerUID, SOO_AGENCY_UID_SIZE);

			winenet_send_beacon(WNET_BEACON_ACKNOWLEDGMENT, &last_beacon.u.medium_request_netstream.speakerUID, &last_beacon.u.medium_request_netstream.speakerUID, get_my_agencyUID(), 0);

			change_state(WNET_STATE_LISTENER);
			return ;
		}

		if (last_beacon.id == WNET_BEACON_TRANSMISSION_COMPLETED_NETSTREAM) {
			/* The first Speaker might not have received our first Acknowledge beacon. Re-send it. */

			DBG("Transmission Completed Netstream beacon received from "); DBG_BUFFER(&tx_rx_data.sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE);

			rtdm_mutex_lock(&tx_rx_data_lock);
			/* Set the destination */
			memcpy(&winenet_get_tx_rx_data()->sl_desc->agencyUID_to, &my_listener.agencyUID, SOO_AGENCY_UID_SIZE);
			rtdm_mutex_unlock(&tx_rx_data_lock);

			DBG("Send ACK to: "); DBG_BUFFER(&last_beacon.u.transmission_completed_netstream.speakerUID, SOO_AGENCY_UID_SIZE);

			winenet_send_beacon(WNET_BEACON_ACKNOWLEDGMENT, &last_beacon.u.transmission_completed_netstream.speakerUID, &last_beacon.u.transmission_completed_netstream.speakerUID, get_my_agencyUID(), 0);

			continue;
		}

		if (request_to_send_pending) {
			change_state(WNET_STATE_SPEAKER);
			return ;
		}

	}
}

/**
 * SPEAKER state.
 * A data packet is being sent in this state.
 */
static void wnet_state_speaker(wnet_state_t old_state) {
	int ret;
	wnet_tx_rx_data_t tx_rx_data;
	wnet_neighbour_t *cur_neighbour;

	DBG("Speaker\n");

	if (unlikely(request_to_send_pending)) {
		cur_neighbour = list_entry(request_to_send_neighbour, wnet_neighbour_t, list);

		winenet_send_beacon(WNET_BEACON_REQUEST_TO_SEND_NETSTREAM, &cur_neighbour->neighbour->agencyUID, get_my_agencyUID(), &cur_neighbour->neighbour->agencyUID, pkt_req_size);

		change_state(WNET_STATE_SPEAKER_WAIT_ACK);
		return ;
	}

	if (unlikely(transmission_completed_pending)) {
		cur_neighbour = list_entry(transmission_completed_neighbour, wnet_neighbour_t, list);

		winenet_send_beacon(WNET_BEACON_TRANSMISSION_COMPLETED_NETSTREAM, &cur_neighbour->neighbour->agencyUID, get_my_agencyUID(), &cur_neighbour->neighbour->agencyUID, 0);

		change_state(WNET_STATE_SPEAKER_WAIT_ACK);
		return ;
	}

	while (1) {

		ret = rtdm_event_timedwait(&wnet_event, MICROSECS(2 * WNET_TLISTENER(n_soos)), NULL);

		winenet_copy_tx_rx_data(&tx_rx_data);

		if (ret == 0) {

			if (send_first_burst) {
				/* First burst sent by the first Speaker to initiate the chain */

				winenet_copy_tx_rx_data(&tx_rx_data);

				send_first_burst = false;

				DBG("Send first burst, recipient: "); DBG_BUFFER(&my_listener.agencyUID, SOO_AGENCY_UID_SIZE);

				rtdm_mutex_lock(&tx_rx_data_lock);
				memcpy(&winenet_get_tx_rx_data()->sl_desc->agencyUID_to, &my_listener.agencyUID, SOO_AGENCY_UID_SIZE);
				rtdm_mutex_unlock(&tx_rx_data_lock);

				/* Send the whole burst to the Listener, that is, the next immediate neighbour */
				rtdm_mutex_lock(&pkt_lock);
				if (pkt_data) {
					DBG("> SEND: ");
					DBG_BUFFER(pkt_data, sizeof(netstream_transceiver_packet_t) + pkt_size);

					sender_tx(tx_rx_data.sl_desc, pkt_data, pkt_size, 0);
				}
				rtdm_mutex_unlock(&pkt_lock);

				change_state(WNET_STATE_LISTENER);
				return ;
			}

			if (tx_rx_data.tx_completed) {
				rtdm_mutex_lock(&tx_rx_data_lock);
				winenet_get_tx_rx_data()->tx_completed = true;
				rtdm_mutex_unlock(&tx_rx_data_lock);

				change_state(WNET_STATE_LISTENER);
				return ;
			}

			if (tx_rx_data.rx_completed) {
				DBG("RX completed\n");

				rtdm_mutex_lock(&tx_rx_data_lock);
				winenet_get_tx_rx_data()->rx_completed = false;
				rtdm_mutex_unlock(&tx_rx_data_lock);
			}

		}

		/* Only the first Speaker can rearm the chain because of a Listener timeout */
		if (my_index == 0) {
			/* Speaker timeout. A packet was lost. */
			DBG("Speaker Timeout\n");

			/* Send the whole burst */
			DBG("Re-send, recipient: "); DBG_BUFFER(&my_listener.agencyUID, SOO_AGENCY_UID_SIZE);

			rtdm_mutex_lock(&tx_rx_data_lock);
			memcpy(&winenet_get_tx_rx_data()->sl_desc->agencyUID_to, &my_listener.agencyUID, SOO_AGENCY_UID_SIZE);
			rtdm_mutex_unlock(&tx_rx_data_lock);

			/* Send the last received packet to the Listener, that is, the next immediate neighbour */
			rtdm_mutex_lock(&pkt_lock);
			if (pkt_data) {
				DBG("> SEND: "); //DBG_BUFFER(pkt_data, sizeof(netstream_transceiver_packet_t) + pkt_size);

				sender_tx(tx_rx_data.sl_desc, pkt_data, pkt_size, 0);
			}
			rtdm_mutex_unlock(&pkt_lock);
		}

	}
}

/**
 * SPEAKER WAIT ACK state.
 */
static void wnet_state_speaker_wait_ack(wnet_state_t old_state) {
	int ret;
	wnet_beacon_t last_beacon;
	wnet_neighbour_t *cur_neighbour;

	DBG("Speaker wait ACK\n");

	while (1) {

		/* Timeout of Tspeaker us */
		ret = rtdm_event_timedwait(&wnet_event, MICROSECS(WNET_TSPEAKER_ACK), NULL);

		if (ret == 0) {

			winenet_get_last_beacon(&last_beacon);

			if ((request_to_send_pending) && (last_beacon.id == WNET_BEACON_ACKNOWLEDGMENT)) {
				DBG("Recv ACK from: "); DBG_BUFFER(&last_beacon.u.acknowledgment.listenerUID, SOO_AGENCY_UID_SIZE);

				cur_neighbour = list_entry(request_to_send_neighbour, wnet_neighbour_t, list);

				if (list_is_last(&cur_neighbour->list, winenet_get_neighbours())) {
					DBG("Last Listener: "); DBG_BUFFER(&cur_neighbour->neighbour->agencyUID, SOO_AGENCY_UID_SIZE);

					request_to_send_pending = false;
					send_first_burst = true;
					rtdm_event_signal(&wnet_event);

					change_state(WNET_STATE_SPEAKER);
					return ;
				}
				else {
					request_to_send_neighbour = request_to_send_neighbour->next;

					change_state(WNET_STATE_SPEAKER);
					return ;
				}
			}

			if ((transmission_completed_pending) && (last_beacon.id == WNET_BEACON_ACKNOWLEDGMENT)) {
				DBG("Recv ACK from: "); DBG_BUFFER(&last_beacon.u.acknowledgment.listenerUID, SOO_AGENCY_UID_SIZE);

				cur_neighbour = list_entry(transmission_completed_neighbour, wnet_neighbour_t, list);

				if (list_is_last(&cur_neighbour->list, winenet_get_neighbours())) {
					DBG("TRANSMISSION COMPLETED NETSTREAM: "); DBG_BUFFER(&cur_neighbour->neighbour->agencyUID, SOO_AGENCY_UID_SIZE);
					DBG("Last Listener!\n");

					/* Reset global variables */
					reset_pkt_data();

					/* Enable Discovery */
					discovery_enable();

					change_state(WNET_STATE_IDLE);
					return ;
				}
				else {
					transmission_completed_neighbour = transmission_completed_neighbour->next;

					change_state(WNET_STATE_SPEAKER);
					return ;
				}
			}

		}
		else {
			/* Go to Speaker state to re-send the REQUEST TO SEND NETSTREAM beacon */
			change_state(WNET_STATE_SPEAKER);
			return ;
		}

	}
}

/**
 * LISTENER state.
 */
static void wnet_state_listener(wnet_state_t old_state) {
	wnet_tx_rx_data_t tx_rx_data;
	wnet_beacon_t last_beacon;
	int ret;

	DBG("Listener\n");

	while (1) {

		ret = rtdm_event_timedwait(&wnet_event, MICROSECS(2 * WNET_TLISTENER(n_soos)), NULL);

		winenet_copy_tx_rx_data(&tx_rx_data);
		winenet_get_last_beacon(&last_beacon);

		if (ret == 0) {

			if (last_beacon.id == WNET_BEACON_TRANSMISSION_COMPLETED_NETSTREAM) {
				/* The first Speker might not have received our first Acknowledge beacon. Re-send it. */
				winenet_send_beacon(WNET_BEACON_ACKNOWLEDGMENT, &last_beacon.u.transmission_completed_netstream.speakerUID, &last_beacon.u.transmission_completed_netstream.speakerUID, get_my_agencyUID(), 0);

				/* Reset global variables */
				reset_pkt_data();

				/* Ask the transcoder to activate the Discovery */
				discovery_enable();

				change_state(WNET_STATE_IDLE);
				return ;
			}

			if (last_beacon.id == WNET_BEACON_REQUEST_TO_SEND_NETSTREAM) {
				/* The first Speaker might not have received our first Acknowledge beacon. Re-send it. */

				DBG("Request To Send Netstream beacon received from: "); DBG_BUFFER(&tx_rx_data.sl_desc->agencyUID_from, SOO_AGENCY_UID_SIZE);

				rtdm_mutex_lock(&tx_rx_data_lock);
				/* Set the destination */
				memcpy(&tx_rx_data.sl_desc->agencyUID_to, &my_listener.agencyUID, SOO_AGENCY_UID_SIZE);
				rtdm_mutex_unlock(&tx_rx_data_lock);

				DBG("Send ACK to: "); DBG_BUFFER(&last_beacon.u.medium_request_netstream.speakerUID, SOO_AGENCY_UID_SIZE);

				winenet_send_beacon(WNET_BEACON_ACKNOWLEDGMENT, &last_beacon.u.medium_request_netstream.speakerUID, &last_beacon.u.medium_request_netstream.speakerUID, get_my_agencyUID(), 0);

				continue;
			}

			if (tx_rx_data.rx_completed) {
				DBG("RX completed\n");

				rtdm_mutex_lock(&tx_rx_data_lock);
				winenet_get_tx_rx_data()->rx_completed = false;
				rtdm_mutex_unlock(&tx_rx_data_lock);

				DBG("Listener > Speaker\n");
				change_state(WNET_STATE_SPEAKER);

				return ;
			}
		}

		/* Only the first Speaker can rearm the chain because of a Listener timeout */
		if (my_index == 0) {
			/* Listener timeout. A packet was lost. */
			DBG("Listener Timeout\n");

			/* Send the whole burst */
			DBG("Re-send, recipient: "); DBG_BUFFER(&my_listener.agencyUID, SOO_AGENCY_UID_SIZE);

			rtdm_mutex_lock(&tx_rx_data_lock);
			memcpy(&winenet_get_tx_rx_data()->sl_desc->agencyUID_to, &my_listener.agencyUID, SOO_AGENCY_UID_SIZE);
			rtdm_mutex_unlock(&tx_rx_data_lock);

			/* Send the last received packet to the Listener, that is, the next immediate neighbour */
			rtdm_mutex_lock(&pkt_lock);
			if (pkt_data) {
				DBG("> SEND: ");
				//DBG_BUFFER(pkt_data, sizeof(netstream_transceiver_packet_t) + pkt_size);

				sender_tx(tx_rx_data.sl_desc, pkt_data, pkt_size, 0);
			}
			rtdm_mutex_unlock(&pkt_lock);
		}

	}
}

/**
 * Change the state of Winenet.
 */
static void change_state(wnet_state_t new_state) {
	winenet_change_state(&fsm_handle, new_state);
}

/**
 * Get the state of Winenet.
 */
static wnet_state_t get_state(void) {
	return winenet_get_state(&fsm_handle);
}

/* FSM function table */
static wnet_state_fn_t fsm_functions[WNET_STATE_N] = {
	[WNET_STATE_IDLE]		= wnet_state_idle,
	[WNET_STATE_SPEAKER]		= wnet_state_speaker,
	[WNET_STATE_SPEAKER_WAIT_ACK]	= wnet_state_speaker_wait_ack,
	[WNET_STATE_LISTENER]		= wnet_state_listener,
};

/***** Initialization *****/

/**
 * Initialization of Winenet.
 * This function is called from Datalink.
 */
void winenet_netstream_init(void) {
	DBG("Winenet netstream initialization\n");

	rtdm_event_init(&wnet_event, 0);

	rtdm_mutex_init(&pkt_lock);

	rtdm_event_init(&fsm_handle.event, 0);
	fsm_handle.funcs = fsm_functions;
	winenet_start_fsm_task("Wnet_netstream", &fsm_handle);
}
