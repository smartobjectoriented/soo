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

#include <soo/dev/vdummy.h>

static struct vbus_device *vdummy_dev = NULL;


static bool thread_created = false;

irq_return_t vdummy_interrupt(int irq, void *dev_id) {
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vdummy_t *vdummy = to_vdummy(vdev);
	vdummy_response_t *ring_rsp;

	DBG("%s, %d\n", __func__, ME_domID());

	while ((ring_rsp = vdummy_get_ring_response(&vdummy->ring)) != NULL) {

		DBG("%s, cons=%d\n", __func__, i);

		/* Do something with the response */
	}

	return IRQ_COMPLETED;
}

#if 0
static int i1 = 1, i2 = 2;
/*
 * The following function is given as an example.
 *
 */

void vdummy_generate_request(char *buffer) {
	vdummy_request_t *ring_req;
	vdummy_t *vdummy;

	if (!vdummy_dev)
		return ;

	vdummy = to_vdummy(vdummy_dev);

	vdevfront_processing_begin(vdummy_dev);

	/*
	 * Try to generate a new request to the backend
	 */
	if (!RING_REQ_FULL(&vdummy->ring)) {
		ring_req = vdummy_new_ring_request(&vdummy->ring);

		memcpy(ring_req->buffer, buffer, VDUMMY_PACKET_SIZE);

		vdummy_ring_request_ready(&vdummy->ring);

		notify_remote_via_irq(vdummy->irq);
	}

	vdevfront_processing_end(vdummy_dev);
}
#endif

void vdummy_probe(struct vbus_device *vdev) {
	int res;
	unsigned int evtchn;
	vdummy_sring_t *sring;
	struct vbus_transaction vbt;
	vdummy_t *vdummy;

	DBG0("[" VDUMMY_NAME "] Frontend probe\n");

	if (vdev->state == VbusStateConnected)
		return ;

	vdummy_dev = vdev;

	vdummy = malloc(sizeof(vdummy_t));
	BUG_ON(!vdummy);
	memset(vdummy, 0, sizeof(vdummy_t));

	dev_set_drvdata(vdev->dev, &vdummy->vdevfront);

	DBG("Frontend: Setup ring\n");

	/* Prepare to set up the ring. */

	vdummy->ring_ref = GRANT_INVALID_REF;

	/* Allocate an event channel associated to the ring */
	res = vbus_alloc_evtchn(vdev, &evtchn);
	BUG_ON(res);

	res = bind_evtchn_to_irq_handler(evtchn, vdummy_interrupt, NULL, vdev);
	if (res <= 0) {
		lprintk("%s - line %d: Binding event channel failed for device %s\n", __func__, __LINE__, vdev->nodename);
		BUG();
	}

	vdummy->evtchn = evtchn;
	vdummy->irq = res;

	/* Allocate a shared page for the ring */
	sring = (vdummy_sring_t *) get_free_vpage();
	if (!sring) {
		lprintk("%s - line %d: Allocating shared ring failed for device %s\n", __func__, __LINE__, vdev->nodename);
		BUG();
	}

	SHARED_RING_INIT(sring);
	FRONT_RING_INIT(&vdummy->ring, sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	res = vbus_grant_ring(vdev, phys_to_pfn(virt_to_phys_pt((uint32_t) vdummy->ring.sring)));
	if (res < 0)
		BUG();

	vdummy->ring_ref = res;

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "ring-ref", "%u", vdummy->ring_ref);
	vbus_printf(vbt, vdev->nodename, "ring-evtchn", "%u", vdummy->evtchn);

	vbus_transaction_end(vbt);

}

/* At this point, the FE is not connected. */
void vdummy_reconfiguring(struct vbus_device *vdev) {
	int res;
	struct vbus_transaction vbt;
	vdummy_t *vdummy = to_vdummy(vdev);

	DBG0("[" VDUMMY_NAME "] Frontend reconfiguring\n");
	/* The shared page already exists */
	/* Re-init */

	gnttab_end_foreign_access_ref(vdummy->ring_ref);

	DBG("Frontend: Setup ring\n");

	/* Prepare to set up the ring. */

	vdummy->ring_ref = GRANT_INVALID_REF;

	SHARED_RING_INIT(vdummy->ring.sring);
	FRONT_RING_INIT(&vdummy->ring, (&vdummy->ring)->sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	res = vbus_grant_ring(vdev, phys_to_pfn(virt_to_phys_pt((uint32_t) vdummy->ring.sring)));
	if (res < 0)
		BUG();

	vdummy->ring_ref = res;

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "ring-ref", "%u", vdummy->ring_ref);
	vbus_printf(vbt, vdev->nodename, "ring-evtchn", "%u", vdummy->evtchn);

	vbus_transaction_end(vbt);
}

void vdummy_shutdown(struct vbus_device *vdev) {

	DBG0("[" VDUMMY_NAME "] Frontend shutdown\n");
}

void vdummy_closed(struct vbus_device *vdev) {
	vdummy_t *vdummy = to_vdummy(vdev);

	DBG0("[" VDUMMY_NAME "] Frontend close\n");

	/**
	 * Free the ring and deallocate the proper data.
	 */

	/* Free resources associated with old device channel. */
	if (vdummy->ring_ref != GRANT_INVALID_REF) {
		gnttab_end_foreign_access(vdummy->ring_ref);
		free_vpage((uint32_t) vdummy->ring.sring);

		vdummy->ring_ref = GRANT_INVALID_REF;
		vdummy->ring.sring = NULL;
	}

	if (vdummy->irq)
		unbind_from_irqhandler(vdummy->irq);

	vdummy->irq = 0;
}

void vdummy_suspend(struct vbus_device *vdev) {

	DBG0("[" VDUMMY_NAME "] Frontend suspend\n");
}

void vdummy_resume(struct vbus_device *vdev) {

	DBG0("[" VDUMMY_NAME "] Frontend resume\n");
}

#if 0
int notify_fn(void *arg) {
	char buffer[VDUMMY_PACKET_SIZE];

	while (1) {
		msleep(50);

		sprintf(buffer, "Hello %d\n", *((int *) arg));

		vdummy_generate_request(buffer);
	}

	return 0;
}
#endif

void vdummy_connected(struct vbus_device *vdev) {
	vdummy_t *vdummy = to_vdummy(vdev);

	DBG0("[" VDUMMY_NAME "] Frontend connected\n");

	/* Force the processing of pending requests, if any */
	notify_remote_via_irq(vdummy->irq);

	if (!thread_created) {
		thread_created = true;
#if 0
		kernel_thread(notify_fn, "notify_th", &i1, 0);
		kernel_thread(notify_fn, "notify_th2", &i2, 0);
#endif
	}
}

vdrvfront_t vdummydrv = {
	.probe = vdummy_probe,
	.reconfiguring = vdummy_reconfiguring,
	.shutdown = vdummy_shutdown,
	.closed = vdummy_closed,
	.suspend = vdummy_suspend,
	.resume = vdummy_resume,
	.connected = vdummy_connected
};

static int vdummy_init(dev_t *dev) {

	vdevfront_init(VDUMMY_NAME, &vdummydrv);

	return 0;
}

REGISTER_DRIVER_POSTCORE("vdummy,frontend", vdummy_init);
