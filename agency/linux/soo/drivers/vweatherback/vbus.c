/*
 * Copyright (C) 2018-2019 Baptiste Delporte <bonel@bonel.net>
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
#include <linux/wait.h>
#include <linux/slab.h>

#include <asm/io.h>
#include <asm/page.h>
#include <asm/mach/map.h>

#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/uapi/debug.h>

#include "common.h"

/* Protection against shutdown (or other) */
static struct mutex processing_lock[MAX_DOMAINS];
static struct mutex processing_count_lock[MAX_DOMAINS];
static volatile uint32_t processing_count[MAX_DOMAINS];

static volatile bool __connected[MAX_DOMAINS];
static struct completion connected_sync[MAX_DOMAINS];
static uint32_t vdev_id[MAX_DOMAINS];

/* The functions processing_start() and processing_end() are used to
 * prevent pre-migration suspending actions.
 * The functions can be called in multiple execution context (threads).
 *
 * <vdev_id> is used to keep track of a particular instance of a backend instance
 * bound to a certain frontend. If this frontend will disappear, but another ME
 * will be quickly take the same slot than the previous backend, the processing
 * which is expected to use this *disappeared* frontend will fail, returning false.
 */
static bool processing_start(domid_t domid) {

	mutex_lock(&processing_count_lock[domid]);

	if (!vweather.vdev[domid] || ((vdev_id != 0) && (vweather.vdev[domid]->id != vdev_id[domid]))) {

		/* Keep track of this instance - Help to maintain the consistency during processing in case
	 	 * of ME swap within a same slot.
	 	 */
		if (vweather.vdev[domid])
			vdev_id[domid] = vweather.vdev[domid]->id;

		mutex_unlock(&processing_count_lock[domid]);

		return false;

	}

	if (processing_count[domid] == 0)
		mutex_lock(&processing_lock[domid]);

	/* In the meanwhile, if the backend has been closed, it does not make sense to
	 * go ahead with activities. This situation may happen if we have been suspended for the processing_lock.
	 * We therefore are alone
	 * The caller must handle this too.
	 */
	if (!vweather.vdev[domid] || ((vdev_id != 0) && (vweather.vdev[domid]->id != vdev_id[domid]))) {

		/* Keep track of this instance - Help to maintain the consistency during processing in case
		 * of ME swap within a same slot.
		 */
		if (vweather.vdev[domid])
			vdev_id[domid] = vweather.vdev[domid]->id;

		/* The processing_count has value 0 in all case, since a value greater than 0 means
		 * any other operations must wait for processing_lock, i.e. processing_count is to 0.
		 * The vdev instance cannot be altered from its existence point of view during processing.
		 */
		mutex_unlock(&processing_count_lock[domid]);
		mutex_unlock(&processing_lock[domid]);

		return false;
	}

	/* Keep track of this instance - Help to maintain the consistency during processing in case
	 * of ME swap within a same slot.
	 */
	vdev_id[domid] = vweather.vdev[domid]->id;

	processing_count[domid]++;

	mutex_unlock(&processing_count_lock[domid]);

	/* At this point, the frontend has not been closed and may be in a transient state
	 * before getting connected. We can wait until it becomes connected.
	 *
	 * If a first call to processing_start() has succeeded, subsequent calls to this function will never lead
	 * to a wait_for_completion as below since the frontend will not be able to disconnect itself (through
	 * suspend for example). The function can therefore be safely called many times (by multiple threads).
	 */

	if (!__connected[domid])
		wait_for_completion(&connected_sync[domid]);

	return true;
}

static void processing_end(domid_t domid) {

	mutex_lock(&processing_count_lock[domid]);

	processing_count[domid]--;

	if (processing_count[domid] == 0)
		mutex_unlock(&processing_lock[domid]);

	mutex_unlock(&processing_count_lock[domid]);
}


bool vweather_start(domid_t domid) {
	return processing_start(domid);
}

void vweather_end(domid_t domid) {
	processing_end(domid);
}

bool vweather_is_connected(domid_t domid) {
	return __connected[domid];
}

/**
 * Setup the shared buffer.
 */
static int setup_shared_buffer(struct vbus_device *dev) {
	vweather_shared_buffer_t *weather_buffer = &vweather.weather_buffers[dev->otherend_id];

	/* Shared data buffer */

	vbus_gather(VBT_NIL, dev->otherend, "data-pfn", "%u", &weather_buffer->pfn, NULL);

	DBG(VWEATHER_PREFIX "Backend: Shared data pfn=%08x\n", weather_buffer->pfn);

	/* The pages allocated by the ME have to be contiguous */
	weather_buffer->data = (unsigned char *) __arm_ioremap(weather_buffer->pfn << PAGE_SHIFT, VWEATHER_DATA_SIZE, MT_MEMORY_RWX_NONCACHED);

	BUG_ON(!weather_buffer->data);

	DBG(VWEATHER_PREFIX "Backend: Shared data mapped: %08x\n", (unsigned int) weather_buffer->data);

	return 0;
}

/**
 * Setup the notification.
 */
static void setup_notification(struct vbus_device *dev) {
	int res, evtchn;

	/* Audio codec interrupt */

	vbus_gather(VBT_NIL, dev->otherend, "data_update-evtchn", "%u", &evtchn, NULL);

	res = bind_interdomain_evtchn_to_virqhandler(dev->otherend_id, evtchn, NULL, NULL, 0, VWEATHER_NAME "-data_update", dev);
	BUG_ON(res < 0);

	vweather.update_notifications[dev->otherend_id].irq = res;
}

/*
 * Entry point to this code when a new device is created.  Allocate the basic
 * structures, and watch the store waiting for the hotplug scripts to tell us
 * the device's physical major and minor numbers.
 */
static int __vweather_probe(struct vbus_device *dev, const struct vbus_device_id *id) {

	mutex_lock(&processing_count_lock[dev->otherend_id]);
	vweather.vdev[dev->otherend_id] = dev;
	mutex_unlock(&processing_count_lock[dev->otherend_id]);

	vweather_probe(dev);

	return 0;
}

/**
 * State machine by the backend's side.
 */
static void frontend_changed(struct vbus_device *dev, enum vbus_state frontend_state) {

	switch (frontend_state) {

	case VbusStateInitialised:
	case VbusStateReconfigured:
		BUG_ON(__connected[dev->otherend_id]);
		setup_shared_buffer(dev);
		setup_notification(dev);
		vweather_reconfigured(dev);
		
		mutex_unlock(&processing_lock[dev->otherend_id]);

		break;

	case VbusStateConnected:
		vweather_connected(dev);

		__connected[dev->otherend_id] = true;

		complete(&connected_sync[dev->otherend_id]);
		break;

	case VbusStateClosing:
	
		BUG_ON(!__connected[dev->otherend_id]);
		mutex_lock(&processing_lock[dev->otherend_id]);

		__connected[dev->otherend_id] = false;
		reinit_completion(&connected_sync[dev->otherend_id]);

		vweather_close(dev);
		
		vweather.vdev[dev->otherend_id] = NULL;

		/* Release the possible waiters on the lock so that they can pursue their work */
		mutex_unlock(&processing_lock[dev->otherend_id]);
		break;

	case VbusStateSuspended:
		break;

	case VbusStateUnknown:
	default:
		break;
	}
}

static int __vweather_suspend(struct vbus_device *dev) {

	mutex_lock(&processing_lock[dev->otherend_id]);

	__connected[dev->otherend_id] = false;
	reinit_completion(&connected_sync[dev->otherend_id]);

	vweather_suspend(dev);

	return 0;
}

static int __vweather_resume(struct vbus_device *dev) {

	/* Resume Step 1 */
	BUG_ON(__connected[dev->otherend_id]);
	
	vweather_resume(dev);

	mutex_unlock(&processing_lock[dev->otherend_id]);

	return 0;
}

static const struct vbus_device_id vweather_ids[] = {
	{ VWEATHER_NAME },
	{ "" }
};

static struct vbus_driver vweather_drv = {
	.name			= VWEATHER_NAME,
	.owner			= THIS_MODULE,
	.ids			= vweather_ids,
	.probe			= __vweather_probe,
	.otherend_changed	= frontend_changed,
	.suspend		= __vweather_suspend,
	.resume			= __vweather_resume,
};

void vweather_vbus_init(void) {
	int i;

	for (i = 0; i < MAX_DOMAINS; i++) {
		__connected[i] = false;
		processing_count[i] = 0;
		vdev_id[i] = 0;

		mutex_init(&processing_lock[i]);
		mutex_init(&processing_count_lock[i]);

		init_completion(&connected_sync[i]);
	}

	vbus_register_backend(&vweather_drv);
}
