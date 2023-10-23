
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

#ifdef CONFIG_ARCH_VEXPRESS
extern void propagate_interrupt_from_rt(void);
#endif

/* Event channels used for directcomm channel between agency and agency-RT or ME */
unsigned int dc_evtchn[MAX_DOMAINS];

spinlock_t dc_lock;

void register_dc_event_callback(dc_event_t dc_event, dc_event_fn_t *callback) {
	dc_event_callback[dc_event] = callback;
}

static int __vbus_switch_state(struct vbus_device *vdev, enum vbus_state state, bool force)
{
	/*
	 * We check whether the state is currently set to the given value, and if not, then the state is set.  We don't want to unconditionally
	 * write the given state, because we don't want to fire watches unnecessarily.  Furthermore, if the node has gone, we don't write
	 * to it, as the device will be tearing down, and we don't want to resurrect that directory.
	 *
	 */

	struct vbus_transaction vbt;

	if (!force && (state == vdev->state))
		return 0;

	/* Make visible the new state to the rest of world NOW...
	 * The remaining code is highly asynchronous...
	 */

	/* We make the strong assumption that the state can NOT be changed in parallel. The state machine is well-defined
	 * and simultaneous changes should simply NEVER happen.
	 */

	vdev->state = state;

	mb();

	vbus_transaction_start(&vbt);
	vbus_printf(vbt, vdev->nodename, "state", "%d", state);
	vbus_transaction_end(vbt);

	return 0;
}

/**
 * vbus_switch_state
 * @vdev: vbus device
 * @state: new state
 *
 * Advertise in the store a change of the given driver to the given new_state.
 * Return 0 on success, or -errno on error.
 */
static int vbus_switch_state(struct vbus_device *vdev, enum vbus_state state)
{
	DBG("(CPU %d) --> changing state of %s from %d to %d\n", smp_processor_id(), vdev->nodename, vdev->state, state);

	return __vbus_switch_state(vdev, state, false);
}

void vbus_set_backend_suspended(struct vbus_device *vdev)
{
	char key[VBS_KEY_LENGTH];
	char val[10];

	sprintf(key, "%s/state", vdev->nodename);
	sprintf(val, "%d", VbusStateSuspended);

	vdev->state = VbusStateSuspended;
	vbs_store_write(key, val);

	mb();

	complete(&vdev->sync_backfront);
}

int vbus_match(struct device *dev, struct device_driver *drv) {
	struct vbus_driver *vdrv = to_vbus_driver(drv);
	struct vbus_device *vdev = to_vbus_device(dev);

	/* Matching is positive if both strings are identical */
	return (strcmp(vdrv->devicetype, vdev->devicetype) == 0);

}
EXPORT_SYMBOL_GPL(vbus_match);

/*
 * Remove the watch associated to remove device (especially useful for monitoring the state).
 */
void free_otherend_watch(struct vbus_device *vdev, bool with_vbus) {

	if (vdev->otherend_watch.node) {

		if (with_vbus)
			unregister_vbus_watch(&vdev->otherend_watch);
		else
			unregister_vbus_watch_without_vbus(&vdev->otherend_watch);

		kfree(vdev->otherend_watch.node);

		vdev->otherend_watch.node = NULL;
	}

	if (vdev->otherend != NULL)
		vdev->otherend[0] = 0;
}

/*
 * Specific watch register function to focus on the state of a device on the other side.
 */
void watch_otherend(struct vbus_device *vdev) {
	struct vbus_type *bus = container_of(vdev->dev.bus, struct vbus_type, bus);

	vbus_watch_pathfmt(vdev, &vdev->otherend_watch, bus->otherend_changed, "%s/%s", vdev->otherend, "state");
}

/*
 * Announce ourself to the otherend managed device. We mainly prepare to set up a watch on the device state.
 */
static void talk_to_otherend(struct vbus_device *vdev) {
	struct vbus_driver *vdrv = to_vbus_driver(vdev->dev.driver);

	BUG_ON(vdev->otherend[0] != 0);
	BUG_ON(vdev->otherend_watch.node != NULL);

	vdrv->read_otherend_details(vdev);

	/* Set up watch on state of otherend */
	watch_otherend(vdev);

}

void vbus_read_otherend_details(struct vbus_device *vdev, char *id_node, char *path_node) {
	vbus_gather(VBT_NIL, vdev->nodename, id_node, "%i", &vdev->otherend_id, path_node, "%s", vdev->otherend, NULL);
}

extern void checkaddr(struct vbus_device *vdev);
/*
 * The following function is called either in the backend OR the frontend.
 * On the backend side, it may run on CPU #0 (non-RT) or CPU #1 if the backend is configured as realtime.
 */
void vbus_otherend_changed(struct vbus_watch *watch) {
	struct vbus_device *vdev = container_of(watch, struct vbus_device, otherend_watch);
	struct vbus_driver *vdrv = to_vbus_driver(vdev->dev.driver);

	enum vbus_state state;

	state = vbus_read_driver_state(vdev->otherend);

	/* Update the FE state */
	vdev->fe_state = state;

	DBG("On CPU %d frontend otherend changed / device: %s  state: %d\n", smp_processor_id(), vdev->nodename, state);

	/* We do not want to call a callback in a frontend on InitWait. This is
	 * a state issued from the backend to tell the frontend it can be probed.
	 */
	if ((vdrv->otherend_changed) && (state != VbusStateInitWait))
		vdrv->otherend_changed(vdev, state);

	switch (state) {
	case VbusStateInitialised:
	case VbusStateReconfigured:

		vbus_switch_state(vdev, VbusStateConnected);
		break;

		/*
		 * Check for a final action.
		 *
		 * The frontend has changed its state to Closing. Therefore, once the backend performs its cleanings/closing,
		 * we remove the underlying device.
		 */
	case VbusStateClosing:
		DBG("Got closing from frontend: %s\n", vdev->nodename);

		remove_device(vdev->nodename);

		break;

	case VbusStateSuspended:

		vbus_set_backend_suspended(vdev);
		break;

	case VbusStateConnected:

		if (vdev->resuming) {

			/*
			 * It happens after migration & resume
			 *
			 * If we have migrated, we are already in connected state;
			 * we have been put in this state during the reconfiguring process.
			 *
			 */

			/* Resume Step 3 */
			DBG("%s: now is %s ...\n", __func__, vdev->nodename);

			if (vdev->state != VbusStateConnected)
				/* Only in the case we are locally resuming, the backend is in state Suspended */
				vbus_switch_state(vdev, VbusStateConnected);

			vdev->resuming = 0;
		}

		complete(&vdev->sync_backfront);

		break;

	default:
		break;

	}

}
EXPORT_SYMBOL_GPL(vbus_otherend_changed);


/*
 * vbus_dev_probe() is called by the Linux device subsystem when probing a device
 */
int vbus_dev_probe(struct device *dev)
{
	struct vbus_device *vdev = to_vbus_device(dev);
	struct vbus_driver *vdrv = to_vbus_driver(dev->driver);
	enum vbus_state otherend_state;

	DBG("%s\n", vdev->nodename);

	if (!vdrv->probe)
		BUG();

	BUG_ON(!vbus_match(dev, dev->driver));

	init_completion(&vdev->down);
	init_completion(&vdev->sync_backfront);

	DBG("CPU %d  talk_to_otherend: %s\n", smp_processor_id(), vdev->nodename);

	talk_to_otherend(vdev);

	/* On frontend side, the probe will be executed as soon as the backend reaches the state InitWait */

	vdrv->probe(vdev);

	/* Get the state from the otherend to see if we are in a post-migration situation. */
	otherend_state = vbus_read_driver_state(vdev->otherend);

	/* If the node is probed after a migration, it means that the frontend will be reconfigured.
	 * In this case, the resuming operation can go ahead.
	 * If the ME is not migrated (injected), the node is created and the synchronization will be
	 * achieved once the device gets connected.
	 */
	if (otherend_state == VbusStateInitialising) {

		/* This state will allow to wait until the frontend has performed the probe of its device, therefore
		 * until we get the state Initialised.
		 */
		vbus_switch_state(vdev, VbusStateInitWait);

		/* For both RT and non-RT initialization, we wait that the device gets connected */
		wait_for_completion(&vdev->sync_backfront);
	}

	return 0;
}


int vbus_dev_remove(struct device *_dev)
{
	struct vbus_device *vdev = to_vbus_device(_dev);
	struct vbus_driver *vdrv = to_vbus_driver(_dev->driver);
	unsigned int dir_exists;

	DBG("%s", vdev->nodename);

	/*
	 * If the ME is running on a Smart Object which does not offer all the backends matching the ME's frontends,
	 * some frontend related entries may not have been created. We must check here if the entry matching the dev
	 * to remove exists.
	 */
	dir_exists = vbus_directory_exists(VBT_NIL, vdev->otherend_watch.node, "");
	if (dir_exists) {

		if (vdrv->remove)
			vdrv->remove(vdev);

		/* Remove the watch on the remote device. */
		free_otherend_watch(vdev, true);

	}

	vbus_switch_state(vdev, VbusStateClosed);

	/* We perform a complete on this down in case of the agency initiated a shutdown (see vbus_dev_shutdown below).
	 * If it is not the case, the vdev will be removed anyway and the completion has no effect anymore.
	 */
	return 0;
}

/*
 * Shutdown a device. This function can be called externally, but *not* by the Linux subsystem
 * during a reboot operation for instance (because of IRQs off). In this case, force_terminate DC events
 * will be sent to each running ME during the reboot process.
 */
void vbus_dev_shutdown(struct device *dev)
{
	struct vbus_device *vdev = to_vbus_device(dev);
	unsigned int dir_exists;

	DBG("%s", vdev->nodename);

	dir_exists = vbus_directory_exists(VBT_NIL, vdev->otherend_watch.node, "");

	/*
	 * If the ME is running on a Smart Object which does not offer all the backends matching the ME's frontends,
	 * some frontend related entries may not have been created. We must check here if the entry matching the dev
	 * to shutdown exists.
	 */
	if (dir_exists) {

		vbus_switch_state(vdev, VbusStateClosing);

		/* Wait that the frontend get closed. */
		wait_for_completion(&vdev->down);
	}
}

void vbus_register_driver_common(struct vbus_driver *vdrv, struct vbus_type *bus, struct module *owner, const char *mod_name)
{
	int ret;

	DBG("Registering driver name: %s\n", vdrv->name);

	vdrv->driver.name = vdrv->name;
	vdrv->driver.bus = &bus->bus;
	vdrv->driver.owner = owner;
	vdrv->driver.mod_name = mod_name;

	ret = driver_register(&vdrv->driver);
	if (ret)
		BUG();
}

void vbus_unregister_driver(struct vbus_driver *vdrv)
{
	driver_unregister(&vdrv->driver);
}

struct vb_find_info {
	struct vbus_device *vdev;
	const char *nodename;
};

static int cmp_dev(struct device *dev, void *data)
{
	struct vbus_device *vdev = to_vbus_device(dev);
	struct vb_find_info *info = data;

	if (!strcmp(vdev->nodename, info->nodename)) {
		info->vdev = vdev;
		get_device(dev);
		return 1;
	}
	return 0;
}

struct vbus_device *vbus_device_find(const char *nodename, struct bus_type *bus)
{
	struct vb_find_info info = { .vdev = NULL, .nodename = nodename };

	bus_for_each_dev(bus, NULL, &info, cmp_dev);
	return info.vdev;
}

static int cleanup_dev(struct device *dev, void *data)
{
	struct vbus_device *vdev = to_vbus_device(dev);
	struct vb_find_info *info = data;

	if (!strcmp(vdev->nodename, info->nodename)) {
		info->vdev = vdev;
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

	info.vdev = NULL;

	bus_for_each_dev(bus, NULL, &info, cleanup_dev);

	if (info.vdev) {
		device_unregister(&info.vdev->dev);
		put_device(&info.vdev->dev);
	}

}

static void vbus_dev_release(struct device *vdev) {
	if (vdev)
		kfree(to_vbus_device(vdev));
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

	state = vbus_read_driver_state(nodename);

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
	vdev->state = VbusStateUnknown;

	vdev->resuming = 0;

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
	struct vbus_driver *vdrv;
	struct vbus_device *vdev;
	unsigned int domID = *((unsigned int *) data);
	unsigned int curDomID;
	char *ptr_item;
	char item[80];

	if (dev->driver == NULL) {
		/* Skip if driver is NULL, i.e. probe failed */
		return 0;
	}

	vdrv = to_vbus_driver(dev->driver);
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

	DBG("Suspending %s (vbdrv=%p, vdev=%p)\n", vdev->nodename, vdrv, vdev);

	vdrv->suspend(vdev);

	/* Now propagate the suspending event to the frontend */

	vbus_switch_state(vdev, VbusStateSuspending);

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
	struct vbus_driver *vdrv;
	struct vbus_device *vdev;
	unsigned int domID = *((unsigned int *) data);
	unsigned int curDomID;
	char *ptr_item;
	char item[80];

	vdrv = to_vbus_driver(dev->driver);
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

	DBG("Resuming %s (vbdrv=%p, vdev=%p)\n", vdev->nodename, vdrv, vdev);

	/* Before resuming, we need to make sure the backend reached a stable state. */
	/* Two possible scenarios: either the backend has been suspended, so it is in VbusStateSuspended, or
	 * the backend has just been created and we make sure it reached the VbusStateInitWait state.
	 */

	/* The backend is now either in VbusStateSuspended OR VbusStateInitWait */
	vdrv->resume(vdev);

	vdev->resuming = 1;

	if (vdev->state != VbusStateSuspended)
		/* we are in StateInitWait because newly create vbstore entries after migration */
		vbus_switch_state(vdev, VbusStateReconfiguring);
	else {
		DBG("Changing state of %s to resuming\n", vdev->nodename);
		vbus_switch_state(vdev, VbusStateResuming);
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

/*
 * Called at each backend creation when a frontend is initializing.
 */
void vbus_dev_changed(const char *node, char *type, struct vbus_type *bus) {

	struct vbus_device *vdev;

	vdev = vbus_device_find(node, &bus->bus);
	if (vdev) {
		lprintk("## (agency) Node %s already exits. It should not!\n", node);
		BUG();
	}

	vbus_probe_node(bus, type, node);
}

/*
 * Perform a bottom half (deferred) processing on the receival of dc_event.
 * Here, we avoid to use a worqueue. Prefer thread instead, it will be also easier to manage with SO3/virtshare.
 */
static irqreturn_t directcomm_isr_thread(int irq, void *args) {
	dc_event_t dc_event;

	dc_event = atomic_read((const atomic_t *) &AVZ_shared->dc_event);

	/* Reset the dc_event now so that the domain can send another dc_event */
	atomic_set((atomic_t *) &AVZ_shared->dc_event, DC_NO_EVENT);

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

	dc_event = atomic_read((const atomic_t *) &AVZ_shared->dc_event);

	DBG("Received directcomm interrupt for event: %d\n", AVZ_shared->dc_event);

	/* We should not receive twice a same dc_event, before it has been fully processed. */
	BUG_ON(atomic_read(&dc_incoming_domID[dc_event]) != -1);

	atomic_set(&dc_incoming_domID[dc_event], domID);

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
	case DC_TRIGGER_DEV_PROBE:
	case DC_TRIGGER_LOCAL_COOPERATION:

		/* FALLTHROUGH */

		/* Check if it is the response to a dc_event. Can be done immediately in the top half. */
		if (atomic_read(&dc_outgoing_domID[dc_event]) != -1) {

			dc_stable(dc_event);
			break; /* Out of the switch */
		}

		/* Start the deferred thread */
		return IRQ_WAKE_THREAD;

	default:
		lprintk("(Agency) %s: something weird happened, directcomm interrupt was triggered with dc_event %d, but no DC event was configured !\n", __func__, dc_event);
		break;
	}

	/* Reset the dc_event now so that the domain can send another dc_event */
	atomic_set((atomic_t *) &AVZ_shared->dc_event, DC_NO_EVENT);

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

	lprintk("%s: initializing vbstore...\n", __func__);

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

		avz_hypercall(__HYPERVISOR_event_channel_op, EVTCHNOP_alloc_unbound, (long) &alloc_unbound, 0, 0);

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
