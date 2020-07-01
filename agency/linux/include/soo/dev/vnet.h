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

#ifndef VNET_H
#define VNET_H

#include <soo/ring.h>
#include <soo/grant_table.h>
#include <soo/vdevback.h>

#define VNET_PACKET_SIZE	32

#define VNET_NAME		"vnet"
#define VNET_PREFIX		"[" VNET_NAME "] "

typedef struct {
	char buffer[VNET_PACKET_SIZE];
} vnet_request_t;

typedef struct  {
	char buffer[VNET_PACKET_SIZE];
} vnet_response_t;

/*
 * Generate ring structures and types.
 */
DEFINE_RING_TYPES(vnet_tx, vnet_request_t, vnet_response_t);
DEFINE_RING_TYPES(vnet_rx, vnet_request_t, vnet_response_t);
DEFINE_RING_TYPES(vnet_ctrl, vnet_request_t, vnet_response_t);

/*
 * General structure for this virtual device (backend side)
 */

typedef struct {
	vdevback_t vdevback;

	vnet_tx_back_ring_t ring_tx;
	vnet_rx_back_ring_t ring_rx;
	vnet_ctrl_back_ring_t ring_ctrl;
	unsigned int irq;

} vnet_t;

static inline vnet_t *to_vnet(struct vbus_device *vdev) {
	vdevback_t *vdevback = dev_get_drvdata(&vdev->dev);
	return container_of(vdevback, vnet_t, vdevback);
}

#endif /* VNET_H */
