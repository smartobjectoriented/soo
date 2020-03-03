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

	if (!vuihandler.vdev[domid] || ((vdev_id != 0) && (vuihandler.vdev[domid]->id != vdev_id[domid]))) {

		/* Keep track of this instance - Help to maintain the consistency during processing in case
	 	 * of ME swap within a same slot.
	 	 */
		if (vuihandler.vdev[domid])
			vdev_id[domid] = vuihandler.vdev[domid]->id;

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
	if (!vuihandler.vdev[domid] || ((vdev_id != 0) && (vuihandler.vdev[domid]->id != vdev_id[domid]))) {

		/* Keep track of this instance - Help to maintain the consistency during processing in case
		 * of ME swap within a same slot.
		 */
		if (vuihandler.vdev[domid])
			vdev_id[domid] = vuihandler.vdev[domid]->id;

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
	vdev_id[domid] = vuihandler.vdev[domid]->id;

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


bool vuihandler_start(domid_t domid) {
	return processing_start(domid);
}

void vuihandler_end(domid_t domid) {
	processing_end(domid);
}

bool vuihandler_is_connected(domid_t domid) {
	return __connected[domid];
}

/**
 * For each ring, retrieve the pfn, re-map and bind to the proper IRQ handler.
 */
static void setup_srings(struct vbus_device *dev) {
	int res;
	unsigned long tx_ring_ref, rx_ring_ref;
	unsigned int tx_evtchn, rx_evtchn;
	vuihandler_tx_sring_t *tx_sring;
	vuihandler_tx_ring_t *tx_ring = &vuihandler.tx_rings[dev->otherend_id];
	vuihandler_rx_sring_t *rx_sring;
	vuihandler_rx_ring_t *rx_ring = &vuihandler.rx_rings[dev->otherend_id];

	DBG(VUIHANDLER_PREFIX "Backend: Setup rings\n");

	/* tx_ring */

	vbus_gather(VBT_NIL, dev->otherend, "tx_ring-ref", "%lu", &tx_ring_ref, "tx_ring-evtchn", "%u", &tx_evtchn, NULL);

	res = vbus_map_ring_valloc(dev, tx_ring_ref, (void **) &tx_sring);
	BUG_ON(res < 0);

	SHARED_RING_INIT(tx_sring);
	BACK_RING_INIT(&tx_ring->ring, tx_sring, PAGE_SIZE);

	res = bind_interdomain_evtchn_to_virqhandler(dev->otherend_id, tx_evtchn, vuihandler_tx_interrupt, NULL, 0, VUIHANDLER_NAME "-tx", dev);
	BUG_ON(res < 0);

	tx_ring->irq = res;

	/* rx_ring */

	vbus_gather(VBT_NIL, dev->otherend, "rx_ring-ref", "%lu", &rx_ring_ref, "rx_ring-evtchn", "%u", &rx_evtchn, NULL);

	res = vbus_map_ring_valloc(dev, rx_ring_ref, (void **) &rx_sring);
	BUG_ON(res < 0);

	SHARED_RING_INIT(rx_sring);
	BACK_RING_INIT(&rx_ring->ring, rx_sring, PAGE_SIZE);

	res = bind_interdomain_evtchn_to_virqhandler(dev->otherend_id, rx_evtchn, vuihandler_rx_interrupt, NULL, 0, VUIHANDLER_NAME "-rx", dev);
	BUG_ON(res < 0);

	rx_ring->irq = res;
}

/**
 * Free the rings.
 */
static void free_srings(struct vbus_device *dev) {
	vuihandler_tx_ring_t *tx_ring = &vuihandler.tx_rings[dev->otherend_id];
	vuihandler_rx_ring_t *rx_ring = &vuihandler.rx_rings[dev->otherend_id];

	/* tx_ring */

	BACK_RING_INIT(&tx_ring->ring, tx_ring->ring.sring, PAGE_SIZE);

	unbind_from_virqhandler(tx_ring->irq, dev);

	vbus_unmap_ring_vfree(dev, tx_ring->ring.sring);
	tx_ring->ring.sring = NULL;

	/* rx_ring */

	BACK_RING_INIT(&rx_ring->ring, rx_ring->ring.sring, PAGE_SIZE);

	unbind_from_virqhandler(rx_ring->irq, dev);

	vbus_unmap_ring_vfree(dev, rx_ring->ring.sring);
	rx_ring->ring.sring = NULL;
}

/**
 * Setup the shared buffers.
 */
static int setup_shared_buffers(struct vbus_device *dev) {
	vuihandler_shared_buffer_t *tx_buffer = &vuihandler.tx_buffers[dev->otherend_id];
	vuihandler_shared_buffer_t *rx_buffer = &vuihandler.rx_buffers[dev->otherend_id];

	/* TX shared data buffer */

	vbus_gather(VBT_NIL, dev->otherend, "tx-pfn", "%u", &tx_buffer->pfn, NULL);

	DBG(VUIHANDLER_PREFIX "Backend: TX shared data pfn=%08x\n", tx_buffer->pfn);

	/* The pages allocated by the ME have to be contiguous */
	tx_buffer->data = (unsigned char *) __arm_ioremap(tx_buffer->pfn << PAGE_SHIFT, VUIHANDLER_BUFFER_SIZE, MT_MEMORY_RWX_NONCACHED);

	BUG_ON(!tx_buffer->data);

	DBG(VUIHANDLER_PREFIX "Backend: TX shared data mapped: %08x\n", (unsigned int) tx_buffer->data);

	/* RX shared data buffer */

	vbus_gather(VBT_NIL, dev->otherend, "rx-pfn", "%u", &rx_buffer->pfn, NULL);

	DBG(VUIHANDLER_PREFIX "Backend: RX shared data pfn=%08x\n", rx_buffer->pfn);

	/* The pages allocated by the ME have to be contiguous */
	rx_buffer->data = (unsigned char *) __arm_ioremap(rx_buffer->pfn << PAGE_SHIFT, VUIHANDLER_BUFFER_SIZE, MT_MEMORY_RWX_NONCACHED);

	BUG_ON(!rx_buffer->data);

	DBG(VUIHANDLER_PREFIX "Backend: RX shared data mapped: %08x\n", (unsigned int) rx_buffer->data);

	return 0;
}

/*
 * Entry point to this code when a new device is created.  Allocate the basic
 * structures, and watch the store waiting for the hotplug scripts to tell us
 * the device's physical major and minor numbers.
 */
static int __vuihandler_probe(struct vbus_device *dev, const struct vbus_device_id *id) {
	DBG("%s: SOO dummy driver for testing\n", __func__);

	mutex_lock(&processing_count_lock[dev->otherend_id]);
	vuihandler.vdev[dev->otherend_id] = dev;
	mutex_unlock(&processing_count_lock[dev->otherend_id]);

	vuihandler_probe(dev);

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

		setup_srings(dev);
		setup_shared_buffers(dev);
		vuihandler_reconfigured(dev);
		break;

	case VbusStateConnected:
		vuihandler_connected(dev);

		__connected[dev->otherend_id] = true;

		complete(&connected_sync[dev->otherend_id]);
		break;

	case VbusStateClosing:
		
		BUG_ON(!__connected[dev->otherend_id]);
		
		mutex_lock(&processing_lock[dev->otherend_id]);

		__connected[dev->otherend_id] = false;
		reinit_completion(&connected_sync[dev->otherend_id]);

		vuihandler_close(dev);
		free_srings(dev);

		vuihandler.vdev[dev->otherend_id] = NULL;

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

static int __vuihandler_suspend(struct vbus_device *dev) {

	mutex_lock(&processing_lock[dev->otherend_id]);

	__connected[dev->otherend_id] = false;
	reinit_completion(&connected_sync[dev->otherend_id]);

	vuihandler_suspend(dev);

	return 0;
}

static int __vuihandler_resume(struct vbus_device *dev) {
	
#warning workaround
	//BUG_ON(__connected[dev->otherend_id]);
	
	vuihandler_resume(dev);

	mutex_unlock(&processing_lock[dev->otherend_id]);

	return 0;
}

static const struct vbus_device_id vuihandler_ids[] = {
	{ VUIHANDLER_NAME },
	{ "" }
};

static struct vbus_driver vuihandler_drv = {
	.name			= VUIHANDLER_NAME,
	.owner			= THIS_MODULE,
	.ids			= vuihandler_ids,
	.probe			= __vuihandler_probe,
	.otherend_changed	= frontend_changed,
	.suspend		= __vuihandler_suspend,
	.resume			= __vuihandler_resume,
};

void vuihandler_vbus_init(void) {
	int i;

	for (i = 0; i < MAX_DOMAINS; i++) {
		__connected[i] = false;
		processing_count[i] = 0;
		vdev_id[i] = 0;

		mutex_init(&processing_lock[i]);
		mutex_init(&processing_count_lock[i]);

		init_completion(&connected_sync[i]);
	}

	vbus_register_backend(&vuihandler_drv);

	/* Initialize the connected application ME SPID to NULL */
	vuihandler_update_spid_vbstore(vuihandler_null_spid);
}
