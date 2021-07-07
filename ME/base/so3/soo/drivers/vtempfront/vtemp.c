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

#include <soo/dev/vtemp.h>

typedef struct {

	/* Must be the first field */
	vtemp_t vtemp;

} vtemp_priv_t;

static struct vbus_device *vtemp_dev = NULL;

static bool thread_created = false;

irq_return_t vtemp_interrupt(int irq, void *dev_id) {
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vtemp_priv_t *vtemp_priv = dev_get_drvdata(vdev->dev);
	vtemp_response_t *ring_rsp;

	DBG("%s, %d\n", __func__, ME_domID());

	while ((ring_rsp = vtemp_get_ring_response(&vtemp_priv->vtemp.ring)) != NULL) {

		DBG("%s, cons=%d\n", __func__, i);

		/* Do something with the response */

#if 0 /* Debug */
		lprintk("## Got from the backend: %s\n", ring_rsp->buffer);
#endif
	}

	return IRQ_COMPLETED;
}

#if 0
static int i1 = 1, i2 = 2;
/*
 * The following function is given as an example.
 *
 */

void vtemp_generate_request(char *buffer) {
	vtemp_request_t *ring_req;
	vtemp_priv_t *vtemp_priv;

	if (!vtemp_dev)
		return ;

	vtemp_priv = (vtemp_priv_t *) dev_get_drvdata(vtemp_dev->dev);

	vdevfront_processing_begin(vtemp_dev);

	/*
	 * Try to generate a new request to the backend
	 */
	if (!RING_REQ_FULL(&vtemp_priv->vtemp.ring)) {
		ring_req = vtemp_new_ring_request(&vtemp_priv->vtemp.ring);

		memcpy(ring_req->buffer, buffer, VTEMP_PACKET_SIZE);

		vtemp_ring_request_ready(&vtemp_priv->vtemp.ring);

		notify_remote_via_virq(vtemp_priv->vtemp.irq);
	}

	vdevfront_processing_end(vtemp_dev);
}
#endif

static void vtemp_probe(struct vbus_device *vdev) {
	int res;
	unsigned int evtchn;
	vtemp_sring_t *sring;
	struct vbus_transaction vbt;
	vtemp_priv_t *vtemp_priv;

	lprintk("[ %s ] FRONTEND PROBE CALLED \n", VTEMP_NAME);

	DBG0("[" VTEMP_NAME "] Frontend probe\n");

	if (vdev->state == VbusStateConnected)
		return ;

	vtemp_priv = dev_get_drvdata(vdev->dev);

	vtemp_dev = vdev;

	DBG("Frontend: Setup ring\n");

	/* Prepare to set up the ring. */

	vtemp_priv->vtemp.ring_ref = GRANT_INVALID_REF;

	/* Allocate an event channel associated to the ring */
	res = vbus_alloc_evtchn(vdev, &evtchn);
	BUG_ON(res);

	res = bind_evtchn_to_irq_handler(evtchn, vtemp_interrupt, NULL, vdev);
	if (res <= 0) {
		lprintk("%s - line %d: Binding event channel failed for device %s\n", __func__, __LINE__, vdev->nodename);
		BUG();
	}

	vtemp_priv->vtemp.evtchn = evtchn;
	vtemp_priv->vtemp.irq = res;

	/* Allocate a shared page for the ring */
	sring = (vtemp_sring_t *) get_free_vpage();
	if (!sring) {
		lprintk("%s - line %d: Allocating shared ring failed for device %s\n", __func__, __LINE__, vdev->nodename);
		BUG();
	}

	SHARED_RING_INIT(sring);
	FRONT_RING_INIT(&vtemp_priv->vtemp.ring, sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	res = vbus_grant_ring(vdev, phys_to_pfn(virt_to_phys_pt((uint32_t) vtemp_priv->vtemp.ring.sring)));
	if (res < 0)
		BUG();

	vtemp_priv->vtemp.ring_ref = res;

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "ring-ref", "%u", vtemp_priv->vtemp.ring_ref);
	vbus_printf(vbt, vdev->nodename, "ring-evtchn", "%u", vtemp_priv->vtemp.evtchn);

	vbus_transaction_end(vbt);

}

/* At this point, the FE is not connected. */
static void vtemp_reconfiguring(struct vbus_device *vdev) {
	int res;
	struct vbus_transaction vbt;
	vtemp_priv_t *vtemp_priv = dev_get_drvdata(vdev->dev);

	DBG0("[" VTEMP_NAME "] Frontend reconfiguring\n");
	/* The shared page already exists */
	/* Re-init */

	gnttab_end_foreign_access_ref(vtemp_priv->vtemp.ring_ref);

	DBG("Frontend: Setup ring\n");

	/* Prepare to set up the ring. */

	vtemp_priv->vtemp.ring_ref = GRANT_INVALID_REF;

	SHARED_RING_INIT(vtemp_priv->vtemp.ring.sring);
	FRONT_RING_INIT(&vtemp_priv->vtemp.ring, (&vtemp_priv->vtemp.ring)->sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	res = vbus_grant_ring(vdev, phys_to_pfn(virt_to_phys_pt((uint32_t) vtemp_priv->vtemp.ring.sring)));
	if (res < 0)
		BUG();

	vtemp_priv->vtemp.ring_ref = res;

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "ring-ref", "%u", vtemp_priv->vtemp.ring_ref);
	vbus_printf(vbt, vdev->nodename, "ring-evtchn", "%u", vtemp_priv->vtemp.evtchn);

	vbus_transaction_end(vbt);
}

static void vtemp_shutdown(struct vbus_device *vdev) {

	DBG0("[" VTEMP_NAME "] Frontend shutdown\n");
}

static void vtemp_closed(struct vbus_device *vdev) {
	vtemp_priv_t *vtemp_priv = dev_get_drvdata(vdev->dev);

	DBG0("[" VTEMP_NAME "] Frontend close\n");

	/**
	 * Free the ring and deallocate the proper data.
	 */

	/* Free resources associated with old device channel. */
	if (vtemp_priv->vtemp.ring_ref != GRANT_INVALID_REF) {
		gnttab_end_foreign_access(vtemp_priv->vtemp.ring_ref);
		free_vpage((uint32_t) vtemp_priv->vtemp.ring.sring);

		vtemp_priv->vtemp.ring_ref = GRANT_INVALID_REF;
		vtemp_priv->vtemp.ring.sring = NULL;
	}

	if (vtemp_priv->vtemp.irq)
		unbind_from_irqhandler(vtemp_priv->vtemp.irq);

	vtemp_priv->vtemp.irq = 0;
}

static void vtemp_suspend(struct vbus_device *vdev) {

	DBG0("[" VTEMP_NAME "] Frontend suspend\n");
}

static void vtemp_resume(struct vbus_device *vdev) {

	DBG0("[" VTEMP_NAME "] Frontend resume\n");
}

#if 0
int notify_fn(void *arg) {
	char buffer[VTEMP_PACKET_SIZE];

	while (1) {
		msleep(50);

		sprintf(buffer, "Hello %d\n", *((int *) arg));

		vtemp_generate_request(buffer);
	}

	return 0;
}
#endif

static void vtemp_connected(struct vbus_device *vdev) {
	vtemp_priv_t *vtemp_priv = dev_get_drvdata(vdev->dev);

	DBG0("[" VTEMP_NAME "] Frontend connected\n");

	/* Force the processing of pending requests, if any */
	notify_remote_via_virq(vtemp_priv->vtemp.irq);

	if (!thread_created) {
		thread_created = true;
#if 0
		kernel_thread(notify_fn, "notify_th", &i1, 0);
		//kernel_thread(notify_fn, "notify_th2", &i2, 0);
#endif
	}
}

vdrvfront_t vtempdrv = {
	.probe = vtemp_probe,
	.reconfiguring = vtemp_reconfiguring,
	.shutdown = vtemp_shutdown,
	.closed = vtemp_closed,
	.suspend = vtemp_suspend,
	.resume = vtemp_resume,
	.connected = vtemp_connected
};

static int vtemp_init(dev_t *dev) {
	vtemp_priv_t *vtemp_priv;

	vtemp_priv = malloc(sizeof(vtemp_priv_t));
	BUG_ON(!vtemp_priv);

	memset(vtemp_priv, 0, sizeof(vtemp_priv_t));

	dev_set_drvdata(dev, vtemp_priv);

	lprintk("[ %s ] FRONTEND INIT CALLED \n", VTEMP_NAME);

	vdevfront_init(VTEMP_NAME, &vtempdrv);

	return 0;
}

REGISTER_DRIVER_POSTCORE("vtemp,frontend", vtemp_init);
