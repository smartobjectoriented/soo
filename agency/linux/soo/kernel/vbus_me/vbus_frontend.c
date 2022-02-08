/*
 * Copyright (C) 2016-2018 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2016-2018 Baptiste Delporte <bonel@bonel.net>
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

#if 0
#define DEBUG
#endif

#include <linux/types.h>
#include <linux/list.h>
#include <linux/string.h>

#include <soo/hypervisor.h>
#include <soo/vbus_me.h>
#include <soo/gnttab.h>
#include <soo/vbstore.h>
#include <soo/vbstore_me.h>

#include <soo/uapi/soo.h>
#include <soo/uapi/debug.h>
#include <soo/uapi/console.h>

/* List of frontend */
struct list_head frontends;

/*
 * Walk through the list of frontend devices and perform an action.
 * When the action returns 1, we stop the walking.
 */
void frontend_for_each(void *data, int (*fn)(struct vbus_me_device *, void *)) {
	struct list_head *pos, *q;
	struct vbus_me_device *vdev;

	list_for_each_safe(pos, q, &frontends)
	{
		vdev = list_entry(pos, struct vbus_me_device, list);

		if (fn(vdev, data) == 1)
			return ;
	}
}

void add_new_dev(struct vbus_me_device *vdev) {
	list_add_tail(&vdev->list, &frontends);
}

/* device/domID/<type>/<id> => <type>-<id> */
static int frontend_bus_id(char bus_id[VBS_KEY_LENGTH], const char *nodename)
{
	/* device/ */
	nodename = strchr(nodename, '/');
	/* domID/ */
	nodename = strchr(nodename+1, '/');
	if (!nodename || strlen(nodename + 1) >= VBS_KEY_LENGTH) {
		printk("vbus: bad frontend %s\n", nodename);
		BUG();
	}

	strncpy(bus_id, nodename + 1, VBS_KEY_LENGTH);
	if (!strchr(bus_id, '/')) {
		printk("vbus: bus_id %s no slash\n", bus_id);
		BUG();
	}
	*strchr(bus_id, '/') = '-';
	return 0;
}

static void backend_changed(struct vbus_watch *watch)
{
	DBG("(CPU %d) Backend changed now: node = %s\n", smp_processor_id(), watch->node);

	vbus_me_otherend_changed(watch);
}

static char root_name[15];
static char initial_rootname[15];

static struct vbus_me_type vbus_frontend = {
	.root = "device",
	.get_bus_id = frontend_bus_id,
	.otherend_changed = backend_changed,
};


static int remove_dev(struct vbus_me_device *vdev, void *data)
{
	if (vdev->vdrv == NULL) {
		/* Skip if driver is NULL, i.e. probe failed */
		return 0;
	}

	/* Remove it from the main list */
	list_del(&vdev->list);

	/* Removal from vbus namespace */
	vbus_me_dev_remove(vdev);

	return 0;
}

/*
 * Remove a device or all devices present on the bus (if path = NULL)
 */
void remove_devices(void)
{
	frontend_for_each(NULL, remove_dev);
}

static int __device_shutdown(struct vbus_me_device *vdev, void *data)
{
	/* Removal from vbus namespace */
	vbus_me_dev_shutdown(vdev);

	return 0;
}

void frontend_device_shutdown(void) {
	frontend_for_each(NULL, __device_shutdown);
}

/*
 * In frontend drivers, otherend_id refering to the agency or *realtime* agency is equal to 0.
 */
static void read_backend_details(struct vbus_me_device *vdev)
{
	vbus_me_read_otherend_details(vdev, "backend-id", "backend");
}

/*
 * The drivers/vbus_fron have to be registered *before* any registered frontend devices.
 */
void vbus_register_frontend(struct vbus_me_driver *vdrv)
{
	DBG("Registering driver %s\n", vdrv->name);

	vdrv->read_otherend_details = read_backend_details;
	DBG("__vbus_register_frontend\n");

	vbus_me_register_driver_common(vdrv);
}

/*
 * Probe a new device on the frontend bus.
 * Typically called by vbstore_dev_init()
 */
int vdev_me_probe(char *node, const char *compat) {
	char *type, *pos;
	char target[VBS_KEY_LENGTH];

	DBG("%s: probing device: %s\n", __func__, node);

	strcpy(target, node);
	pos = target;
	type = strsep(&pos, "/");    /* "device/" */
	type = strsep(&pos, "/");    /* "device/<domid>/" */
	type = strsep(&pos, "/");    /* "/device/<domid>/<type>" */

	vbus_me_dev_changed(node, type, &vbus_frontend, compat);

	return 0;
}

void vbus_probe_frontend_init(void)
{
	lprintk("%s: Initializing...\n", __func__);

	/* Preserve the initial root node name */
	strcpy(initial_rootname, vbus_frontend.root);

	/* Re-adjust the root node name with the dom ID */

	/* OpenCN */
	sprintf(root_name, "%s/%d", vbus_frontend.root, smp_processor_id());

	vbus_frontend.root = root_name;

	INIT_LIST_HEAD(&frontends);
}




