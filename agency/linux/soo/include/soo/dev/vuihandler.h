/*
 * Copyright (C) 2018-2019 Baptiste Delporte <bonel@bonel.net>
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

#ifndef VUIHANDLER_H
#define VUIHANDLER_H

#include <soo/uapi/soo.h>

#include <soo/ring.h>
#include <soo/grant_table.h>
#include <soo/vdevback.h>

#define VUIHANDLER_NAME		"vuihandler"
#define VUIHANDLER_PREFIX	"[" VUIHANDLER_NAME "] "

#define VUIHANDLER_DEV_MAJOR	121
#define VUIHANDLER_DEV_NAME	"/dev/vuihandler"

/* Periods and delays in ms */

#define VUIHANDLER_APP_WATCH_PERIOD	10000
#define VUIHANDLER_APP_RSP_TIMEOUT	2000
#define VUIHANDLER_APP_VBSTORE_DIR	"backend/" VUIHANDLER_NAME
#define VUIHANDLER_APP_VBSTORE_NODE	"connected-app-me-spid"

typedef struct __attribute__((packed)) {
	int32_t		slotID;
	uint8_t		type;
	uint8_t		payload[0];
} vuihandler_pkt_t;

#define VUIHANDLER_MAX_PACKETS		8

/* Maximal size of a BT packet payload */
#define VUIHANDLER_MAX_PAYLOAD_SIZE	1024

/* Maximal size of a BT packet's data (the header is included) */
#define VUIHANDLER_MAX_PKT_SIZE		(sizeof(vuihandler_pkt_t) + VUIHANDLER_MAX_PAYLOAD_SIZE)


/* Shared buffer size */
#define VUIHANDLER_BUFFER_SIZE		(VUIHANDLER_MAX_PACKETS * VUIHANDLER_MAX_PKT_SIZE)

#define VUIHANDLER_BEACON	0
#define VUIHANDLER_DATA		1 /* Simple ME state data, could be anything to update the ME state */
#define VUIHANDLER_ME_INJECT	2 /* Specify the packet contains a chunk of the ME to be injected  */
#define VUIHANDLER_ME_SIZE	3 /* Specify that a ME injection is to be initiated. The packet contains the ME size */
#define VUIHANDLER_ASK_LIST	4 /* Ask for the XML ME list */
#define VUIHANDLER_SEND		5 /* Specify that the packet contains an event data to be forwarded to the ME */
#define VUIHANDLER_SELECT	6 /* Ask for the ME model */
#define VUIHANDLER_POST		7

/* The header size is stripped from the payload address, as the payload
directly follows the type in term of bytes in memory */
#define VUIHANDLER_BT_PKT_HEADER_SIZE	(sizeof(uint32_t) + sizeof(uint8_t))

/* Number of TX packets which can be queued in the TX buffer */
#define VUIHANDLER_TX_BUF_SIZE 	8

#define RING_BUF_SIZE		1024

typedef struct {
	vuihandler_pkt_t *pkt;
	uint32_t size;
} tx_pkt_t;

/* Ring buffer used to queue the TX packets */
typedef struct {
	tx_pkt_t ring[VUIHANDLER_TX_BUF_SIZE];
	uint32_t put_index;
	uint32_t get_index;
	uint32_t cur_size;
} vuihandler_tx_buf_t;


typedef struct {
	uint32_t		id;
	uint8_t 		type;
	size_t			size;
	uint8_t 		buf[RING_BUF_SIZE];
} vuihandler_tx_request_t;

/* Not used */
typedef struct {
	uint32_t		val;
} vuihandler_tx_response_t;

DEFINE_RING_TYPES(vuihandler_tx, vuihandler_tx_request_t, vuihandler_tx_response_t);

/* Not used */
typedef struct {
	uint32_t		val;
} vuihandler_rx_request_t;

typedef struct {
	uint32_t		id;
	size_t			size;
	uint8_t			type;
	uint8_t 		buf[RING_BUF_SIZE];
} vuihandler_rx_response_t;

DEFINE_RING_TYPES(vuihandler_rx, vuihandler_rx_request_t, vuihandler_rx_response_t);

typedef struct {
	uint64_t		spid;
	struct list_head	list;
} vuihandler_connected_app_t;

bool vuihandler_ready(void);

void vuihandler_open_rfcomm(pid_t pid);

void vuihandler_sl_recv(vuihandler_pkt_t *vuihandler_pkt, size_t vuihandler_pkt_size);
void vuihandler_send(void *data, size_t size);
void vuihandler_get_app_spid(uint64_t spid);

#if defined(CONFIG_BT_RFCOMM)

#include <asm/signal.h>

void vuihandler_close_rfcomm(void);

#endif /* CONFIG_BT_RFCOMM */

typedef struct {
	vuihandler_tx_back_ring_t ring;
	unsigned int	irq;

} vuihandler_tx_ring_t;

typedef struct {
	vuihandler_rx_back_ring_t ring;
	unsigned int	irq;

} vuihandler_rx_ring_t;

typedef struct {
	char		*data;
	unsigned int	pfn;
} vuihandler_shared_buffer_t;


typedef struct {
	vdevback_t vdevback;

	vuihandler_tx_ring_t	tx_rings;
	vuihandler_rx_ring_t	rx_rings;

	vuihandler_shared_buffer_t	tx_buffers;
	vuihandler_shared_buffer_t	rx_buffers;

	int32_t otherend_id;

	uint64_t spid; /* Kept here as in case we want to use it */

} vuihandler_t;

/* ISRs associated to the rings */
irqreturn_t vuihandler_tx_interrupt(int irq, void *dev_id);
irqreturn_t vuihandler_rx_interrupt(int irq, void *dev_id);

void vuihandler_update_spid_vbstore(uint64_t spid);

int vuihandler_send_from_agency(uint8_t *data, uint32_t size, uint8_t type);

/* State management */
void vuihandler_probe(struct vbus_device *dev);
void vuihandler_remove(struct vbus_device *dev);
void vuihandler_close(struct vbus_device *dev);
void vuihandler_connected(struct vbus_device *dev);
void vuihandler_reconfigured(struct vbus_device *dev);
void vuihandler_suspend(struct vbus_device *dev);
void vuihandler_resume(struct vbus_device *dev);


#endif /* VUIHANDLER_H */
