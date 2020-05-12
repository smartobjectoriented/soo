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

#ifndef VDUMMY_H
#define VDUMMY_H

#include <soo/ring.h>
#include <soo/grant_table.h>

#define VDUMMY_PACKET_SIZE	32

#define VDUMMY_NAME		"vdummy"
#define VDUMMY_PREFIX		"[" VDUMMY_NAME "] "

typedef struct {
	char buffer[VDUMMY_PACKET_SIZE];
} vdummy_request_t;

typedef struct  {
	char buffer[VDUMMY_PACKET_SIZE];
} vdummy_response_t;

/*
 * Generate blkif ring structures and types.
 */
DEFINE_RING_TYPES(vdummy, vdummy_request_t, vdummy_response_t);

typedef struct {

	vdummy_back_ring_t ring;
	unsigned int irq;

} vdummy_ring_t;

/*
 * General structure for this virtual device (backend side)
 */
typedef struct {

	vdummy_ring_t rings[MAX_DOMAINS];
	struct vbus_device *vdev[MAX_DOMAINS];

} vdummy_t;

extern vdummy_t vdummy;

irqreturn_t vdummy_interrupt(int irq, void *dev_id);

void vdummy_probe(struct vbus_device *dev);
void vdummy_close(struct vbus_device *dev);
void vdummy_suspend(struct vbus_device *dev);
void vdummy_resume(struct vbus_device *dev);
void vdummy_connected(struct vbus_device *dev);
void vdummy_reconfigured(struct vbus_device *dev);
void vdummy_shutdown(struct vbus_device *dev);

extern void vdummy_vbus_init(void);

bool vdummy_start(domid_t domid);
void vdummy_end(domid_t domid);
bool vdummy_is_connected(domid_t domid);

#endif /* VDUMMY_H */
