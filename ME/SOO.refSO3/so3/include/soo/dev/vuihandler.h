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

#include <types.h>

#include <soo/soo.h>
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

/* vUIHandler packet send period */
#define VUIHANDLER_PERIOD	1000

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

typedef struct {
	uint8_t			spid[SPID_SIZE];
	struct list_head	list;
} vuihandler_connected_app_t;

bool vuihandler_ready(void);

typedef struct {

	struct vbus_device	*dev;

	vuihandler_tx_front_ring_t	tx_ring;
	grant_ref_t	tx_ring_ref;
	grant_handle_t	tx_handle;
	uint32_t	tx_evtchn;
	uint32_t	tx_irq;

	vuihandler_rx_front_ring_t	rx_ring;
	grant_ref_t 	rx_ring_ref;
	grant_handle_t	rx_handle;
	uint32_t	rx_evtchn;
	uint32_t	rx_irq;

	char		*tx_data;
	unsigned int	tx_pfn;

	char		*rx_data;
	unsigned int	rx_pfn;

	struct vbus_watch	app_watch;

} vuihandler_t;

extern vuihandler_t vuihandler;

typedef void(*ui_update_spid_t)(uint8_t *);
typedef void(*ui_interrupt_t)(char *data, size_t size);

void vuihandler_register_callback(ui_update_spid_t ui_update_spid, ui_interrupt_t ui_interrupt);

void vuihandler_send(void *data, size_t size);

void vuihandler_get_app_spid(uint8_t spid[SPID_SIZE]);

/* ISRs associated to the rings */
irq_return_t vuihandler_tx_interrupt(int irq, void *dev_id);
irq_return_t vuihandler_rx_interrupt(int irq, void *dev_id);

void vuihandler_app_watch_fn(struct vbus_watch *watch);

/*
 * Interface with the client.
 * These functions must be provided in the applicative part.
 */
void ui_update_app_spid(uint8_t *spid);
void ui_interrupt(char *data, size_t size);

/* Shared buffer setup */
int vuihandler_set_shared_buffer(void *data, size_t size);
int vuihandler_setup_shared_buffer(void);

/* State management */
void vuihandler_probe(void);
void vuihandler_closed(void);
void vuihandler_suspend(void);
void vuihandler_resume(void);
void vuihandler_connected(void);
void vuihandler_reconfiguring(void);
void vuihandler_shutdown(void);

void vuihandler_vbus_init(void);

/* Processing and connected state management */
void vuihandler_start(void);
void vuihandler_end(void);
bool vuihandler_is_connected(void);

#endif /* VUIHANDLER_H */
