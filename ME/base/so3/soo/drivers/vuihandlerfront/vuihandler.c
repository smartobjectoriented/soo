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
#include <memory.h>
#include <asm/mmu.h>

#include <device/driver.h>

#include <soo/evtchn.h>
#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/console.h>
#include <soo/debug.h>

#include <soo/dev/vuihandler.h>

ui_update_spid_t __ui_update_spid = NULL;
ui_interrupt_t __ui_interrupt = NULL;

/* Sent BT packet count */

typedef struct {
	void *data;
	size_t size;
} send_param_t;

typedef struct {
	vuihandler_t vuihandler;

	uint32_t send_count;
	completion_t *send_compl;
	send_param_t sp;
} vuihandler_priv_t;

static struct vbus_device *vuihandler_dev = NULL;


/* In lib/vsprintf.c */
unsigned long simple_strtoul(const char *cp, char **endp, unsigned int base);


/**
 * Process pending responses in the tx_ It should not be used in this direction.
 */
static void process_pending_tx_rsp(struct vbus_device *vdev) {
	vuihandler_t *vuihandler = to_vuihandler(vdev);
	vuihandler_tx_response_t *ring_req;
	
	dmb();
	/* Consume the responses without doing anything */
	while ((ring_req = vuihandler_tx_get_ring_response(&vuihandler->tx_ring)) != NULL);
}


/**
 * tx_ring interrupt. It should not be used in this direction.
 */
irq_return_t vuihandler_tx_interrupt(int irq, void *dev_id) {
	struct vbus_device *vdev = (struct vbus_device *) dev_id;

	process_pending_tx_rsp(vdev);

	return IRQ_COMPLETED;
}

#if 1
/**
 * Process pending responses in the rx_
 */
static void process_pending_rx_rsp(struct vbus_device *vdev) {
	vuihandler_rx_response_t *ring_rsp;
	vuihandler_priv_t *vuihandler_priv = dev_get_drvdata(vdev->dev);
	vuihandler_t *vuihandler = &vuihandler_priv->vuihandler;


	while ((ring_rsp = vuihandler_rx_get_ring_response(&vuihandler->rx_ring)) != NULL) {
		DBG("rsp->id = %d, rsp->size = %d\n", ring_rsp->id, ring_rsp->size);
		DBG("Packet as string is: %s\n", ring_rsp->buf);
		if (__ui_interrupt)
			(*__ui_interrupt)(vuihandler->rx_data + (ring_rsp->id % VUIHANDLER_MAX_PACKETS) * VUIHANDLER_MAX_PKT_SIZE, ring_rsp->size);
	
	}
}
#endif

/**
 * rx_ring interrupt.
 */
irq_return_t vuihandler_rx_interrupt(int irq, void *dev_id) {
	process_pending_rx_rsp(vuihandler_dev);

	return IRQ_COMPLETED;
}


/**
 * Send a packet to the tablet/smartphone.
 */
void vuihandler_send(void *data, size_t size) {
	vuihandler_priv_t *vuihandler_priv;

	vuihandler_priv = (vuihandler_priv_t *) dev_get_drvdata(vuihandler_dev->dev);
	vuihandler_priv->sp.data = data;
	vuihandler_priv->sp.size = size;
	complete(vuihandler_priv->send_compl);
}

int vuihandler_send_fn(void *arg) {
	struct vbus_device *vdev = (struct vbus_device *) arg;
	vuihandler_tx_request_t *ring_req;
	vuihandler_priv_t *vuihandler_priv;

	vuihandler_priv = (vuihandler_priv_t *) dev_get_drvdata(vdev->dev);

	while(1) {
		wait_for_completion(vuihandler_priv->send_compl);

		vdevfront_processing_begin(vdev);
		/*
		* Try to generate a new request to the backend
		*/
		if (!RING_REQ_FULL(&vuihandler_priv->vuihandler.tx_ring)) {
			ring_req = vuihandler_tx_new_ring_request(&vuihandler_priv->vuihandler.tx_ring);

			ring_req->id = vuihandler_priv->send_count;
			ring_req->size = vuihandler_priv->sp.size;

			memcpy(ring_req->buf, vuihandler_priv->sp.data, vuihandler_priv->sp.size);

			vuihandler_priv->send_count++;

			vuihandler_tx_ring_request_ready(&vuihandler_priv->vuihandler.tx_ring);

			notify_remote_via_virq(vuihandler_priv->vuihandler.tx_irq);
		}

		vdevfront_processing_end(vdev);

	}

	return 0;
}


void vuihandler_probe(struct vbus_device *vdev) {
	unsigned int rx_evtchn, tx_evtchn;
	vuihandler_rx_sring_t *rx_sring;
	vuihandler_tx_sring_t *tx_sring;
	struct vbus_transaction vbt;
	vuihandler_priv_t *vuihandler_priv;

	DBG0(VUIHANDLER_PREFIX "Frontend probe\n");

	vuihandler_priv = dev_get_drvdata(vdev->dev);
	vuihandler_dev = vdev;

	/* RX ring init */
	vuihandler_priv->vuihandler.rx_ring_ref = GRANT_INVALID_REF;

	vbus_alloc_evtchn(vdev, &rx_evtchn);

	vuihandler_priv->vuihandler.rx_irq = bind_evtchn_to_irq_handler(rx_evtchn, vuihandler_rx_interrupt, NULL, vdev);
	vuihandler_priv->vuihandler.rx_evtchn = rx_evtchn;

	rx_sring = (vuihandler_rx_sring_t *) get_free_vpage();

	if (!rx_sring) {
		lprintk("%s - line %d: Allocating shared ring failed for device %s\n", __func__, __LINE__, vdev->nodename);
		BUG();
	}

	SHARED_RING_INIT(rx_sring);
	FRONT_RING_INIT(&vuihandler_priv->vuihandler.rx_ring, rx_sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	vuihandler_priv->vuihandler.rx_ring_ref = vbus_grant_ring(vdev, phys_to_pfn(virt_to_phys_pt((uint32_t) vuihandler_priv->vuihandler.rx_ring.sring)));

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "rx_ring-ref", "%u", vuihandler_priv->vuihandler.rx_ring_ref);
	vbus_printf(vbt, vdev->nodename, "rx_ring-evtchn", "%u", vuihandler_priv->vuihandler.rx_evtchn);

	vbus_transaction_end(vbt);
	
	/* TX ring init */
	vuihandler_priv->vuihandler.tx_ring_ref = GRANT_INVALID_REF;

	vbus_alloc_evtchn(vdev, &tx_evtchn);

	vuihandler_priv->vuihandler.tx_irq = bind_evtchn_to_irq_handler(tx_evtchn, vuihandler_tx_interrupt, NULL, vdev);
	vuihandler_priv->vuihandler.tx_evtchn = tx_evtchn;

	tx_sring = (vuihandler_tx_sring_t *) get_free_vpage();

	if (!tx_sring) {
		lprintk("%s - line %d: Allocating shared ring failed for device %s\n", __func__, __LINE__, vdev->nodename);
		BUG();
	}

	SHARED_RING_INIT(tx_sring);
	FRONT_RING_INIT(&vuihandler_priv->vuihandler.tx_ring, tx_sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	vuihandler_priv->vuihandler.tx_ring_ref = vbus_grant_ring(vdev, phys_to_pfn(virt_to_phys_pt((uint32_t) vuihandler_priv->vuihandler.tx_ring.sring)));

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "tx_ring-ref", "%u", vuihandler_priv->vuihandler.tx_ring_ref);
	vbus_printf(vbt, vdev->nodename, "tx_ring-evtchn", "%u", vuihandler_priv->vuihandler.tx_evtchn);

	vbus_transaction_end(vbt);
}

void vuihandler_suspend(struct vbus_device *vdev) {
	DBG0(VUIHANDLER_PREFIX "Frontend suspend\n");
}

void vuihandler_resume(struct vbus_device *vdev) {
	DBG0(VUIHANDLER_PREFIX "Frontend resume\n");

	process_pending_rx_rsp(vdev);
}

void vuihandler_connected(struct vbus_device *vdev) {
	vuihandler_priv_t *vuihandler_priv = dev_get_drvdata(vdev->dev);
	
	vuihandler_priv->send_compl = malloc(sizeof(completion_t));

	DBG0(VUIHANDLER_PREFIX "Frontend connected\n");

	/* Force the processing of pending requests, if any */
	notify_remote_via_virq(vuihandler_priv->vuihandler.tx_irq);
	notify_remote_via_virq(vuihandler_priv->vuihandler.rx_irq);

	init_completion(vuihandler_priv->send_compl);

	kernel_thread(vuihandler_send_fn, "vuihandler_send_fn", (void *) vdev, 0);
}

void vuihandler_reconfiguring(struct vbus_device *vdev) {
	struct vbus_transaction vbt;
	vuihandler_priv_t *vuihandler_priv;

	DBG0(VUIHANDLER_PREFIX "Frontend reconfiguring\n");

	vuihandler_priv = dev_get_drvdata(vdev->dev);

	/* RX ring init */
	gnttab_end_foreign_access_ref(vuihandler_priv->vuihandler.rx_ring_ref);
	vuihandler_priv->vuihandler.rx_ring_ref = GRANT_INVALID_REF;

	SHARED_RING_INIT(vuihandler_priv->vuihandler.rx_ring.sring);
	FRONT_RING_INIT(&vuihandler_priv->vuihandler.rx_ring, (&vuihandler_priv->vuihandler.rx_ring)->sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	vuihandler_priv->vuihandler.rx_ring_ref = vbus_grant_ring(vdev, phys_to_pfn(virt_to_phys_pt((uint32_t) vuihandler_priv->vuihandler.rx_ring.sring)));

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "rx_ring-ref", "%u", vuihandler_priv->vuihandler.rx_ring_ref);
	vbus_printf(vbt, vdev->nodename, "rx_ring-evtchn", "%u", vuihandler_priv->vuihandler.rx_evtchn);

	vbus_transaction_end(vbt);

	/* TX ring init */
	gnttab_end_foreign_access_ref(vuihandler_priv->vuihandler.tx_ring_ref);
	vuihandler_priv->vuihandler.tx_ring_ref = GRANT_INVALID_REF;

	SHARED_RING_INIT(vuihandler_priv->vuihandler.tx_ring.sring);
	FRONT_RING_INIT(&vuihandler_priv->vuihandler.tx_ring, (&vuihandler_priv->vuihandler.tx_ring)->sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	vuihandler_priv->vuihandler.tx_ring_ref = vbus_grant_ring(vdev, phys_to_pfn(virt_to_phys_pt((uint32_t) vuihandler_priv->vuihandler.tx_ring.sring)));

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "tx_ring-ref", "%u", vuihandler_priv->vuihandler.tx_ring_ref);
	vbus_printf(vbt, vdev->nodename, "tx_ring-evtchn", "%u", vuihandler_priv->vuihandler.tx_evtchn);

	vbus_transaction_end(vbt);
}

void vuihandler_shutdown(struct vbus_device *vdev) {
	DBG0(VUIHANDLER_PREFIX "Frontend shutdown\n");
}

void vuihandler_closed(struct vbus_device *vdev) {
	vuihandler_priv_t *vuihandler_priv = dev_get_drvdata(vdev->dev);

	DBG0(VUIHANDLER_PREFIX "Frontend close\n");

	/**
	 * Free the ring and deallocate the proper data.
	 */

	/* Free resources associated with old device channel. */
	/* RX side */
	if (vuihandler_priv->vuihandler.rx_ring_ref != GRANT_INVALID_REF) {
		gnttab_end_foreign_access(vuihandler_priv->vuihandler.rx_ring_ref);
		free_vpage((uint32_t) vuihandler_priv->vuihandler.rx_ring.sring);

		vuihandler_priv->vuihandler.rx_ring_ref = GRANT_INVALID_REF;
		vuihandler_priv->vuihandler.rx_ring.sring = NULL;
	}

	if (vuihandler_priv->vuihandler.rx_irq)
		unbind_from_irqhandler(vuihandler_priv->vuihandler.rx_irq);
	vuihandler_priv->vuihandler.rx_irq = 0;
	
	/* TX side */
	if (vuihandler_priv->vuihandler.tx_ring_ref != GRANT_INVALID_REF) {
		gnttab_end_foreign_access(vuihandler_priv->vuihandler.tx_ring_ref);
		free_vpage((uint32_t) vuihandler_priv->vuihandler.tx_ring.sring);

		vuihandler_priv->vuihandler.tx_ring_ref = GRANT_INVALID_REF;
		vuihandler_priv->vuihandler.tx_ring.sring = NULL;
	}

	if (vuihandler_priv->vuihandler.tx_irq)
		unbind_from_irqhandler(vuihandler_priv->vuihandler.tx_irq);
	vuihandler_priv->vuihandler.tx_irq = 0;
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

	vuihandler_priv_t *vuihandler_priv;

	vuihandler_priv = malloc(sizeof(vuihandler_priv_t));
	BUG_ON(!vuihandler_priv);
	memset(vuihandler_priv, 0, sizeof(vuihandler_priv_t));

	dev_set_drvdata(dev, vuihandler_priv);

	vdevfront_init(VUIHANDLER_NAME, &vuihandlerdrv);

	return 0;
}

REGISTER_DRIVER_POSTCORE("vuihandler,frontend", vuihandler_init);
