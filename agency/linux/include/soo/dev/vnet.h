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
#include <soo/dev/vnetbuff.h>
#include <linux/if_ether.h>


#define VNET_PACKET_SIZE	32

#define VNET_NAME		"vnet"
#define VNET_PREFIX		"[" VNET_NAME "] "
#define VNET_INTERFACE		"eth0"

#define INVALID_DOM -1

enum vnet_type{
	NET_STATUS = 0,
};

struct vnet_shared_data {
	unsigned char ethaddr[ETH_ALEN];
	uint32_t network;
	uint32_t mask;
	uint32_t me_ip;
	uint32_t agency_ip;
};

struct ip_conf {
	uint32_t ip;
	uint32_t mask;
	uint32_t gw;
};


typedef struct {
	int broadcast_token;
	int connected;
} vnet_net_status_t;

typedef struct {
	uint16_t type;
	union {
		struct vbuff_data buff;
		vnet_net_status_t network;
	};
	char buffer[2];
} vnet_request_t;

typedef struct  {
	uint16_t type;
	union {
		struct vbuff_data buff;
		vnet_net_status_t network;
	};
	char buffer[2];
} vnet_response_t;

/*
 * Generate ring structures and types.
 */
DEFINE_RING_TYPES(vnet_data, vnet_request_t, vnet_response_t);
DEFINE_RING_TYPES(vnet_ctrl, vnet_request_t, vnet_response_t);

/*
 * General structure for this virtual device (backend side)
 */

typedef struct {
	vdevback_t vdevback;

	vnet_data_back_ring_t ring_data;
	vnet_ctrl_back_ring_t ring_ctrl;

	/* pointer to arrays of size PAGE_COUNT
	 * Those arrays are shared between the frontend and the backend
	 * They track the status of the packet buffers */
	struct vbuff_buff vbuff_tx;
	struct vbuff_buff vbuff_rx;

	struct vnet_shared_data *shared_data;

	unsigned long grant_buff;

	struct vnetif *vif;

	unsigned int irq;

	int has_connected_token;

	void (*send)(void*, u8*, int);

} vnet_t;

static inline vnet_t *to_vnet(struct vbus_device *vdev) {
	vdevback_t *vdevback = dev_get_drvdata(&vdev->dev);
	return container_of(vdevback, vnet_t, vdevback);
}

#endif /* VNET_H */
