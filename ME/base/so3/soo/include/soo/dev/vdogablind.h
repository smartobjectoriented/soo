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

#ifndef VDOGABLIND_H
#define VDOGABLIND_H

#include <soo/ring.h>
#include <soo/grant_table.h>
#include <soo/vdevfront.h>

#define VDOGABLIND_PACKET_SIZE	32

#define VDOGABLIND_NAME		"vdogablind"
#define VDOGABLIND_PREFIX		"[" VDOGABLIND_NAME "] "

#define VDOGABLIND_STOP_CMD		0
#define VDOGABLIND_UP_CMD		1
#define VDOGABLIND_DOWN_CMD		2

typedef struct {
	int cmd_blind;
} vdogablind_request_t;

typedef struct  {
	/* empty */
} vdogablind_response_t;

/*
 * Generate ring structures and types.
 */
DEFINE_RING_TYPES(vdogablind, vdogablind_request_t, vdogablind_response_t);

/*
 * General structure for this virtual device (backend side)
 */

typedef struct {
	/* Must be the first field */
	vdevfront_t vdevfront;

	vdogablind_front_ring_t ring;
	unsigned int irq;

	grant_ref_t ring_ref;
	grant_handle_t handle;
	uint32_t evtchn;

} vdogablind_t;

static inline vdogablind_t *to_vdogablind(struct vbus_device *vdev) {
	vdevfront_t *vdevback = dev_get_drvdata(vdev->dev);
	return container_of(vdevback, vdogablind_t, vdevfront);
}

void vdogablind_send_blind_cmd(int cmd);

#endif /* VDOGABLIND_H */
