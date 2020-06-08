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

/*  This is a reserved char code we use to query (patched) Qemu to retrieve the window size. */
#define SERIAL_GWINSZ   '\254'

typedef struct {
	char c;
} vuart_request_t;

typedef struct {
	char c;
} vuart_response_t;

DEFINE_RING_TYPES(vuart, vuart_request_t, vuart_response_t);

/*
 * General structure for this virtual device (backend side)
 */

typedef struct {
	vdevback_t vdevback;

	spinlock_t ring_lock;
	vuart_back_ring_t ring;
	unsigned int irq;

} vuart_t;

bool vuart_ready(void);

static inline vuart_t *to_vuart(struct vbus_device *vdev) {
	vdevback_t *vdevback = dev_get_drvdata(&vdev->dev);
	return container_of(vdevback, vuart_t, vdevback);
}


#endif /* VUART_H */
