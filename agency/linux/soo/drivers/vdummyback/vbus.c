/*
 * Copyright (C) 2015-2018 Daniel Rossier <daniel.rossier@soo.tech>
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

	if (!vdummy.vdev[domid] || ((vdev_id != 0) && (vdummy.vdev[domid]->id != vdev_id[domid]))) {

		/* Keep track of this instance - Help to maintain the consistency during processing in case
	 	 * of ME swap within a same slot.
	 	 */
		if (vdummy.vdev[domid])
			vdev_id[domid] = vdummy.vdev[domid]->id;

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
	if (!vdummy.vdev[domid] || ((vdev_id != 0) && (vdummy.vdev[domid]->id != vdev_id[domid]))) {

		/* Keep track of this instance - Help to maintain the consistency during processing in case
		 * of ME swap within a same slot.
		 */
		if (vdummy.vdev[domid])
			vdev_id[domid] = vdummy.vdev[domid]->id;

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
	vdev_id[domid] = vdummy.vdev[domid]->id;

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


bool vdummy_start(domid_t domid) {
	return processing_start(domid);
}

void vdummy_end(domid_t domid) {
	processing_end(domid);
}

bool vdummy_is_connected(domid_t domid) {
	return __connected[domid];
}

/*
 * Set up a ring (shared page & event channel) between the agency and the ME.
 */
static void setup_sring(struct vbus_device *dev) {
	int res;
	unsigned long ring_ref;
	unsigned int evtchn;
	vdummy_sring_t *sring;
	vdummy_ring_t *p_vdummy_ring;

	p_vdummy_ring = &vdummy.rings[dev->otherend_id];

	vbus_gather(VBT_NIL, dev->otherend, "ring-ref", "%lu", &ring_ref, "ring-evtchn", "%u", &evtchn, NULL);

	DBG("BE: ring-ref=%u, event-channel=%u\n", ring_ref, evtchn);

	res = vbus_map_ring_valloc(dev, ring_ref, (void **) &sring);
	BUG_ON(res < 0);

	SHARED_RING_INIT(sring);
	BACK_RING_INIT(&p_vdummy_ring->ring, sring, PAGE_SIZE);

	res = bind_interdomain_evtchn_to_virqhandler(dev->otherend_id, evtchn, vdummy_interrupt, NULL, 0, VDUMMY_NAME "-backend", dev);

	BUG_ON(res < 0);

	p_vdummy_ring->irq = res;
}

/*
 * Free the ring and unbind evtchn.
 */
static void free_sring(struct vbus_device *dev) {
	vdummy_ring_t *p_vdummy_ring = &vdummy.rings[dev->otherend_id];

	/* Prepare to empty all buffers */
	BACK_RING_INIT(&p_vdummy_ring->ring, (&p_vdummy_ring->ring)->sring, PAGE_SIZE);

	unbind_from_virqhandler(p_vdummy_ring->irq, dev);

	vbus_unmap_ring_vfree(dev, p_vdummy_ring->ring.sring);
	p_vdummy_ring->ring.sring = NULL;
}

/*
 * Entry point to this code when a new device is created.  Allocate the basic
 * structures, and watch the store waiting for the hotplug scripts to tell us
 * the device's physical major and minor numbers.
 */
static int __vdummy_probe(struct vbus_device *dev, const struct vbus_device_id *id) {
	DBG("%s: SOO dummy driver for testing\n", __func__);

	mutex_lock(&processing_count_lock[dev->otherend_id]);
	vdummy.vdev[dev->otherend_id] = dev;
	mutex_unlock(&processing_count_lock[dev->otherend_id]);

	vdummy_probe(dev);

	return 0;
}

/*
 * Callback received when the frontend's state changes.
 */
static void frontend_changed(struct vbus_device *dev, enum vbus_state frontend_state) {

	DBG("%s\n", vbus_strstate(frontend_state));

	switch (frontend_state) {

	case VbusStateInitialised:
	case VbusStateReconfigured:
		DBG0("SOO Dummy: reconfigured...\n");

		BUG_ON(__connected[dev->otherend_id]);

		setup_sring(dev);

		vdummy_reconfigured(dev);

		mutex_unlock(&processing_lock[dev->otherend_id]);

		break;

	case VbusStateConnected:
		DBG0("vdummy frontend connected, all right.\n");
		vdummy_connected(dev);

		__connected[dev->otherend_id] = true;

		complete(&connected_sync[dev->otherend_id]);
		break;

	case VbusStateClosing:
		DBG0("Got that the virtual dummy frontend now closing...\n");

		BUG_ON(!__connected[dev->otherend_id]);
		mutex_lock(&processing_lock[dev->otherend_id]);

		__connected[dev->otherend_id] = false;
		reinit_completion(&connected_sync[dev->otherend_id]);

		vdummy_close(dev);
		free_sring(dev);

		vdummy.vdev[dev->otherend_id] = NULL;

		/* Release the possible waiters on the lock so that they can pursue their work */
		mutex_unlock(&processing_lock[dev->otherend_id]);

		break;

	case VbusStateSuspended:
		/* Suspend Step 3 */
		DBG("frontend_suspended: %s ...\n", dev->nodename);

		break;

	case VbusStateUnknown:
	default:
		break;
	}
}

static int __vdummy_suspend(struct vbus_device *dev) {

	DBG0("vdummy_suspend: wait for frontend now...\n");

	mutex_lock(&processing_lock[dev->otherend_id]);

	__connected[dev->otherend_id] = false;
	reinit_completion(&connected_sync[dev->otherend_id]);

	vdummy_suspend(dev);

	return 0;
}

static int __vdummy_resume(struct vbus_device *dev) {
	/* Resume Step 1 */
#warning workaround
	//BUG_ON(__connected[dev->otherend_id]);

	DBG("backend resuming: %s ...\n", dev->nodename);
	vdummy_resume(dev);

	mutex_unlock(&processing_lock[dev->otherend_id]);

	return 0;
}

/* ** Driver Registration ** */

static const struct vbus_device_id vdummy_ids[] = {
	{ VDUMMY_NAME },
	{ "" }
};

static struct vbus_driver vdummy_drv = {
	.name = VDUMMY_NAME,
	.owner = THIS_MODULE,
	.ids = vdummy_ids,
	.probe = __vdummy_probe,
	.otherend_changed = frontend_changed,
	.suspend = __vdummy_suspend,
	.resume = __vdummy_resume,
};

void vdummy_vbus_init(void) {
	int i;

	for (i = 0; i < MAX_DOMAINS; i++) {
		__connected[i] = false;
		processing_count[i] = 0;
		vdev_id[i] = 0;

		mutex_init(&processing_lock[i]);
		mutex_init(&processing_count_lock[i]);

		init_completion(&connected_sync[i]);
	}

	vbus_register_backend(&vdummy_drv);
}
