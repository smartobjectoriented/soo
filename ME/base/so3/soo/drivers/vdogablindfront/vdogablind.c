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

#include <soo/dev/vdogablind.h>

typedef struct {

	/* Must be the first field */
	vdogablind_t vdogablind;

} vdogablind_priv_t;

static struct vbus_device *vdogablind_dev = NULL;

static bool thread_created = false;

irq_return_t vdogablind_interrupt(int irq, void *dev_id) {
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vdogablind_priv_t *vdogablind_priv = dev_get_drvdata(vdev->dev);
	vdogablind_response_t *ring_rsp;

	DBG("%s, %d\n", __func__, ME_domID());

	while ((ring_rsp = vdogablind_get_ring_response(&vdogablind_priv->vdogablind.ring)) != NULL) {

		DBG("%s, cons=%d\n", __func__, i);

	}

	return IRQ_COMPLETED;
}


void vdogablind_send_blind_cmd(int cmd) {


	vdogablind_priv_t *vdogablind_priv;
	vdogablind_request_t *ring_req;

	if (!vdogablind_dev)
		return;

	vdogablind_priv = (vdogablind_priv_t *) dev_get_drvdata(vdogablind_dev->dev);


	vdevfront_processing_begin(vdogablind_dev);


	lprintk(VDOGABLIND_PREFIX ", FE sending %02hhx to BE\n", cmd);

	/*
	 * Try to generate a new request to the backend
	 */
	if (!RING_REQ_FULL(&vdogablind_priv->vdogablind.ring)) {
		ring_req = vdogablind_new_ring_request(&vdogablind_priv->vdogablind.ring);

		ring_req->cmd_blind = cmd;

		vdogablind_ring_request_ready(&vdogablind_priv->vdogablind.ring);

		notify_remote_via_virq(vdogablind_priv->vdogablind.irq);
	}

	vdevfront_processing_end(vdogablind_dev);

}

static void vdogablind_probe(struct vbus_device *vdev) {
	int res;
	unsigned int evtchn;
	vdogablind_sring_t *sring;
	struct vbus_transaction vbt;
	vdogablind_priv_t *vdogablind_priv;

	DBG0("[" VDOGABLIND_NAME "] Frontend probe\n");

	if (vdev->state == VbusStateConnected)
		return ;

	vdogablind_priv = dev_get_drvdata(vdev->dev);

	vdogablind_dev = vdev;

	DBG("Frontend: Setup ring\n");

	/* Prepare to set up the ring. */

	vdogablind_priv->vdogablind.ring_ref = GRANT_INVALID_REF;

	/* Allocate an event channel associated to the ring */
	res = vbus_alloc_evtchn(vdev, &evtchn);
	BUG_ON(res);

	res = bind_evtchn_to_irq_handler(evtchn, vdogablind_interrupt, NULL, vdev);
	if (res <= 0) {
		lprintk("%s - line %d: Binding event channel failed for device %s\n", __func__, __LINE__, vdev->nodename);
		BUG();
	}

	vdogablind_priv->vdogablind.evtchn = evtchn;
	vdogablind_priv->vdogablind.irq = res;

	/* Allocate a shared page for the ring */
	sring = (vdogablind_sring_t *) get_free_vpage();
	if (!sring) {
		lprintk("%s - line %d: Allocating shared ring failed for device %s\n", __func__, __LINE__, vdev->nodename);
		BUG();
	}

	SHARED_RING_INIT(sring);
	FRONT_RING_INIT(&vdogablind_priv->vdogablind.ring, sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	res = vbus_grant_ring(vdev, phys_to_pfn(virt_to_phys_pt((uint32_t) vdogablind_priv->vdogablind.ring.sring)));
	if (res < 0)
		BUG();

	vdogablind_priv->vdogablind.ring_ref = res;

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "ring-ref", "%u", vdogablind_priv->vdogablind.ring_ref);
	vbus_printf(vbt, vdev->nodename, "ring-evtchn", "%u", vdogablind_priv->vdogablind.evtchn);

	vbus_transaction_end(vbt);

}

/* At this point, the FE is not connected. */
static void vdogablind_reconfiguring(struct vbus_device *vdev) {
	int res;
	struct vbus_transaction vbt;
	vdogablind_priv_t *vdogablind_priv = dev_get_drvdata(vdev->dev);

	DBG0("[" VDOGABLIND_NAME "] Frontend reconfiguring\n");
	/* The shared page already exists */
	/* Re-init */

	gnttab_end_foreign_access_ref(vdogablind_priv->vdogablind.ring_ref);

	DBG("Frontend: Setup ring\n");

	/* Prepare to set up the ring. */

	vdogablind_priv->vdogablind.ring_ref = GRANT_INVALID_REF;

	SHARED_RING_INIT(vdogablind_priv->vdogablind.ring.sring);
	FRONT_RING_INIT(&vdogablind_priv->vdogablind.ring, (&vdogablind_priv->vdogablind.ring)->sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	res = vbus_grant_ring(vdev, phys_to_pfn(virt_to_phys_pt((uint32_t) vdogablind_priv->vdogablind.ring.sring)));
	if (res < 0)
		BUG();

	vdogablind_priv->vdogablind.ring_ref = res;

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "ring-ref", "%u", vdogablind_priv->vdogablind.ring_ref);
	vbus_printf(vbt, vdev->nodename, "ring-evtchn", "%u", vdogablind_priv->vdogablind.evtchn);

	vbus_transaction_end(vbt);
}

static void vdogablind_shutdown(struct vbus_device *vdev) {

	DBG0("[" VDOGABLIND_NAME "] Frontend shutdown\n");
}

static void vdogablind_closed(struct vbus_device *vdev) {
	vdogablind_priv_t *vdogablind_priv = dev_get_drvdata(vdev->dev);

	DBG0("[" VDOGABLIND_NAME "] Frontend close\n");

	/**
	 * Free the ring and deallocate the proper data.
	 */

	/* Free resources associated with old device channel. */
	if (vdogablind_priv->vdogablind.ring_ref != GRANT_INVALID_REF) {
		gnttab_end_foreign_access(vdogablind_priv->vdogablind.ring_ref);
		free_vpage((uint32_t) vdogablind_priv->vdogablind.ring.sring);

		vdogablind_priv->vdogablind.ring_ref = GRANT_INVALID_REF;
		vdogablind_priv->vdogablind.ring.sring = NULL;
	}

	if (vdogablind_priv->vdogablind.irq)
		unbind_from_irqhandler(vdogablind_priv->vdogablind.irq);

	vdogablind_priv->vdogablind.irq = 0;
}

static void vdogablind_suspend(struct vbus_device *vdev) {

	DBG0("[" VDOGABLIND_NAME "] Frontend suspend\n");
}

static void vdogablind_resume(struct vbus_device *vdev) {

	DBG0("[" VDOGABLIND_NAME "] Frontend resume\n");
}

#if 0
int notify_fn(void *arg) {
	char buffer[VDOGABLIND_PACKET_SIZE];

	while (1) {
		msleep(50);

		sprintf(buffer, "Hello %d\n", *((int *) arg));

		vdogablind_generate_request(buffer);
	}

	return 0;
}
#endif

static void vdogablind_connected(struct vbus_device *vdev) {
	vdogablind_priv_t *vdogablind_priv = dev_get_drvdata(vdev->dev);

	DBG0("[" VDOGABLIND_NAME "] Frontend connected\n");

	/* Force the processing of pending requests, if any */
	notify_remote_via_virq(vdogablind_priv->vdogablind.irq);

	if (!thread_created) {
		thread_created = true;
#if 0
		kernel_thread(notify_fn, "notify_th", &i1, 0);
		//kernel_thread(notify_fn, "notify_th2", &i2, 0);
#endif
	}
}

vdrvfront_t vdogablinddrv = {
	.probe = vdogablind_probe,
	.reconfiguring = vdogablind_reconfiguring,
	.shutdown = vdogablind_shutdown,
	.closed = vdogablind_closed,
	.suspend = vdogablind_suspend,
	.resume = vdogablind_resume,
	.connected = vdogablind_connected
};

static int vdogablind_init(dev_t *dev) {
	vdogablind_priv_t *vdogablind_priv;

	vdogablind_priv = malloc(sizeof(vdogablind_priv_t));
	BUG_ON(!vdogablind_priv);

	memset(vdogablind_priv, 0, sizeof(vdogablind_priv_t));

	dev_set_drvdata(dev, vdogablind_priv);

	vdevfront_init(VDOGABLIND_NAME, &vdogablinddrv);

	return 0;
}

REGISTER_DRIVER_POSTCORE("vdogablind,frontend", vdogablind_init);
