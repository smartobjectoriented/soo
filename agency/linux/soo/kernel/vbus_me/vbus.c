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
#include <linux/device.h>

#include <soo/hypervisor.h>
#include <soo/vbus_me.h>
#include <soo/evtchn.h>
#include <soo/vbstore.h>

#include <soo/uapi/avz.h>
#include <soo/uapi/soo.h>
#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>

#define SYNC_BACKFRONT_COMPLETE		0
#define SYNC_BACKFRONT_SUSPEND		1
#define SYNC_BACKFRONT_RESUME		2
#define SYNC_BACKFRONT_SUSPENDED	3

#define VBUS_TIMEOUT	120

/* List of device drivers */
LIST_HEAD(vbus_me_drivers);

/*
 * Walk through the list of vbus device drivers and perform an action.
 * When the action returns 1, we stop the walking.
 */
void vbus_drivers_for_each(void *data, int (*fn)(struct vbus_me_driver *, void *)) {
	struct list_head *pos;
	struct vbus_me_driver *vdrv;

	list_for_each(pos, &vbus_me_drivers)
	{
		vdrv = list_entry(pos, struct vbus_me_driver, list);
		if (fn(vdrv, data) == 1)
			return ;
	}
}

static int __vbus_switch_state(struct vbus_me_device *vdev, enum vbus_state state, bool force)
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
static int vbus_switch_state(struct vbus_me_device *vdev, enum vbus_state state)
{
	DBG("--> changing state of %s from %d to %d\n", vdev->nodename, vdev->state, state);

	return __vbus_switch_state(vdev, state, false);
}

/*
 * Remove the watch associated to remove device (especially useful for monitoring the state).
 */
void vbus_me_free_otherend_watch(struct vbus_me_device *vdev, bool with_vbus) {

	if (vdev->otherend_watch.node) {

		if (with_vbus)
			unregister_vbus_watch(&vdev->otherend_watch);
		else
			unregister_vbus_watch_without_vbus(&vdev->otherend_watch);

		kfree(vdev->otherend_watch.node);
		vdev->otherend_watch.node = NULL;

		/* No watch on otherend, and no interactions anymoire. */
		vdev->otherend[0] = 0;
	}
}

/*
 * Specific watch register function to focus on the state of a device on the other side.
 */
void watch_me_otherend(struct vbus_me_device *vdev) {
	vbus_me_watch_pathfmt(vdev, &vdev->otherend_watch, vdev->vbus->otherend_changed, "%s/%s", vdev->otherend, "state");
}

/*
 * Announce ourself to the otherend managed device. We mainly prepare to set up a watch on the device state.
 */
static void me_talk_to_otherend(struct vbus_me_device *vdev) {
	struct vbus_me_driver *vdrv = vdev->vdrv;

	BUG_ON(vdev->otherend[0] != 0);
	BUG_ON(vdev->otherend_watch.node != NULL);
	
	vdrv->read_otherend_details(vdev);

	/* Set up watch on state of otherend */
	watch_me_otherend(vdev);
}

void vbus_me_read_otherend_details(struct vbus_me_device *vdev, char *id_node, char *path_node) {
	rtdm_vbus_gather(VBT_NIL, vdev->nodename, id_node, "%i", &vdev->otherend_id, path_node, "%s", vdev->otherend, NULL);
}

void vbus_me_otherend_changed(struct vbus_watch *watch) {
	struct vbus_me_device *vdev = container_of(watch, struct vbus_me_device, otherend_watch);
	struct vbus_me_driver *vdrv = vdev->vdrv;

	enum vbus_state state;

	state = vbus_read_driver_state(vdev->otherend);

        DBG("On domID: %d, otherend changed / device: %s  state: %d, CPU %d\n", smp_processor_id(), vdev->nodename, state, smp_processor_id());
	
	/* We do not want to call a callback in a frontend on InitWait. This is
	 * a state issued from the backend to tell the frontend it can be probed.
	 */
	if ((vdrv->otherend_changed) && (state != VbusStateInitWait))
		vdrv->otherend_changed(vdev, state);

	BUG_ON(irqs_disabled());

	switch (state) {

	case VbusStateInitWait:

		/* Check if we are suspended (before migration). In this case, we do nothing since the backend will
		 * set its state in resuming later on.
		 */
		if (vdev->state != VbusStateSuspended) {
			/*
			 * We set up the watch on the state at this time since the frontend probe will lead to
			 * state Initialised, which will trigger rather quickly a Connected state event from the backend.
			 * We have to be ready to process it.
			 */
			DBG("%s: Backend probed device: %s, now the frontend will be probing on its side.\n", __func__, vdev->nodename);

			vdrv->probe(vdev);

			vbus_switch_state(vdev, VbusStateInitialised);
		}
		break;


	case VbusStateSuspending:
		vbus_switch_state(vdev, VbusStateSuspended);
		break;

		/*
		 * Check for a final action.
		 *
		 * The backend has been shut down. Once the frontend has finished its work,
		 * we need to release the pending completion lock.
		 */
	case VbusStateClosed:
		/* In the frontend, we are completing the closing. */
		complete(&vdev->down);
		break;

	case VbusStateReconfiguring:
		vbus_switch_state(vdev, VbusStateReconfigured);
		break;

	case VbusStateResuming:
		vbus_switch_state(vdev, VbusStateConnected);
		break;

	case VbusStateConnected:

		if (vdev->state != VbusStateConnected)
			/* The frontend is in VbusStateReconfigured after been migrated. */
			vbus_switch_state(vdev, VbusStateConnected);

		break;

	default:
		break;

	}
}

/*
 * vbus_dev_probe() is called by the Linux device subsystem when probing a device
 */
int vbus_me_dev_probe(struct vbus_me_device *vdev)
{
	struct vbus_me_driver *vdrv = vdev->vdrv;

	DBG("%s\n", vdev->nodename);

	if (!vdrv->probe)
		BUG();

	init_completion(&vdev->down);
	init_completion(&vdev->sync_backfront);

	DBG("ME #%d  talk_to_otherend: %s\n", smp_processor_id(), vdev->nodename);

	me_talk_to_otherend(vdev);

	/* On frontend side, the probe will be executed as soon as the backend reaches the state InitWait */

	return 0;
}

int vbus_me_dev_remove(struct vbus_me_device *vdev)
{
	unsigned int dir_exists;

	/*
	 * If the ME is running on a Smart Object which does not offer all the backends matching the ME's frontends,
	 * some frontend related entries may not have been created. We must check here if the entry matching the dev
	 * to remove exists.
	 */
	dir_exists = vbus_directory_exists(VBT_NIL, vdev->otherend_watch.node, "");
	if (dir_exists) {
		DBG("%s", vdev->nodename);

		/* Remove the watch on the remote device. */
		vbus_me_free_otherend_watch(vdev, true);

		/* Definitively remove everything about this device */
		kfree(vdev);
	}

	return 0;
}

/*
 * Shutdown a device.
 */
void vbus_me_dev_shutdown(struct vbus_me_device *vdev)
{
	struct vbus_me_driver *vdrv = vdev->vdrv;
	unsigned int dir_exists;

	DBG("%s", vdev->nodename);

	dir_exists = vbus_directory_exists(VBT_NIL, vdev->otherend_watch.node, "");
	if (dir_exists) {
		if (vdev->state != VbusStateConnected) {
			printk("%s: %s: %s != Connected, skipping\n", __func__, vdev->nodename, vbus_strstate(vdev->state));
			BUG();
		}

		if (vdrv->shutdown != NULL)
			vdrv->shutdown(vdev);

		vbus_switch_state(vdev, VbusStateClosing);

		wait_for_completion(&vdev->down);
	}
}

struct vb_me_find_info {
	struct vbus_me_device *vdev;
	const char *nodename;
};

static int cmp_dev(struct vbus_me_device *vdev, void *data)
{
	struct vb_me_find_info *info = data;

	if (!strcmp(vdev->nodename, info->nodename)) {
		info->vdev = vdev;
		return 1;
	}
	return 0;
}

struct vbus_me_device *vbus_me_device_find(const char *nodename)
{
	struct vb_me_find_info info = { .vdev = NULL, .nodename = nodename };

	frontend_for_each(&info, cmp_dev);

	return info.vdev;
}


int vbus_me_match(struct vbus_me_driver *vdrv, void *data) {
	struct vbus_me_device *vdev = (struct vbus_me_device *) data;

	if (!strcmp(vdrv->devicetype, vdev->devicetype)) {
		vdev->vdrv = vdrv;
		vbus_me_dev_probe(vdev);
		return 1;
	}

	return 0;
}

/*
 * Create a new node and initialize basic structure (vdev)
 */
static struct vbus_me_device *vbus_me_probe_node(struct vbus_me_type *bus, const char *type, const char *nodename, char const *compat)
{
	char devname[VBUS_ID_SIZE];
	int err;
	struct vbus_me_device *vdev;
	enum vbus_state state;

	state = vbus_read_driver_state(nodename);

	/* If the backend driver entry exists, but no frontend is using it, there is no
	 * vbstore entry related to the state and we simply skip it.
	 */
	if (state == VbusStateUnknown)
		return 0;

	BUG_ON(state != VbusStateInitialising);

	vdev = kzalloc(sizeof(struct vbus_me_device), GFP_ATOMIC);
	if (!vdev)
		BUG();

	vdev->state = VbusStateInitialising;

	vdev->resuming = 0;

	vdev->vbus = bus;

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
	vbus_drivers_for_each(vdev, vbus_me_match);

	return vdev;
}

int vbus_me_register_driver_common(struct vbus_me_driver *vdrv)
{
	DBG("Registering driver name: %s\n", vdrv->name);

	/* Add the new driver to the main list */
	list_add_tail(&vdrv->list, &vbus_me_drivers);

	return 0;
}


/******************/

void vbus_me_dev_changed(const char *node, char *type, struct vbus_me_type *bus, const char *compat) {
	struct vbus_me_device *vdev;

	/*
	 * Either the device does not exist (backend or frontend) and the dev must be allocated, initialized
	 * and probed via the dev subsystem of Linux, OR the device exists (after migration)
	 * and in this case, the device exists on the frontend side only, and we only have to "talk_to_otherend" to
	 * set up the watch on its state (and retrieve the otherend id and name).
	 */

	vdev = vbus_me_device_find(node);
	if (!vdev) {
		vdev = vbus_me_probe_node(bus, type, node, compat);

		/* Add the new device to the main list */
		add_new_dev(vdev);

	} else {

		BUG_ON(smp_processor_id() == DOMID_AGENCY);

		/* Update the our state in vbstore. */
		/* We force the update, this will not trigger a watch since the watch is set right afterwards */
		 __vbus_switch_state(vdev, vdev->state, true);

		/* Setting the watch on the state */
		me_talk_to_otherend(vdev);
	}
}

