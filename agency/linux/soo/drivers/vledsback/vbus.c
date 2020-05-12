/*
 * Copyright (C) 2018,2019 Baptiste Delporte <bonel@bonel.net>
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

#include <soo/dev/vleds.h>

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

	if (!vleds.vdev[domid] || ((vdev_id != 0) && (vleds.vdev[domid]->id != vdev_id[domid]))) {

		/* Keep track of this instance - Help to maintain the consistency during processing in case
	 	 * of ME swap within a same slot.
	 	 */
		if (vleds.vdev[domid])
			vdev_id[domid] = vleds.vdev[domid]->id;

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
	if (!vleds.vdev[domid] || ((vdev_id != 0) && (vleds.vdev[domid]->id != vdev_id[domid]))) {

		/* Keep track of this instance - Help to maintain the consistency during processing in case
		 * of ME swap within a same slot.
		 */
		if (vleds.vdev[domid])
			vdev_id[domid] = vleds.vdev[domid]->id;

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
	vdev_id[domid] = vleds.vdev[domid]->id;

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

bool vleds_start(domid_t domid) {
	return processing_start(domid);
}

void vleds_end(domid_t domid) {
	processing_end(domid);
}

bool vleds_is_connected(domid_t domid) {
	return __connected[domid];
}

/**
 * Retrieve the ring pfns, re-map and bind to the IRQ handlers.
 */
static void setup_sring(struct vbus_device *dev) {
	int res;
	unsigned long cmd_ring_ref;
	unsigned int cmd_evtchn;
	vleds_cmd_sring_t *cmd_sring;
	vleds_cmd_ring_t *cmd_ring = &vleds.cmd_rings[dev->otherend_id];

	DBG(VUIHANDLER_PREFIX "Backend: Setup rings\n");

	/* cmd_ring */

	vbus_gather(VBT_NIL, dev->otherend, "cmd_ring-ref", "%lu", &cmd_ring_ref, "cmd_ring-evtchn", "%u", &cmd_evtchn, NULL);

	res = vbus_map_ring_valloc(dev, cmd_ring_ref, (void **) &cmd_sring);
	BUG_ON(res < 0);

	SHARED_RING_INIT(cmd_sring);
	BACK_RING_INIT(&cmd_ring->ring, cmd_sring, PAGE_SIZE);

	res = bind_interdomain_evtchn_to_virqhandler(dev->otherend_id, cmd_evtchn, vleds_cmd_interrupt, NULL, 0, VLEDS_NAME "-cmd", dev);
	BUG_ON(res < 0);

	cmd_ring->irq = res;
}

/**
 * Free the rings.
 */
static void free_sring(struct vbus_device *dev) {
	vleds_cmd_ring_t *cmd_ring = &vleds.cmd_rings[dev->otherend_id];

	/* cmd_ring */

	BACK_RING_INIT(&cmd_ring->ring, cmd_ring->ring.sring, PAGE_SIZE);

	unbind_from_virqhandler(cmd_ring->irq, dev);

	vbus_unmap_ring_vfree(dev, cmd_ring->ring.sring);
	cmd_ring->ring.sring = NULL;
}

/**
 * State machine by the backend's side.
 */
static void frontend_changed(struct vbus_device *dev, enum vbus_state frontend_state) {
	switch (frontend_state) {
	case VbusStateInitialised:
	case VbusStateReconfigured:

		BUG_ON(__connected[dev->otherend_id]);

		setup_sring(dev);
		vleds_reconfigured(dev);

		mutex_unlock(&processing_lock[dev->otherend_id]);

		break;

	case VbusStateConnected:
		vleds_connected(dev);

		__connected[dev->otherend_id] = true;

		complete(&connected_sync[dev->otherend_id]);
		break;

	case VbusStateClosing:
		BUG_ON(!__connected[dev->otherend_id]);
		mutex_lock(&processing_lock[dev->otherend_id]);

		__connected[dev->otherend_id] = false;
		reinit_completion(&connected_sync[dev->otherend_id]);

		vleds_close(dev);
		free_sring(dev);
		vleds.vdev[dev->otherend_id] = NULL;

		/* Release the possible waiters on the lock so that they can pursue their work */
		mutex_unlock(&processing_lock[dev->otherend_id]);

		break;

	case VbusStateSuspended:
		break;

	case VbusStateUnknown:
	default:
		lprintk("%s - line %d: Unknown state %d (frontend) for device %s\n", __func__, __LINE__, dev->nodename, dev->state);
		BUG();
		break;
	}
}

static int __vleds_suspend(struct vbus_device *dev) {

	mutex_lock(&processing_lock[dev->otherend_id]);

	__connected[dev->otherend_id] = false;
	reinit_completion(&connected_sync[dev->otherend_id]);

	vleds_suspend(dev);

	return 0;
}

static int __vleds_resume(struct vbus_device *dev) {

	/* Resume Step 1 */
	BUG_ON(__connected[dev->otherend_id]);
	vleds_resume(dev);

	mutex_unlock(&processing_lock[dev->otherend_id]);

	return 0;
}

int __vleds_probe(struct vbus_device *dev, const struct vbus_device_id *id) {
	vleds_probe(dev);

	return 0;
}

static const struct vbus_device_id vleds_ids[] = {
	{ VLEDS_NAME },
	{ "" }
};

static struct vbus_driver vleds_drv = {
	.name			= VLEDS_NAME,
	.owner			= THIS_MODULE,
	.ids			= vleds_ids,
	.probe			= __vleds_probe,
	.otherend_changed	= frontend_changed,
	.suspend		= __vleds_suspend,
	.resume			= __vleds_resume,
};

void vleds_vbus_init(void) {
	int i;

	for (i = 0; i < MAX_DOMAINS; i++) {
		__connected[i] = false;
		processing_count[i] = 0;
		vdev_id[i] = 0;

		mutex_init(&processing_lock[i]);
		mutex_init(&processing_count_lock[i]);

		init_completion(&connected_sync[i]);
	}

	vbus_register_backend(&vleds_drv);
}
