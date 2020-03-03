/*
 * Copyright (C) 2016 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2018 David Truan <david.truan@heig-vd.ch>
 * Copyright (C) 2018,2019 Baptiste Delporte <bonel@bonel.net>
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

#include <linux/irqreturn.h>
#include <soo/evtchn.h>
#include <soo/uapi/avz.h>
#include <soo/vbus.h>

#include <soo/dev/vdoga12v6nm.h>

typedef struct {

	vdoga12v6nm_cmd_back_ring_t ring;
	unsigned int	irq;

} vdoga12v6nm_cmd_ring_t;

typedef struct {
	unsigned int	irq;
} vdoga12v6nm_notification_t;

typedef struct {

	struct vbus_device *vdev[MAX_DOMAINS];

	vdoga12v6nm_cmd_ring_t		cmd_rings[MAX_DOMAINS];

	vdoga12v6nm_notification_t	up_notifications[MAX_DOMAINS];
	vdoga12v6nm_notification_t	down_notifications[MAX_DOMAINS];

} vdoga12v6nm_t;

extern vdoga12v6nm_t vdoga12v6nm;

/* ISRs associated to the ring and notifications */
irqreturn_t vdoga12v6nm_cmd_interrupt(int irq, void *dev_id);
irqreturn_t vdoga12v6nm_up_interrupt(int irq, void *dev_id);
irqreturn_t vdoga12v6nm_down_interrupt(int irq, void *dev_id);

/* State management */
void vdoga12v6nm_probe(struct vbus_device *dev);
void vdoga12v6nm_close(struct vbus_device *dev);
void vdoga12v6nm_suspend(struct vbus_device *dev);
void vdoga12v6nm_resume(struct vbus_device *dev);
void vdoga12v6nm_connected(struct vbus_device *dev);
void vdoga12v6nm_reconfigured(struct vbus_device *dev);
void vdoga12v6nm_shutdown(struct vbus_device *dev);

void vdoga12v6nm_vbus_init(void);

bool vdoga12v6nm_start(domid_t domid);
void vdoga12v6nm_end(domid_t domid);
bool vdoga12v6nm_is_connected(domid_t domid);

