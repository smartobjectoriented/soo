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

#ifndef VENOCEANSW_H
#define VENOCEANSW_H

#include <soo/ring.h>
#include <soo/grant_table.h>
#include <soo/vdevfront.h>

#define VENOCEANSW_PACKET_SIZE	32

#define VENOCEANSW_NAME		"venoceansw"
#define VENOCEANSW_PREFIX		"[" VENOCEANSW_NAME "] "

#define SWITCH_IS_RELEASED 	0x00 
#define SWITCH_IS_DOWN 		0x50
#define SWITCH_IS_UP 		0x70

typedef struct {
	/* empty */
} venoceansw_request_t;

typedef struct  {
	int sw_cmd;
	int sw_id;
} venoceansw_response_t;

/*
 * Generate ring structures and types.
 */
DEFINE_RING_TYPES(venoceansw, venoceansw_request_t, venoceansw_response_t);

/*
 * General structure for this virtual device (backend side)
 */

typedef struct {
	/* Must be the first field */
	vdevfront_t vdevfront;

	venoceansw_front_ring_t ring;
	unsigned int irq;

	grant_ref_t ring_ref;
	grant_handle_t handle;
	uint32_t evtchn;

} venoceansw_t;

static inline venoceansw_t *to_venoceansw(struct vbus_device *vdev) {
	vdevfront_t *vdevback = dev_get_drvdata(vdev->dev);
	return container_of(vdevback, venoceansw_t, vdevfront);
}

void get_sw_data(int *sw_cmd, int *sw_id);

#endif /* VENOCEANSW_H */
