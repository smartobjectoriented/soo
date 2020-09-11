/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef VDOGA_H
#define VDOGA_H

#include <soo/ring.h>
#include <soo/grant_table.h>
#include <soo/vdevfront.h>

#define VDOGA_PACKET_SIZE	32

#define VDOGA_NAME		"vdoga"
#define VDOGA_PREFIX		"[" VDOGA_NAME "] "

typedef struct {
	char buffer[VDOGA_PACKET_SIZE];
} vdoga_request_t;

typedef struct  {
	char buffer[VDOGA_PACKET_SIZE];
} vdoga_response_t;

/*
 * Generate ring structures and types.
 */
DEFINE_RING_TYPES(vdoga, vdoga_request_t, vdoga_response_t);

/*
 * General structure for this virtual device (backend side)
 */

typedef struct {
	vdevfront_t vdevfront;

	vdoga_front_ring_t ring;
	unsigned int irq;

	grant_ref_t ring_ref;
	grant_handle_t handle;
	uint32_t evtchn;

} vdoga_t;

static inline vdoga_t *to_vdoga(struct vbus_device *vdev) {
	vdevfront_t *vdevback = dev_get_drvdata(vdev->dev);
	return container_of(vdevback, vdoga_t, vdevfront);
}

#endif /* VDOGA_H */
