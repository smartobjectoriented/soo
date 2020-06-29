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

#if 1
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

#include <soo/dev/vnet.h>

static bool thread_created = false;

irq_return_t vnet_interrupt(int irq, void *dev_id) {
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vnet_t *vnet = to_vnet(vdev);
	vnet_response_t *ring_rsp;

	DBG("%s, %d\n", __func__, ME_domID());

	while ((ring_rsp = vnet_ring_response(&vnet->ring)) != NULL) {

		DBG("%s, rsp=%p\n", __func__, ring_rsp);

		/* Do something with the response */
	}

	return IRQ_COMPLETED;
}


void vnet_lwip_send(){


}


/*
 * The following function is given as an example.
 *
 */
#if 0
void vnet_generate_request(char *buffer) {
	vnet_request_t *ring_req;

	vdevfront_processing_start();

	/*
	 * Try to generate a new request to the backend
	 */
	if (!RING_FULL(&vnet.ring)) {
		ring_req = RING_GET_REQUEST(&vnet.ring, vnet.ring.req_prod_pvt);

		memcpy(ring_req->buffer, buffer, VNET_PACKET_SIZE);

		/* Fill in the ring_req structure */

		/* Make sure the other end "sees" the request when updating the index */
		dmb();

		vnet.ring.req_prod_pvt++;

		RING_PUSH_REQUESTS(&vnet.ring);

		notify_remote_via_irq(vnet.irq);
	}

	vdevfront_processing_end();
}
#endif

void vnet_probe(struct vbus_device *vdev) {
	int res;
	unsigned int evtchn;
	vnet_sring_t *sring;
	struct vbus_transaction vbt;
	vnet_t *vnet;

	DBG0("[" VNET_NAME "] Frontend probe\n");

	if (vdev->state == VbusStateConnected)
		return ;


	vnet = malloc(sizeof(vnet_t));
	BUG_ON(!vnet);
	memset(vnet, 0, sizeof(vnet_t));

	dev_set_drvdata(vdev->dev, &vnet->vdevfront);

	DBG("Frontend: Setup ring\n");

	/* Prepare to set up the ring. */

	vnet->ring_ref = GRANT_INVALID_REF;

	/* Allocate an event channel associated to the ring */
	res = vbus_alloc_evtchn(vdev, &evtchn);
	BUG_ON(res);

	res = bind_evtchn_to_irq_handler(evtchn, vnet_interrupt, NULL, vdev);
	if (res <= 0) {
		lprintk("%s - line %d: Binding event channel failed for device %s\n", __func__, __LINE__, vdev->nodename);
		BUG();
	}

	vnet->evtchn = evtchn;
	vnet->irq = res;

	/* Allocate a shared page for the ring */
	sring = (vnet_sring_t *) get_free_vpage();
	if (!sring) {
		lprintk("%s - line %d: Allocating shared ring failed for device %s\n", __func__, __LINE__, vdev->nodename);
		BUG();
	}

	SHARED_RING_INIT(sring);
	FRONT_RING_INIT(&vnet->ring, sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	res = vbus_grant_ring(vdev, phys_to_pfn(virt_to_phys_pt((uint32_t) vnet->ring.sring)));
	if (res < 0)
		BUG();

	vnet->ring_ref = res;

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "ring-ref", "%u", vnet->ring_ref);
	vbus_printf(vbt, vdev->nodename, "ring-evtchn", "%u", vnet->evtchn);

	vbus_transaction_end(vbt);

}

/* At this point, the FE is not connected. */
void vnet_reconfiguring(struct vbus_device *vdev) {
	int res;
	struct vbus_transaction vbt;
	vnet_t *vnet = to_vnet(vdev);

	DBG0("[" VNET_NAME "] Frontend reconfiguring\n");
	/* The shared page already exists */
	/* Re-init */

	gnttab_end_foreign_access_ref(vnet->ring_ref);

	DBG("Frontend: Setup ring\n");

	/* Prepare to set up the ring. */

	vnet->ring_ref = GRANT_INVALID_REF;

	SHARED_RING_INIT(vnet->ring.sring);
	FRONT_RING_INIT(&vnet->ring, (&vnet->ring)->sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	res = vbus_grant_ring(vdev, phys_to_pfn(virt_to_phys_pt((uint32_t) vnet->ring.sring)));
	if (res < 0)
		BUG();

	vnet->ring_ref = res;

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "ring-ref", "%u", vnet->ring_ref);
	vbus_printf(vbt, vdev->nodename, "ring-evtchn", "%u", vnet->evtchn);

	vbus_transaction_end(vbt);
}

void vnet_shutdown(struct vbus_device *vdev) {

	DBG0("[" VNET_NAME "] Frontend shutdown\n");
}

void vnet_closed(struct vbus_device *vdev) {
	vnet_t *vnet = to_vnet(vdev);

	DBG0("[" VNET_NAME "] Frontend close\n");

	/**
	 * Free the ring and deallocate the proper data.
	 */

	/* Free resources associated with old device channel. */
	if (vnet->ring_ref != GRANT_INVALID_REF) {
		gnttab_end_foreign_access(vnet->ring_ref);
		free_vpage((uint32_t) vnet->ring.sring);

		vnet->ring_ref = GRANT_INVALID_REF;
		vnet->ring.sring = NULL;
	}

	if (vnet->irq)
		unbind_from_irqhandler(vnet->irq);

	vnet->irq = 0;
}

void vnet_suspend(struct vbus_device *vdev) {

	DBG0("[" VNET_NAME "] Frontend suspend\n");
}

void vnet_resume(struct vbus_device *vdev) {

	DBG0("[" VNET_NAME "] Frontend resume\n");
}

void vnet_connected(struct vbus_device *vdev) {
	vnet_t *vnet = to_vnet(vdev);

	DBG0("[" VNET_NAME "] Frontend connected\n");

	/* Force the processing of pending requests, if any */
	notify_remote_via_irq(vnet->irq);

	if (!thread_created) {
		thread_created = true;
	}
}

vdrvfront_t vnetdrv = {
	.probe = vnet_probe,
	.reconfiguring = vnet_reconfiguring,
	.shutdown = vnet_shutdown,
	.closed = vnet_closed,
	.suspend = vnet_suspend,
	.resume = vnet_resume,
	.connected = vnet_connected
};

static int vnet_init(dev_t *dev) {

	vdevfront_init(VNET_NAME, &vnetdrv);

	return 0;
}

REGISTER_DRIVER_POSTCORE("vnet,frontend", vnet_init);
