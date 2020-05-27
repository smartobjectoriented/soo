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

#ifndef VFB_H
#define VFB_H

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

	vfb_back_ring_t ring;
	unsigned int irq;

} vfb_ring_t;

/*
 * General structure for this virtual device (backend side)
 */
typedef struct {

	vfb_ring_t rings[MAX_DOMAINS];
	struct vbus_device *vdev[MAX_DOMAINS];

#warning only for one ME...
	struct vbus_watch watch;
} vfb_t;

extern vfb_t vfb;

irqreturn_t vfb_interrupt(int irq, void *dev_id);

void vfb_probe(struct vbus_device *dev);
void vfb_close(struct vbus_device *dev);
void vfb_suspend(struct vbus_device *dev);
void vfb_resume(struct vbus_device *dev);
void vfb_connected(struct vbus_device *dev);
void vfb_reconfigured(struct vbus_device *dev);
void vfb_shutdown(struct vbus_device *dev);

extern void vfb_vbus_init(void);

bool vfb_start(domid_t domid);
void vfb_end(domid_t domid);
bool vfb_is_connected(domid_t domid);

#endif /* VFB_H */
