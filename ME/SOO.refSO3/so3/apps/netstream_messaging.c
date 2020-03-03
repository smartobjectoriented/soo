/*
 * Copyright (C) 2016-2018 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2016-2018 Baptiste Delporte <bonel@bonel.net>
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

#include <mutex.h>
#include <delay.h>
#include <timer.h>
#include <heap.h>

#include <virtshare/avz.h>
#include <virtshare/hypervisor.h>
#include <virtshare/vbus.h>
#include <virtshare/soo.h>
#include <virtshare/console.h>
#include <virtshare/debug.h>
#include <virtshare/debug/dbgvar.h>
#include <virtshare/debug/logbool.h>

#include <virtshare/dev/vnetstream.h>

/* Index of this SOO in the global ecosystem, 0xff=uninitialized */
static uint32_t my_index = 0xff;
/* Number of neighbours in the global ecosystem, 0xff=uninitialized */
static uint32_t n_neighbours = 0xff;
/* Number of SOOs in the global ecosystem, 0xff=uninitialized */
static uint32_t n_soos = 0xff;

/* Netstream data */
static uint8_t *data_buffer = NULL; /* Packet buffer */
static size_t data_size;

/* Ecosystem table */
typedef struct {
	uint32_t	index;
	uint8_t		agencyUID[SOO_AGENCY_UID_SIZE];
} soo_desc_t;

typedef struct {
	soo_desc_t	soo[VNETSTREAM_MAX_SOO];
} soo_ecosystem_t;

static soo_ecosystem_t soo_ecosystem;

static struct completion msg_complete;
static struct mutex msg_lock;

static struct completion trigger_msg_complete;
static uint32_t debug_number = 0xffffffff;

static uint32_t pending_index = 0xffffffff;
static vnetstream_msg_t pending_msg;

static void send_msg_to_index(uint32_t index, vnetstream_msg_t *msg) {
	mutex_lock(&msg_lock);
	if (pending_index != 0xffffffff) {
		mutex_unlock(&msg_lock);
		return ;
	}

	pending_index = index;
	memcpy(&pending_msg, msg, sizeof(vnetstream_msg_t));
	mutex_unlock(&msg_lock);
}

void send_netstream_msg(uint8_t *dest_agencyUID, vnetstream_msg_t *msg) {
	uint32_t i;

	for (i = 0; i < 6; i++) {
		if (!memcmp(soo_ecosystem.soo[i].agencyUID, dest_agencyUID, SOO_AGENCY_UID_SIZE))
			send_msg_to_index(i, msg);
	}
}

/* Debugging: send a message manually */
#if 1
static void stream_init(void);
void do_debug0(void) {
	stream_init();
}

void debug0(void) {
	lprintk("%s\n", __func__);

	debug_number = 0;
	complete(&trigger_msg_complete);
}

void do_debug1(void) {
	vnetstream_msg_t msg;
	static uint32_t count = 0;

	memset(&msg, 0, sizeof(vnetstream_msg_t));
	memcpy(msg.sender_agencyUID, &my_agencyUID, SOO_AGENCY_UID_SIZE);
	memcpy((uint32_t *) msg.message, &count, sizeof(uint32_t));
	msg.status = 1;

	send_msg_to_index(0, &msg);

	count++;
}

void debug1(void) {
	debug_number = 1;
	complete(&trigger_msg_complete);
}

void do_debug2(void) {
	vnetstream_msg_t msg;
	static uint32_t count = 0;

	memset(&msg, 0, sizeof(vnetstream_msg_t));
	memcpy(msg.sender_agencyUID, &my_agencyUID, SOO_AGENCY_UID_SIZE);
	memcpy((uint32_t *) msg.message, &count, sizeof(uint32_t));
	msg.status = 1;

	send_msg_to_index(1, &msg);

	count++;
}

void debug2(void) {
	debug_number = 2;
	complete(&trigger_msg_complete);
}

void do_debug3(void) {
	vnetstream_msg_t msg;
	static uint32_t count = 0;

	memset(&msg, 0, sizeof(vnetstream_msg_t));
	memcpy(msg.sender_agencyUID, &my_agencyUID, SOO_AGENCY_UID_SIZE);
	memcpy((uint32_t *) msg.message, &count, sizeof(uint32_t));
	msg.status = 1;

	send_msg_to_index(2, &msg);

	count++;
}

void debug3(void) {
	debug_number = 3;
	complete(&trigger_msg_complete);
}

void do_debug4(void) {
	vnetstream_msg_t msg;
	static uint32_t count = 0;

	memset(&msg, 0, sizeof(vnetstream_msg_t));
	memcpy(msg.sender_agencyUID, &my_agencyUID, SOO_AGENCY_UID_SIZE);
	memcpy((uint32_t *) msg.message, &count, sizeof(uint32_t));
	msg.status = 1;

	send_msg_to_index(3, &msg);

	count++;
}

void debug4(void) {
	debug_number = 4;
	complete(&trigger_msg_complete);
}

static int trigger_msg_fn(void *arg) {
	while (1) {
		wait_for_completion(&trigger_msg_complete);

		switch (debug_number) {
		case 0:
			do_debug0();
			break;
		case 1:
			do_debug1();
			break;
		case 2:
			do_debug2();
			break;
		case 3:
			do_debug3();
			break;
		case 4:
			do_debug4();
			break;
		}

		debug_number = 0xffffffff;
	}

	return 0;
}
#endif

static void process_msg(void) {
	vnetstream_pkt_t *pkt = (vnetstream_pkt_t *) ((netstream_transceiver_packet_t *) data_buffer)->payload;

	if (pkt->msg[my_index].status == 1) {
		lprintk("Sender: "); lprintk_buffer(pkt->msg[my_index].sender_agencyUID, SOO_AGENCY_UID_SIZE);
		lprintk("Message: %d\n", *((uint32_t *) pkt->msg[my_index].message));

#if 0
		/* Forward the message to the client */
		netstream_msg(pkt->msg[my_index].sender_agencyUID, pkt->msg[my_index].message);
#endif

		pkt->msg[my_index].status = 0;
	}
}

static int msg_task_fn(void *arg) {
	vnetstream_pkt_t *pkt;
	vnetstream_msg_t *target_msg;

	while (1) {
		wait_for_completion(&msg_complete);

		mutex_lock(&msg_lock);

		if (pending_index != 0xffffffff) {
			pkt = (vnetstream_pkt_t *) ((netstream_transceiver_packet_t *) data_buffer)->payload;
			target_msg = &pkt->msg[pending_index];

			memcpy(target_msg, &pending_msg, sizeof(vnetstream_msg_t));
			pending_index = 0xffffffff;
		}

		process_msg();
		vnetstream_send(data_buffer);

		mutex_unlock(&msg_lock);
	}

	return 0;
}

void recv_interrupt(void *data) {
	complete(&msg_complete);
}

/***** Neighbourhood and global ecosystem management *****/

/**
 * Determine the index of this SOO.dio using the neighbourhood returned by vNetstream.
 */
static uint32_t get_my_index(char *neighbourhood, uint32_t n) {
	uint32_t i;
	int position_prev, position_cur = 0;
	uint32_t count = 0;

	/* This SOO.dio is alone */
	if (n == 0)
		return 0;

	for (i = 0; i < n; i++) {
		position_prev = position_cur;
		position_cur = memcmp(&neighbourhood[i * SOO_AGENCY_UID_SIZE], &my_agencyUID, SOO_AGENCY_UID_SIZE);

		if (position_cur > 0) {
			/* Head of the list */
			if (position_prev == 0) {
				/* This SOO is the first Speaker */
				return 0;
			} else if (position_prev < 0) {
				/* This SOO is between two neighbours in the list */
				return count;
			}
		}

		count++;
	}

	/* Tail of the list */
	if (position_cur < 0)
		return count;

	/* This should never be reached */
	BUG();
	return 0;
}

static void stream_init(void) {
	char *neighbourhood;
	uint32_t count = 0, i;
	int nr_pages;

	/*
	 * Allocate the buffer that will contain the incoming/outgoing vNetstream packets. Its size has to be
	 * sufficient to handle the case with several packets in one burst.
	 * The size of the netstream transceiver packet header has to be taken into consideration
	 * when allocating the packet buffer.
	 * The size that will be considered during the stream init refers to the payload.
	 */

	data_size = VNETSTREAM_PACKET_SIZE;
	nr_pages = DIV_ROUND_UP(sizeof(netstream_transceiver_packet_t) + data_size, PAGE_SIZE);
	data_buffer = (uint8_t *) get_contig_free_vpages(nr_pages);
	memset(data_buffer, 0, sizeof(netstream_transceiver_packet_t) + data_size);

	/* Retrieve the neighbourhood from vNetstream and parse it */

	neighbourhood = vnetstream_get_neighbourhood();

	while (memcmp(&neighbourhood[count * SOO_AGENCY_UID_SIZE], &null_agencyUID, SOO_AGENCY_UID_SIZE)) {
		DBG("Neighbour %d: ", count); DBG_BUFFER(&neighbourhood[count * SOO_AGENCY_UID_SIZE], SOO_AGENCY_UID_SIZE);
		count++;
	}

	n_neighbours = count;
	n_soos = n_neighbours + 1;
	my_index = get_my_index(neighbourhood, n_neighbours);

	DBG("#neighbours: %d\n", n_neighbours);
	DBG("#SOOs: %d\n", n_soos);
	DBG("My index: %d\n", my_index);

	/* Fill the ecosystem table with the indexes and the matching agency UIDs */
	count = 0;
	for (i = 0; i < n_soos; i++) {
		if (i == my_index)
			memcpy(soo_ecosystem.soo[i].agencyUID, &my_agencyUID, SOO_AGENCY_UID_SIZE);
		else {
			memcpy(soo_ecosystem.soo[i].agencyUID, &neighbourhood[count * SOO_AGENCY_UID_SIZE], SOO_AGENCY_UID_SIZE);
			count++;
		}
	}
	lprintk_buffer(&soo_ecosystem, sizeof(soo_ecosystem_t));

	/* Free the neighbour list after use */
	free(neighbourhood);

	/* vNetstream stream init operation */
	vnetstream_stream_init(data_buffer, data_size);
}

#warning Debugging: automatically perform stream init after 5 seconds
#if 1
static int stream_init_task_fn(void *arg) {
	uint32_t count = 5;
	while (count > 0) {
		msleep(1000);
		lprintk("%d...", count);
		count--;
	}
	lprintk("\n");

	stream_init();

	return 0;
}
void start_stream_init_task(void) {
	kernel_thread(stream_init_task_fn, "stream_init", NULL, 0);
}
#endif

/*
 * The main application of the ME is executed right after the bootstrap. It may be empty since activities can be triggered
 * by external events based on frontend activities.
 */
void messaging_init(void) {
	uint32_t i;

	memset(&soo_ecosystem, 0, sizeof(soo_ecosystem_t));
	for (i = 0; i < VNETSTREAM_MAX_SOO; i++)
		soo_ecosystem.soo[i].index = i;

#if 1
#warning Debugging: automatically perform stream init after 5 seconds
	start_stream_init_task();
#endif

	mutex_init(&msg_lock);
	init_completion(&msg_complete);
	kernel_thread(msg_task_fn, "msg", NULL, 0);

	init_completion(&trigger_msg_complete);
	kernel_thread(trigger_msg_fn, "trigger", NULL, 0);
}
