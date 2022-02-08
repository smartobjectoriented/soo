/*
 * Copyright (C) 2016-2019 Daniel Rossier <daniel.rossier@soo.tech>
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

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/slab.h>

#include <linux/device.h>
#include <linux/sysfs.h>

#include <asm/page.h>
#include <asm/pgtable.h>
#include <soo/hypervisor.h>
#include <soo/vbstore.h>
#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>
#include <soo/guest_api.h>
#include <soo/uapi/soo.h>

#include <soo/uapi/logbool.h>

static struct vbus_type vbus_backend;

struct completion backend_initialized;

/* backend/<type>/<fe-uuid>/<id> => <type>-<fe-domid>-<id> */
static void backend_bus_id(char bus_id[VBS_KEY_LENGTH], const char *nodename)
{
	int domid;
	const char *devid, *type, frontend[VBS_KEY_LENGTH];
	unsigned int typelen;

	type = strchr(nodename, '/');
	if (!type)
		BUG();

	type++;
	typelen = strcspn(type, "/");
	if (!typelen || type[typelen] != '/')
		BUG();

	devid = strrchr(nodename, '/') + 1;

	vbus_gather(VBT_NIL, nodename, "frontend-id", "%i", &domid, "frontend", "%s", frontend, NULL);

	if (strlen(frontend) == 0)
		BUG();

	if (snprintf(bus_id, VBS_KEY_LENGTH, "%.*s-%i-%s", typelen, type, domid, devid) >= VBS_KEY_LENGTH)
		BUG();
}

static int vbus_uevent_backend(struct device *dev, struct kobj_uevent_env *env)
{
	struct vbus_device *xdev;
	struct vbus_driver *drv;
	struct vbus_type *bus;


	if (dev == NULL)
		return -ENODEV;

	xdev = to_vbus_device(dev);
	bus = container_of(xdev->dev.bus, struct vbus_type, bus);

	if (add_uevent_var(env, "MODALIAS=soo-backend:%s", xdev->devicetype))
		return -ENOMEM;

	/* stuff we want to pass to /sbin/hotplug */
	if (add_uevent_var(env, "VBus_TYPE=%s", xdev->devicetype))
		return -ENOMEM;

	if (add_uevent_var(env, "VBus_PATH=%s", xdev->nodename))
		return -ENOMEM;

	if (add_uevent_var(env, "VBus_BASE_PATH=%s", bus->root))
		return -ENOMEM;

	if (dev->driver) {
		drv = to_vbus_driver(dev->driver);
		if (drv && drv->uevent)
			return drv->uevent(xdev, env);
	}

	return 0;
}

/* backend/<typename>/<frontend-domID> */
int vbus_probe_backend(int domid)
{
	char noderoot[VBS_KEY_LENGTH];
	char target[VBS_KEY_LENGTH];
	int err = 0;
	char **dir;
	char *type, *pos;
	unsigned int i, dir_n = 0;

	DBG("Probing all registered devices bound to domain %d...\n", domid);

	strcpy(noderoot, "backend");

	dir = vbus_directory(VBT_NIL, noderoot, "", &dir_n);
	if (IS_ERR(dir))
		BUG();

	for (i = 0; i < dir_n; i++) {
		DBG("%s %s\n", __func__, dir[i]);

		strcpy(noderoot, "backend/");
		strcat(noderoot, dir[i]);
		strcat(noderoot, "/%d/0");   /* Currently, only interface 0 is considered... */

		sprintf(target, noderoot, domid);

		strcpy(noderoot, target);

		pos = target;
		strsep(&pos, "/");    	/* "backend/" */
		type = strsep(&pos, "/");     /* "<type> */

		vbus_dev_changed(noderoot, type, &vbus_backend);

	}
	if (dir)
		kfree(dir);
	

	return err;
}

static void frontend_changed(struct vbus_watch *watch)
{
	vbus_otherend_changed(watch);
}

/* device attributes */

extern const struct attribute_group *vbus_dev_groups[];

static struct attribute *vbus_backend_dev_attrs[5];

static struct attribute_group vbus_backend_dev_group = {
	.attrs = vbus_backend_dev_attrs,
};

const struct attribute_group *vbus_backend_dev_groups[] = {
	&vbus_backend_dev_group,
	NULL,
};

/* bus attributes */

static ssize_t vbstore_show(struct bus_type *bus, char *buf)
{
	vbs_dump();

	return sprintf(buf, "%s:vbstore\n", bus->name);
}
static BUS_ATTR_RO(vbstore);

static ssize_t watches_show(struct bus_type *bus, char *buf)
{
	vbs_dump_watches();

	return sprintf(buf, "%s:watches\n", bus->name);
}
static BUS_ATTR_RO(watches);

static struct attribute *vbus_bus_attrs[] = {
	&bus_attr_vbstore.attr,
	&bus_attr_watches.attr,
	NULL,
};


static const struct attribute_group vbus_bus_group = {
	.attrs = vbus_bus_attrs,
};

const struct attribute_group *vbus_bus_groups[] = {
	&vbus_bus_group,
	NULL,
};

static struct attribute *vbus_backend_bus_attrs[3];

static struct attribute_group vbus_backend_bus_group = {
  .attrs = vbus_backend_bus_attrs,
};

const struct attribute_group *vbus_backend_bus_groups[] = {
	&vbus_backend_bus_group,
	NULL,
};

static struct vbus_type vbus_backend = {
	.root = "soo",
	.get_bus_id = backend_bus_id,
	.otherend_changed = frontend_changed,
	.bus = {
		.name		= "backend",
		.match		= vbus_match,
		.uevent		= vbus_uevent_backend,
		.probe		= vbus_dev_probe,
		.remove		= vbus_dev_remove,
		.dev_groups	= vbus_backend_dev_groups,
		.bus_groups 	= vbus_backend_bus_groups,
	},
};

static void read_frontend_details(struct vbus_device *vdev)
{
	vbus_read_otherend_details(vdev, "frontend-id", "frontend");
}

void remove_device(const char *path) {
	vbus_cleanup_device(path, &vbus_backend.bus);
}

int vbus_dev_is_online(struct vbus_device *dev)
{
	int rc, val;

	rc = vbus_scanf(VBT_NIL, dev->nodename, "online", "%d", &val);
	if (rc != 1)
		val = 0; /* no online node present */

	return val;
}
EXPORT_SYMBOL_GPL(vbus_dev_is_online);

void __vbus_register_backend(struct vbus_driver *drv, struct module *owner, const char *mod_name)
{
	drv->read_otherend_details = read_frontend_details;

	vbus_register_driver_common(drv, &vbus_backend, owner, mod_name);
}


int vbus_probe_backend_init(void)
{

	int err;
	DBG0("Initializing...");

	/* Initialize the sysfs structure for all sysfs attrs */
	vbus_backend_dev_attrs[0] = vbus_dev_groups[0]->attrs[0];
	vbus_backend_dev_attrs[1] = vbus_dev_groups[0]->attrs[1];
	vbus_backend_dev_attrs[2] = vbus_dev_groups[0]->attrs[2];
	vbus_backend_dev_attrs[3] = vbus_dev_groups[0]->attrs[3];
	vbus_backend_dev_attrs[4] = NULL;

	vbus_backend_bus_attrs[0] = vbus_bus_groups[0]->attrs[0];
	vbus_backend_bus_attrs[1] = vbus_bus_groups[0]->attrs[1];
	vbus_backend_bus_attrs[2]= NULL;

	init_completion(&backend_initialized);

	/* Register ourselves with the kernel bus subsystem */
	err = bus_register(&vbus_backend.bus);
	if (err) {
		printk("%s failed with err = %d\n",__FUNCTION__, err);
		return err;
	}

	return 0;
}

/*
 * vbus_suspend_devices
 *
 * Inform all vbus devices that we are preparing to migrate (or some other scenarios requiring
 * temporary suspension of their activities)
 *
 */
int vbus_suspend_devices(unsigned int domID)
{
  return vbus_suspend_dev(&vbus_backend.bus, domID);
}

/*
 * vbus_resume_devices
 *
 * Inform all vbus devices that we are preparing to migrate (or some other scenarios requiring
 * temporary suspension of their activities)
 *
 */
int vbus_resume_devices(unsigned int domID)
{
  return vbus_resume_dev(&vbus_backend.bus, domID);
}


/* This function is called from vs_prepare_to_resume() in vbus/vbus_xs.c
 * after the FSM thread activities in order to setup post-migration vbus and frontend drivers.
 */
void postmig_setup(void) {

  /* Nothing at the moment */

}
