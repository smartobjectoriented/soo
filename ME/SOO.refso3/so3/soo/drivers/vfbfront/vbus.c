/*
 * Copyright (C) 2020 Nikolaos Garanis <nikolaos.garanis@heig-vd.ch>
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

#if 1
#define DEBUG
#endif

#include <mutex.h>

#include <asm/mmu.h>

#include <soo/gnttab.h>
#include <soo/evtchn.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/console.h>
#include <soo/debug.h>

#include <soo/dev/vfb.h>

/* Protection against shutdown (or other) */
static mutex_t processing_lock;
static uint32_t processing_count;
static struct mutex processing_count_lock;

/*
 * Boolean that tells if the interface is in the Connected state.
 * In this case, and only in this case, the interface can be used.
 */
static volatile bool __connected;
static struct completion connected_sync;

/*
 * The functions processing_start() and processing_end() are used to
 * prevent pre-migration suspending actions.
 * The functions can be called in multiple execution context (threads).
 *
 * Assumptions:
 * - If the frontend is suspended during processing_start, it is for a short time, until the FE gets connected.
 * - If the frontend is suspended and a shutdown operation is in progress, the ME will disappear! Therefore,
 *   we do not take care about ongoing activities. All will disappear...
 *
 */
static void processing_start(void) {

	mutex_lock(&processing_count_lock);

	if (processing_count == 0)
		mutex_lock(&processing_lock);

	processing_count++;

	mutex_unlock(&processing_count_lock);

	/* At this point, the frontend has not been closed and may be in a transient state
	 * before getting connected. We can wait until it becomes connected.
	 *
	 * If a first call to processing_start() has succeeded, subsequent calls to this function will never lead
	 * to a wait_for_completion as below since the frontend will not be able to disconnect itself (through
	 * suspend for example). The function can therefore be safely called many times (by multiple threads).
	 */

	if (!__connected)
		wait_for_completion(&connected_sync);

}

static void processing_end(void) {

	mutex_lock(&processing_count_lock);

	processing_count--;

	if (processing_count == 0)
		mutex_unlock(&processing_lock);

	mutex_unlock(&processing_count_lock);
}

void vfb_start(void) {
	processing_start();
}

void vfb_end(void) {
	processing_end();
}

bool vfb_is_connected(void) {
	return __connected;
}

/*
 * When the ME begins its execution, it needs to set up an event channel associated to this device.
 * But after migration, the ME can keep its previous allocated event channel and just
 * push it again onto vbstore.
 *
 * The free page allocated to the ring can also be kept the same.
 */
static int setup_sring(struct vbus_device *dev, bool initall) {
	int res;
	unsigned int evtchn;
	vfb_sring_t *sring;
	struct vbus_transaction vbt;

	if (dev->state == VbusStateConnected)
		return 0;

	DBG("Frontend: Setup ring\n");

	/* Prepare to set up the ring. */

	vfb.ring_ref = GRANT_INVALID_REF;

	if (initall) {
		/* Allocate an event channel associated to the ring */
		res = vbus_alloc_evtchn(dev, &evtchn);
		BUG_ON(res);

		res = bind_evtchn_to_irq_handler(evtchn, vfb_interrupt, NULL, &vfb);
		if (res <= 0) {
			lprintk("%s - line %d: Binding event channel failed for device %s\n", __func__, __LINE__, dev->nodename);
			BUG();
		}

		vfb.evtchn = evtchn;
		vfb.irq = res;

		/* Allocate a shared page for the ring */
		sring = (vfb_sring_t *) get_free_vpage();
		if (!sring) {
			lprintk("%s - line %d: Allocating shared ring failed for device %s\n", __func__, __LINE__, dev->nodename);
			BUG();
		}

		SHARED_RING_INIT(sring);
		FRONT_RING_INIT(&vfb.ring, sring, PAGE_SIZE);
	} else {
		SHARED_RING_INIT(vfb.ring.sring);
		FRONT_RING_INIT(&vfb.ring, vfb.ring.sring, PAGE_SIZE);
	}

	/* Prepare the shared to page to be visible on the other end */

	res = vbus_grant_ring(dev, phys_to_pfn(virt_to_phys_pt((uint32_t) vfb.ring.sring)));
	if (res < 0)
		BUG();

	vfb.ring_ref = res;

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, dev->nodename, "ring-ref", "%u", vfb.ring_ref);
	vbus_printf(vbt, dev->nodename, "ring-evtchn", "%u", vfb.evtchn);

	vbus_transaction_end(vbt);

	return 0;
}

/**
 * Free the ring and deallocate the proper data.
 */
static void free_sring(void) {

	/* Free resources associated with old device channel. */
	if (vfb.ring_ref != GRANT_INVALID_REF) {
		gnttab_end_foreign_access(vfb.ring_ref);
		free_vpage((uint32_t) vfb.ring.sring);

		vfb.ring_ref = GRANT_INVALID_REF;
		vfb.ring.sring = NULL;
	}

	if (vfb.irq)
		unbind_from_irqhandler(vfb.irq);

	vfb.irq = 0;

}

/**
 * Post-migration re-configuration.
 */
static void postmig_setup_sring(struct vbus_device *dev) {

	/* The shared page already exists */
	/* Re-init */

	gnttab_end_foreign_access_ref(vfb.ring_ref);

	setup_sring(dev, false);
}

/**
 * Entry point to this code when a new device is created.  Allocate the basic
 * structures and the ring buffer for communication with the backend, and
 * inform the backend of the appropriate details for those.  Switch to
 * Initialised state.
 *
 */
static int __vfb_probe(struct vbus_device *dev, const struct vbus_device_id *id) {

	vfb.dev = dev;

	DBG0("SOO Virtual dummy frontend driver\n");

	DBG("%s: allocate a shared page and set up the ring...\n", __func__);

	setup_sring(dev, true);

	vfb_probe();

	__connected = false;

	return 0;
}

/**
 * State machine by the frontend's side.
 */
static void backend_changed(struct vbus_device *dev, enum vbus_state backend_state) {
	DBG("SOO vfb frontend, backend %s changed its state to %d.\n", dev->nodename, backend_state);

	switch (backend_state) {

	case VbusStateReconfiguring:
		BUG_ON(__connected);

		postmig_setup_sring(dev);
		vfb_reconfiguring();

		mutex_unlock(&processing_lock);
		break;

	case VbusStateClosed:
		BUG_ON(__connected);

		vfb_closed();
		free_sring();

		/* The processing_lock is kept forever, since it has to keep all processing activities suspended.
		 * Until the ME disappears...
		 */

		break;

	case VbusStateSuspending:
		/* Suspend Step 2 */
		DBG("Got that backend %s suspending now ...\n", dev->nodename);
		mutex_lock(&processing_lock);

		__connected = false;
		reinit_completion(&connected_sync);

		vfb_suspend();

		break;

	case VbusStateResuming:
		/* Resume Step 2 */
		DBG("Got that backend %s resuming now.....\n", dev->nodename);
		BUG_ON(__connected);

		vfb_resume();

		mutex_unlock(&processing_lock);
		break;

	case VbusStateConnected:
		vfb_connected();

		/* Now, the FE is considered as connected */
		__connected = true;

		complete(&connected_sync);
		break;

	case VbusStateUnknown:
	default:
		lprintk("%s - line %d: Unknown state %d (backend) for device %s\n", __func__, __LINE__, backend_state, dev->nodename);
		BUG();
	}
}

int __vfb_shutdown(struct vbus_device *dev) {

	/*
	 * Ensure all frontend processing is in a stable state.
	 * The lock will be never released once acquired.
	 * The frontend will be never be in a shutdown procedure before the end of resuming operation.
	 * It's mainly the case of a force_terminate callback which may intervene only after the frontend
	 * gets connected (not before).
	 */

	mutex_lock(&processing_lock);

	__connected = false;
	reinit_completion(&connected_sync);

	vfb_shutdown();

	return 0;
}

static const struct vbus_device_id vfb_ids[] = {
	{ VFB_NAME },
	{ "" }
};

static struct vbus_driver vfb_drv = {
	.name = VFB_NAME,
	.ids = vfb_ids,
	.probe = __vfb_probe,
	.shutdown = __vfb_shutdown,
	.otherend_changed = backend_changed,
};

void vfb_vbus_init(void)
{
	vbus_register_frontend(&vfb_drv);

	mutex_init(&processing_lock);
	mutex_init(&processing_count_lock);

	processing_count = 0;

	init_completion(&connected_sync);
	DBG("vfb bus init ok\n");
}
