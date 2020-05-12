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

#define VUIHANDLER_NAME		"vuihandler"
#define VUIHANDLER_PREFIX	"[" VUIHANDLER_NAME "] "

#define VUIHANDLER_DEV_MAJOR	121
#define VUIHANDLER_DEV_NAME	"/dev/vuihandler"

/* Periods and delays in ms */

#define VUIHANDLER_APP_WATCH_PERIOD	10000
#define VUIHANDLER_APP_RSP_TIMEOUT	2000
#define VUIHANDLER_APP_VBSTORE_DIR	"backend/" VUIHANDLER_NAME
#define VUIHANDLER_APP_VBSTORE_NODE	"connected-app-me-spid"

typedef struct {
	uint8_t		type;
	uint8_t		spid[SPID_SIZE];
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
#define VUIHANDLER_DATA		1

#define VUIHANDLER_BT_PKT_HEADER_SIZE	sizeof(vuihandler_pkt_t)

typedef struct {
	uint32_t		id;
	size_t			size;
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
} vuihandler_rx_response_t;

DEFINE_RING_TYPES(vuihandler_rx, vuihandler_rx_request_t, vuihandler_rx_response_t);

typedef struct {
	uint8_t			spid[SPID_SIZE];
	struct list_head	list;
} vuihandler_connected_app_t;

bool vuihandler_ready(void);
void vuihandler_open_rfcomm(pid_t pid);

void vuihandler_sl_recv(vuihandler_pkt_t *vuihandler_pkt, size_t vuihandler_pkt_size);
void vuihandler_send(void *data, size_t size);
void vuihandler_get_app_spid(uint8_t spid[SPID_SIZE]);

#if defined(CONFIG_BT_RFCOMM)

#include <asm/signal.h>

void rfcomm_send_sigterm(void);

#endif /* CONFIG_BT_RFCOMM */

typedef struct {

	struct vbus_device  *dev;

	vuihandler_tx_back_ring_t ring;
	unsigned int	irq;

} vuihandler_tx_ring_t;

typedef struct {

	struct vbus_device  *dev;

	vuihandler_rx_back_ring_t ring;
	unsigned int	irq;

} vuihandler_rx_ring_t;

typedef struct {
	char		*data;
	unsigned int	pfn;

} vuihandler_shared_buffer_t;

typedef struct {

	vuihandler_tx_ring_t	tx_rings[MAX_DOMAINS];
	vuihandler_rx_ring_t	rx_rings[MAX_DOMAINS];

	vuihandler_shared_buffer_t	tx_buffers[MAX_DOMAINS];
	vuihandler_shared_buffer_t	rx_buffers[MAX_DOMAINS];

	/* Table that holds the SPID of the ME whose frontends are connected */
	uint8_t spid[MAX_DOMAINS][SPID_SIZE];

	struct vbus_device  *vdev[MAX_DOMAINS];

} vuihandler_t;

extern vuihandler_t vuihandler;

extern uint8_t vuihandler_null_spid[SPID_SIZE];

/* ISRs associated to the rings */
irqreturn_t vuihandler_tx_interrupt(int irq, void *dev_id);
irqreturn_t vuihandler_rx_interrupt(int irq, void *dev_id);

void vuihandler_update_spid_vbstore(uint8_t spid[SPID_SIZE]);

/* State management */
void vuihandler_probe(struct vbus_device *dev);
void vuihandler_close(struct vbus_device *dev);
void vuihandler_suspend(struct vbus_device *dev);
void vuihandler_resume(struct vbus_device *dev);
void vuihandler_connected(struct vbus_device *dev);
void vuihandler_reconfigured(struct vbus_device *dev);
void vuihandler_shutdown(struct vbus_device *dev);

void vuihandler_vbus_init(void);

bool vuihandler_start(domid_t domid);
void vuihandler_end(domid_t domid);
bool vuihandler_is_connected(domid_t domid);

#endif /* VUIHANDLER_H */
