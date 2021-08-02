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

#include <soo/dev/venoceansw.h>

typedef struct {

	/* Must be the first field */
	venoceansw_t venoceansw;

} venoceansw_priv_t;

static struct vbus_device *venoceansw_dev = NULL;

static bool thread_created = false;

irq_return_t venoceansw_interrupt(int irq, void *dev_id) {
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	venoceansw_priv_t *venoceansw_priv = dev_get_drvdata(vdev->dev);
	venoceansw_response_t *ring_rsp;

	DBG("%s, %d\n", __func__, ME_domID());

	while ((ring_rsp = venoceansw_get_ring_response(&venoceansw_priv->venoceansw.ring)) != NULL) {

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

void venoceansw_generate_request(char *buffer) {
	venoceansw_request_t *ring_req;
	venoceansw_priv_t *venoceansw_priv;

	if (!venoceansw_dev)
		return ;

	venoceansw_priv = (venoceansw_priv_t *) dev_get_drvdata(venoceansw_dev->dev);

	vdevfront_processing_begin(venoceansw_dev);

	/*
	 * Try to generate a new request to the backend
	 */
	if (!RING_REQ_FULL(&venoceansw_priv->venoceansw.ring)) {
		ring_req = venoceansw_new_ring_request(&venoceansw_priv->venoceansw.ring);

		memcpy(ring_req->buffer, buffer, VENOCEANSW_PACKET_SIZE);

		venoceansw_ring_request_ready(&venoceansw_priv->venoceansw.ring);

		notify_remote_via_virq(venoceansw_priv->venoceansw.irq);
	}

	vdevfront_processing_end(venoceansw_dev);
}
#endif

static void venoceansw_probe(struct vbus_device *vdev) {
	int res;
	unsigned int evtchn;
	venoceansw_sring_t *sring;
	struct vbus_transaction vbt;
	venoceansw_priv_t *venoceansw_priv;

	DBG0("[" VENOCEANSW_NAME "] Frontend probe\n");

	if (vdev->state == VbusStateConnected)
		return ;

	venoceansw_priv = dev_get_drvdata(vdev->dev);

	venoceansw_dev = vdev;

	DBG("Frontend: Setup ring\n");

	/* Prepare to set up the ring. */

	venoceansw_priv->venoceansw.ring_ref = GRANT_INVALID_REF;

	/* Allocate an event channel associated to the ring */
	res = vbus_alloc_evtchn(vdev, &evtchn);
	BUG_ON(res);

	res = bind_evtchn_to_irq_handler(evtchn, venoceansw_interrupt, NULL, vdev);
	if (res <= 0) {
		lprintk("%s - line %d: Binding event channel failed for device %s\n", __func__, __LINE__, vdev->nodename);
		BUG();
	}

	venoceansw_priv->venoceansw.evtchn = evtchn;
	venoceansw_priv->venoceansw.irq = res;

	/* Allocate a shared page for the ring */
	sring = (venoceansw_sring_t *) get_free_vpage();
	if (!sring) {
		lprintk("%s - line %d: Allocating shared ring failed for device %s\n", __func__, __LINE__, vdev->nodename);
		BUG();
	}

	SHARED_RING_INIT(sring);
	FRONT_RING_INIT(&venoceansw_priv->venoceansw.ring, sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	res = vbus_grant_ring(vdev, phys_to_pfn(virt_to_phys_pt((uint32_t) venoceansw_priv->venoceansw.ring.sring)));
	if (res < 0)
		BUG();

	venoceansw_priv->venoceansw.ring_ref = res;

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "ring-ref", "%u", venoceansw_priv->venoceansw.ring_ref);
	vbus_printf(vbt, vdev->nodename, "ring-evtchn", "%u", venoceansw_priv->venoceansw.evtchn);

	vbus_transaction_end(vbt);

}

/* At this point, the FE is not connected. */
static void venoceansw_reconfiguring(struct vbus_device *vdev) {
	int res;
	struct vbus_transaction vbt;
	venoceansw_priv_t *venoceansw_priv = dev_get_drvdata(vdev->dev);

	DBG0("[" VENOCEANSW_NAME "] Frontend reconfiguring\n");
	/* The shared page already exists */
	/* Re-init */

	gnttab_end_foreign_access_ref(venoceansw_priv->venoceansw.ring_ref);

	DBG("Frontend: Setup ring\n");

	/* Prepare to set up the ring. */

	venoceansw_priv->venoceansw.ring_ref = GRANT_INVALID_REF;

	SHARED_RING_INIT(venoceansw_priv->venoceansw.ring.sring);
	FRONT_RING_INIT(&venoceansw_priv->venoceansw.ring, (&venoceansw_priv->venoceansw.ring)->sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	res = vbus_grant_ring(vdev, phys_to_pfn(virt_to_phys_pt((uint32_t) venoceansw_priv->venoceansw.ring.sring)));
	if (res < 0)
		BUG();

	venoceansw_priv->venoceansw.ring_ref = res;

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "ring-ref", "%u", venoceansw_priv->venoceansw.ring_ref);
	vbus_printf(vbt, vdev->nodename, "ring-evtchn", "%u", venoceansw_priv->venoceansw.evtchn);

	vbus_transaction_end(vbt);
}

static void venoceansw_shutdown(struct vbus_device *vdev) {

	DBG0("[" VENOCEANSW_NAME "] Frontend shutdown\n");
}

static void venoceansw_closed(struct vbus_device *vdev) {
	venoceansw_priv_t *venoceansw_priv = dev_get_drvdata(vdev->dev);

	DBG0("[" VENOCEANSW_NAME "] Frontend close\n");

	/**
	 * Free the ring and deallocate the proper data.
	 */

	/* Free resources associated with old device channel. */
	if (venoceansw_priv->venoceansw.ring_ref != GRANT_INVALID_REF) {
		gnttab_end_foreign_access(venoceansw_priv->venoceansw.ring_ref);
		free_vpage((uint32_t) venoceansw_priv->venoceansw.ring.sring);

		venoceansw_priv->venoceansw.ring_ref = GRANT_INVALID_REF;
		venoceansw_priv->venoceansw.ring.sring = NULL;
	}

	if (venoceansw_priv->venoceansw.irq)
		unbind_from_irqhandler(venoceansw_priv->venoceansw.irq);

	venoceansw_priv->venoceansw.irq = 0;
}

static void venoceansw_suspend(struct vbus_device *vdev) {

	DBG0("[" VENOCEANSW_NAME "] Frontend suspend\n");
}

static void venoceansw_resume(struct vbus_device *vdev) {

	DBG0("[" VENOCEANSW_NAME "] Frontend resume\n");
}

#if 0
int notify_fn(void *arg) {
	char buffer[VENOCEANSW_PACKET_SIZE];

	while (1) {
		msleep(50);

		sprintf(buffer, "Hello %d\n", *((int *) arg));

		venoceansw_generate_request(buffer);
	}

	return 0;
}
#endif

static void venoceansw_connected(struct vbus_device *vdev) {
	venoceansw_priv_t *venoceansw_priv = dev_get_drvdata(vdev->dev);

	DBG0("[" VENOCEANSW_NAME "] Frontend connected\n");

	/* Force the processing of pending requests, if any */
	notify_remote_via_virq(venoceansw_priv->venoceansw.irq);

	if (!thread_created) {
		thread_created = true;
#if 0
		kernel_thread(notify_fn, "notify_th", &i1, 0);
		//kernel_thread(notify_fn, "notify_th2", &i2, 0);
#endif
	}
}

vdrvfront_t venoceanswdrv = {
	.probe = venoceansw_probe,
	.reconfiguring = venoceansw_reconfiguring,
	.shutdown = venoceansw_shutdown,
	.closed = venoceansw_closed,
	.suspend = venoceansw_suspend,
	.resume = venoceansw_resume,
	.connected = venoceansw_connected
};

static int venoceansw_init(dev_t *dev) {
	venoceansw_priv_t *venoceansw_priv;

	venoceansw_priv = malloc(sizeof(venoceansw_priv_t));
	BUG_ON(!venoceansw_priv);

	memset(venoceansw_priv, 0, sizeof(venoceansw_priv_t));

	dev_set_drvdata(dev, venoceansw_priv);

	vdevfront_init(VENOCEANSW_NAME, &venoceanswdrv);

	return 0;
}

REGISTER_DRIVER_POSTCORE("venoceansw,frontend", venoceansw_init);
