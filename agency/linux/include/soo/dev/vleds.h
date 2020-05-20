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

#ifndef VLEDS_H
#define VLEDS_H

#include <soo/ring.h>

#include <asm/ioctl.h>

#define VLEDS_NAME	"vleds"
#define VLEDS_PREFIX	"[" VLEDS_NAME "] "

#define VLEDS_MAJOR			126
#define VLEDS_DEV_NAME			"soo/" VLEDS_NAME
#define VLEDS_DEV			"/dev/" VLEDS_DEV_NAME

#define VLEDS_N_LEDS			6

#define VLEDS_IOCTL_SET_BRIGHTNESS	_IOW(0x500001edu, 1, uint32_t)
#define VLEDS_IOCTL_SET_ON		_IOW(0x500001edu, 2, uint32_t)
#define VLEDS_IOCTL_SET_OFF		_IOW(0x500001edu, 3, uint32_t)
#define VLEDS_IOCTL_SET_BLINK		_IOW(0x500001edu, 4, uint32_t)

#define VLEDS_MAX_COMMANDS	1024

typedef struct {
	uint32_t cmd;
	uint32_t arg;
} vleds_cmd_request_t;

/* Not used */
typedef struct {
	uint32_t val;
} vleds_cmd_response_t;

DEFINE_RING_TYPES(vleds_cmd, vleds_cmd_request_t, vleds_cmd_response_t);

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

#endif /* VLEDS_H */
