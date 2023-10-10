/*
 * Copyright (C) 2018-2019 Baptiste Delporte <bonel@bonel.net>
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

#include <soo/dev/vweather.h>

typedef struct {
	char		*data;
	unsigned int	pfn;
} vweather_shared_buffer_t;

typedef struct {
	unsigned int	irq;
} vweather_notification_t;

typedef struct {
	vweather_shared_buffer_t	weather_buffers[MAX_DOMAINS];
	vweather_notification_t		update_notifications[MAX_DOMAINS];
	struct vbus_device 		*vdev[MAX_DOMAINS];

} vweather_t;

extern vweather_t vweather;

extern size_t vweather_packet_size;

/* State management */
void vweather_probe(struct vbus_device *dev);
void vweather_close(struct vbus_device *dev);
void vweather_suspend(struct vbus_device *dev);
void vweather_resume(struct vbus_device *dev);
void vweather_connected(struct vbus_device *dev);
void vweather_reconfigured(struct vbus_device *dev);
void vweather_shutdown(struct vbus_device *dev);

/* Shared buffer setup */
int vweather_setup_shared_buffer(struct vbus_device *dev);

void vweather_vbus_init(void);

bool vweather_start(domid_t domid);
void vweather_end(domid_t domid);
bool vweather_is_connected(domid_t domid);

irqreturn_t vweather_interrupt(int irq, void *dev_id);
