/*
 * Copyright (C) 2018-2019 Daniel Rossier <daniel.rossier@soo.tech>
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

#include <asm/mmu.h>
#include <memory.h>

#include <soo/gnttab.h>
#include <soo/evtchn.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/console.h>
#include <soo/debug.h>

#include "common.h"

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

void vnetstream_start(void) {
	processing_start();
}

void vnetstream_end(void) {
	processing_end();
}

bool vnetstream_is_connected(void) {
	return __connected;
}

/**
 * Allocate the rings (including the event channels) and bind to the IRQ handlers.
 */
static int setup_sring(struct vbus_device *dev, bool initall) {
	int res;
	unsigned int cmd_evtchn, tx_evtchn, rx_evtchn;
	vnetstream_cmd_sring_t *cmd_sring;
	vnetstream_tx_sring_t *tx_sring;
	vnetstream_rx_sring_t *rx_sring;
	struct vbus_transaction vbt;

	if (dev->state == VbusStateConnected)
		return 0;

	DBG(VNETSTREAM_PREFIX "Frontend: Setup rings\n");

	/* cmd_ring */

	vnetstream.cmd_ring_ref = GRANT_INVALID_REF;

	if (initall) {
		res = vbus_alloc_evtchn(dev, &cmd_evtchn);
		BUG_ON(res);

		res = bind_evtchn_to_irq_handler(cmd_evtchn, vnetstream_cmd_interrupt, NULL, &vnetstream);
		BUG_ON(res <= 0);

		vnetstream.cmd_evtchn = cmd_evtchn;
		vnetstream.cmd_irq = res;

		cmd_sring = (vnetstream_cmd_sring_t *) get_free_vpage();
		BUG_ON(!cmd_sring);

		SHARED_RING_INIT(cmd_sring);
		FRONT_RING_INIT(&vnetstream.cmd_ring, cmd_sring, PAGE_SIZE);
	} else {
		SHARED_RING_INIT(vnetstream.cmd_ring.sring);
		FRONT_RING_INIT(&vnetstream.cmd_ring, vnetstream.cmd_ring.sring, PAGE_SIZE);
	}

	res = vbus_grant_ring(dev, phys_to_pfn(virt_to_phys_pt((uint32_t) vnetstream.cmd_ring.sring)));
	if (res < 0) {
		free_page((unsigned long) cmd_sring);
		vnetstream.cmd_ring.sring = NULL;
		BUG();
	}

	vnetstream.cmd_ring_ref = res;

	/* tx_ring */

	vnetstream.tx_ring_ref = GRANT_INVALID_REF;

	if (initall) {
		res = vbus_alloc_evtchn(dev, &tx_evtchn);
		BUG_ON(res);

		res = bind_evtchn_to_irq_handler(tx_evtchn, vnetstream_tx_interrupt, NULL, &vnetstream);
		BUG_ON(res <= 0);

		vnetstream.tx_evtchn = tx_evtchn;
		vnetstream.tx_irq = res;

		tx_sring = (vnetstream_tx_sring_t *) get_free_vpage();
		BUG_ON(!tx_sring);

		SHARED_RING_INIT(tx_sring);
		FRONT_RING_INIT(&vnetstream.tx_ring, tx_sring, PAGE_SIZE);
	} else {
		SHARED_RING_INIT(vnetstream.tx_ring.sring);
		FRONT_RING_INIT(&vnetstream.tx_ring, vnetstream.tx_ring.sring, PAGE_SIZE);
	}

	res = vbus_grant_ring(dev, phys_to_pfn(virt_to_phys_pt((uint32_t) vnetstream.tx_ring.sring)));
	if (res < 0) {
		free_page((unsigned long) tx_sring);
		vnetstream.tx_ring.sring = NULL;
		BUG();
	}

	vnetstream.tx_ring_ref = res;

	/* rx_ring */

	vnetstream.rx_ring_ref = GRANT_INVALID_REF;

	if (initall) {
		res = vbus_alloc_evtchn(dev, &rx_evtchn);
		BUG_ON(res);

		res = bind_evtchn_to_irq_handler(rx_evtchn, vnetstream_rx_interrupt, NULL, &vnetstream);
		BUG_ON(res <= 0);

		vnetstream.rx_evtchn = rx_evtchn;
		vnetstream.rx_irq = res;

		rx_sring = (vnetstream_rx_sring_t *) get_free_vpage();
		BUG_ON(!rx_sring);

		SHARED_RING_INIT(rx_sring);
		FRONT_RING_INIT(&vnetstream.rx_ring, rx_sring, PAGE_SIZE);
	} else {
		SHARED_RING_INIT(vnetstream.rx_ring.sring);
		FRONT_RING_INIT(&vnetstream.rx_ring, vnetstream.rx_ring.sring, PAGE_SIZE);
	}

	res = vbus_grant_ring(dev, phys_to_pfn(virt_to_phys_pt((uint32_t) vnetstream.rx_ring.sring)));
	if (res < 0) {
		free_page((unsigned long) rx_sring);
		vnetstream.rx_ring.sring = NULL;
		BUG();
	}

	vnetstream.rx_ring_ref = res;

	/* Store the event channels and the ring refs in vbstore */

	vbus_transaction_start(&vbt);

	/* cmd_ring */

	vbus_printf(vbt, dev->nodename, "cmd_ring-ref", "%u", vnetstream.cmd_ring_ref);
	vbus_printf(vbt, dev->nodename, "cmd_ring-evtchn", "%u", vnetstream.cmd_evtchn);

	/* tx_ring */

	vbus_printf(vbt, dev->nodename, "tx_ring-ref", "%u", vnetstream.tx_ring_ref);
	vbus_printf(vbt, dev->nodename, "tx_ring-evtchn", "%u", vnetstream.tx_evtchn);

	/* rx_ring */

	vbus_printf(vbt, dev->nodename, "rx_ring-ref", "%u", vnetstream.rx_ring_ref);
	vbus_printf(vbt, dev->nodename, "rx_ring-evtchn", "%u", vnetstream.rx_evtchn);

	vbus_transaction_end(vbt);

	return 0;
}

/**
 * Free the rings and deallocate the proper data.
 */
static void free_sring(void) {
	DBG(VNETSTREAM_PREFIX "Frontend: Free rings\n");

	/* cmd_ring */

	if (vnetstream.cmd_ring_ref != GRANT_INVALID_REF) {
		gnttab_end_foreign_access(vnetstream.cmd_ring_ref);
		free_vpage((uint32_t) vnetstream.cmd_ring.sring);

		vnetstream.cmd_ring_ref = GRANT_INVALID_REF;
		vnetstream.cmd_ring.sring = NULL;
	}

	if (vnetstream.cmd_irq)
		unbind_from_irqhandler(vnetstream.cmd_irq);

	vnetstream.cmd_irq = 0;

	/* tx_ring */

	if (vnetstream.tx_ring_ref != GRANT_INVALID_REF) {
		gnttab_end_foreign_access(vnetstream.tx_ring_ref);
		free_vpage((uint32_t) vnetstream.tx_ring.sring);

		vnetstream.tx_ring_ref = GRANT_INVALID_REF;
		vnetstream.tx_ring.sring = NULL;
	}

	if (vnetstream.tx_irq)
		unbind_from_irqhandler(vnetstream.tx_irq);

	vnetstream.tx_irq = 0;

	/* rx_ring */

	if (vnetstream.rx_ring_ref != GRANT_INVALID_REF) {
		gnttab_end_foreign_access(vnetstream.rx_ring_ref);
		free_vpage((uint32_t) vnetstream.rx_ring.sring);

		vnetstream.rx_ring_ref = GRANT_INVALID_REF;
		vnetstream.rx_ring.sring = NULL;
	}

	if (vnetstream.rx_irq)
		unbind_from_irqhandler(vnetstream.rx_irq);

	vnetstream.rx_irq = 0;
}

static int readjust_shared_buffer(void);

/**
 * Gnttab for the rings after migration.
 */
static void postmig_setup_sring(struct vbus_device *dev) {
	DBG(VNETSTREAM_PREFIX "Frontend: Postmig setup rings\n");

	gnttab_end_foreign_access_ref(vnetstream.cmd_ring_ref);

	setup_sring(dev, false);
	readjust_shared_buffer();
	vnetstream_setup_shared_buffer();
}

/**
 * Reset the shared buffer information to 0.
 */
static void clear_shared_buffer(void) {
	vnetstream.txrx_data = NULL;
	vnetstream.txrx_pfn = 0;
}

/**
 * Set the shared buffer data information according to provided parameters.
 */
int vnetstream_set_shared_buffer(void *data, size_t size) {
	/* TXRX shared buffer */

	vnetstream.txrx_data = data;
	vnetstream.txrx_pfn = phys_to_pfn(virt_to_phys_pt((uint32_t) vnetstream.txrx_data));

	DBG(VNETSTREAM_PREFIX "Frontend: TXRX shared buffer: %08x\n", vnetstream.txrx_data);

	return 0;
}

/**
 * Apply the pfn offset to the pages devoted to the shared buffer.
 */
static int readjust_shared_buffer(void) {
	DBG(VNETSTREAM_PREFIX "Frontend: pfn offset=%d\n", get_pfn_offset());

	/* TXRX shared buffer */

	if ((vnetstream.txrx_data) && (vnetstream.txrx_pfn)) {
		vnetstream.txrx_pfn += get_pfn_offset();
		DBG(VNETSTREAM_PREFIX "Frontend: TXRX shared buffer pfn=%x\n", vnetstream.txrx_pfn);
	}

	return 0;
}

/**
 * Store the pfn of the shared buffer.
 */
int vnetstream_setup_shared_buffer(void) {
	struct vbus_transaction vbt;

	/* TXRX shared buffer */

	if ((vnetstream.txrx_data) && (vnetstream.txrx_pfn)) {
		DBG(VNETSTREAM_PREFIX "Frontend: TXRX shared buffer pfn=%x\n", vnetstream.txrx_pfn);

		vbus_transaction_start(&vbt);
		vbus_printf(vbt, vnetstream.dev->nodename, "data-pfn", "%u", vnetstream.txrx_pfn);
		vbus_transaction_end(vbt);
	}

	return 0;
}

static int __vnetstream_probe(struct vbus_device *dev, const struct vbus_device_id *id) {
	vnetstream.dev = dev;

	setup_sring(dev, true);
	clear_shared_buffer();

	vnetstream_probe();
	
	__connected = false;

	return 0;
}

static void backend_changed(struct vbus_device *dev, enum vbus_state backend_state) {

	switch (backend_state) {

	case VbusStateReconfiguring:
		BUG_ON(__connected);

		postmig_setup_sring(dev);
		vnetstream_reconfiguring();

		mutex_unlock(&processing_lock);
		break;

	case VbusStateClosed:
		BUG_ON(__connected);

		vnetstream_closed();
		free_sring();

		/* The processing_lock is kept forever, since it has to keep all processing activities suspended.
		 * Until the ME disappears...
		 */
		break;

	case VbusStateSuspending:
		/* Suspend Step 2 */
		mutex_lock(&processing_lock);

		__connected = false;
		reinit_completion(&connected_sync);

		vnetstream_suspend();
		break;

	case VbusStateResuming:
		/* Resume Step 2 */
		BUG_ON(__connected);

		vnetstream_resume();

		mutex_unlock(&processing_lock);
		break;

	case VbusStateConnected:
		vnetstream_connected();

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

int __vnetstream_shutdown(struct vbus_device *dev) {

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

	vnetstream_shutdown();

	return 0;
}

static const struct vbus_device_id vnetstream_ids[] = {
	{ VNETSTREAM_NAME },
	{ "" }
};

static struct vbus_driver vnetstream_drv = {
	.name			= VNETSTREAM_NAME,
	.ids			= vnetstream_ids,
	.probe			= __vnetstream_probe,
	.shutdown		= __vnetstream_shutdown,
	.otherend_changed	= backend_changed,
};

void vnetstream_vbus_init(void) {
	vbus_register_frontend(&vnetstream_drv);

	mutex_init(&processing_lock);
	mutex_init(&processing_count_lock);

	processing_count = 0;

	init_completion(&connected_sync);

}
