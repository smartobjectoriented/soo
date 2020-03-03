
/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
 * Copyright (C) 2016-2017 Baptiste Delporte <bonel@bonel.net>
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
#include <linux/proc_fs.h>
#include <linux/notifier.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/blkdev.h>
#include <linux/genhd.h>
#include <linux/fs.h>
#include <linux/fsnotify.h>
#include <linux/file.h>
#include <linux/delay.h>
#include <linux/spinlock.h>

#include <asm/page.h>
#include <asm/pgtable.h>

#include <soo/hypervisor.h>
#include <soo/uapi/avz.h>
#include <soo/vbus.h>
#include <soo/evtchn.h>
#include <soo/vbstore.h>

#include <soo/uapi/soo.h>
#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>
#include <soo/uapi/logbool.h>

#include <soo/debug/dbgvar.h>

#define SYNC_BACKFRONT_COMPLETE		0
#define SYNC_BACKFRONT_SUSPEND		1
#define SYNC_BACKFRONT_RESUME		2
#define SYNC_BACKFRONT_SUSPENDED	3

#define VBUS_TIMEOUT	120

dc_event_fn_t *dc_event_callback[DC_EVENT_MAX];

#ifdef CONFIG_BT_MRVL
extern void sdio_wake_up_sdio_irq_thread(void);
#endif

static rtdm_task_t rt_vbus_task;

#ifdef CONFIG_ARCH_VEXPRESS
extern void propagate_interrupt_from_rt(void);
#endif

/* Event channels used for directcomm channel between agency and agency-RT or ME */
unsigned int dc_evtchn[MAX_DOMAINS];

/* Unique ID assigned to a backend node instance. */
static uint32_t backend_node_signature = 0;

spinlock_t dc_lock;

void register_dc_event_callback(dc_event_t dc_event, dc_event_fn_t *callback) {
	dc_event_callback[dc_event] = callback;
}

static int __vbus_switch_state(struct vbus_device *dev, enum vbus_state state, bool force)
{
	/*
	 * We check whether the state is currently set to the given value, and if not, then the state is set.  We don't want to unconditionally
	 * write the given state, because we don't want to fire watches unnecessarily.  Furthermore, if the node has gone, we don't write
	 * to it, as the device will be tearing down, and we don't want to resurrect that directory.
	 *
	 */

	struct vbus_transaction vbt;

	if (!force && (state == dev->state))
		return 0;

	/* Make visible the new state to the rest of world NOW...
	 * The remaining code is highly asynchronous...
	 */

	/* We make the strong assumption that the state can NOT be changed in parallel. The state machine is well-defined
	 * and simultaneous changes should simply NEVER happen.
	 */

	dev->state = state;

	mb();

	vbus_transaction_start(&vbt);
	vbus_printf(vbt, dev->nodename, "state", "%d", state);
	vbus_transaction_end(vbt);

	return 0;
}

/**
 * vbus_switch_state
 * @dev: vbus device
 * @state: new state
 *
 * Advertise in the store a change of the given driver to the given new_state.
 * Return 0 on success, or -errno on error.
 */
static int vbus_switch_state(struct vbus_device *dev, enum vbus_state state)
{
	DBG("--> changing state of %s from %d to %d\n", dev->nodename, dev->state, state);

	return __vbus_switch_state(dev, state, false);
}

void vbus_set_backend_suspended(struct vbus_device *dev)
{
	char key[VBS_KEY_LENGTH];
	char val[10];

	sprintf(key, "%s/state", dev->nodename);
	sprintf(val, "%d", VbusStateSuspended);

	dev->state = VbusStateSuspended;
	vbs_store_write(key, val);

	mb();

	complete(&dev->sync_backfront);
}

/* If something in array of ids matches this device, return it. */
static const struct vbus_device_id *match_device(const struct vbus_device_id *arr, struct vbus_device *dev) {
	for (; *arr->devicetype != '\0'; arr++) {
		if (!strcmp(arr->devicetype, dev->devicetype))
			return arr;
	}
	return NULL;
}

int vbus_match(struct device *_dev, struct device_driver *_drv) {
	struct vbus_driver *drv = to_vbus_driver(_drv);

	if (!drv->ids)
		return 0;

	return match_device(drv->ids, to_vbus_device(_dev)) != NULL;
}
EXPORT_SYMBOL_GPL(vbus_match);

/*
 * Remove the watch associated to remove device (especially useful for monitoring the state).
 */
void free_otherend_watch(struct vbus_device *dev, bool with_vbus) {

	if (dev->otherend_watch.node) {

		if (with_vbus)
			unregister_vbus_watch(&dev->otherend_watch);
		else
			unregister_vbus_watch_without_vbus(&dev->otherend_watch);

		kfree(dev->otherend_watch.node);

		dev->otherend_watch.node = NULL;
	}

	if (dev->otherend != NULL)
		dev->otherend[0] = 0;
}

/* Callback used by the RT agency to react to events issued from non-RT agency. */
static void event_from_nonrt_callback(struct vbus_watch *watch)
{
	unsigned int val;
	struct vbus_device *vdev = container_of(watch, struct vbus_device, event_from_nonrt);
	struct vbus_driver *drv = to_vbus_driver(vdev->dev.driver);

	rtdm_vbus_gather(VBT_NIL, vdev->nodename, "sync_backfront_rt", "%d", &val, NULL);

	switch (val) {
	case SYNC_BACKFRONT_SUSPEND:

		drv->suspend(vdev);

		/* Now propagate the suspending event to the frontend */

		vbus_switch_state(vdev, VbusStateSuspending);
		break;

	case SYNC_BACKFRONT_RESUME:

		/* The backend is now either in VbusStateSuspended OR VbusStateInitWait */
		drv->resume(vdev);

		vdev->resuming = 1;

		if (vdev->state != VbusStateSuspended)
			/* we are in StateInitWait because newly create vbstore entries after migration */
			vbus_switch_state(vdev, VbusStateReconfiguring);
		else {
			DBG("Changing state of %s to resuming\n", vdev->nodename);
			vbus_switch_state(vdev, VbusStateResuming);
		}
		break;

	}
}

/* Callback used by the non-RT agency to react to events issued from the RT agency. */
static void event_from_rt_callback(struct vbus_watch *watch)
{
	unsigned int val;
	struct vbus_device *vdev = container_of(watch, struct vbus_device, event_from_rt);

	vbus_gather(VBT_NIL, vdev->nodename, "sync_backfront", "%d", &val, NULL);

	switch (val) {
	case SYNC_BACKFRONT_COMPLETE:
		/* We are ready to raise up the completion lock */
		complete(&vdev->sync_backfront);
		break;

	case SYNC_BACKFRONT_SUSPENDED:
		/* Backend suspended */
		vbus_set_backend_suspended(vdev);
		break;
	}

}

/*
 * Specific watch register function to focus on the state of a device on the other side.
 */
void watch_otherend(struct vbus_device *dev) {
	struct vbus_type *bus = container_of(dev->dev.bus, struct vbus_type, bus);

	vbus_watch_pathfmt(dev, &dev->otherend_watch, bus->otherend_changed, "%s/%s", dev->otherend, "state");

	if (dev->realtime) {

		/* The following watch is set on the <sync_backfront> property that may be changed by the non-RT side */
		dev->event_from_nonrt.node = kzalloc(VBS_KEY_LENGTH, GFP_ATOMIC);

		strcpy(dev->event_from_nonrt.node, dev->nodename);
		strcat(dev->event_from_nonrt.node, "/sync_backfront_rt");

		dev->event_from_nonrt.callback = event_from_nonrt_callback;

		register_vbus_watch(&dev->event_from_nonrt);
	}

}

/*
 * Announce ourself to the otherend managed device. We mainly prepare to set up a watch on the device state.
 */
static void talk_to_otherend(struct vbus_device *dev) {
	struct vbus_driver *drv = to_vbus_driver(dev->dev.driver);

	BUG_ON(dev->otherend[0] != 0);
	BUG_ON(dev->otherend_watch.node != NULL);
	
	drv->read_otherend_details(dev);

	/* Set up watch on state of otherend */
	watch_otherend(dev);

}

void vbus_read_otherend_details(struct vbus_device *vdev, char *id_node, char *path_node) {

	if (smp_processor_id() == AGENCY_RT_CPU)
		rtdm_vbus_gather(VBT_NIL, vdev->nodename, id_node, "%i", &vdev->otherend_id, path_node, "%s", vdev->otherend, NULL);
	else
		vbus_gather(VBT_NIL, vdev->nodename, id_node, "%i", &vdev->otherend_id, path_node, "%s", vdev->otherend, NULL);
}


/*
 * The following function is called either in the backend OR the frontend.
 * On the backend side, it may run on CPU #0 (non-RT) or CPU #1 if the backend is configured as realtime.
 */
void vbus_otherend_changed(struct vbus_watch *watch) {
	struct vbus_device *dev = container_of(watch, struct vbus_device, otherend_watch);
	struct vbus_driver *drv = to_vbus_driver(dev->dev.driver);
	struct vbus_transaction vbt;
	char sync_backfront_prop[2];

	enum vbus_state state;

	state = vbus_read_driver_state(dev->otherend);

        DBG("On domID: %d, otherend changed / device: %s  state: %d, CPU %d\n", ME_domID(), dev->nodename, state, smp_processor_id());
	
	/* We do not want to call a callback in a frontend on InitWait. This is
	 * a state issued from the backend to tell the frontend it can be probed.
	 */
	if ((drv->otherend_changed) && (state != VbusStateInitWait))
		drv->otherend_changed(dev, state);

	switch (state) {
	case VbusStateInitialised:
	case VbusStateReconfigured:

		vbus_switch_state(dev, VbusStateConnected);
		break;

		/*
		 * Check for a final action.
		 *
		 * The frontend has changed its state to Closing. Therefore, once the backend performs its cleanings/closing,
		 * we remove the underlying device.
		 */
	case VbusStateClosing:
		DBG("Got closing from frontend: %s\n", dev->nodename);
		remove_device(dev->nodename);
		break;

	case VbusStateSuspended:

		/* If the device is a realtime backend, we inform the non-RT side that we are suspended by
		 * resetting the sync_backfront property in VBstore.
		 */

		if (dev->realtime) {
			vbus_transaction_start(&vbt);

			/* Set the sync_backfront in backend_suspended state */
			sprintf(sync_backfront_prop, "%d", SYNC_BACKFRONT_SUSPENDED);
			vbus_write(vbt, dev->nodename, "sync_backfront", sync_backfront_prop);

			vbus_transaction_end(vbt);

		} else
			vbus_set_backend_suspended(dev);
		break;

	case VbusStateConnected:

		if (dev->resuming) {

			/*
			 * It happens after migration & resume
			 *
			 * If we have migrated, we are already in connected state;
			 * we have been put in this state during the reconfiguring process.
			 *
			 */

			/* Resume Step 3 */
			DBG("%s: now is %s ...\n", __func__, dev->nodename);

			if (dev->state != VbusStateConnected)
				/* Only in the case we are locally resuming, the backend is in state Suspended */
				vbus_switch_state(dev, VbusStateConnected);

			dev->resuming = 0;
		}

		if (dev->realtime) {
			vbus_transaction_start(&vbt);

			sprintf(sync_backfront_prop, "%d", SYNC_BACKFRONT_COMPLETE);

			vbus_write(vbt, dev->nodename, "sync_backfront", sync_backfront_prop);

			vbus_transaction_end(vbt);

		} else
			complete(&dev->sync_backfront);

		break;

	default:
		break;

	}

}
EXPORT_SYMBOL_GPL(vbus_otherend_changed);

/*
 * rtdm Cobalt task dedicated to a RT-backend (each realtime backend owns such a task).
 */
static void vbus_rt_task(void *args) {
	struct device *_dev = (struct device *) args;
	struct vbus_device *dev = to_vbus_device(_dev);
	struct vbus_driver *drv = to_vbus_driver(_dev->driver);
	const struct vbus_device_id *id;
	struct vbus_transaction vbt;
	char sync_backfront_prop[2];
	enum vbus_state otherend_state;

	talk_to_otherend(dev);

	/* Retrieve the id again (avoiding more args passing) */
	id = match_device(drv->ids, dev);
	if (!id)
		BUG();

	drv->probe(dev, id);

	/* Announce that the backend is ready to interact with the frontend. */
	vbus_switch_state(dev, VbusStateInitWait);

	/* Get the state from the otherend to see if we are in a post-migration situation. */
	otherend_state = vbus_read_driver_state(dev->otherend);

	/* If the node is probed after a migration, it means that the frontend will be reconfigured.
	 * In this case, the synchronization will be achieved at the end of the resuming operation.
	 * If the ME is not migrated (injected), the node is created and the synchronization will be
	 * achieved once the device gets connected.
	 */
	if (otherend_state != VbusStateInitialising) {

		/*
		 * We are now ready to inform the non-RT side that the backend is ready
		 * for interactions with the frontend.
		 */
		vbus_transaction_start(&vbt);

		sprintf(sync_backfront_prop, "%d", SYNC_BACKFRONT_COMPLETE);
		vbus_write(vbt, dev->nodename, "sync_backfront", sync_backfront_prop);

		vbus_transaction_end(vbt);
	}
}

/*
 * Task prologue to initiate a new Cobalt task 
 */
static int __rt_task_prologue(void *args) {

	rtdm_task_init(&rt_vbus_task, "vbus_rt_task", vbus_rt_task, args, 50, 0);

	/* We can leave this thread die. Our system is living anyway... */
	do_exit(0);
}

/*
 * vbus_dev_probe() is called by the Linux device subsystem when probing a device
 */
int vbus_dev_probe(struct device *_dev)
{
	struct vbus_device *dev = to_vbus_device(_dev);
	struct vbus_driver *drv = to_vbus_driver(_dev->driver);
	const struct vbus_device_id *id;
	enum vbus_state otherend_state;

	DBG("%s\n", dev->nodename);

	if (!drv->probe)
		BUG();

	id = match_device(drv->ids, dev);
	if (!id)
		BUG();

	init_completion(&dev->down);
	init_completion(&dev->sync_backfront);

	/* If the device is a realtime backend, we spawn a thread to initiate Cobalt (realtime) activities */
#warning need to review the init/reconfiguring sequence in the RT be/fe; to be aligned with the non-RT...
	if (dev->realtime) {

		/* Set a watch callback on the special properties like sync_backfront. */

		dev->event_from_rt.node = kzalloc(VBS_KEY_LENGTH, GFP_ATOMIC);
		strcpy(dev->event_from_rt.node, dev->nodename);
		strcat(dev->event_from_rt.node, "/sync_backfront");

		dev->event_from_rt.callback = event_from_rt_callback;

		register_vbus_watch(&dev->event_from_rt);

		kernel_thread(__rt_task_prologue, _dev, 0);

		/* We wait until the RT thread finished to probe the realtime device
		 * and the device gets connected (or we proceed with a resuming operation).
		 */

		wait_for_completion(&dev->sync_backfront);

		return 0;
	}

	DBG("CPU %d  talk_to_otherend: %s\n", ME_domID(), dev->nodename);

	talk_to_otherend(dev);

	/* On frontend side, the probe will be executed as soon as the backend reaches the state InitWait */

	drv->probe(dev, id);

	/* Get the state from the otherend to see if we are in a post-migration situation. */
	otherend_state = vbus_read_driver_state(dev->otherend);

	/* If the node is probed after a migration, it means that the frontend will be reconfigured.
	 * In this case, the resuming operation can go ahead.
	 * If the ME is not migrated (injected), the node is created and the synchronization will be
	 * achieved once the device gets connected.
	 */
	if (otherend_state == VbusStateInitialising) {

		/* This state will allow to wait until the frontend has performed the probe of its device, therefore
		 * until we get the state Initialised.
		 */
		vbus_switch_state(dev, VbusStateInitWait);

		/* For both RT and non-RT initialization, we wait that the device gets connected */
		wait_for_completion(&dev->sync_backfront);
	}

	return 0;
}


int vbus_dev_remove(struct device *_dev)
{
	struct vbus_device *dev = to_vbus_device(_dev);
	unsigned int dir_exists;

	DBG("%s", dev->nodename);

	/*
	 * If the ME is running on a Smart Object which does not offer all the backends matching the ME's frontends,
	 * some frontend related entries may not have been created. We must check here if the entry matching the dev
	 * to remove exists.
	 */
	dir_exists = vbus_directory_exists(VBT_NIL, dev->otherend_watch.node, "");
	if (dir_exists)
		/* Remove the watch on the remote device. */
		free_otherend_watch(dev, true);

	if (dev->realtime) {
		kfree(dev->event_from_rt.node);
		kfree(dev->event_from_nonrt.node);
	}

	vbus_switch_state(dev, VbusStateClosed);

	return 0;
}

/*
 * Shutdown a device. The function is called from the Linux subsystem
 */
void vbus_dev_shutdown(struct device *_dev)
{
	struct vbus_device *dev = to_vbus_device(_dev);
	struct vbus_driver *drv = to_vbus_driver(_dev->driver);
	unsigned int dir_exists;

	DBG("%s", dev->nodename);

	get_device(&dev->dev);

	dir_exists = vbus_directory_exists(VBT_NIL, dev->otherend_watch.node, "");
	if (dir_exists) {
		/*
		 * If the ME is running on a Smart Object which does not offer all the backends matching the ME's frontends,
		 * some frontend related entries may not have been created. We must check here if the entry matching the dev
		 * to shutdown exists.
		 */

		if (drv->shutdown != NULL)
			drv->shutdown(dev);

		vbus_switch_state(dev, VbusStateClosing);

		wait_for_completion(&dev->down);
	}

	put_device(&dev->dev);

}

void vbus_register_driver_common(struct vbus_driver *drv, struct vbus_type *bus, struct module *owner, const char *mod_name)
{
	int ret;

	DBG("Registering driver name: %s\n", drv->name);

	drv->driver.name = drv->name;
	drv->driver.bus = &bus->bus;
	drv->driver.owner = owner;
	drv->driver.mod_name = mod_name;

	ret = driver_register(&drv->driver);
	if (ret)
		BUG();
}

void vbus_unregister_driver(struct vbus_driver *drv)
{
	driver_unregister(&drv->driver);
}

struct vb_find_info {
	struct vbus_device *dev;
	const char *nodename;
};

static int cmp_dev(struct device *dev, void *data)
{
	struct vbus_device *vdev = to_vbus_device(dev);
	struct vb_find_info *info = data;

	if (!strcmp(vdev->nodename, info->nodename)) {
		info->dev = vdev;
		get_device(dev);
		return 1;
	}
	return 0;
}

struct vbus_device *vbus_device_find(const char *nodename, struct bus_type *bus)
{
	struct vb_find_info info = { .dev = NULL, .nodename = nodename };

	bus_for_each_dev(bus, NULL, &info, cmp_dev);
	return info.dev;
}

static int cleanup_dev(struct device *dev, void *data)
{
	struct vbus_device *vdev = to_vbus_device(dev);
	struct vb_find_info *info = data;

	if (!strcmp(vdev->nodename, info->nodename)) {
		info->dev = vdev;
		get_device(dev);

		return 1; /* found */
	}

	return 0;
}

/*
 * Look for a specific device to be cleaned up.
 */
void vbus_cleanup_device(const char *path, struct bus_type *bus)
{
	struct vb_find_info info = { .nodename = path };

	info.dev = NULL;

	bus_for_each_dev(bus, NULL, &info, cleanup_dev);

	if (info.dev) {
		device_unregister(&info.dev->dev);
		put_device(&info.dev->dev);
	}

}

static void vbus_dev_release(struct device *dev) {
	if (dev)
		kfree(to_vbus_device(dev));
}

static ssize_t nodename_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", to_vbus_device(dev)->nodename);
}
static DEVICE_ATTR_RO(nodename);

static ssize_t devtype_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", to_vbus_device(dev)->devicetype);
}
static DEVICE_ATTR_RO(devtype);

static ssize_t modalias_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s:%s\n", dev->bus->name, to_vbus_device(dev)->devicetype);
}
static DEVICE_ATTR_RO(modalias);

static ssize_t devstate_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", to_vbus_device(dev)->state);
}
static DEVICE_ATTR_RO(devstate);

static struct attribute *vbus_dev_attrs[] = {
		&dev_attr_nodename.attr,
		&dev_attr_devtype.attr,
		&dev_attr_modalias.attr,
		&dev_attr_devstate.attr,
		NULL,
};

static const struct attribute_group vbus_dev_group = {
		.attrs = vbus_dev_attrs,
};

const struct attribute_group *vbus_dev_groups[] = {
		&vbus_dev_group,
		NULL,
};

/*
 * Create a new node and initialize basic structure (vdev).
 * This function is NOT re-entrant and must be executed in the specific context during injection or post-migration sequence.
 */
static int vbus_probe_node(struct vbus_type *bus, const char *type, const char *nodename)
{
	char devname[VBS_KEY_LENGTH];
	int err;
	struct vbus_device *vdev;
	enum vbus_state state;
	bool realtime;

	state = vbus_read_driver_state(nodename);
	realtime = vbus_read_driver_realtime(nodename);

	/* If the backend driver entry exists, but no frontend is using it, there is no
	 * vbstore entry related to the state and we simply skip it.
	 */
	if (state == VbusStateUnknown)
		return 0;

	BUG_ON(state != VbusStateInitialising);

	vdev = kzalloc(sizeof(struct vbus_device), GFP_KERNEL);
	if (!vdev)
		BUG();

	memset(vdev, 0, sizeof(struct vbus_device));

	vdev->state = VbusStateInitialising;

	/* Put a unique signature to this instance */
	vdev->id = backend_node_signature++;

	vdev->resuming = 0;
	vdev->realtime = realtime;

	/* Copy the strings into the extra space. */

	strcpy(vdev->nodename, nodename);
	strcpy(vdev->devicetype, type);

	vdev->dev.bus = &bus->bus;
	vdev->dev.release = vbus_dev_release;

	bus->get_bus_id(devname, vdev->nodename);

	dev_set_name(&vdev->dev, "%s", devname);

	/*
	 * Register with generic device framework.
	 * The link with the driver is also done at this moment.
	 */
	err = device_register(&vdev->dev);
	if (err)
		BUG();

	return 0;
}


/******************/

/* Suspend devices which eventually match with a certain root name (specific ME) */
static int suspend_dev(struct device *dev, void *data)
{
	struct vbus_driver *vbdrv;
	struct vbus_device *vdev;
	unsigned int domID = *((unsigned int *) data);
	unsigned int curDomID;
	char *ptr_item;
	char item[80];
	struct vbus_transaction vbt;
	char sync_backfront_prop[2];

	if (dev->driver == NULL) {
		/* Skip if driver is NULL, i.e. probe failed */
		return 0;
	}

	vbdrv = to_vbus_driver(dev->driver);
	vdev = to_vbus_device(dev);

	if (data != NULL) {

		/* backend/ */
		ptr_item = strchr(vdev->nodename, '/');
		/* <type>/ */
		ptr_item = strchr(ptr_item+1, '/');
		ptr_item++;

		sscanf(ptr_item, "%d/%s", &curDomID, item);

		if (curDomID != domID)
			return 0;
	}

	DBG("Suspending %s (vbdrv=%p, vdev=%p)\n", vdev->nodename, vbdrv, vdev);

	/* If the device is a realtime backend, then we changed the @sync_backfront property
	 * to inform the RT agency to suspend the device. When the RT side will reset the property,
	 * the watch callback will trigger a call to complete on @vdev->sync_backfront.
	 */

	if (vdev->realtime) {

		vbus_transaction_start(&vbt);

		sprintf(sync_backfront_prop, "%d", SYNC_BACKFRONT_SUSPEND);
		vbus_write(vbt, vdev->nodename, "sync_backfront_rt", sync_backfront_prop);

		vbus_transaction_end(vbt);

	} else {

		vbdrv->suspend(vdev);

		/* Now propagate the suspending event to the frontend */

		vbus_switch_state(vdev, VbusStateSuspending);

	}

	wait_for_completion(&vdev->sync_backfront);

	return 0;
}

int vbus_suspend_dev(struct bus_type *bus, unsigned int domID)
{

	if (domID == -1)
		bus_for_each_dev(bus, NULL, NULL, suspend_dev);
	else
		bus_for_each_dev(bus, NULL, &domID, suspend_dev);

	return 0;
}

static int resume_dev(struct device *dev, void *data)
{
	struct vbus_driver *vbdrv;
	struct vbus_device *vdev;
	unsigned int domID = *((unsigned int *) data);
	unsigned int curDomID;
	char *ptr_item;
	char item[80];
	struct vbus_transaction vbt;
	char sync_backfront_prop[2];

	vbdrv = to_vbus_driver(dev->driver);
	vdev = to_vbus_device(dev);

	if (dev->driver == NULL) {
		/* Skip if driver is NULL, i.e. probe failed */
		return 0;
	}

	if (data != NULL) {

		/* backend/ */
		ptr_item = strchr(vdev->nodename, '/');
		/* <type>/ */
		ptr_item = strchr(ptr_item+1, '/');
		ptr_item++;

		sscanf(ptr_item, "%d/%s", &curDomID, item);

		if (curDomID != domID)
			return 0;
	}

	DBG("Resuming %s (vbdrv=%p, vdev=%p)\n", vdev->nodename, vbdrv, vdev);

	/* Before resuming, we need to make sure the backend reached a stable state. */
	/* Two possible scenarios: either the backend has been suspended, so it is in VbusStateSuspended, or
	 * the backend has just been created and we make sure it reached the VbusStateInitWait state.
	 */

	if (vdev->realtime) {

		/* Write the sync_backfront property to "resuming" mode. */
		vbus_transaction_start(&vbt);

		sprintf(sync_backfront_prop, "%d", SYNC_BACKFRONT_RESUME);
		vbus_write(vbt, vdev->nodename, "sync_backfront_rt", sync_backfront_prop);

		vbus_transaction_end(vbt);

	} else {

		/* The backend is now either in VbusStateSuspended OR VbusStateInitWait */
		vbdrv->resume(vdev);

		vdev->resuming = 1;

		if (vdev->state != VbusStateSuspended)
			/* we are in StateInitWait because newly create vbstore entries after migration */
			vbus_switch_state(vdev, VbusStateReconfiguring);
		else {
			DBG("Changing state of %s to resuming\n", vdev->nodename);
			vbus_switch_state(vdev, VbusStateResuming);
		}
	}

	/*
	 * The resuming operation is synchronized on the connected state.
	 * It avoids to have next suspending operation too early, before the device has time to get connected.
	 */
	wait_for_completion(&vdev->sync_backfront);

	return 0;
}

int vbus_resume_dev(struct bus_type *bus, unsigned int domID)
{

	if (domID == -1)
		bus_for_each_dev(bus, NULL, NULL, resume_dev);
	else
		bus_for_each_dev(bus, NULL, &domID, resume_dev);

	return 0;
}

/******************/


void vbus_dev_changed(const char *node, char *type, struct vbus_type *bus) {
	struct vbus_device *dev;

	/*
	 * Either the device does not exist (backend or frontend) and the dev must be allocated, initialized
	 * and probed via the dev subsystem of Linux, OR the device exists (after migration)
	 * and in this case, the device exists on the frontend side only, and we only have to "talk_to_otherend" to
	 * set up the watch on its state (and retrieve the otherend id and name).
	 */

	dev = vbus_device_find(node, &bus->bus);
	if (!dev)
		vbus_probe_node(bus, type, node);
	else {
#warning temporary fix until vbstore entries are removed when a ME is dead....

		//BUG_ON(ME_domID() == DOMID_AGENCY);
		if (ME_domID() == DOMID_AGENCY)
			return ;

		/* Update the state in vbstore. */
		/* We force the update, this will not trigger a watch since the watch is set right afterwards */
		 __vbus_switch_state(dev, dev->state, true);

		/* Setting the watch on the state */
		talk_to_otherend(dev);
	}

}

/*
 * Perform a bottom half (deferred) processing on the receival of dc_event.
 * Here, we avoid to use a worqueue. Prefer thread instead, it will be also easier to manage with SO3/virtshare.
 */
static irqreturn_t directcomm_isr_thread(int irq, void *args) {
	dc_event_t dc_event;

	dc_event = atomic_read((const atomic_t *) &avz_shared_info->dc_event);

	/* Reset the dc_event now so that the domain can send another dc_event */
	atomic_set((atomic_t *) &avz_shared_info->dc_event, DC_NO_EVENT);

	/* Perform the associated callback function to this particular dc_event */
	if (dc_event_callback[dc_event] != NULL)
		(*dc_event_callback[dc_event])(dc_event);

	return IRQ_HANDLED;
}

/*
 * Interrupt routine for direct communication event channel
 * IRQs are off
 */
static irqreturn_t directcomm_isr(int irq, void *args) {
	dc_event_t dc_event;
	unsigned int domID = *((unsigned int *) args);

	dc_event = atomic_read((const atomic_t *) &avz_shared_info->dc_event);

	DBG("Received directcomm interrupt for event: %d\n", avz_shared_info->dc_event);

	switch (dc_event) {

	/*
	 * Some of these events are processed in the agency *only* when the remote domain will complete the dc_event (dc_stable).
	 * If no dc_event is in progress, this will fail in perform_task().
	 */

	case DC_PRE_SUSPEND:
	case DC_SUSPEND:
	case DC_RESUME:
	case DC_FORCE_TERMINATE:
	case DC_POST_ACTIVATE:
	case DC_LOCALINFO_UPDATE:
	case DC_TRIGGER_DEV_PROBE:

	/* At SOOlink core API level, the requester can send a command for send/recv from the non-RT domain */
	case DC_SL_WLAN_SEND:
	case DC_SL_WLAN_RECV:
	case DC_SL_ETH_SEND:
	case DC_SL_ETH_RECV:
	case DC_SL_TCP_SEND:
	case DC_SL_TCP_RECV:
	case DC_SL_BT_SEND:
	case DC_SL_BT_RECV:
	case DC_SL_LO_SEND:
	case DC_SL_LO_RECV:

	case DC_PLUGIN_WLAN_SEND:
	case DC_PLUGIN_ETHERNET_SEND:
	case DC_PLUGIN_TCP_SEND:
	case DC_PLUGIN_BLUETOOTH_SEND:
	case DC_PLUGIN_LOOPBACK_SEND:

	/* The following events are present as reply to a sync-dom operation (tell_dc_stable).
	 * It will not invoke perform_task.
	 */
	case DC_PLUGIN_BLUETOOTH_RECV:
	case DC_PLUGIN_TCP_RECV:
	case DC_PLUGIN_ETHERNET_RECV:
	case DC_PLUGIN_WLAN_RECV:
	case DC_PLUGIN_LOOPBACK_RECV:

		/* Check if it is the response to a dc_event. Can be done immediately in the top half. */
		if (atomic_read(&dc_outgoing_domID[dc_event]) != -1) {

			dc_stable(dc_event);
			break; /* Out of the switch */
		}

		/* We should not receive twice a same dc_event, before it has been fully processed. */
		BUG_ON(atomic_read(&dc_incoming_domID[dc_event]) != -1);
		atomic_set(&dc_incoming_domID[dc_event], domID);

		/* Start the deferred thread */
		return IRQ_WAKE_THREAD;

	default:
		lprintk("(Agency) %s: something weird happened, directcomm interrupt was triggered with dc_event %d, but no DC event was configured !\n", __func__, dc_event);
		break;
	}

	/* Reset the dc_event now so that the domain can send another dc_event */
	atomic_set((atomic_t *) &avz_shared_info->dc_event, DC_NO_EVENT);

	return IRQ_HANDLED;
}

static int __init vbus_init(void)
{
	int res = 0;
	int i;
	int evtchn;
	struct evtchn_alloc_unbound alloc_unbound;
	unsigned int *p_domID;
	char buf[20];
	struct vbus_transaction vbt;

	res = -ENODEV;

	spin_lock_init(&dc_lock);

	for (i = 0; i < DC_EVENT_MAX; i++)
		dc_event_callback[i] = NULL;

	/* Now setting up the VBstore */
	vbstore_init();

	/*
	 * Set up the directcomm communication channel that
	 * is used between the different domains, mainly between the agency and MEs,
	 * or the non-realtime and realtime agency.
	 */

	for (i = 1; i < MAX_DOMAINS; i++) {
		/* Get a free event channel */
		alloc_unbound.dom = DOMID_SELF;
		alloc_unbound.remote_dom = i;

		res = hypercall_trampoline(__HYPERVISOR_event_channel_op, EVTCHNOP_alloc_unbound, (long) &alloc_unbound, 0, 0);
		if (res < 0) {
			printk(KERN_ERR "Error: allocating event channel failed");
			BUG();
		}
		BUG_ON(res);

		dc_evtchn[i] = alloc_unbound.evtchn;

		/* Keep a valid reference to the domID */
		p_domID = kmalloc(sizeof(int), GFP_KERNEL);

		*p_domID = i;

		/* Binding this event channel to an interrupt handler makes the evtchn state not "unbound" anymore */
		evtchn = bind_evtchn_to_virq_handler(dc_evtchn[i], directcomm_isr, directcomm_isr_thread, 0, "directcomm_isr", p_domID);

		if (evtchn <= 0) {
			printk(KERN_ERR "Error: bind_evtchn_to_irqhandler failed");
			BUG();
		}

		/* Finally, we put the directcomm event channel in vbstore (intended to ME usage) */
		/* Save it in vbstore */
		DBG("%s: writing initial vbstore entries for directcomm activities for ME %d ...\n", __func__, i);

		vbus_transaction_start(&vbt);

		sprintf(buf, "soo/directcomm/%d", i);
		vbus_printf(vbt, buf, "event-channel", "%d", (unsigned int) dc_evtchn[i]);

		vbus_transaction_end(vbt);

		DBG("%s: direct communication set up between Agency and ME %d with event channel: %d irq: %d\n", __func__, i, dc_evtchn[i], evtchn);
	}

	DBG("vbus_init OK!\n");

	return 0;
}

arch_initcall(vbus_init);


/*
 * DOMCALL_sync_directcomm
 */
int do_sync_directcomm(void *arg)
{
	struct DOMCALL_directcomm_args *args = arg;

	unsigned int domID = args->directcomm_evtchn;

	args->directcomm_evtchn = dc_evtchn[domID];

	return 0;
}
