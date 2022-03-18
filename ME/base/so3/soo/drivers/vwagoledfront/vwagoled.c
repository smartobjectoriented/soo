/*
 * Copyright (C) 2022 Mattia Gallacchi <mattia.gallaccchi@heig-vd.ch>
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

#include <soo/dev/vwagoled.h>

typedef struct {

	/* Must be the first field */
	wagoled_t wagoled;

} wagoled_priv_t;

static struct vbus_device *wagoled_dev = NULL;

static bool thread_created = false;

irq_return_t wagoled_interrupt(int irq, void *dev_id) {
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	wagoled_priv_t *wagoled_priv = dev_get_drvdata(vdev->dev);
	wagoled_response_t *ring_rsp;

	DBG("%s, %d\n", __func__, ME_domID());

	while ((ring_rsp = wagoled_get_ring_response(&wagoled_priv->wagoled.ring)) != NULL) {

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

void wagoled_generate_request(char *buffer) {
	wagoled_request_t *ring_req;
	wagoled_priv_t *wagoled_priv;

	if (!wagoled_dev)
		return ;

	wagoled_priv = (wagoled_priv_t *) dev_get_drvdata(wagoled_dev->dev);

	vdevfront_processing_begin(wagoled_dev);

	/*
	 * Try to generate a new request to the backend
	 */
	if (!RING_REQ_FULL(&wagoled_priv->wagoled.ring)) {
		ring_req = wagoled_new_ring_request(&wagoled_priv->wagoled.ring);

		memcpy(ring_req->buffer, buffer, wagoled_PACKET_SIZE);

		wagoled_ring_request_ready(&wagoled_priv->wagoled.ring);

		notify_remote_via_virq(wagoled_priv->wagoled.irq);
	}

	vdevfront_processing_end(wagoled_dev);
}
#endif

static void wagoled_probe(struct vbus_device *vdev) {
	int res;
	unsigned int evtchn;
	wagoled_sring_t *sring;
	struct vbus_transaction vbt;
	wagoled_priv_t *wagoled_priv;

	DBG0("[" WAGOLED_NAME "] Frontend probe\n");

	if (vdev->state == VbusStateConnected)
		return ;

	wagoled_priv = dev_get_drvdata(vdev->dev);

	wagoled_dev = vdev;

	DBG("Frontend: Setup ring\n");

	/* Prepare to set up the ring. */

	wagoled_priv->wagoled.ring_ref = GRANT_INVALID_REF;

	/* Allocate an event channel associated to the ring */
	res = vbus_alloc_evtchn(vdev, &evtchn);
	BUG_ON(res);

	res = bind_evtchn_to_irq_handler(evtchn, wagoled_interrupt, NULL, vdev);
	if (res <= 0) {
		lprintk("%s - line %d: Binding event channel failed for device %s\n", __func__, __LINE__, vdev->nodename);
		BUG();
	}

	wagoled_priv->wagoled.evtchn = evtchn;
	wagoled_priv->wagoled.irq = res;

	/* Allocate a shared page for the ring */
	sring = (wagoled_sring_t *) get_free_vpage();
	if (!sring) {
		lprintk("%s - line %d: Allocating shared ring failed for device %s\n", __func__, __LINE__, vdev->nodename);
		BUG();
	}

	SHARED_RING_INIT(sring);
	FRONT_RING_INIT(&wagoled_priv->wagoled.ring, sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	res = vbus_grant_ring(vdev, phys_to_pfn(virt_to_phys_pt((uint32_t) wagoled_priv->wagoled.ring.sring)));
	if (res < 0)
		BUG();

	wagoled_priv->wagoled.ring_ref = res;

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "ring-ref", "%u", wagoled_priv->wagoled.ring_ref);
	vbus_printf(vbt, vdev->nodename, "ring-evtchn", "%u", wagoled_priv->wagoled.evtchn);

	vbus_transaction_end(vbt);

}

/* At this point, the FE is not connected. */
static void wagoled_reconfiguring(struct vbus_device *vdev) {
	int res;
	struct vbus_transaction vbt;
	wagoled_priv_t *wagoled_priv = dev_get_drvdata(vdev->dev);

	DBG0("[" WAGOLED_NAME "] Frontend reconfiguring\n");
	/* The shared page already exists */
	/* Re-init */

	gnttab_end_foreign_access_ref(wagoled_priv->wagoled.ring_ref);

	DBG("Frontend: Setup ring\n");

	/* Prepare to set up the ring. */

	wagoled_priv->wagoled.ring_ref = GRANT_INVALID_REF;

	SHARED_RING_INIT(wagoled_priv->wagoled.ring.sring);
	FRONT_RING_INIT(&wagoled_priv->wagoled.ring, (&wagoled_priv->wagoled.ring)->sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	res = vbus_grant_ring(vdev, phys_to_pfn(virt_to_phys_pt((uint32_t) wagoled_priv->wagoled.ring.sring)));
	if (res < 0)
		BUG();

	wagoled_priv->wagoled.ring_ref = res;

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "ring-ref", "%u", wagoled_priv->wagoled.ring_ref);
	vbus_printf(vbt, vdev->nodename, "ring-evtchn", "%u", wagoled_priv->wagoled.evtchn);

	vbus_transaction_end(vbt);
}

static void wagoled_shutdown(struct vbus_device *vdev) {

	DBG0("[" WAGOLED_NAME "] Frontend shutdown\n");
}

static void wagoled_closed(struct vbus_device *vdev) {
	wagoled_priv_t *wagoled_priv = dev_get_drvdata(vdev->dev);

	DBG0("[" WAGOLED_NAME "] Frontend close\n");

	/**
	 * Free the ring and deallocate the proper data.
	 */

	/* Free resources associated with old device channel. */
	if (wagoled_priv->wagoled.ring_ref != GRANT_INVALID_REF) {
		gnttab_end_foreign_access(wagoled_priv->wagoled.ring_ref);
		free_vpage((uint32_t) wagoled_priv->wagoled.ring.sring);

		wagoled_priv->wagoled.ring_ref = GRANT_INVALID_REF;
		wagoled_priv->wagoled.ring.sring = NULL;
	}

	if (wagoled_priv->wagoled.irq)
		unbind_from_irqhandler(wagoled_priv->wagoled.irq);

	wagoled_priv->wagoled.irq = 0;
}

static void wagoled_suspend(struct vbus_device *vdev) {

	DBG0("[" WAGOLE_NAME "] Frontend suspend\n");
}

static void wagoled_resume(struct vbus_device *vdev) {

	DBG0("[" WAGOLED_NAME "] Frontend resume\n");
}

#if 0
int notify_fn(void *arg) {
	char buffer[wagoled_PACKET_SIZE];

	while (1) {
		msleep(50);

		sprintf(buffer, "Hello %d\n", *((int *) arg));

		wagoled_generate_request(buffer);
	}

	return 0;
}
#endif

static void wagoled_connected(struct vbus_device *vdev) {
	wagoled_priv_t *wagoled_priv = dev_get_drvdata(vdev->dev);

	DBG0("[" WAGOLED_NAME "] Frontend connected\n");

	/* Force the processing of pending requests, if any */
	notify_remote_via_virq(wagoled_priv->wagoled.irq);

	if (!thread_created) {
		thread_created = true;
#if 0
		kernel_thread(notify_fn, "notify_th", &i1, 0);
		//kernel_thread(notify_fn, "notify_th2", &i2, 0);
#endif
	}
}

vdrvfront_t wagoleddrv = {
	.probe = wagoled_probe,
	.reconfiguring = wagoled_reconfiguring,
	.shutdown = wagoled_shutdown,
	.closed = wagoled_closed,
	.suspend = wagoled_suspend,
	.resume = wagoled_resume,
	.connected = wagoled_connected
};

static int wagoled_init(dev_t *dev) {
	wagoled_priv_t *wagoled_priv;

	wagoled_priv = malloc(sizeof(wagoled_priv_t));
	BUG_ON(!wagoled_priv);

	memset(wagoled_priv, 0, sizeof(wagoled_priv_t));

	dev_set_drvdata(dev, wagoled_priv);

	vdevfront_init(WAGOLED_NAME, &wagoleddrv);

	return 0;
}

REGISTER_DRIVER_POSTCORE("wagoled,frontend", wagoled_init);
