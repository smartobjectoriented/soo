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

#include <soo/ring.h>
#include <soo/grant_table.h>

#include <linux/vt_kern.h>

#define VUART_NAME	"vuart"
#define VUART_PREFIX	"[" VUART_NAME "] "

#define INPUT_TRANSMIT_DESTINATION_VTTY (1 << 0)
#define INPUT_TRANSMIT_DESTINATION_TTY (1 << 1)

#define MAX_BUF_CHARS	16

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

typedef struct {

	vuart_back_ring_t ring;
	unsigned int irq;

} vuart_ring_t;

/*
 * General structure for this virtual device (backend side)
 */
typedef struct {

	vuart_ring_t rings[MAX_DOMAINS];
	struct vbus_device  *vdev[MAX_DOMAINS];

	/* Array and counters handling bufferized chars when the backend is not Connected yet */
	char buf_chars[MAX_DOMAINS][MAX_BUF_CHARS];
	uint32_t buf_chars_prod[MAX_DOMAINS];
	uint32_t buf_chars_cons[MAX_DOMAINS];

} vuart_t;

extern vuart_t vuart;

irqreturn_t vuart_interrupt(int irq, void *dev_id);

void vuart_probe(struct vbus_device *dev);
void vuart_close(struct vbus_device *dev);
void vuart_suspend(struct vbus_device *dev);
void vuart_resume(struct vbus_device *dev);
void vuart_connected(struct vbus_device *dev);
void vuart_reconfigured(struct vbus_device *dev);
void vuart_shutdown(struct vbus_device *dev);

void vuart_vbus_init(void);

bool vuart_start(domid_t domid);
void vuart_end(domid_t domid);
bool vuart_is_connected(domid_t domid);

#endif /* VUART_H */
