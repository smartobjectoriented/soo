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

#include <device/network.h>
#include <soo/ring.h>
#include <soo/grant_table.h>
#include <soo/vdevfront.h>
#include <soo/dev/vnetbuff.h>


#define VNET_PACKET_SIZE	32

#define VNET_NAME		"vnet"
#define VNET_PREFIX		"[" VNET_NAME "] "

struct vbuff_buff *vbuff_tx;
struct vbuff_buff *vbuff_rx;
unsigned char *vbuff_ethaddr;

grant_ref_t grant_buff = 0;

enum vnet_type{
        SET_IP = 0,
        GET_IP,
        ENABLE,
        DISABLE,
        ETHADDR,
};


struct ip_conf {
        uint32_t ip;
        uint32_t mask;
        uint32_t gw;
};

typedef struct {
        uint16_t type;
        union {
                struct vbuff_data buff;
                struct ip_conf ip;
                grant_handle_t grant;
                unsigned char ethaddr[ARP_HLEN];
        };
	char buffer[2];
} vnet_request_t;

typedef struct  {
        uint16_t type;
        union {
                struct vbuff_data buff;
                struct ip_conf ip;
                grant_handle_t grant;
                unsigned char ethaddr[ARP_HLEN];
        };
        char buffer[2];
} vnet_response_t;

typedef struct {
        uint16_t type;

        char pad[VNET_PACKET_SIZE - sizeof(struct vbuff_data) -2];
} vnet_request_tx_t;

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
	vdevfront_t vdevfront;

	vnet_tx_front_ring_t ring_tx;
	vnet_rx_front_ring_t ring_rx;
	vnet_ctrl_front_ring_t ring_ctrl;
	unsigned int irq;

	grant_ref_t ring_tx_ref;
	grant_ref_t ring_rx_ref;
	grant_ref_t ring_ctrl_ref;
	grant_handle_t handle;
	uint32_t evtchn;

} vnet_t;

static inline vnet_t *to_vnet(struct vbus_device *vdev) {
	vdevfront_t *vdevback = dev_get_drvdata(vdev->dev);
	return container_of(vdevback, vnet_t, vdevfront);
}

#endif /* VNET_H */
