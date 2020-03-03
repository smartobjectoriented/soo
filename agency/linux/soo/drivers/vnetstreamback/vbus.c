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

#include <soolink/transceiver.h>

#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/uapi/debug.h>

#include "common.h"

/* Protection against shutdown (or other) */
static rtdm_mutex_t processing_lock[MAX_DOMAINS];
static rtdm_mutex_t processing_count_lock[MAX_DOMAINS];
static volatile uint32_t processing_count[MAX_DOMAINS];

static volatile bool __connected[MAX_DOMAINS];
static rtdm_event_t connected_sync[MAX_DOMAINS];
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

	rtdm_mutex_lock(&processing_count_lock[domid]);

	if (!vnetstream.vdev[domid] || ((vdev_id != 0) && (vnetstream.vdev[domid]->id != vdev_id[domid]))) {
		
		/* Keep track of this instance - Help to maintain the consistency during processing in case
	 	 * of ME swap within a same slot.
	 	 */
		if (vnetstream.vdev[domid])
			vdev_id[domid] = vnetstream.vdev[domid]->id;

		rtdm_mutex_unlock(&processing_count_lock[domid]);

		return false;

	}

	if (processing_count[domid] == 0)
		rtdm_mutex_lock(&processing_lock[domid]);

	/* In the meanwhile, if the backend has been closed, it does not make sense to
	 * go ahead with activities. This situation may happen if we have been suspended for the processing_lock.
	 * We therefore are alone
	 * The caller must handle this too.
	 */
	if (!vnetstream.vdev[domid] || ((vdev_id != 0) && (vnetstream.vdev[domid]->id != vdev_id[domid]))) {
		
		/* Keep track of this instance - Help to maintain the consistency during processing in case
		 * of ME swap within a same slot.
		 */
		if (vnetstream.vdev[domid])
			vdev_id[domid] = vnetstream.vdev[domid]->id;

		/* The processing_count has value 0 in all case, since a value greater than 0 means
		 * any other operations must wait for processing_lock, i.e. processing_count is to 0.
		 * The vdev instance cannot be altered from its existence point of view during processing.
		 */
		rtdm_mutex_unlock(&processing_count_lock[domid]);
		rtdm_mutex_unlock(&processing_lock[domid]);

		return false;
	}

	/* Keep track of this instance - Help to maintain the consistency during processing in case
	 * of ME swap within a same slot.
	 */
	vdev_id[domid] = vnetstream.vdev[domid]->id;

	processing_count[domid]++;

	rtdm_mutex_unlock(&processing_count_lock[domid]);

	/* At this point, the frontend has not been closed and may be in a transient state
	 * before getting connected. We can wait until it becomes connected.
	 *
	 * If a first call to processing_start() has succeeded, subsequent calls to this function will never lead
	 * to a wait_for_completion as below since the frontend will not be able to disconnect itself (through
	 * suspend for example). The function can therefore be safely called many times (by multiple threads).
	 */

	if (!__connected[domid])
		rtdm_event_wait(&connected_sync[domid]);

	return true;
}

static void processing_end(domid_t domid) {

	rtdm_mutex_lock(&processing_count_lock[domid]);

	processing_count[domid]--;

	if (processing_count[domid] == 0)
		rtdm_mutex_unlock(&processing_lock[domid]);

	rtdm_mutex_unlock(&processing_count_lock[domid]);
}


bool vnetstream_start(domid_t domid) {
	return processing_start(domid);
}

void vnetstream_end(domid_t domid) {
	processing_end(domid);
}

bool vnetstream_is_connected(domid_t domid) {
	return __connected[domid];
}


/**
 * For each ring, retrieve the pfn, re-map and bind to the proper IRQ handler.
 */
static void setup_srings(struct vbus_device *dev) {
	int res;
	unsigned long cmd_ring_ref, tx_ring_ref, rx_ring_ref;
	unsigned int cmd_evtchn, tx_evtchn, rx_evtchn;
	vnetstream_cmd_sring_t *cmd_sring;
	vnetstream_cmd_ring_t *cmd_ring = &vnetstream.cmd_rings[dev->otherend_id];
	vnetstream_tx_sring_t *tx_sring;
	vnetstream_tx_ring_t *tx_ring = &vnetstream.tx_rings[dev->otherend_id];
	vnetstream_rx_sring_t *rx_sring;
	vnetstream_rx_ring_t *rx_ring = &vnetstream.rx_rings[dev->otherend_id];

	DBG(VNETSTREAM_PREFIX "Backend: Setup rings\n");

	/* cmd_ring */

	rtdm_vbus_gather(VBT_NIL, dev->otherend, "cmd_ring-ref", "%lu", &cmd_ring_ref, "cmd_ring-evtchn", "%u", &cmd_evtchn, NULL);

	res = vbus_map_ring_valloc(dev, cmd_ring_ref, (void **) &cmd_sring);
	BUG_ON(res < 0);

	SHARED_RING_INIT(cmd_sring);
	BACK_RING_INIT(&cmd_ring->ring, cmd_sring, PAGE_SIZE);

	res = rtdm_bind_interdomain_evtchn_to_virqhandler(&cmd_ring->irq_handle, dev->otherend_id, cmd_evtchn, vnetstream_cmd_interrupt, 0, VNETSTREAM_NAME "-cmd", dev);
	BUG_ON(res < 0);

	/* tx_ring */

	rtdm_vbus_gather(VBT_NIL, dev->otherend, "tx_ring-ref", "%lu", &tx_ring_ref, "tx_ring-evtchn", "%u", &tx_evtchn, NULL);

	res = vbus_map_ring_valloc(dev, tx_ring_ref, (void **) &tx_sring);
	BUG_ON(res < 0);

	SHARED_RING_INIT(tx_sring);
	BACK_RING_INIT(&tx_ring->ring, tx_sring, PAGE_SIZE);

	res = rtdm_bind_interdomain_evtchn_to_virqhandler(&tx_ring->irq_handle, dev->otherend_id, tx_evtchn, vnetstream_tx_interrupt, 0, VNETSTREAM_NAME "-tx", dev);
	BUG_ON(res < 0);

	/* rx_ring */

	rtdm_vbus_gather(VBT_NIL, dev->otherend,
				"rx_ring-ref", "%lu", &rx_ring_ref,
				"rx_ring-evtchn", "%u", &rx_evtchn,
				NULL);

	res = vbus_map_ring_valloc(dev, rx_ring_ref, (void **) &rx_sring);
	BUG_ON(res < 0);

	SHARED_RING_INIT(rx_sring);
	BACK_RING_INIT(&rx_ring->ring, rx_sring, PAGE_SIZE);

	res = rtdm_bind_interdomain_evtchn_to_virqhandler(&rx_ring->irq_handle, dev->otherend_id, rx_evtchn, vnetstream_rx_interrupt, 0, VNETSTREAM_NAME "-rx", dev);
	BUG_ON(res < 0);
}

/**
 * Free the rings.
 */
static void free_srings(struct vbus_device *dev) {
	vnetstream_cmd_ring_t *cmd_ring = &vnetstream.cmd_rings[dev->otherend_id];
	vnetstream_tx_ring_t *tx_ring = &vnetstream.tx_rings[dev->otherend_id];
	vnetstream_rx_ring_t *rx_ring = &vnetstream.rx_rings[dev->otherend_id];

	DBG(VNETSTREAM_PREFIX "Backend: Free rings\n");

	/* cmd_ring */

	BACK_RING_INIT(&cmd_ring->ring, cmd_ring->ring.sring, PAGE_SIZE);

	rtdm_unbind_from_virqhandler(&cmd_ring->irq_handle);

	vbus_unmap_ring_vfree(dev, cmd_ring->ring.sring);
	cmd_ring->ring.sring = NULL;

	/* tx_ring */

	BACK_RING_INIT(&tx_ring->ring, tx_ring->ring.sring, PAGE_SIZE);

	rtdm_unbind_from_virqhandler(&tx_ring->irq_handle);

	vbus_unmap_ring_vfree(dev, tx_ring->ring.sring);
	tx_ring->ring.sring = NULL;

	/* rx_ring */

	BACK_RING_INIT(&rx_ring->ring, rx_ring->ring.sring, PAGE_SIZE);

	rtdm_unbind_from_virqhandler(&rx_ring->irq_handle);

	vbus_unmap_ring_vfree(dev, rx_ring->ring.sring);
	rx_ring->ring.sring = NULL;
}

/**
 * Setup the shared buffer.
 */
void vnetstream_setup_shared_buffer(struct vbus_device *dev) {
	vnetstream_shared_buffer_t *txrx_buffer = &vnetstream.txrx_buffers[dev->otherend_id];

	/* TXRX shared data buffer */

	rtdm_vbus_gather(VBT_NIL, dev->otherend, "data-pfn", "%u", &txrx_buffer->pfn, NULL);

	DBG(VNETSTREAM_PREFIX "Backend: TXRX shared data pfn=%08x\n", txrx_buffer->pfn);

	/* The pages allocated by the ME have to be contiguous */
	txrx_buffer->data = (unsigned char *) __arm_ioremap(txrx_buffer->pfn << PAGE_SHIFT, VNETSTREAM_MAX_SOO * (sizeof(netstream_transceiver_packet_t) + vnetstream_packet_size), MT_MEMORY_RWX_NONCACHED);

	BUG_ON(!txrx_buffer->data);

	DBG(VNETSTREAM_PREFIX "Backend: TXRX shared data mapped: %08x\n", (unsigned int) txrx_buffer->data);

}

/*
 * Entry point to this code when a new device is created.  Allocate the basic
 * structures, and watch the store waiting for the hotplug scripts to tell us
 * the device's physical major and minor numbers.
 */
static int __vnetstream_probe(struct vbus_device *dev, const struct vbus_device_id *id) {

	rtdm_mutex_lock(&processing_count_lock[dev->otherend_id]);
	vnetstream.vdev[dev->otherend_id] = dev;
	rtdm_mutex_unlock(&processing_count_lock[dev->otherend_id]);

	vnetstream_probe(dev);

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
		vnetstream_reconfigured(dev);

		rtdm_mutex_unlock(&processing_lock[dev->otherend_id]);
		break;

	case VbusStateConnected:
		vnetstream_connected(dev);
		__connected[dev->otherend_id] = true;

		rtdm_event_signal(&connected_sync[dev->otherend_id]);
		break;

	case VbusStateClosing:
		BUG_ON(!__connected[dev->otherend_id]);
		rtdm_mutex_lock(&processing_lock[dev->otherend_id]);

		__connected[dev->otherend_id] = false;

		rtdm_event_clear(&connected_sync[dev->otherend_id]);

		vnetstream_close(dev);
		free_srings(dev);
	
		vnetstream.vdev[dev->otherend_id] = NULL;

		/* Release the possible waiters on the lock so that they can pursue their work */
		rtdm_mutex_unlock(&processing_lock[dev->otherend_id]);
		break;

	case VbusStateSuspended:
		break;

	case VbusStateUnknown:
	default:
		break;
	}
}

static int __vnetstream_suspend(struct vbus_device *dev) {
	
	rtdm_mutex_lock(&processing_lock[dev->otherend_id]);

	__connected[dev->otherend_id] = false;

	rtdm_event_clear(&connected_sync[dev->otherend_id]);

	vnetstream_suspend(dev);

	return 0;
}

static int __vnetstream_resume(struct vbus_device *dev) {
	/* Resume Step 1 */
	BUG_ON(__connected[dev->otherend_id]);

	vnetstream_resume(dev);

	rtdm_mutex_unlock(&processing_lock[dev->otherend_id]);

	return 0;
}

static const struct vbus_device_id vnetstream_ids[] = {
	{ VNETSTREAM_NAME },
	{ "" }
};

static struct vbus_driver vnetstream_drv = {
	.name			= VNETSTREAM_NAME,
	.owner			= THIS_MODULE,
	.ids			= vnetstream_ids,
	.probe			= __vnetstream_probe,
	.otherend_changed	= frontend_changed,
	.suspend		= __vnetstream_suspend,
	.resume			= __vnetstream_resume,
};

void vnetstream_vbus_init(void) {
	int i;

	for (i = 0; i < MAX_DOMAINS; i++) {
		__connected[i] = false;
		processing_count[i] = 0;
		vdev_id[i] = 0;

		rtdm_mutex_init(&processing_lock[i]);
		rtdm_mutex_init(&processing_count_lock[i]);

		rtdm_event_init(&connected_sync[i], 0);
	}

	vbus_register_backend(&vnetstream_drv);
}
