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

#include <soo/dev/vvalve.h>

typedef struct {

	/* Must be the first field */
	vvalve_t vvalve;

	/* contains data receives from backend */
	vvalve_data_t vvalve_data;

} vvalve_priv_t;




static struct vbus_device *vvalve_dev = NULL;

static bool thread_created = false;

irq_return_t vvalve_interrupt(int irq, void *dev_id) {
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vvalve_priv_t *vvalve_priv = dev_get_drvdata(vdev->dev);
	vvalve_response_t *ring_rsp;

	DBG("%s, %d\n", __func__, ME_domID());

	while ((ring_rsp = vvalve_get_ring_response(&vvalve_priv->vvalve.ring)) != NULL) {

		DBG("%s, cons=%d\n", __func__, i);

		/* Do something with the response */

#if 0 /* Debug */
		lprintk("## Got from the backend: %s\n", ring_rsp->buffer);
#endif
	}

	return IRQ_COMPLETED;
}

#if 1
/*
 * The following function is given as an example.
 *
 */

void vvalve_generate_request(char *buffer) {
	vvalve_request_t *ring_req;
	vvalve_priv_t *vvalve_priv;

	if (!vvalve_dev)
		return ;

	vvalve_priv = (vvalve_priv_t *) dev_get_drvdata(vvalve_dev->dev);

	vdevfront_processing_begin(vvalve_dev);

	/*
	 * Try to generate a new request to the backend
	 */
	if (!RING_REQ_FULL(&vvalve_priv->vvalve.ring)) {
		ring_req = vvalve_new_ring_request(&vvalve_priv->vvalve.ring);

		memcpy(ring_req->buffer, buffer, VVALVE_PACKET_SIZE);

		vvalve_ring_request_ready(&vvalve_priv->vvalve.ring);

		notify_remote_via_virq(vvalve_priv->vvalve.irq);
	}

	vdevfront_processing_end(vvalve_dev);
}
#endif

static void vvalve_probe(struct vbus_device *vdev) {
	int res;
	unsigned int evtchn;
	vvalve_sring_t *sring;
	struct vbus_transaction vbt;
	vvalve_priv_t *vvalve_priv;

	lprintk("[ %s ] FRONTEND PROBE CALLED \n", VVALVE_NAME);

	DBG0("[" VVALVE_NAME "] Frontend probe\n");

	if (vdev->state == VbusStateConnected)
		return ;

	vvalve_priv = dev_get_drvdata(vdev->dev);

	vvalve_dev = vdev;

	DBG("Frontend: Setup ring\n");

	/* Prepare to set up the ring. */

	vvalve_priv->vvalve.ring_ref = GRANT_INVALID_REF;

	/* Allocate an event channel associated to the ring */
	res = vbus_alloc_evtchn(vdev, &evtchn);
	BUG_ON(res);

	res = bind_evtchn_to_irq_handler(evtchn, vvalve_interrupt, NULL, vdev);
	if (res <= 0) {
		lprintk("%s - line %d: Binding event channel failed for device %s\n", __func__, __LINE__, vdev->nodename);
		BUG();
	}

	vvalve_priv->vvalve.evtchn = evtchn;
	vvalve_priv->vvalve.irq = res;

	/* Allocate a shared page for the ring */
	sring = (vvalve_sring_t *) get_free_vpage();
	if (!sring) {
		lprintk("%s - line %d: Allocating shared ring failed for device %s\n", __func__, __LINE__, vdev->nodename);
		BUG();
	}

	SHARED_RING_INIT(sring);
	FRONT_RING_INIT(&vvalve_priv->vvalve.ring, sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	res = vbus_grant_ring(vdev, phys_to_pfn(virt_to_phys_pt((uint32_t) vvalve_priv->vvalve.ring.sring)));
	if (res < 0)
		BUG();

	vvalve_priv->vvalve.ring_ref = res;

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "ring-ref", "%u", vvalve_priv->vvalve.ring_ref);
	vbus_printf(vbt, vdev->nodename, "ring-evtchn", "%u", vvalve_priv->vvalve.evtchn);

	vbus_transaction_end(vbt);

}

/* At this point, the FE is not connected. */
static void vvalve_reconfiguring(struct vbus_device *vdev) {
	int res;
	struct vbus_transaction vbt;
	vvalve_priv_t *vvalve_priv = dev_get_drvdata(vdev->dev);

	DBG0("[" VVALVE_NAME "] Frontend reconfiguring\n");
	/* The shared page already exists */
	/* Re-init */

	gnttab_end_foreign_access_ref(vvalve_priv->vvalve.ring_ref);

	DBG("Frontend: Setup ring\n");

	/* Prepare to set up the ring. */

	vvalve_priv->vvalve.ring_ref = GRANT_INVALID_REF;

	SHARED_RING_INIT(vvalve_priv->vvalve.ring.sring);
	FRONT_RING_INIT(&vvalve_priv->vvalve.ring, (&vvalve_priv->vvalve.ring)->sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	res = vbus_grant_ring(vdev, phys_to_pfn(virt_to_phys_pt((uint32_t) vvalve_priv->vvalve.ring.sring)));
	if (res < 0)
		BUG();

	vvalve_priv->vvalve.ring_ref = res;

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "ring-ref", "%u", vvalve_priv->vvalve.ring_ref);
	vbus_printf(vbt, vdev->nodename, "ring-evtchn", "%u", vvalve_priv->vvalve.evtchn);

	vbus_transaction_end(vbt);
}

static void vvalve_shutdown(struct vbus_device *vdev) {

	DBG0("[" VVALVE_NAME "] Frontend shutdown\n");
}

static void vvalve_closed(struct vbus_device *vdev) {
	vvalve_priv_t *vvalve_priv = dev_get_drvdata(vdev->dev);

	DBG0("[" VVALVE_NAME "] Frontend close\n");

	/**
	 * Free the ring and deallocate the proper data.
	 */

	/* Free resources associated with old device channel. */
	if (vvalve_priv->vvalve.ring_ref != GRANT_INVALID_REF) {
		gnttab_end_foreign_access(vvalve_priv->vvalve.ring_ref);
		free_vpage((uint32_t) vvalve_priv->vvalve.ring.sring);

		vvalve_priv->vvalve.ring_ref = GRANT_INVALID_REF;
		vvalve_priv->vvalve.ring.sring = NULL;
	}

	if (vvalve_priv->vvalve.irq)
		unbind_from_irqhandler(vvalve_priv->vvalve.irq);

	vvalve_priv->vvalve.irq = 0;
}

static void vvalve_suspend(struct vbus_device *vdev) {

	DBG0("[" VVALVE_NAME "] Frontend suspend\n");
}

static void vvalve_resume(struct vbus_device *vdev) {

	DBG0("[" VVALVE_NAME "] Frontend resume\n");
}

#if 0
int notify_fn(void *arg) {
	char buffer[VVALVE_PACKET_SIZE];

	while (1) {
		msleep(50);

		sprintf(buffer, "Hello %d\n", *((int *) arg));

		vvalve_generate_request(buffer);
	}

	return 0;
}
#endif

static void vvalve_connected(struct vbus_device *vdev) {
	vvalve_priv_t *vvalve_priv = dev_get_drvdata(vdev->dev);

	DBG0("[" VVALVE_NAME "] Frontend connected\n");

	/* Force the processing of pending requests, if any */
	notify_remote_via_virq(vvalve_priv->vvalve.irq);

	if (!thread_created) {
		thread_created = true;
#if 0
		kernel_thread(notify_fn, "notify_th", &i1, 0);
		//kernel_thread(notify_fn, "notify_th2", &i2, 0);
#endif
	}
}

vdrvfront_t vvalvedrv = {
	.probe = vvalve_probe,
	.reconfiguring = vvalve_reconfiguring,
	.shutdown = vvalve_shutdown,
	.closed = vvalve_closed,
	.suspend = vvalve_suspend,
	.resume = vvalve_resume,
	.connected = vvalve_connected
};

static int vvalve_init(dev_t *dev) {
	vvalve_priv_t *vvalve_priv;

	vvalve_priv = malloc(sizeof(vvalve_priv_t));
	BUG_ON(!vvalve_priv);

	memset(vvalve_priv, 0, sizeof(vvalve_priv_t));

	dev_set_drvdata(dev, vvalve_priv);

	lprintk("[ %s ] FRONTEND INIT CALLED \n", VVALVE_NAME);

	vdevfront_init(VVALVE_NAME, &vvalvedrv);

	return 0;
}

REGISTER_DRIVER_POSTCORE("vvalve,frontend", vvalve_init);
