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

#ifndef VENOCEAN_H
#define VENOCEAN_H

#include <soo/ring.h>
#include <soo/grant_table.h>

#include <linux/vt_kern.h>

#define VENOCEAN_NAME	"venocean"
#define VENOCEAN_PREFIX	"================== [" VENOCEAN_NAME "] "

#define ENOCEAN_UART5_DEV "ttyAMA5"

#define VWEATHER_FRAME_SIZE 10

/*  This is a reserved char code we use to query (patched) Qemu to retrieve the window size. */
#define SERIAL_GWINSZ   '\254'

typedef struct {
	char c;
} venocean_request_t;

typedef struct {
	char c;
} venocean_response_t;

DEFINE_RING_TYPES(venocean, venocean_request_t, venocean_response_t);

/*
 * General structure for this virtual device (backend side)
 */

typedef struct {
	vdevback_t vdevback;

	spinlock_t ring_lock;
	venocean_back_ring_t ring;
	unsigned int irq;

} venocean_t;

bool venocean_ready(void);

static inline venocean_t *to_venocean(struct vbus_device *vdev) {
	vdevback_t *vdevback = dev_get_drvdata(&vdev->dev);
	return container_of(vdevback, venocean_t, vdevback);
}


#endif /* VENOCEAN_H */
