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

#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of.h>

#include <xenomai/rtdm/driver.h>

#include <soo/evtchn.h>
#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus_me.h>
#include <soo/paging.h>

#include <soo/dev/vdummy.h>

typedef struct {

	/* Must be the first field */
	vdummyrt_t vdummy;

} vdummy_priv_t;

#if 0 /* Debugging purposes */
static rtdm_task_t rtdm_task_gen;
#endif

static struct vbus_me_device *vdummy_dev = NULL;

static bool thread_created = false;

typedef int (*rtdm_irq_handler_t)(rtdm_irq_t *irq_handle);

static int vdummy_interrupt(rtdm_irq_t *irq_handle) {
	struct vbus_me_device *vdev = rtdm_irq_get_arg(irq_handle, struct vbus_me_device);
	vdummy_priv_t *vdummy_priv = vdev->vdrv->priv;
	vdummy_response_t *ring_rsp;

	DBG("%s, %d\n", __func__, smp_processor_id());

	while ((ring_rsp = vdummy_get_ring_response(&vdummy_priv->vdummy.ring)) != NULL) {

		/* Do something with the response */

#if 0 /* Debugging purposes */
		lprintk("## Got from the backend: %s\n", ring_rsp->buffer);
#endif
	}

	return RTDM_IRQ_HANDLED;
}

#if 0 /* Debugging purposes */

/*
 * The following function is given as an example.
 *
 */

void vdummy_generate_request(void *args) {
	vdummy_request_t *ring_req;
	vdummy_priv_t *vdummy_priv;
	char *buffer = (char *) args;

	if (!vdummy_dev)
		return ;

	vdummy_priv = (vdummy_priv_t *) vdummy_dev->vdrv->priv;

	vdevfront_processing_begin(vdummy_dev);

	/*
	 * Try to generate a new request to the backend
	 */
	if (!RING_REQ_FULL(&vdummy_priv->vdummy.ring)) {
		ring_req = vdummy_new_ring_request(&vdummy_priv->vdummy.ring);

		memcpy(ring_req->buffer, buffer, VDUMMY_PACKET_SIZE);

		vdummy_ring_request_ready(&vdummy_priv->vdummy.ring);

		notify_remote_via_virq(vdummy_priv->vdummy.irq);
	}

	vdevfront_processing_end(vdummy_dev);
}
#endif

static void vdummy_probe(struct vbus_me_device *vdev) {
	int res;
	unsigned int evtchn;
	vdummy_sring_t *sring;
	struct vbus_transaction vbt;
	vdummy_priv_t *vdummy_priv;

	DBG("[" VDUMMY_NAME "] Frontend probe\n");

	if (vdev->state == VbusStateConnected)
		return ;

	vdummy_priv = vdev->vdrv->priv;

	vdummy_dev = vdev;

	DBG("Frontend: Setup ring\n");

	/* Prepare to set up the ring. */

	vdummy_priv->vdummy.ring_ref = GRANT_INVALID_REF;

	/* Allocate an event channel associated to the ring */
	res = vbus_me_alloc_evtchn(vdev, &evtchn);
	BUG_ON(res);

	res = rtdm_bind_evtchn_to_virq_handler(&vdummy_priv->vdummy.rtdm_irq, evtchn, vdummy_interrupt, 0, "vdummyrt-virq", vdev);
	if (res <= 0) {
		lprintk("%s - line %d: Binding event channel failed for device %s\n", __func__, __LINE__, vdev->nodename);
		BUG();
	}

	vdummy_priv->vdummy.evtchn = evtchn;
	vdummy_priv->vdummy.irq = res;

	vdummy_priv->vdummy.ring_pages = alloc_pages(GFP_ATOMIC, 1); /* Only one page, order 0 */
	BUG_ON(!vdummy_priv->vdummy.ring_pages);

	sring = paging_remap(page_to_phys(vdummy_priv->vdummy.ring_pages), PAGE_SIZE);

	if (!sring) {
		lprintk("%s - line %d: Allocating shared ring failed for device %s\n", __func__, __LINE__, vdev->nodename);
		BUG();
	}

	memset(sring, 0, PAGE_SIZE);

	SHARED_RING_INIT(sring);
	FRONT_RING_INIT(&vdummy_priv->vdummy.ring, sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	res = vbus_me_grant_ring(vdev, page_to_pfn(vdummy_priv->vdummy.ring_pages));
	if (res < 0)
		BUG();

	vdummy_priv->vdummy.ring_ref = res;

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "ring-ref", "%u", vdummy_priv->vdummy.ring_ref);
	vbus_printf(vbt, vdev->nodename, "ring-evtchn", "%u", vdummy_priv->vdummy.evtchn);

	vbus_transaction_end(vbt);

}

/* At this point, the FE is not connected. */
static void vdummy_reconfiguring(struct vbus_me_device *vdev) {
	int res;
	struct vbus_transaction vbt;
	vdummy_priv_t *vdummy_priv = vdev->vdrv->priv;

	DBG("[" VDUMMY_NAME "] Frontend reconfiguring\n");
	/* The shared page already exists */
	/* Re-init */

	gnttab_me_end_foreign_access_ref(vdummy_priv->vdummy.ring_ref);

	DBG("Frontend: Setup ring\n");

	/* Prepare to set up the ring. */

	vdummy_priv->vdummy.ring_ref = GRANT_INVALID_REF;

	SHARED_RING_INIT(vdummy_priv->vdummy.ring.sring);
	FRONT_RING_INIT(&vdummy_priv->vdummy.ring, (&vdummy_priv->vdummy.ring)->sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	res = vbus_me_grant_ring(vdev, page_to_phys(vdummy_priv->vdummy.ring_pages));
	if (res < 0)
		BUG();

	vdummy_priv->vdummy.ring_ref = res;

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "ring-ref", "%u", vdummy_priv->vdummy.ring_ref);
	vbus_printf(vbt, vdev->nodename, "ring-evtchn", "%u", vdummy_priv->vdummy.evtchn);

	vbus_transaction_end(vbt);
}

static void vdummy_shutdown(struct vbus_me_device *vdev) {

	DBG("[" VDUMMY_NAME "] Frontend shutdown\n");
}

static void vdummy_closed(struct vbus_me_device *vdev) {
	vdummy_priv_t *vdummy_priv = vdev->vdrv->priv;

	DBG("[" VDUMMY_NAME "] Frontend close\n");

	/**
	 * Free the ring and deallocate the proper data.
	 */

	/* Free resources associated with old device channel. */
	if (vdummy_priv->vdummy.ring_ref != GRANT_INVALID_REF) {
		gnttab_me_end_foreign_access(vdummy_priv->vdummy.ring_ref);
		free_page((unsigned long) vdummy_priv->vdummy.ring.sring);

		vdummy_priv->vdummy.ring_ref = GRANT_INVALID_REF;
		vdummy_priv->vdummy.ring.sring = NULL;
	}

	if (vdummy_priv->vdummy.rtdm_irq.cookie)
		rtdm_unbind_from_virqhandler(&vdummy_priv->vdummy.rtdm_irq);

	vdummy_priv->vdummy.rtdm_irq.cookie = NULL;
}

static void vdummy_suspend(struct vbus_me_device *vdev) {

	DBG("[" VDUMMY_NAME "] Frontend suspend\n");
}

static void vdummy_resume(struct vbus_me_device *vdev) {

	DBG("[" VDUMMY_NAME "] Frontend resume\n");
}

#if 0
void notify_fn(void *arg) {
	char buffer[VDUMMY_PACKET_SIZE];

	while (1) {
		rtdm_task_wait_period(NULL);

		sprintf(buffer, "Hello %s\n", (char *) arg);

		vdummy_generate_request(buffer);
	}

}
#endif

static void vdummy_connected(struct vbus_me_device *vdev) {
	vdummy_priv_t *vdummy_priv = vdev->vdrv->priv;

	DBG("[" VDUMMY_NAME "] Frontend connected\n");

	/* Force the processing of pending requests, if any */
	notify_remote_via_virq(vdummy_priv->vdummy.irq);

	if (!thread_created) {
		thread_created = true;

#if 0 /* Debugging purposes */
		kernel_thread(notify_fn, "notify_th", &i1, 0);
		//kernel_thread(notify_fn, "notify_th2", &i2, 0);
#endif
	}
}

static vdrvfront_t vdummydrv = {
	.probe = vdummy_probe,
	.reconfiguring = vdummy_reconfiguring,
	.shutdown = vdummy_shutdown,
	.closed = vdummy_closed,
	.suspend = vdummy_suspend,
	.resume = vdummy_resume,
	.connected = vdummy_connected
};

static int vdummyrt_init(void) {
	vdummy_priv_t *vdummy_priv;
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "vdummy,frontend");

	/* Check if DTS has vuihandler enabled */
	if (!of_device_is_available(np))
		return 0;

	vdummy_priv = kzalloc(sizeof(vdummy_priv_t), GFP_ATOMIC);
	BUG_ON(!vdummy_priv);

	vdummydrv.vdrv.priv = vdummy_priv;

#if 0 /* Debugging purposes */
	rtdm_task_init(&rtdm_task_gen, "vdummy-gen", notify_fn, "RT1", 50, SECONDS(1));
#endif

	vdevfront_init(VDUMMY_NAME, &vdummydrv);

	return 0;
}

/* Initcall to be called after a *standard* device_initcall, once all backends have been initalized */
device_initcall_sync(vdummyrt_init);


