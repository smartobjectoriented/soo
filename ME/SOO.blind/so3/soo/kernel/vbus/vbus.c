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

#include <types.h>
#include <heap.h>
#include <list.h>

#include <device/device.h>

#include <mach/domcall.h>

#include <soo/hypervisor.h>
#include <soo/avz.h>
#include <soo/vbus.h>
#include <soo/evtchn.h>
#include <soo/vbstore.h>

#include <soo/soo.h>
#include <soo/console.h>
#include <soo/debug.h>
#include <soo/debug/dbgvar.h>
#include <soo/debug/logbool.h>

#define SYNC_BACKFRONT_COMPLETE		0
#define SYNC_BACKFRONT_SUSPEND		1
#define SYNC_BACKFRONT_RESUME		2
#define SYNC_BACKFRONT_SUSPENDED	3

#define VBUS_TIMEOUT	120

/* Event channels used for directcomm channel between agency and ME */
unsigned int dc_evtchn;
spinlock_t dc_lock;

/* List of device drivers */
LIST_HEAD(vbus_drivers);

/*
 * Walk through the list of vbus device drivers and perform an action.
 * When the action returns 1, we stop the walking.
 */
void vbus_drivers_for_each(void *data, int (*fn)(struct vbus_driver *, void *)) {
	struct list_head *pos;
	struct vbus_driver *drv;

	list_for_each(pos, &vbus_drivers)
	{
		drv = list_entry(pos, struct vbus_driver, list);
		if (fn(drv, data) == 1)
			return ;
	}
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

	dmb();

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

/*
 * Remove the watch associated to remove device (especially useful for monitoring the state).
 */
void free_otherend_watch(struct vbus_device *dev, bool with_vbus) {

	if (dev->otherend_watch.node) {

		if (with_vbus)
			unregister_vbus_watch(&dev->otherend_watch);
		else
			unregister_vbus_watch_without_vbus(&dev->otherend_watch);

		free(dev->otherend_watch.node);
		dev->otherend_watch.node = NULL;

		/* No watch on otherend, and no interactions anymoire. */
		dev->otherend[0] = 0;
	}
}

/*
 * Specific watch register function to focus on the state of a device on the other side.
 */
void watch_otherend(struct vbus_device *dev) {
	vbus_watch_pathfmt(dev, &dev->otherend_watch, dev->bus->otherend_changed, "%s/%s", dev->otherend, "state");
}

/*
 * Announce ourself to the otherend managed device. We mainly prepare to set up a watch on the device state.
 */
static void talk_to_otherend(struct vbus_device *dev) {
	struct vbus_driver *drv = dev->drv;

	BUG_ON(dev->otherend[0] != 0);
	BUG_ON(dev->otherend_watch.node != NULL);
	
	drv->read_otherend_details(dev);

	/* Set up watch on state of otherend */
	watch_otherend(dev);
}

void vbus_read_otherend_details(struct vbus_device *vdev, char *id_node, char *path_node) {
	vbus_gather(VBT_NIL, vdev->nodename, id_node, "%i", &vdev->otherend_id, path_node, "%s", vdev->otherend, NULL);
}

/* If something in array of ids matches this device, return it. */
static const struct vbus_device_id *match_device(const struct vbus_device_id *arr, struct vbus_device *dev) {
	for (; *arr->devicetype != '\0'; arr++) {
		if (!strcmp(arr->devicetype, dev->devicetype))
			return arr;
	}
	return NULL;
}

/*
 * The following function is called either in the backend OR the frontend.
 * On the backend side, it may run on CPU #0 (non-RT) or CPU #1 if the backend is configured as realtime.
 */
void vbus_otherend_changed(struct vbus_watch *watch) {
	struct vbus_device *dev = container_of(watch, struct vbus_device, otherend_watch);
	struct vbus_driver *drv = dev->drv;
	const struct vbus_device_id *id;

	enum vbus_state state;

	state = vbus_read_driver_state(dev->otherend);

        DBG("On domID: %d, otherend changed / device: %s  state: %d, CPU %d\n", ME_domID(), dev->nodename, state, smp_processor_id());
	
	/* We do not want to call a callback in a frontend on InitWait. This is
	 * a state issued from the backend to tell the frontend it can be probed.
	 */
	if ((drv->otherend_changed) && (state != VbusStateInitWait))
		drv->otherend_changed(dev, state);

	BUG_ON(local_irq_is_disabled());

	switch (state) {

	case VbusStateInitWait:

		/* Check if we are suspended (before migration). In this case, we do nothing since the backend will
		 * set its state in resuming later on.
		 */
		if (dev->state != VbusStateSuspended) {
			/*
			 * We set up the watch on the state at this time since the frontend probe will lead to
			 * state Initialised, which will trigger rather quickly a Connected state event from the backend.
			 * We have to be ready to process it.
			 */
			DBG("%s: Backend probed device: %s, now the frontend will be probing on its side.\n", __func__, dev->nodename);

			id = match_device(drv->ids, dev);
			if (!id)
				BUG();

			drv->probe(dev, id);

			vbus_switch_state(dev, VbusStateInitialised);
		}
		break;


	case VbusStateSuspending:
		vbus_switch_state(dev, VbusStateSuspended);
		break;

		/*
		 * Check for a final action.
		 *
		 * The backend has been shut down. Once the frontend has finished its work,
		 * we need to release the pending completion lock.
		 */
	case VbusStateClosed:
		/* In the frontend, we are completing the closing. */
		complete(&dev->down);
		break;

	case VbusStateReconfiguring:
		vbus_switch_state(dev, VbusStateReconfigured);
		break;

	case VbusStateResuming:
		vbus_switch_state(dev, VbusStateConnected);
		break;

	case VbusStateConnected:

		if (dev->state != VbusStateConnected)
			/* The frontend is in VbusStateReconfigured after been migrated. */
			vbus_switch_state(dev, VbusStateConnected);

		break;

	default:
		break;

	}
}

/*
 * vbus_dev_probe() is called by the Linux device subsystem when probing a device
 */
int vbus_dev_probe(struct vbus_device *dev)
{
	struct vbus_driver *drv = dev->drv;
	const struct vbus_device_id *id;

	DBG("%s\n", dev->nodename);

	if (!drv->probe)
		BUG();

	id = match_device(drv->ids, dev);
	if (!id)
		BUG();

	init_completion(&dev->down);
	init_completion(&dev->sync_backfront);

	DBG("ME #%d  talk_to_otherend: %s\n", ME_domID(), dev->nodename);

	talk_to_otherend(dev);

	/* On frontend side, the probe will be executed as soon as the backend reaches the state InitWait */

	return 0;
}

int vbus_dev_remove(struct vbus_device *dev)
{
	unsigned int dir_exists;

	/*
	 * If the ME is running on a Smart Object which does not offer all the backends matching the ME's frontends,
	 * some frontend related entries may not have been created. We must check here if the entry matching the dev
	 * to remove exists.
	 */
	dir_exists = vbus_directory_exists(VBT_NIL, dev->otherend_watch.node, "");
	if (dir_exists) {
		DBG("%s", dev->nodename);

		/* Remove the watch on the remote device. */
		free_otherend_watch(dev, true);

		/* Definitively remove everything about this device */
		free(dev);
	}

	return 0;
}

/*
 * Shutdown a device.
 */
void vbus_dev_shutdown(struct vbus_device *dev)
{
	struct vbus_driver *drv = dev->drv;
	unsigned int dir_exists;

	DBG("%s", dev->nodename);

	dir_exists = vbus_directory_exists(VBT_NIL, dev->otherend_watch.node, "");
	if (dir_exists) {
		if (dev->state != VbusStateConnected) {
			printk("%s: %s: %s != Connected, skipping\n", __func__, dev->nodename, vbus_strstate(dev->state));
			return ;
		}

		if (drv->shutdown != NULL)
			drv->shutdown(dev);

		vbus_switch_state(dev, VbusStateClosing);

		wait_for_completion(&dev->down);
	}
}

struct vb_find_info {
	struct vbus_device *dev;
	const char *nodename;
};

static int cmp_dev(struct vbus_device *dev, void *data)
{
	struct vb_find_info *info = data;

	if (!strcmp(dev->nodename, info->nodename)) {
		info->dev = dev;
		return 1;
	}
	return 0;
}

struct vbus_device *vbus_device_find(const char *nodename)
{
	struct vb_find_info info = { .dev = NULL, .nodename = nodename };

	frontend_for_each(&info, cmp_dev);

	return info.dev;
}

/* Driver management */

int vbus_match(struct vbus_driver *drv, void *data) {
	struct vbus_device *dev = (struct vbus_device *) data;

	if (!drv->ids)
		return 0;

	if (match_device(drv->ids, dev) != NULL) {
		dev->drv = drv;
		vbus_dev_probe(dev);
		return 1;
	}

	return 0;
}

/*
 * Create a new node and initialize basic structure (vdev)
 */
static struct vbus_device *vbus_probe_node(struct vbus_type *bus, const char *type, const char *nodename)
{
	char devname[VBUS_ID_SIZE];
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

	vdev = malloc(sizeof(struct vbus_device));
	if (!vdev)
		BUG();

	memset(vdev, 0, sizeof(struct vbus_device));

	vdev->state = VbusStateInitialising;

	vdev->resuming = 0;
	vdev->realtime = realtime;

	vdev->drv = NULL;
	vdev->bus = bus;

	strcpy(vdev->nodename, nodename);
	strcpy(vdev->devicetype, type);

	err = bus->get_bus_id(devname, vdev->nodename);
	if (err)
		BUG();
	/*
	 * Register with generic device framework.
	 * The link with the driver is also done at this moment.
	 */

	/* Check for a driver and device matching */
	vbus_drivers_for_each(vdev, vbus_match);

	return vdev;
}


int vbus_register_driver_common(struct vbus_driver *drv)
{
	DBG("Registering driver name: %s\n", drv->name);

	/* Add the new driver to the main list */
	list_add_tail(&drv->list, &vbus_drivers);

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

	dev = vbus_device_find(node);
	if (!dev) {
		dev = vbus_probe_node(bus, type, node);

		/* Add the new device to the main list */
		add_new_dev(dev);

	} else {

		BUG_ON(ME_domID() == DOMID_AGENCY);

		/* Update the our state in vbstore. */
		/* We force the update, this will not trigger a watch since the watch is set right afterwards */
		 __vbus_switch_state(dev, dev->state, true);

		/* Setting the watch on the state */
		talk_to_otherend(dev);
	}
}

/*
 * Perform a bottom half (deferred) processing on the receival of dc_event.
 * Here, we avoid to use a worqueue. Prefer thread instead, it will be also easier to manage with SO3.
 */
static irq_return_t directcomm_isr_thread(int irq, void *data) {
	dc_event_t dc_event;

	dc_event = atomic_read(&avz_shared_info->dc_event);

	/* Reset the dc_event now so that the domain can send another dc_event */
	atomic_set(&avz_shared_info->dc_event, DC_NO_EVENT);

	perform_task(dc_event);

	return IRQ_COMPLETED;
}

/*
 * Interrupt routine for direct communication event channel
 * IRQs are off
 */
static irq_return_t directcomm_isr(int irq, void *data) {
	dc_event_t dc_event;

	dc_event = atomic_read(&avz_shared_info->dc_event);

	DBG("(ME domid %d): Received directcomm interrupt for event: %d\n", ME_domID(), avz_shared_info->dc_event);

	/* We should not receive twice a same dc_event, before it has been fully processed. */
	BUG_ON(atomic_read(&dc_incoming_domID[dc_event]) != -1);

	atomic_set(&dc_incoming_domID[dc_event], DOMID_AGENCY); /* At the moment, only from the agency */

	/* Work to be done in ME */

	switch (dc_event) {

	case DC_RESUME:
	case DC_SUSPEND:
	case DC_PRE_SUSPEND:
	case DC_FORCE_TERMINATE:
	case DC_POST_ACTIVATE:
	case DC_LOCALINFO_UPDATE:
	case DC_TRIGGER_DEV_PROBE:

		/* Check if it is the response to a dc_event. */
		if (atomic_read(&dc_outgoing_domID[dc_event]) != -1) {
			dc_stable(dc_event);
			break; /* Out of the switch */
		}

		/* Start the deferred thread */
		return IRQ_BOTTOM;

	default:
		printk("(ME) %s: something weird happened, directcomm interrupt was triggered, but no DC event was configured !\n", __func__);
		break;

	}

	/* Reset the dc_event now so that the domain can send another dc_event */
	atomic_set(&avz_shared_info->dc_event, DC_NO_EVENT);

	return IRQ_COMPLETED;
}

/*
 * Vbus initialization function.
 */
void vbus_init(void)
{
	int res;
	char buf[20];
	struct vbus_transaction vbt;

	spin_lock_init(&dc_lock);

	vbstore_me_init();

	/* Set up the direct communication channel for post-migration activities
	 * previously established by dom0.
	 */

	vbus_transaction_start(&vbt);

	sprintf(buf, "soo/directcomm/%d", ME_domID());

	res = vbus_scanf(vbt, buf, "event-channel", "%d", &dc_evtchn);

	if (res != 1) {
		printk("%s: reading soo/directcomm failed. Error code: %d\n", __func__, res);
		BUG();
	}

	vbus_transaction_end(vbt);

	/* Binding the irqhandler to the eventchannel */
	DBG("%s: setting up the direct comm event channel (%d) ...\n", __func__, dc_evtchn);
	res = bind_interdomain_evtchn_to_irqhandler(DOMID_AGENCY, dc_evtchn, directcomm_isr, directcomm_isr_thread, NULL);

	if (res <= 0) {
		printk("Error: bind_evtchn_to_irqhandler failed");
		BUG();
	}

	dc_evtchn = evtchn_from_irq(res);
	DBG("%s: local event channel bound to directcomm towards non-RT Agency : %d\n", __func__, dc_evtchn);

	DBG("vbus_init OK!\n");
}

/*
 * DOMCALL_sync_directcomm
 */
int do_sync_directcomm(void *arg)
{
	struct DOMCALL_directcomm_args *args = arg;

	args->directcomm_evtchn = dc_evtchn;

	return 0;
}
