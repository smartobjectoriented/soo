/*
 * Copyright (C) 2018 Baptiste Delporte <bonel@bonel.net>
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

#include <soo/dev/vleds.h>

typedef struct {

	vleds_cmd_back_ring_t ring;
	unsigned int	irq;

} vleds_cmd_ring_t;

typedef struct {

	vleds_cmd_ring_t	cmd_rings[MAX_DOMAINS];
	struct vbus_device	*vdev[MAX_DOMAINS];

} vleds_t;

extern vleds_t vleds;

irqreturn_t vleds_cmd_interrupt(int irq, void *dev_id);

/* State management */
void vleds_probe(struct vbus_device *dev);
void vleds_close(struct vbus_device *dev);
void vleds_suspend(struct vbus_device *dev);
void vleds_resume(struct vbus_device *dev);
void vleds_connected(struct vbus_device *dev);
void vleds_reconfigured(struct vbus_device *dev);
void vleds_shutdown(struct vbus_device *dev);

extern void vleds_vbus_init(void);

bool vleds_start(domid_t domid);
void vleds_end(domid_t domid);
bool vleds_is_connected(domid_t domid);
