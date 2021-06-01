
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

#if 0
#define DEBUG
#endif

#include <soo/evtchn.h>
#include <linux/slab.h>

#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/uapi/debug.h>

#include <soo/vdevback.h>

/* vdevback_processing_start() and vdevback_processing_end() will prevent against
 * suspending/closing actions.
 *
 * The functions can be called in multiple execution context (threads).
 *
 * Return true if the frontend is connected and the processing can safely be performed.
 * Return false *only* if the frontend is closing (and will be never connected again).
 *
 * The current thread is suspended until the frontend gets connected.
 *
 */
bool vdevback_processing_begin(struct vbus_device *vdev) {
	void *priv = dev_get_drvdata(&vdev->dev);
	vdevback_t *vdevback = (vdevback_t *) priv;

	/* Make sure we are still connected */
	if (vdev->fe_state == VbusStateClosing)
		return false;

	/* Could be still being initialized... */
	if (vdev->fe_state != VbusStateConnected)
		wait_for_completion(&vdevback->sync);

	if (atomic_read(&vdevback->processing_count) == 0) {
		atomic_inc(&vdevback->processing_count);

		mutex_lock(&vdevback->processing_lock);

		/* Make sure we are still connected after a lock contention. */
		if (vdev->fe_state == VbusStateClosing) {
			mutex_unlock(&vdevback->processing_lock);

			complete(&vdevback->sync);
			return false;
		}

		if (vdev->fe_state != VbusStateConnected)
			wait_for_completion(&vdevback->sync);


		BUG_ON(vdev->fe_state != VbusStateConnected);
	} else
		atomic_inc(&vdevback->processing_count);

	return true;
}

/*
 * Finish a processing section against suspend/close prevention
 */
void vdevback_processing_end(struct vbus_device *vdev) {
	void *priv = dev_get_drvdata(&vdev->dev);
	vdevback_t *vdevback = (vdevback_t *) priv;

	atomic_dec(&vdevback->processing_count);

	if (atomic_read(&vdevback->processing_count) == 0)
		mutex_unlock(&vdevback->processing_lock);

}

/*
 * Probe entry point for our vbus backend.
 * The probe is executed as soon as a frontend is showing up.
 */
static void __probe(struct vbus_device *vdev) {
	void *priv;
	vdevback_t *vdevback;
	vdrvback_t *vdrvback = to_vdrvback(vdev);

	DBG("%s: SOO dummy backend driver for testing\n", __func__);

	vdrvback->probe(vdev);

	priv = dev_get_drvdata(&vdev->dev);
	vdevback = (vdevback_t *) priv;

	atomic_set(&vdevback->processing_count, 0);

	mutex_init(&vdevback->processing_lock);

	init_completion(&vdevback->sync);

}

static void __remove(struct vbus_device *vdev) {
	void *priv = dev_get_drvdata(&vdev->dev);
	vdevback_t *vdevback = (vdevback_t *) priv;
	vdrvback_t *vdrvback = to_vdrvback(vdev);

	/*
	 * If a processing is being started, but is locked by the processing_lock mutex,
	 * it must be first woken up to go ahead and abort the function call.
	 */

	if (atomic_read(&vdevback->processing_count) > 0) {

		/* Release the possible waiters on the lock so that they can pursue their work */
		mutex_unlock(&vdevback->processing_lock);

		while (atomic_read(&vdevback->processing_count) > 0)
			wait_for_completion(&vdevback->sync);

	} else
		mutex_unlock(&vdevback->processing_lock);

	vdrvback->remove(vdev);
}

/*
 * Callback received when the frontend's state changes.
 */
static void __otherend_changed(struct vbus_device *vdev, enum vbus_state frontend_state) {
	void *priv = dev_get_drvdata(&vdev->dev);
	vdevback_t *vdevback = (vdevback_t *) priv;
	vdrvback_t *vdrvback = to_vdrvback(vdev);

	DBG("%s\n", vbus_strstate(frontend_state));

	switch (frontend_state) {

	case VbusStateInitialised:
	case VbusStateReconfigured:
		DBG0("SOO Dummy: reconfigured...\n");

		BUG_ON(vdev->fe_state == VbusStateConnected);

		vdrvback->reconfigured(vdev);

		mutex_unlock(&vdevback->processing_lock);

		break;

	case VbusStateConnected:
		DBG0("vdummy frontend connected, all right.\n");
		vdrvback->connected(vdev);

		complete(&vdevback->sync);
		break;

	case VbusStateClosing:
		DBG0("Got that the virtual dummy frontend now closing...\n");

		BUG_ON(vdev->state != VbusStateConnected);

		/* Make sure no further processing is going on. */
		mutex_lock(&vdevback->processing_lock);

		/* Prepare the sync completion to coordinate the removal of device. */
		reinit_completion(&vdevback->sync);

		vdrvback->close(vdev);
		break;

	case VbusStateSuspended:
		/* Suspend Step 3 */
		DBG("frontend_suspended: %s ...\n", vdev->nodename);

		break;

	case VbusStateUnknown:
	default:
		break;
	}
}

static int __suspend(struct vbus_device *vdev) {
	void *priv = dev_get_drvdata(&vdev->dev);
	vdevback_t *vdevback = (vdevback_t *) priv;
	vdrvback_t *vdrvback = to_vdrvback(vdev);

	DBG0("vdummy_suspend: wait for frontend now...\n");

	BUG_ON(vdev->fe_state != VbusStateConnected);

	mutex_lock(&vdevback->processing_lock);

	/* Prepare the next processing activity for waiting on FE to get connected. */
	reinit_completion(&vdevback->sync);

	vdrvback->suspend(vdev);

	return 0;
}

static int __resume(struct vbus_device *vdev) {
	void *priv = dev_get_drvdata(&vdev->dev);
	vdevback_t *vdevback = (vdevback_t *) priv;
	vdrvback_t *vdrvback = to_vdrvback(vdev);

	/* Resume Step 1 */

	DBG("backend resuming: %s ...\n", vdev->nodename);

	BUG_ON(vdev->fe_state == VbusStateConnected);

	vdrvback->resume(vdev);

	mutex_unlock(&vdevback->processing_lock);

	return 0;
}

/* ** Driver Registration ** */

void vdevback_init(char *name, vdrvback_t *vbackdrv) {

	vbackdrv->vdrv.owner = THIS_MODULE;
	vbackdrv->vdrv.name = name;
	strcpy(vbackdrv->vdrv.devicetype, name);

	vbackdrv->vdrv.probe = __probe;
	vbackdrv->vdrv.remove = __remove;
	vbackdrv->vdrv.otherend_changed = __otherend_changed;
	vbackdrv->vdrv.suspend = __suspend;
	vbackdrv->vdrv.resume = __resume;

	vbus_register_backend(&vbackdrv->vdrv);
}



void vdevback_add_entry(struct vbus_device *vdev, struct list_head *list) {
	vdev_entry_t *entry;

	entry = kzalloc(sizeof(vdev_entry_t), GFP_ATOMIC);
	BUG_ON(!entry);

	entry->vdev = vdev;

	list_add(&entry->list, list);
}

/*
 * Search for a console related to a specific ME according to its domid.
 */
struct vbus_device *vdevback_get_entry(uint32_t domid, struct list_head *_list) {
	vdev_entry_t *entry;

	list_for_each_entry(entry, _list, list)
		if (entry->vdev->otherend_id == domid)
			return entry->vdev;
	return NULL;
}

/*
 * Remove a console attached to a specific ME
 */
void vdevback_del_entry(struct vbus_device *vdev, struct list_head *_list) {
	vdev_entry_t *entry;

	list_for_each_entry(entry, _list, list)
		if (entry->vdev == vdev) {
			list_del(&entry->list);
			kfree(entry);
			return ;
		}
	BUG();
}