/*
 * Copyright (C) 2020 Nikolaos Garanis <nikolaos.garanis@heig-vd.ch>
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

#ifndef VFB_H
#define VFB_H

#include <device/irq.h>

#include <soo/ring.h>
#include <soo/grant_table.h>

#define VFB_PACKET_SIZE	32

#define VFB_NAME		"vfb"
#define VFB_PREFIX		"[" VFB_NAME "] "

typedef struct {
	char buffer[VFB_PACKET_SIZE];
} vfb_request_t;

typedef struct  {
	char buffer[VFB_PACKET_SIZE];
} vfb_response_t;

/*
 * Generate blkif ring structures and types.
 */
DEFINE_RING_TYPES(vfb, vfb_request_t, vfb_response_t);

typedef struct {

	struct vbus_device  *dev;

	vfb_front_ring_t ring;
	grant_ref_t ring_ref;
	grant_handle_t handle;
	uint32_t evtchn;
	uint32_t irq;

} vfb_t;

extern vfb_t vfb;

/* ISR associated to the ring */
irq_return_t vfb_interrupt(int irq, void *data);

/* State management */
void vfb_probe(void);
void vfb_closed(void);
void vfb_suspend(void);
void vfb_resume(void);
void vfb_connected(void);
void vfb_reconfiguring(void);
void vfb_shutdown(void);

void vfb_vbus_init(void);

/* Processing and connected state management */
void vfb_start(void);
void vfb_end(void);
bool vfb_is_connected(void);

/**/
void write_vbstore(void);

#endif /* VFB_H */
