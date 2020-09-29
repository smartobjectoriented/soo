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

#include <mutex.h>
#include <heap.h>
#include <completion.h>

#include <device/driver.h>

#include <soo/evtchn.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/console.h>
#include <soo/debug.h>

#include <soo/dev/vuihandler.h>

// vuihandler_t vuihandler;

ui_update_spid_t __ui_update_spid = NULL;
ui_interrupt_t __ui_interrupt = NULL;

/* Sent BT packet count */
static uint32_t send_count = 0;

/* In lib/vsprintf.c */
unsigned long simple_strtoul(const char *cp, char **endp, unsigned int base);

/**
 * Read the current connected application ME SPID in vbstore.
 */
static void get_app_spid(uint8_t spid[SPID_SIZE]) {
	uint32_t i;
	int len, res;
	unsigned long spid_number;
	char spid_digit[3] = { 0 };
	char connected_app_spid[3 * SPID_SIZE];

	res = vbus_scanf(VBT_NIL, VUIHANDLER_APP_VBSTORE_DIR, VUIHANDLER_APP_VBSTORE_NODE, "%s", connected_app_spid);
	if (res != 1) {
		lprintk(VUIHANDLER_PREFIX "Error when retrieving connected app ME SPID: %d\n", res);
		BUG();
		return ;
	}

	len = strlen(connected_app_spid);

	if (len != (3 * SPID_SIZE - 1)) {
		lprintk(VUIHANDLER_PREFIX "Invalid connected app ME SPID: %s\n", connected_app_spid);
		BUG();
		return ;
	}

	for (i = 0 ; i < SPID_SIZE ; i++) {
		memcpy(spid_digit, &connected_app_spid[3 * i], 2);
		spid_number = simple_strtoul(spid_digit, NULL, 16);
		spid[i] = (uint8_t) spid_number;
	}
}

/**
 * Function called when the connected application ME SPID changes. This allows the detection
 * of the remote application running on the tablet.
 */
void vuihandler_app_watch_fn(struct vbus_watch *watch) {
	uint8_t spid[SPID_SIZE];

	vuihandler_start();

	get_app_spid(spid);

#ifdef DEBUG
	DBG(VUIHANDLER_PREFIX "ME SPID: ");
	lprintk_buffer(spid, SPID_SIZE);
#endif /* DEBUG */

	if (__ui_update_spid)
		(*__ui_update_spid)(spid);

	vuihandler_end();
}

/**
 * Process pending responses in the tx_ It should not be used in this direction.
 */
static void process_pending_tx_rsp(void) {
	
	RING_IDX i, rp;

	rp = vuihandler.tx_ring.sring->rsp_prod;
	dmb();

	for (i = vuihandler.tx_ring.sring->rsp_cons; i != rp; i++) {
		/* Do nothing */
	}

	vuihandler.tx_ring.sring->rsp_cons = i;
}

/**
 * tx_ring interrupt. It should not be used in this direction.
 */
irq_return_t vuihandler_tx_interrupt(int irq, void *dev_id) {
	if (!vuihandler_is_connected())
		return IRQ_COMPLETED;

	process_pending_tx_rsp();

	return IRQ_COMPLETED;
}

/**
 * Process pending responses in the rx_
 */
static void process_pending_rx_rsp(void) {
	RING_IDX i, rp;
	vuihandler_rx_response_t *ring_rsp;

	rp = vuihandler.rx_ring.sring->rsp_prod;
	dmb();

	for (i = vuihandler.rx_ring.sring->rsp_cons; i != rp; i++) {
		ring_rsp = RING_GET_RESPONSE(&vuihandler.rx_ring, i);

		if (__ui_interrupt)
			(*__ui_interrupt)(vuihandler.rx_data + (ring_rsp->id % VUIHANDLER_MAX_PACKETS) * VUIHANDLER_MAX_PKT_SIZE, ring_rsp->size);
	}

	vuihandler.rx_ring.sring->rsp_cons = i;
}

/**
 * rx_ring interrupt.
 */
irq_return_t vuihandler_rx_interrupt(int irq, void *dev_id) {
	if (!vuihandler_is_connected())
		return IRQ_COMPLETED;

	process_pending_rx_rsp();

	return IRQ_COMPLETED;
}

/**
 * Send a packet to the tablet/smartphone.
 */
void vuihandler_send(void *data, size_t size) {
	vuihandler_tx_request_t *ring_req;

	vuihandler_start();

	DBG(VUIHANDLER_PREFIX "0x%08x %d\n", (unsigned int) data, size);

	ring_req = RING_GET_REQUEST(&vuihandler.tx_ring, vuihandler.tx_ring.req_prod_pvt);

	ring_req->id = send_count;
	ring_req->size = size;

	memcpy(vuihandler.tx_data + (ring_req->id % VUIHANDLER_MAX_PACKETS) * VUIHANDLER_MAX_PKT_SIZE, data, size);

	dmb();

	vuihandler.tx_ring.req_prod_pvt++;

	RING_PUSH_REQUESTS(&vuihandler.tx_ring);

	notify_remote_via_irq(vuihandler.tx_irq);

	send_count++;

	vuihandler_end();
}

/**** Rings/Shared buffers setup ****/

/**
 * Allocate the rings (including the event channels) and bind to the IRQ handlers.
 */
static int setup_sring(struct vbus_device *vdev, bool initall) {
	vuihander_t *vhuihandler = to_vuihandler(vdev);
	int res;
	unsigned int tx_evtchn, rx_evtchn;
	vuihandler_tx_sring_t *tx_sring;
	vuihandler_rx_sring_t *rx_sring;
	struct vbus_transaction vbt;

	if (dev->state == VbusStateConnected)
		return 0;

	DBG(VUIHANDLER_PREFIX "Frontend: Setup rings\n");

	/* tx_ring */

	vuihandler->tx_ring_ref = GRANT_INVALID_REF;

	if (initall) {
		res = vbus_alloc_evtchn(dev, &tx_evtchn);
		BUG_ON(res);

		res = bind_evtchn_to_irq_handler(tx_evtchn, vuihandler_tx_interrupt, NULL, &vuihandler);
		BUG_ON(res <= 0);

		vuihandler->tx_evtchn = tx_evtchn;
		vuihandler->tx_irq = res;

		tx_sring = (vuihandler_tx_sring_t *) get_free_vpage();
		BUG_ON(!tx_sring);

		SHARED_RING_INIT(tx_sring);
		FRONT_RING_INIT(&vuihandler->tx_ring, tx_sring, PAGE_SIZE);
	} else {
		SHARED_RING_INIT(vuihandler->tx_ring.sring);
		FRONT_RING_INIT(&vuihandler->tx_ring, vuihandler->tx_ring.sring, PAGE_SIZE);
	}

	res = vbus_grant_ring(dev, phys_to_pfn(virt_to_phys_pt((uint32_t) vuihandler->tx_ring.sring)));
	BUG_ON(res < 0);

	vuihandler->tx_ring_ref = res;

	/* rx_ring */

	vuihandler->rx_ring_ref = GRANT_INVALID_REF;

	if (initall) {
		res = vbus_alloc_evtchn(dev, &rx_evtchn);
		BUG_ON(res);

		res = bind_evtchn_to_irq_handler(rx_evtchn, vuihandler_rx_interrupt, NULL, &vuihandler);
		BUG_ON(res <= 0);

		vuihandler->rx_evtchn = rx_evtchn;
		vuihandler->rx_irq = res;

		rx_sring = (vuihandler_rx_sring_t *) get_free_vpage();
		BUG_ON(!rx_sring);

		SHARED_RING_INIT(rx_sring);
		FRONT_RING_INIT(&vuihandler->rx_ring, rx_sring, PAGE_SIZE);
	} else {
		SHARED_RING_INIT(vuihandler->rx_ring.sring);
		FRONT_RING_INIT(&vuihandler->rx_ring, vuihandler->rx_ring.sring, PAGE_SIZE);
	}

	res = vbus_grant_ring(dev, phys_to_pfn(virt_to_phys_pt((uint32_t) vuihandler->rx_ring.sring)));
	BUG_ON(res < 0);

	vuihandler->rx_ring_ref = res;

	/* Store the event channels and the ring refs in vbstore */

	vbus_transaction_start(&vbt);

	/* tx_ring */

	vbus_printf(vbt, dev->nodename, "tx_ring-ref", "%u", vuihandler->tx_ring_ref);
	vbus_printf(vbt, dev->nodename, "tx_ring-evtchn", "%u", vuihandler->tx_evtchn);

	/* rx_ring */

	vbus_printf(vbt, dev->nodename, "rx_ring-ref", "%u", vuihandler->rx_ring_ref);
	vbus_printf(vbt, dev->nodename, "rx_ring-evtchn", "%u", vuihandler->rx_evtchn);

	vbus_transaction_end(vbt);

	return 0;
}

/**
 * Free the rings and deallocate the proper data.
 */
static void free_sring(struct vbus_device *vdev) {
	vuihander_t *vhuihandler = to_vuihandler(vdev);
	/* tx_ring */

	if (vuihandler->tx_ring_ref != GRANT_INVALID_REF) {
		gnttab_end_foreign_access(vuihandler->tx_ring_ref);
		free_vpage((uint32_t) vuihandler->tx_ring.sring);

		vuihandler->tx_ring_ref = GRANT_INVALID_REF;
		vuihandler->tx_ring.sring = NULL;
	}

	if (vuihandler->tx_irq)
		unbind_from_irqhandler(vuihandler->tx_irq);

	vuihandler->tx_irq = 0;

	/* rx_ring */

	if (vuihandler->rx_ring_ref != GRANT_INVALID_REF) {
		gnttab_end_foreign_access(vuihandler->rx_ring_ref);
		free_vpage((uint32_t) vuihandler->rx_ring.sring);

		vuihandler->rx_ring_ref = GRANT_INVALID_REF;
		vuihandler->rx_ring.sring = NULL;
	}

	if (vuihandler->rx_irq)
		unbind_from_irqhandler(vuihandler->rx_irq);

	vuihandler->rx_irq = 0;
}

/**
 * Gnttab for the rings after migration.
 */
static void postmig_setup_sring(struct vbus_device *vdev) {
	vuihander_t *vhuihandler = to_vuihandler(vdev);
	gnttab_end_foreign_access_ref(vuihandler->tx_ring_ref);
	gnttab_end_foreign_access_ref(vuihandler->rx_ring_ref);

	setup_sring(dev, false);
}

/**
 * Allocate the pages dedicated to the shared buffers.
 */
static int alloc_shared_buffers(struct vbus_device *vdev) {
	vuihander_t *vhuihandler = to_vuihandler(vdev);
	int nr_pages = DIV_ROUND_UP(VUIHANDLER_BUFFER_SIZE, PAGE_SIZE);

	/* TX shared buffer */

	vuihandler->tx_data = (char *) get_contig_free_vpages(nr_pages);
	memset(vuihandler->tx_data, 0, VUIHANDLER_BUFFER_SIZE);

	BUG_ON(!vuihandler->tx_data);

	vuihandler->tx_pfn = phys_to_pfn(virt_to_phys_pt((uint32_t) vuihandler->tx_data));

	DBG(VUIHANDLER_PREFIX "Frontend: TX shared buffer pfn=%x\n", vuihandler->tx_pfn);

	/* RX shared buffer */

	vuihandler->rx_data = (char *) get_contig_free_vpages(nr_pages);
	memset(vuihandler->rx_data, 0, VUIHANDLER_BUFFER_SIZE);

	BUG_ON(!vuihandler->rx_data);

	vuihandler->rx_pfn = phys_to_pfn(virt_to_phys_pt((uint32_t) vuihandler->rx_data));

	DBG(VUIHANDLER_PREFIX "Frontend: RX shared buffer pfn=%x\n", vuihandler->rx_pfn);

	return 0;
}

/**
 * Store the pfn of the shared buffers.
 */
static int setup_shared_buffers(struct vbus_device *vdev) {
	vuihander_t *vhuihandler = to_vuihandler(vdev);
	struct vbus_transaction vbt;

	/* TX shared buffer */

	if ((vuihandler->tx_data) && (vuihandler->tx_pfn)) {
		DBG(VUIHANDLER_PREFIX "Frontend: TX shared buffer pfn=%x\n", vuihandler->shared_buffers.tx_pfn);

		vbus_transaction_start(&vbt);
		vbus_printf(vbt, vuihandler->dev->nodename, "tx-pfn", "%u", vuihandler->tx_pfn);
		vbus_transaction_end(vbt);
	}

	/* RX shared buffer */

	if ((vuihandler->rx_data) && (vuihandler->rx_pfn)) {
		DBG(VUIHANDLER_PREFIX "Frontend: RX shared buffer pfn=%x\n", vuihandler->shared_buffers.rx_pfn);

		vbus_transaction_start(&vbt);
		vbus_printf(vbt, vuihandler->dev->nodename, "rx-pfn", "%u", vuihandler->rx_pfn);
		vbus_transaction_end(vbt);
	}

	return 0;
}

/**
 * Apply the pfn offset to the pages devoted to the shared buffers.
 */
static int readjust_shared_buffers(struct vbus_device *vdev) {
	vuihander_t *vhuihandler = to_vuihandler(vdev);
	DBG(VUIHANDLER_PREFIX "Frontend: pfn offset=%d\n", get_pfn_offset());

	/* TX shared buffer */

	if ((vuihandler->tx_data) && (vuihandler->tx_pfn)) {
		vuihandler->tx_pfn += get_pfn_offset();
		DBG(VUIHANDLER_PREFIX "Frontend: TX shared buffer pfn=%x\n", vuihandler->shared_buffers.tx_pfn);
	}

	/* RX shared buffer */

	if ((vuihandler->rx_data) && (vuihandler->rx_pfn)) {
		vuihandler->rx_pfn += get_pfn_offset();
		DBG(VUIHANDLER_PREFIX "Frontend: RX shared buffer pfn=%x\n", vuihandler->shared_buffers.rx_pfn);
	}

	return 0;
}

/**
 * Free the shared buffers and deallocate the proper data.
 */
static void free_shared_buffers(struct vbus_device *vdev) {
	vuihander_t *vhuihandler = to_vuihandler(vdev);
	/* TX shared buffer */

	if ((vuihandler->tx_data) && (vuihandler->tx_pfn)) {
		free_vpage((uint32_t) vuihandler->tx_data);

		vuihandler->tx_data = NULL;
		vuihandler->tx_pfn = 0;
	}

	/* RX shared buffer */

	if ((vuihandler->rx_data) && (vuihandler->rx_pfn)) {
		free_vpage((uint32_t) vuihandler->rx_data);

		vuihandler->rx_data = NULL;
		vuihandler->rx_pfn = 0;
	}
}


/**** Vbus watch functions ****/
/**
 * Unset, then set the connected application ME SPID watcher.
 */
static void adjust_watch(struct vbus_device *vdev) {
	vuihander_t *vhuihandler = to_vuihandler(vdev);
	unregister_vbus_watch(&vuihandler->app_watch);
	vbus_watch_pathfmt(dev, &vuihandler->app_watch, vuihandler_app_watch_fn, VUIHANDLER_APP_VBSTORE_DIR "/" VUIHANDLER_APP_VBSTORE_NODE);
}

/**
 * Unset the connected application ME SPID watcher.
 */
static void free_watch(void) {
	vuihander_t *vhuihandler = to_vuihandler(vdev);
	unregister_vbus_watch(&vuihandler->app_watch);
};


void vuihandler_probe(struct vbus_device *vdev) {
	DBG0(VUIHANDLER_PREFIX "Frontend probe\n");
}

void vuihandler_suspend(struct vbus_device *vdev) {
	DBG0(VUIHANDLER_PREFIX "Frontend suspend\n");
}

void vuihandler_resume(struct vbus_device *vdev) {
	DBG0(VUIHANDLER_PREFIX "Frontend resume\n");

	process_pending_rx_rsp();
}

void vuihandler_connected(struct vbus_device *vdev) {
	vuihandler_t *vuihander = to_vuihandler(vdev);
	DBG0(VUIHANDLER_PREFIX "Frontend connected\n");

	/* Force the processing of pending requests, if any */
	notify_remote_via_irq(vuihandler->tx_irq);
	notify_remote_via_irq(vuihandler->rx_irq);
}

void vuihandler_reconfiguring(struct vbus_device *vdev) {
	DBG0(VUIHANDLER_PREFIX "Frontend reconfiguring\n");

	postmig_setup_sring(vdev);
	readjust_shared_buffers(vdev);
	setup_shared_buffers(vdev);
	adjust_watch(vdev);
}

void vuihandler_shutdown(struct vbus_device *vdev) {
	DBG0(VUIHANDLER_PREFIX "Frontend shutdown\n");
}

void vuihandler_closed(struct vbus_device *vdev) {
	DBG0(VUIHANDLER_PREFIX "Frontend close\n");

	free_watch(vdev);
	free_sring(vdev);
	free_shared_buffers(vdev);
}

void vuihandler_register_callback(ui_update_spid_t ui_update_spid, ui_interrupt_t ui_interrupt) {
	__ui_update_spid = ui_update_spid;
	__ui_interrupt = ui_interrupt;
}

vdrvfront_t vuihandlerdrv = {
	.probe = vuihandler_probe,
	.reconfiguring = vuihandler_reconfiguring,
	.shutdown = vuihandler_shutdown,
	.closed = vuihandler_closed,
	.suspend = vuihandler_suspend,
	.resume = vuihandler_resume,
	.connected = vuihandler_connected
};

static int vuihandler_init(dev_t *dev) {

	vdevfront_init(VHUIHANDLER_NAME, &vuihandlerdrv);

	return 0;
}

REGISTER_DRIVER_POSTCORE("vuihandler,frontend", vuihandler_init);
