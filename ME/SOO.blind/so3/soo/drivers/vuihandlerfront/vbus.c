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

void vuihandler_start(void) {
	processing_start();
}

void vuihandler_end(void) {
	processing_end();
}

bool vuihandler_is_connected(void) {
	return __connected;
}

/**
 * Allocate the rings (including the event channels) and bind to the IRQ handlers.
 */
static int setup_sring(struct vbus_device *dev, bool initall) {
	int res;
	unsigned int tx_evtchn, rx_evtchn;
	vuihandler_tx_sring_t *tx_sring;
	vuihandler_rx_sring_t *rx_sring;
	struct vbus_transaction vbt;

	if (dev->state == VbusStateConnected)
		return 0;

	DBG(VUIHANDLER_PREFIX "Frontend: Setup rings\n");

	/* tx_ring */

	vuihandler.tx_ring_ref = GRANT_INVALID_REF;

	if (initall) {
		res = vbus_alloc_evtchn(dev, &tx_evtchn);
		BUG_ON(res);

		res = bind_evtchn_to_irq_handler(tx_evtchn, vuihandler_tx_interrupt, NULL, &vuihandler);
		BUG_ON(res <= 0);

		vuihandler.tx_evtchn = tx_evtchn;
		vuihandler.tx_irq = res;

		tx_sring = (vuihandler_tx_sring_t *) get_free_vpage();
		BUG_ON(!tx_sring);

		SHARED_RING_INIT(tx_sring);
		FRONT_RING_INIT(&vuihandler.tx_ring, tx_sring, PAGE_SIZE);
	} else {
		SHARED_RING_INIT(vuihandler.tx_ring.sring);
		FRONT_RING_INIT(&vuihandler.tx_ring, vuihandler.tx_ring.sring, PAGE_SIZE);
	}

	res = vbus_grant_ring(dev, phys_to_pfn(virt_to_phys_pt((uint32_t) vuihandler.tx_ring.sring)));
	BUG_ON(res < 0);

	vuihandler.tx_ring_ref = res;

	/* rx_ring */

	vuihandler.rx_ring_ref = GRANT_INVALID_REF;

	if (initall) {
		res = vbus_alloc_evtchn(dev, &rx_evtchn);
		BUG_ON(res);

		res = bind_evtchn_to_irq_handler(rx_evtchn, vuihandler_rx_interrupt, NULL, &vuihandler);
		BUG_ON(res <= 0);

		vuihandler.rx_evtchn = rx_evtchn;
		vuihandler.rx_irq = res;

		rx_sring = (vuihandler_rx_sring_t *) get_free_vpage();
		BUG_ON(!rx_sring);

		SHARED_RING_INIT(rx_sring);
		FRONT_RING_INIT(&vuihandler.rx_ring, rx_sring, PAGE_SIZE);
	} else {
		SHARED_RING_INIT(vuihandler.rx_ring.sring);
		FRONT_RING_INIT(&vuihandler.rx_ring, vuihandler.rx_ring.sring, PAGE_SIZE);
	}

	res = vbus_grant_ring(dev, phys_to_pfn(virt_to_phys_pt((uint32_t) vuihandler.rx_ring.sring)));
	BUG_ON(res < 0);

	vuihandler.rx_ring_ref = res;

	/* Store the event channels and the ring refs in vbstore */

	vbus_transaction_start(&vbt);

	/* tx_ring */

	vbus_printf(vbt, dev->nodename, "tx_ring-ref", "%u", vuihandler.tx_ring_ref);
	vbus_printf(vbt, dev->nodename, "tx_ring-evtchn", "%u", vuihandler.tx_evtchn);

	/* rx_ring */

	vbus_printf(vbt, dev->nodename, "rx_ring-ref", "%u", vuihandler.rx_ring_ref);
	vbus_printf(vbt, dev->nodename, "rx_ring-evtchn", "%u", vuihandler.rx_evtchn);

	vbus_transaction_end(vbt);

	return 0;
}

/**
 * Free the rings and deallocate the proper data.
 */
static void free_sring(void) {
	/* tx_ring */

	if (vuihandler.tx_ring_ref != GRANT_INVALID_REF) {
		gnttab_end_foreign_access(vuihandler.tx_ring_ref);
		free_vpage((uint32_t) vuihandler.tx_ring.sring);

		vuihandler.tx_ring_ref = GRANT_INVALID_REF;
		vuihandler.tx_ring.sring = NULL;
	}

	if (vuihandler.tx_irq)
		unbind_from_irqhandler(vuihandler.tx_irq);

	vuihandler.tx_irq = 0;

	/* rx_ring */

	if (vuihandler.rx_ring_ref != GRANT_INVALID_REF) {
		gnttab_end_foreign_access(vuihandler.rx_ring_ref);
		free_vpage((uint32_t) vuihandler.rx_ring.sring);

		vuihandler.rx_ring_ref = GRANT_INVALID_REF;
		vuihandler.rx_ring.sring = NULL;
	}

	if (vuihandler.rx_irq)
		unbind_from_irqhandler(vuihandler.rx_irq);

	vuihandler.rx_irq = 0;
}

/**
 * Gnttab for the rings after migration.
 */
static void postmig_setup_sring(struct vbus_device *dev) {
	gnttab_end_foreign_access_ref(vuihandler.tx_ring_ref);
	gnttab_end_foreign_access_ref(vuihandler.rx_ring_ref);

	setup_sring(dev, false);
}

/**
 * Allocate the pages dedicated to the shared buffers.
 */
static int alloc_shared_buffers(void) {
	int nr_pages = DIV_ROUND_UP(VUIHANDLER_BUFFER_SIZE, PAGE_SIZE);

	/* TX shared buffer */

	vuihandler.tx_data = (char *) get_contig_free_vpages(nr_pages);
	memset(vuihandler.tx_data, 0, VUIHANDLER_BUFFER_SIZE);

	BUG_ON(!vuihandler.tx_data);

	vuihandler.tx_pfn = phys_to_pfn(virt_to_phys_pt((uint32_t) vuihandler.tx_data));

	DBG(VUIHANDLER_PREFIX "Frontend: TX shared buffer pfn=%x\n", vuihandler.tx_pfn);

	/* RX shared buffer */

	vuihandler.rx_data = (char *) get_contig_free_vpages(nr_pages);
	memset(vuihandler.rx_data, 0, VUIHANDLER_BUFFER_SIZE);

	BUG_ON(!vuihandler.rx_data);

	vuihandler.rx_pfn = phys_to_pfn(virt_to_phys_pt((uint32_t) vuihandler.rx_data));

	DBG(VUIHANDLER_PREFIX "Frontend: RX shared buffer pfn=%x\n", vuihandler.rx_pfn);

	return 0;
}

/**
 * Store the pfn of the shared buffers.
 */
static int setup_shared_buffers(void) {
	struct vbus_transaction vbt;

	/* TX shared buffer */

	if ((vuihandler.tx_data) && (vuihandler.tx_pfn)) {
		DBG(VUIHANDLER_PREFIX "Frontend: TX shared buffer pfn=%x\n", vuihandler.shared_buffers.tx_pfn);

		vbus_transaction_start(&vbt);
		vbus_printf(vbt, vuihandler.dev->nodename, "tx-pfn", "%u", vuihandler.tx_pfn);
		vbus_transaction_end(vbt);
	}

	/* RX shared buffer */

	if ((vuihandler.rx_data) && (vuihandler.rx_pfn)) {
		DBG(VUIHANDLER_PREFIX "Frontend: RX shared buffer pfn=%x\n", vuihandler.shared_buffers.rx_pfn);

		vbus_transaction_start(&vbt);
		vbus_printf(vbt, vuihandler.dev->nodename, "rx-pfn", "%u", vuihandler.rx_pfn);
		vbus_transaction_end(vbt);
	}

	return 0;
}

/**
 * Apply the pfn offset to the pages devoted to the shared buffers.
 */
static int readjust_shared_buffers(void) {
	DBG(VUIHANDLER_PREFIX "Frontend: pfn offset=%d\n", get_pfn_offset());

	/* TX shared buffer */

	if ((vuihandler.tx_data) && (vuihandler.tx_pfn)) {
		vuihandler.tx_pfn += get_pfn_offset();
		DBG(VUIHANDLER_PREFIX "Frontend: TX shared buffer pfn=%x\n", vuihandler.shared_buffers.tx_pfn);
	}

	/* RX shared buffer */

	if ((vuihandler.rx_data) && (vuihandler.rx_pfn)) {
		vuihandler.rx_pfn += get_pfn_offset();
		DBG(VUIHANDLER_PREFIX "Frontend: RX shared buffer pfn=%x\n", vuihandler.shared_buffers.rx_pfn);
	}

	return 0;
}

/**
 * Free the shared buffers and deallocate the proper data.
 */
static void free_shared_buffers(void) {
	/* TX shared buffer */

	if ((vuihandler.tx_data) && (vuihandler.tx_pfn)) {
		free_vpage((uint32_t) vuihandler.tx_data);

		vuihandler.tx_data = NULL;
		vuihandler.tx_pfn = 0;
	}

	/* RX shared buffer */

	if ((vuihandler.rx_data) && (vuihandler.rx_pfn)) {
		free_vpage((uint32_t) vuihandler.rx_data);

		vuihandler.rx_data = NULL;
		vuihandler.rx_pfn = 0;
	}
}

/**
 * Unset, then set the connected application ME SPID watcher.
 */
static void adjust_watch(struct vbus_device *dev) {
	unregister_vbus_watch(&vuihandler.app_watch);
	vbus_watch_pathfmt(dev, &vuihandler.app_watch, vuihandler_app_watch_fn, VUIHANDLER_APP_VBSTORE_DIR "/" VUIHANDLER_APP_VBSTORE_NODE);
}

/**
 * Unset the connected application ME SPID watcher.
 */
static void free_watch(void) {
	unregister_vbus_watch(&vuihandler.app_watch);
};

static int __vuihandler_probe(struct vbus_device *dev, const struct vbus_device_id *id) {
	vuihandler.dev = dev;

	DBG0("SOO Virtual dummy frontend driver\n");

	DBG("%s: allocate a shared page and set up the ring...\n", __func__);

	setup_sring(dev, true);

	alloc_shared_buffers();
	setup_shared_buffers();

	/* Set the connected application ME SPID watcher */
	vbus_watch_pathfmt(dev, &vuihandler.app_watch, vuihandler_app_watch_fn, VUIHANDLER_APP_VBSTORE_DIR "/" VUIHANDLER_APP_VBSTORE_NODE);

	vuihandler_probe();

	__connected = false;

	return 0;
}

/**
 * State machine by the frontend's side.
 */
static void backend_changed(struct vbus_device *dev, enum vbus_state backend_state) {
	
	switch (backend_state) {
	case VbusStateReconfiguring:

		BUG_ON(__connected);

		postmig_setup_sring(dev);
		readjust_shared_buffers();
		setup_shared_buffers();
		adjust_watch(dev);
		vuihandler_reconfiguring();
	
		mutex_unlock(&processing_lock);
		break;

	case VbusStateClosed:
		
		BUG_ON(__connected);
	
		vuihandler_closed();
		free_watch();
		free_sring();
		free_shared_buffers();

		/* The processing_lock is kept forever, since it has to keep all processing activities suspended.
		 * Until the ME disappears...
		 */

		break;

	case VbusStateSuspending:
		/* Suspend Step 2 */
		mutex_lock(&processing_lock);

		__connected = false;
		complete(&connected_sync);

		vuihandler_suspend();
		break;

	case VbusStateResuming:
		/* Resume Step 2 */
		BUG_ON(__connected);

		vuihandler_resume();

		mutex_unlock(&processing_lock);
		break;

	case VbusStateConnected:
		vuihandler_connected();

		/* Now, the FE is considered as connected */
		__connected = true;

		complete(&connected_sync);
		break;

	case VbusStateUnknown:
	default:
		lprintk("%s - line %d: Unknown state %d (backend) for device %s\n", __func__, __LINE__, backend_state, dev->nodename);
		BUG();
		break;
	}
}

int __vuihandler_shutdown(struct vbus_device *dev) {

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

	vuihandler_shutdown();

	return 0;
}

static const struct vbus_device_id vuihandler_ids[] = {
	{ VUIHANDLER_NAME },
	{ "" }
};

static struct vbus_driver vuihandler_drv = {
	.name = VUIHANDLER_NAME,
	.ids = vuihandler_ids,
	.probe = __vuihandler_probe,
	.shutdown = __vuihandler_shutdown,
	.otherend_changed = backend_changed,
};

void vuihandler_vbus_init(void) {
	vbus_register_frontend(&vuihandler_drv);

	mutex_init(&processing_lock);
	mutex_init(&processing_count_lock);

	processing_count = 0;

	init_completion(&connected_sync);

}
