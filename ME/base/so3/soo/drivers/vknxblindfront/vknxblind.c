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
#include <completion.h>

#include <device/driver.h>

#include <soo/evtchn.h>
#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/console.h>
#include <soo/debug.h>

#include <soo/dev/vknxblind.h>

typedef struct {

	/* Must be the first field */
	vknxblind_t vknxblind;

} vknxblind_priv_t;




static struct vbus_device *vknxblind_dev = NULL;

static bool thread_created = false;

/**
 * Never used ?
 **/
irq_return_t vknxblind_interrupt(int irq, void *dev_id) {

	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vknxblind_priv_t *vknxblind_priv = dev_get_drvdata(vdev->dev);
	vknxblind_response_t *ring_rsp;


	while ((ring_rsp = vknxblind_get_ring_response(&vknxblind_priv->vknxblind.ring)) != NULL) {

		return IRQ_COMPLETED;
	}

	return IRQ_COMPLETED;
}

#if 0
/*
 * The following function is given as an example.
 *
 */

void vknxblind_generate_request(char *buffer) {
	vknxblind_request_t *ring_req;
	vknxblind_priv_t *vknxblind_priv;

	if (!vknxblind_dev)
		return ;

	vknxblind_priv = (vknxblind_priv_t *) dev_get_drvdata(vknxblind_dev->dev);

	vdevfront_processing_begin(vknxblind_dev);

	/*
	 * Try to generate a new request to the backend
	 */
	if (!RING_REQ_FULL(&vknxblind_priv->vknxblind.ring)) {
		ring_req = vknxblind_new_ring_request(&vknxblind_priv->vknxblind.ring);

		memcpy(ring_req->buffer, buffer, CMD_DATA_SIZE);

		vknxblind_ring_request_ready(&vknxblind_priv->vknxblind.ring);

		notify_remote_via_virq(vknxblind_priv->vknxblind.irq);
	}

	vdevfront_processing_end(vknxblind_dev);
}
#endif


void vknxblind_send_cmd(int cmd) {
	vknxblind_priv_t *vknxblind_priv;
	vknxblind_request_t *ring_req;

	if (!vknxblind_dev)
		return;

	vknxblind_priv = (vknxblind_priv_t *) dev_get_drvdata(vknxblind_dev->dev);


	vdevfront_processing_begin(vknxblind_dev);


	lprintk(VKNXBLIND_PREFIX ", FE sending %d to BE\n", cmd);
	/*
	 * Try to generate a new request to the backend
	 */
	if (!RING_REQ_FULL(&vknxblind_priv->vknxblind.ring)) {
		ring_req = vknxblind_new_ring_request(&vknxblind_priv->vknxblind.ring);

		ring_req->knxblind_cmd = cmd;

		vknxblind_ring_request_ready(&vknxblind_priv->vknxblind.ring);

		notify_remote_via_virq(vknxblind_priv->vknxblind.irq);
	}

	vdevfront_processing_end(vknxblind_dev);
}

void vknxblind_stop_blind(void) {

	vknxblind_send_cmd(VKNXBLIND_STOP_CMD);
}

void vknxblind_up_blind(void) {
	
	vknxblind_send_cmd(VKNXBLIND_UP_CMD);
}

void vknxblind_down_blind(void) {
	
	vknxblind_send_cmd(VKNXBLIND_DOWN_CMD);
}

static void vknxblind_probe(struct vbus_device *vdev) {
	int res;
	unsigned int evtchn;
	vknxblind_sring_t *sring;
	struct vbus_transaction vbt;
	vknxblind_priv_t *vknxblind_priv;

	lprintk("[ %s ] FRONTEND PROBE CALLED \n", VKNXBLIND_NAME);

	DBG0("[" VKNXBLIND_NAME "] Frontend probe\n");

	if (vdev->state == VbusStateConnected)
		return ;

	vknxblind_priv = dev_get_drvdata(vdev->dev);

	vknxblind_dev = vdev;

	DBG("Frontend: Setup ring\n");

	/* Prepare to set up the ring. */

	vknxblind_priv->vknxblind.ring_ref = GRANT_INVALID_REF;

	/* Allocate an event channel associated to the ring */
	res = vbus_alloc_evtchn(vdev, &evtchn);
	BUG_ON(res);

	res = bind_evtchn_to_irq_handler(evtchn, vknxblind_interrupt, NULL, vdev);
	if (res <= 0) {
		lprintk("%s - line %d: Binding event channel failed for device %s\n", __func__, __LINE__, vdev->nodename);
		BUG();
	}

	vknxblind_priv->vknxblind.evtchn = evtchn;
	vknxblind_priv->vknxblind.irq = res;


	/* Allocate a shared page for the ring */
	sring = (vknxblind_sring_t *) get_free_vpage();
	if (!sring) {
		lprintk("%s - line %d: Allocating shared ring failed for device %s\n", __func__, __LINE__, vdev->nodename);
		BUG();
	}

	SHARED_RING_INIT(sring);
	FRONT_RING_INIT(&vknxblind_priv->vknxblind.ring, sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	res = vbus_grant_ring(vdev, phys_to_pfn(virt_to_phys_pt((uint32_t) vknxblind_priv->vknxblind.ring.sring)));
	if (res < 0)
		BUG();

	vknxblind_priv->vknxblind.ring_ref = res;

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "ring-ref", "%u", vknxblind_priv->vknxblind.ring_ref);
	vbus_printf(vbt, vdev->nodename, "ring-evtchn", "%u", vknxblind_priv->vknxblind.evtchn);

	vbus_transaction_end(vbt);

}

/* At this point, the FE is not connected. */
static void vknxblind_reconfiguring(struct vbus_device *vdev) {
	int res;
	struct vbus_transaction vbt;
	vknxblind_priv_t *vknxblind_priv = dev_get_drvdata(vdev->dev);

	DBG0("[" VKNXBLIND_NAME "] Frontend reconfiguring\n");
	/* The shared page already exists */
	/* Re-init */

	gnttab_end_foreign_access_ref(vknxblind_priv->vknxblind.ring_ref);

	DBG("Frontend: Setup ring\n");

	/* Prepare to set up the ring. */

	vknxblind_priv->vknxblind.ring_ref = GRANT_INVALID_REF;

	SHARED_RING_INIT(vknxblind_priv->vknxblind.ring.sring);
	FRONT_RING_INIT(&vknxblind_priv->vknxblind.ring, (&vknxblind_priv->vknxblind.ring)->sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	res = vbus_grant_ring(vdev, phys_to_pfn(virt_to_phys_pt((uint32_t) vknxblind_priv->vknxblind.ring.sring)));
	if (res < 0)
		BUG();

	vknxblind_priv->vknxblind.ring_ref = res;

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "ring-ref", "%u", vknxblind_priv->vknxblind.ring_ref);
	vbus_printf(vbt, vdev->nodename, "ring-evtchn", "%u", vknxblind_priv->vknxblind.evtchn);

	vbus_transaction_end(vbt);
}

static void vknxblind_shutdown(struct vbus_device *vdev) {

	DBG0("[" VKNXBLIND_NAME "] Frontend shutdown\n");
}

static void vknxblind_closed(struct vbus_device *vdev) {
	vknxblind_priv_t *vknxblind_priv = dev_get_drvdata(vdev->dev);

	DBG0("[" VKNXBLIND_NAME "] Frontend close\n");

	/**
	 * Free the ring and deallocate the proper data.
	 */

	/* Free resources associated with old device channel. */
	if (vknxblind_priv->vknxblind.ring_ref != GRANT_INVALID_REF) {
		gnttab_end_foreign_access(vknxblind_priv->vknxblind.ring_ref);
		free_vpage((uint32_t) vknxblind_priv->vknxblind.ring.sring);

		vknxblind_priv->vknxblind.ring_ref = GRANT_INVALID_REF;
		vknxblind_priv->vknxblind.ring.sring = NULL;
	}

	if (vknxblind_priv->vknxblind.irq)
		unbind_from_irqhandler(vknxblind_priv->vknxblind.irq);

	vknxblind_priv->vknxblind.irq = 0;
}

static void vknxblind_suspend(struct vbus_device *vdev) {

	DBG0("[" VKNXBLIND_NAME "] Frontend suspend\n");
}

static void vknxblind_resume(struct vbus_device *vdev) {

	DBG0("[" VKNXBLIND_NAME "] Frontend resume\n");
}

#if 0
int notify_fn(void *arg) {
	char buffer[VKNXBLIND_PACKET_SIZE];

	while (1) {
		msleep(50);

		sprintf(buffer, "Hello %d\n", *((int *) arg));

		vknxblind_generate_request(buffer);
	}

	return 0;
}
#endif

static void vknxblind_connected(struct vbus_device *vdev) {
	vknxblind_priv_t *vknxblind_priv = dev_get_drvdata(vdev->dev);

	DBG0("[" VKNXBLIND_NAME "] Frontend connected\n");

	/* Force the processing of pending requests, if any */
	notify_remote_via_virq(vknxblind_priv->vknxblind.irq);

	if (!thread_created) {
		thread_created = true;
#if 0
		kernel_thread(notify_fn, "notify_th", &i1, 0);
		//kernel_thread(notify_fn, "notify_th2", &i2, 0);
#endif
	}
}

vdrvfront_t vknxblinddrv = {
	.probe = vknxblind_probe,
	.reconfiguring = vknxblind_reconfiguring,
	.shutdown = vknxblind_shutdown,
	.closed = vknxblind_closed,
	.suspend = vknxblind_suspend,
	.resume = vknxblind_resume,
	.connected = vknxblind_connected
};

static int vknxblind_init(dev_t *dev) {
	vknxblind_priv_t *vknxblind_priv;

	vknxblind_priv = malloc(sizeof(vknxblind_priv_t));
	BUG_ON(!vknxblind_priv);

	memset(vknxblind_priv, 0, sizeof(vknxblind_priv_t));

	dev_set_drvdata(dev, vknxblind_priv);

	lprintk("[ %s ] FRONTEND INIT CALLED \n", VKNXBLIND_NAME);

	vdevfront_init(VKNXBLIND_NAME, &vknxblinddrv);

	return 0;
}

REGISTER_DRIVER_POSTCORE("vknxblind,frontend", vknxblind_init);
