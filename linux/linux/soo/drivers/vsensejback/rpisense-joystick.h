/*
 * Copyright (C) 2021 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef RPISENSE_JOYSTICK_H
#define RPISENSE_JOYSTICK_H

#include <linux/platform_device.h>

#include <soo/vbus.h>

typedef void(*joystick_handler_t)(struct vbus_device *vdev, int key);

#define RPISENSE_JS_UP      	0x04
#define RPISENSE_JS_DOWN    	0x01
#define RPISENSE_JS_RIGHT   	0x02
#define RPISENSE_JS_LEFT    	0x10
#define RPISENSE_JS_CENTER	0x08

#define RPISENSE_JS_ADDR	0xF2

void sensej_init(void);
void rpisense_joystick_handler_register(struct vbus_device *vdev, joystick_handler_t joystick_handler);
void rpisense_joystick_handler_unregister(struct vbus_device *vdev);

#endif /* RPISENSE_JOYSTICK_H */
