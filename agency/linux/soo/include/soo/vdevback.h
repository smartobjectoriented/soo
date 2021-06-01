/*
 * Copyright (C) 2020 Daniel Rossier <daniel.rossier@soo.tech>
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

#ifndef VDEVBACK_H
#define VDEVBACK_H

#include <linux/mutex.h>
#include <soo/vbus.h>
#include <asm/atomic.h>
#include <linux/list.h>

struct vdrvback;

struct vdevback {

	/* Backend activity related declarations - managed by vdevback generic code */

	atomic_t processing_count;

	/* Protection against shutdown (or other) */
	struct mutex processing_lock;

	/* Synchronization between ongoing processing and suspend/closing */
	struct completion sync;
};
typedef struct vdevback vdevback_t;

struct vdrvback {

	struct vbus_driver vdrv;

	void (*probe)(struct vbus_device *vdev);
	void (*remove)(struct vbus_device *vdev);

	void (*close)(struct vbus_device *vdev);

	void (*connected)(struct vbus_device *vdev);
	void (*reconfigured)(struct vbus_device *vdev);
	void (*resume)(struct vbus_device *vdev);
	void (*suspend)(struct vbus_device *vdev);

};
typedef struct vdrvback vdrvback_t;

static inline vdrvback_t *to_vdrvback(struct vbus_device *vdev) {
	struct vbus_driver *vdrv = to_vbus_driver(vdev->dev.driver);
	return container_of(vdrv, vdrvback_t, vdrv);
}

void vdevback_init(char *name, vdrvback_t *vdrvback);
bool vdevback_processing_begin(struct vbus_device *vdev);
void vdevback_processing_end(struct vbus_device *vdev);

/* Used to generically store a list of connected vbus_device */
struct vdev_entry {
	struct list_head list;
	struct vbus_device *vdev;
};
typedef struct vdev_entry vdev_entry_t;

void vdevback_add_entry(struct vbus_device *vdev, struct list_head *list);
struct vbus_device *vdevback_get_entry(uint32_t domid, struct list_head *_list);
void vdevback_del_entry(struct vbus_device *vdev, struct list_head *_list);

#endif /* VDEVBACK_H */




