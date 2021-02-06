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

#include <heap.h>
#include <mutex.h>
#include <delay.h>
#include <memory.h>

#include <asm/mmu.h>

#include <device/driver.h>

#include <soo/evtchn.h>
#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/console.h>
#include <soo/debug.h>

#include <soo/dev/vuart.h>

/* Our unique uart instance. */
static struct vbus_device *vdev_console = NULL;

irq_return_t vuart_interrupt(int irq, void *dev_id) {
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vuart_t *vuart = to_vuart(vdev);

	complete(&vuart->reader_wait);

	return IRQ_COMPLETED;
}

/*
 * Can be used outside the frontend by other subsystems.
 */
bool vuart_ready(void) {
	return (vdev_console && (vdev_console->state == VbusStateConnected));
}

/**
 * Send a string on the vuart device.
 */
void vuart_write(char *buffer, int count) {
	int i;
	vuart_request_t *ring_req;
	vuart_t *vuart;

	if (!vdev_console)
		return ;

	vuart = to_vuart(vdev_console);

	vdevfront_processing_begin(vdev_console);

	for (i = 0; i < count; i++) {
		ring_req = vuart_new_ring_request(&vuart->ring);

		ring_req->c = buffer[i];
	}

	vuart_ring_request_ready(&vuart->ring);

	notify_remote_via_irq(vuart->irq);

	vdevfront_processing_end(vdev_console);

}

char vuart_read_char(void) {
	vuart_response_t *ring_rsp;
	vuart_t *vuart;

	if (!vdev_console)
		return 0;

	vuart = to_vuart(vdev_console);

	vdevfront_processing_begin(vdev_console);
	ring_rsp = vuart_get_ring_response(&vuart->ring);

	if (!ring_rsp) {
		vdevfront_processing_end(vdev_console);

		wait_for_completion(&vuart->reader_wait);
		vdevfront_processing_begin(vdev_console);

		ring_rsp = vuart_get_ring_response(&vuart->ring);
	}

	vdevfront_processing_end(vdev_console);

	return ring_rsp->c;
}

void vuart_probe(struct vbus_device *vdev) {
	int res;
	unsigned int evtchn;
	vuart_sring_t *sring;
	struct vbus_transaction vbt;
	vuart_t *vuart;

	DBG0("[vuart] Frontend probe\n");

	if (vdev->state == VbusStateConnected)
		return ;

	vuart = malloc(sizeof(vuart_t));
	BUG_ON(!vuart);
	memset(vuart, 0, sizeof(vuart_t));

	/* Local instance */
	vdev_console = vdev;

	dev_set_drvdata(vdev->dev, &vuart->vdevfront);

	init_completion(&vuart->reader_wait);

	DBG("Frontend: Setup ring\n");

	/* Prepare to set up the ring. */

	vuart->ring_ref = GRANT_INVALID_REF;

	/* Allocate an event channel associated to the ring */
	res = vbus_alloc_evtchn(vdev, &evtchn);
	BUG_ON(res);

	res = bind_evtchn_to_irq_handler(evtchn, vuart_interrupt, NULL, vdev);
	if (res <= 0) {
		lprintk("%s - line %d: Binding event channel failed for device %s\n", __func__, __LINE__, vdev->nodename);
		BUG();
	}

	vuart->evtchn = evtchn;
	vuart->irq = res;

	/* Allocate a shared page for the ring */
	sring = (vuart_sring_t *) get_free_vpage();
	if (!sring) {
		lprintk("%s - line %d: Allocating shared ring failed for device %s\n", __func__, __LINE__, vdev->nodename);
		BUG();
	}

	SHARED_RING_INIT(sring);
	FRONT_RING_INIT(&vuart->ring, sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	res = vbus_grant_ring(vdev, phys_to_pfn(virt_to_phys_pt((uint32_t) vuart->ring.sring)));
	if (res < 0)
		BUG();

	vuart->ring_ref = res;

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "ring-ref", "%u", vuart->ring_ref);
	vbus_printf(vbt, vdev->nodename, "ring-evtchn", "%u", vuart->evtchn);

	vbus_transaction_end(vbt);

}

/* At this point, the FE is not connected. */
void vuart_reconfiguring(struct vbus_device *vdev) {
	int res;
	struct vbus_transaction vbt;
	vuart_t *vuart = to_vuart(vdev);

	DBG0("[vuart] Frontend reconfiguring\n");
	/* The shared page already exists */
	/* Re-init */

	gnttab_end_foreign_access_ref(vuart->ring_ref);

	DBG("Frontend: Setup ring\n");

	/* Prepare to set up the ring. */

	vuart->ring_ref = GRANT_INVALID_REF;

	SHARED_RING_INIT(vuart->ring.sring);
	FRONT_RING_INIT(&vuart->ring, (&vuart->ring)->sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	res = vbus_grant_ring(vdev, phys_to_pfn(virt_to_phys_pt((uint32_t) vuart->ring.sring)));
	if (res < 0)
		BUG();

	vuart->ring_ref = res;

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "ring-ref", "%u", vuart->ring_ref);
	vbus_printf(vbt, vdev->nodename, "ring-evtchn", "%u", vuart->evtchn);

	vbus_transaction_end(vbt);
}

void vuart_shutdown(struct vbus_device *vdev) {

	DBG0("[vuart] Frontend shutdown\n");
}

void vuart_closed(struct vbus_device *vdev) {
	vuart_t *vuart = to_vuart(vdev);

	DBG0("[vuart] Frontend close\n");

	/**
	 * Free the ring and deallocate the proper data.
	 */

	/* Free resources associated with old device channel. */
	if (vuart->ring_ref != GRANT_INVALID_REF) {
		gnttab_end_foreign_access(vuart->ring_ref);
		free_vpage((uint32_t) vuart->ring.sring);

		vuart->ring_ref = GRANT_INVALID_REF;
		vuart->ring.sring = NULL;
	}

	if (vuart->irq)
		unbind_from_irqhandler(vuart->irq);

	vuart->irq = 0;
}

void vuart_suspend(struct vbus_device *vdev) {

	DBG0("[vuart] Frontend suspend\n");
}

void vuart_resume(struct vbus_device *vdev) {

	DBG0("[vuart] Frontend resume\n");
}

void vuart_connected(struct vbus_device *vdev) {

	DBG0("[vuart] Frontend connected\n");

}

vdrvfront_t vuartdrv = {
	.probe = vuart_probe,
	.reconfiguring = vuart_reconfiguring,
	.shutdown = vuart_shutdown,
	.closed = vuart_closed,
	.suspend = vuart_suspend,
	.resume = vuart_resume,
	.connected = vuart_connected
};

static int vuart_init(dev_t *dev) {

	vdevfront_init(VUART_NAME, &vuartdrv);

	return 0;
}

REGISTER_DRIVER_POSTCORE("vuart,frontend", vuart_init);
