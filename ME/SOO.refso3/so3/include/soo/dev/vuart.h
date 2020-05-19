/*
 * Copyright (C) 2016-2018 Baptiste Delporte <bonel@bonel.net>
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

#ifndef VUART_H
#define VUART_H

#include <device/irq.h>

#include <soo/ring.h>
#include <soo/grant_table.h>

#define VUART_NAME	"vuart"
#define VUART_PREFIX	"[" VUART_NAME "] "

#define INPUT_TRANSMIT_DESTINATION_VTTY (1 << 0)
#define INPUT_TRANSMIT_DESTINATION_TTY (1 << 1)

typedef struct {
	char c;
	uint8_t	pad[1];
} vuart_request_t;

typedef struct {
	char c;
	uint8_t	pad[1];
} vuart_response_t;

DEFINE_RING_TYPES(vuart, vuart_request_t, vuart_response_t);

bool vuart_ready(void);

extern struct vc_data *__vc;

void vuart_write(char *buffer, int count);
void send_to_console(char ch);
char vuart_read_char(void);

typedef struct {

	struct vbus_device  *dev;

	vuart_front_ring_t ring;
	grant_ref_t ring_ref;
	grant_handle_t handle;
	uint32_t evtchn;
	uint32_t irq;


} vuart_t;

/* ISR associated to the ring */
extern vuart_t vuart;

irq_return_t vuart_interrupt(int irq, void *data);

/* State management */
void vuart_probe(void);
void vuart_closed(void);
void vuart_suspend(void);
void vuart_resume(void);
void vuart_connected(void);
void vuart_reconfiguring(void);
void vuart_shutdown(void);

void vuart_vbus_init(void);

/* Processing and connected state management */
void vuart_start(void);
void vuart_end(void);
bool vuart_is_connected(void);


#endif /* VUART_H */
