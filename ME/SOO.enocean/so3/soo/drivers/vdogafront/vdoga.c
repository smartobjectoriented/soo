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

#include <soo/dev/vdoga.h>


static bool thread_created = false;

irq_return_t vdoga_interrupt(int irq, void *dev_id) {
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vdoga_t *vdoga = to_vdoga(vdev);
	vdoga_response_t *ring_rsp;

	DBG("%s, %d\n", __func__, ME_domID());

	while ((ring_rsp = vdoga_ring_response(&vdoga->ring)) != NULL) {

		DBG("%s, cons=%d\n", __func__, i);

		/* Do something with the response */
	}

	return IRQ_COMPLETED;
}

/*
 * The following function is given as an example.
 *
 */
#if 0
void vdummy_generate_request(char *buffer) {
	vdummy_request_t *ring_req;

	vdevfront_processing_start();

	/*
	 * Try to generate a new request to the backend
	 */
	if (!RING_FULL(&vdummy.ring)) {
		ring_req = RING_GET_REQUEST(&vdummy.ring, vdummy.ring.req_prod_pvt);

		memcpy(ring_req->buffer, buffer, VDUMMY_PACKET_SIZE);

		/* Fill in the ring_req structure */

		/* Make sure the other end "sees" the request when updating the index */
		dmb();

		vdummy.ring.req_prod_pvt++;

		RING_PUSH_REQUESTS(&vdummy.ring);

		notify_remote_via_irq(vdummy.irq);
	}

	vdevfront_processing_end();
}
#endif

void vdoga_probe(struct vbus_device *vdev) {
	int res;
	unsigned int evtchn;
	vdoga_sring_t *sring;
	struct vbus_transaction vbt;
	vdoga_t *vdoga;

	DBG0("[" VDOGA_NAME "] Frontend probe\n");

	if (vdev->state == VbusStateConnected)
		return ;


	vdoga = malloc(sizeof(vdoga_t));
	BUG_ON(!vdoga);
	memset(vdoga, 0, sizeof(vdoga_t));

	dev_set_drvdata(vdev->dev, &vdoga->vdevfront);

	DBG("Frontend: Setup ring\n");

	/* Prepare to set up the ring. */

	vdoga->ring_ref = GRANT_INVALID_REF;

	/* Allocate an event channel associated to the ring */
	res = vbus_alloc_evtchn(vdev, &evtchn);
	BUG_ON(res);

	res = bind_evtchn_to_irq_handler(evtchn, vdoga_interrupt, NULL, vdev);
	if (res <= 0) {
		lprintk("%s - line %d: Binding event channel failed for device %s\n", __func__, __LINE__, vdev->nodename);
		BUG();
	}

	vdoga->evtchn = evtchn;
	vdoga->irq = res;

	/* Allocate a shared page for the ring */
	sring = (vdoga_sring_t *) get_free_vpage();
	if (!sring) {
		lprintk("%s - line %d: Allocating shared ring failed for device %s\n", __func__, __LINE__, vdev->nodename);
		BUG();
	}

	SHARED_RING_INIT(sring);
	FRONT_RING_INIT(&vdoga->ring, sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	res = vbus_grant_ring(vdev, phys_to_pfn(virt_to_phys_pt((uint32_t) vdoga->ring.sring)));
	if (res < 0)
		BUG();

	vdoga->ring_ref = res;

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "ring-ref", "%u", vdoga->ring_ref);
	vbus_printf(vbt, vdev->nodename, "ring-evtchn", "%u", vdoga->evtchn);

	vbus_transaction_end(vbt);

}

/* At this point, the FE is not connected. */
void vdoga_reconfiguring(struct vbus_device *vdev) {
	int res;
	struct vbus_transaction vbt;
	vdoga_t *vdoga = to_vdoga(vdev);

	DBG0("[" VDOGA_NAME "] Frontend reconfiguring\n");
	/* The shared page already exists */
	/* Re-init */

	gnttab_end_foreign_access_ref(vdoga->ring_ref);

	DBG("Frontend: Setup ring\n");

	/* Prepare to set up the ring. */

	vdoga->ring_ref = GRANT_INVALID_REF;

	SHARED_RING_INIT(vdoga->ring.sring);
	FRONT_RING_INIT(&vdoga->ring, (&vdoga->ring)->sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	res = vbus_grant_ring(vdev, phys_to_pfn(virt_to_phys_pt((uint32_t) vdoga->ring.sring)));
	if (res < 0)
		BUG();

	vdoga->ring_ref = res;

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "ring-ref", "%u", vdoga->ring_ref);
	vbus_printf(vbt, vdev->nodename, "ring-evtchn", "%u", vdoga->evtchn);

	vbus_transaction_end(vbt);
}

void vdoga_shutdown(struct vbus_device *vdev) {

	DBG0("[" VDOGA_NAME "] Frontend shutdown\n");
}

void vdoga_closed(struct vbus_device *vdev) {
	vdoga_t *vdoga = to_vdoga(vdev);

	DBG0("[" VDOGA_NAME "] Frontend close\n");

	/**
	 * Free the ring and deallocate the proper data.
	 */

	/* Free resources associated with old device channel. */
	if (vdoga->ring_ref != GRANT_INVALID_REF) {
		gnttab_end_foreign_access(vdoga->ring_ref);
		free_vpage((uint32_t) vdoga->ring.sring);

		vdoga->ring_ref = GRANT_INVALID_REF;
		vdoga->ring.sring = NULL;
	}

	if (vdoga->irq)
		unbind_from_irqhandler(vdoga->irq);

	vdoga->irq = 0;
}

void vdoga_suspend(struct vbus_device *vdev) {

	DBG0("[" VDOGA_NAME "] Frontend suspend\n");
}

void vdoga_resume(struct vbus_device *vdev) {

	DBG0("[" VDOGA_NAME "] Frontend resume\n");
}

#if 0
int notify_fn(void *arg) {

	while (1) {
		msleep(50);

		vdummy_start();

		/* Make sure the backend is connected and ready for interactions. */

		notify_remote_via_irq(vdummy.irq);

		vdummy_end();

	}

	return 0;
}
#endif

void vdoga_connected(struct vbus_device *vdev) {
	vdoga_t *vdoga = to_vdoga(vdev);

	DBG0("[" VDOGA_NAME "] Frontend connected\n");

	/* Force the processing of pending requests, if any */
	notify_remote_via_irq(vdoga->irq);

	if (!thread_created) {
		thread_created = true;
#if 0
		kernel_thread(notify_fn, "notify_th", NULL, 0);
#endif
	}
}

vdrvfront_t vdogadrv = {
	.probe = vdoga_probe,
	.reconfiguring = vdoga_reconfiguring,
	.shutdown = vdoga_shutdown,
	.closed = vdoga_closed,
	.suspend = vdoga_suspend,
	.resume = vdoga_resume,
	.connected = vdoga_connected
};

static int vdoga_init(dev_t *dev) {

	vdevfront_init(VDOGA_NAME, &vdogadrv);

	return 0;
}

REGISTER_DRIVER_POSTCORE("vdoga,frontend", vdoga_init);
