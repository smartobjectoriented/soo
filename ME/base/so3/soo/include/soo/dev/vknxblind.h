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

#ifndef VKNXBLIND_H
#define VKNXBLIND_H

#include <soo/ring.h>
#include <soo/grant_table.h>
#include <soo/vdevfront.h>


#define VKNXBLIND_NAME		"vknxblind"
#define VKNXBLIND_PREFIX		"[" VKNXBLIND_NAME "] "


#define VKNXBLIND_STOP_CMD		0
#define VKNXBLIND_UP_CMD		1
#define VKNXBLIND_DOWN_CMD		2


typedef struct {
	int knxblind_cmd;
} vknxblind_request_t;

typedef struct  {
	/* empty */
} vknxblind_response_t;

/*
 * Generate ring structures and types.
 */
DEFINE_RING_TYPES(vknxblind, vknxblind_request_t, vknxblind_response_t);

/*
 * General structure for this virtual device (backend side)
 */

typedef struct {
	/* Must be the first field */
	vdevfront_t vdevfront;

	vknxblind_front_ring_t ring;
	unsigned int irq;

	grant_ref_t ring_ref;
	grant_handle_t handle;
	uint32_t evtchn;

} vknxblind_t;

static inline vknxblind_t *to_vknxblind(struct vbus_device *vdev) {
	vdevfront_t *vdevback = dev_get_drvdata(vdev->dev);
	return container_of(vdevback, vknxblind_t, vdevfront);
}

void vknxblind_stop_blind(void);
void vknxblind_up_blind(void);
void vknxblind_down_blind(void);

#endif /* VKNXBLIND_H */
