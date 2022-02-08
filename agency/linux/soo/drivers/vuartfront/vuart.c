/*
 * Copyright (C) 2018-2021 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <soo/dev/vuart.h>

typedef struct {

	/* Must be the first field */
	vuartrt_t vuart;

} vuart_priv_t;

static bool __vuart_ready = false;

/* Helpful to disable output log from the RT domain */
static bool __rt_log_enabled = true;

static struct vbus_me_device *vuart_dev = NULL;

typedef int (*rtdm_irq_handler_t)(rtdm_irq_t *irq_handle);

/* Get the status of the RT logging */
bool rt_log_enabled(void) {
	return __rt_log_enabled;
}

void rt_log_enable(void) {
	__rt_log_enabled = true;
}

void rt_log_disable(void) {
	__rt_log_enabled = false;
}

static int vuart_interrupt(rtdm_irq_t *irq_handle) {

	/* Nothing in this case. */

	return RTDM_IRQ_HANDLED;
}

/*
 * Propagate a string to the vuart backend so that the non-RT domain
 * can display or re-direct to a file.
 */
void vuart_send(char *str) {
	vuart_request_t *req;
	vuart_priv_t *vuart_priv;

	if (!vuart_ready() || !__rt_log_enabled)
		return ;

	vuart_priv = (vuart_priv_t *) vuart_dev->vdrv->priv;

	req = vuart_new_ring_request(&vuart_priv->vuart.ring);

	strcpy(req->str, str);

	vuart_ring_request_ready(&vuart_priv->vuart.ring);

	notify_remote_via_virq(vuart_priv->vuart.irq);
}

static void vuart_probe(struct vbus_me_device *vdev) {
	int res;
	unsigned int evtchn;
	vuart_sring_t *sring;
	struct vbus_transaction vbt;
	vuart_priv_t *vuart_priv;

	DBG("[" VUART_NAME "] Frontend probe\n");

	if (vdev->state == VbusStateConnected)
		return ;

	vuart_priv = vdev->vdrv->priv;

	vuart_dev = vdev;

	DBG("Frontend: Setup ring\n");

	/* Prepare to set up the ring. */

	vuart_priv->vuart.ring_ref = GRANT_INVALID_REF;

	/* Allocate an event channel associated to the ring */
	res = vbus_me_alloc_evtchn(vdev, &evtchn);
	BUG_ON(res);

	res = rtdm_bind_evtchn_to_virq_handler(&vuart_priv->vuart.rtdm_irq, evtchn, vuart_interrupt, 0, "vuartrt-virq", vdev);
	if (res <= 0) {
		lprintk("%s - line %d: Binding event channel failed for device %s\n", __func__, __LINE__, vdev->nodename);
		BUG();
	}

	vuart_priv->vuart.evtchn = evtchn;
	vuart_priv->vuart.irq = res;

	/* Allocate a shared page for the ring */
	vuart_priv->vuart.ring_pages = alloc_pages(GFP_ATOMIC, 1); /* Only one page, order 0 */
	BUG_ON(!vuart_priv->vuart.ring_pages);

	sring = paging_remap(page_to_phys(vuart_priv->vuart.ring_pages), PAGE_SIZE);

	if (!sring) {
		lprintk("%s - line %d: Allocating shared ring failed for device %s\n", __func__, __LINE__, vdev->nodename);
		BUG();
	}

	memset(sring, 0, PAGE_SIZE);

	SHARED_RING_INIT(sring);
	FRONT_RING_INIT(&vuart_priv->vuart.ring, sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	res = vbus_me_grant_ring(vdev, page_to_pfn(vuart_priv->vuart.ring_pages));
	if (res < 0)
		BUG();

	vuart_priv->vuart.ring_ref = res;

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "ring-ref", "%u", vuart_priv->vuart.ring_ref);
	vbus_printf(vbt, vdev->nodename, "ring-evtchn", "%u", vuart_priv->vuart.evtchn);

	vbus_transaction_end(vbt);

}

/* At this point, the FE is not connected. */
static void vuart_reconfiguring(struct vbus_me_device *vdev) {
	int res;
	struct vbus_transaction vbt;
	vuart_priv_t *vuart_priv = vdev->vdrv->priv;

	DBG("[" VUART_NAME "] Frontend reconfiguring\n");
	/* The shared page already exists */
	/* Re-init */

	gnttab_me_end_foreign_access_ref(vuart_priv->vuart.ring_ref);

	DBG("Frontend: Setup ring\n");

	/* Prepare to set up the ring. */

	vuart_priv->vuart.ring_ref = GRANT_INVALID_REF;

	SHARED_RING_INIT(vuart_priv->vuart.ring.sring);
	FRONT_RING_INIT(&vuart_priv->vuart.ring, (&vuart_priv->vuart.ring)->sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	res = vbus_me_grant_ring(vdev, page_to_phys(vuart_priv->vuart.ring_pages));
	if (res < 0)
		BUG();

	vuart_priv->vuart.ring_ref = res;

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "ring-ref", "%u", vuart_priv->vuart.ring_ref);
	vbus_printf(vbt, vdev->nodename, "ring-evtchn", "%u", vuart_priv->vuart.evtchn);

	vbus_transaction_end(vbt);
}

static void vuart_shutdown(struct vbus_me_device *vdev) {

	DBG("[" VUART_NAME "] Frontend shutdown\n");
}

static void vuart_closed(struct vbus_me_device *vdev) {
	vuart_priv_t *vuart_priv = vdev->vdrv->priv;

	DBG("[" VUART_NAME "] Frontend close\n");

	/**
	 * Free the ring and deallocate the proper data.
	 */

	/* Free resources associated with old device channel. */
	if (vuart_priv->vuart.ring_ref != GRANT_INVALID_REF) {
		gnttab_me_end_foreign_access(vuart_priv->vuart.ring_ref);
		free_page((unsigned long) vuart_priv->vuart.ring.sring);

		vuart_priv->vuart.ring_ref = GRANT_INVALID_REF;
		vuart_priv->vuart.ring.sring = NULL;
	}

	if (vuart_priv->vuart.rtdm_irq.cookie)
		rtdm_unbind_from_virqhandler(&vuart_priv->vuart.rtdm_irq);

	vuart_priv->vuart.rtdm_irq.cookie = NULL;
}

static void vuart_suspend(struct vbus_me_device *vdev) {

	DBG("[" VUART_NAME "] Frontend suspend\n");
}

static void vuart_resume(struct vbus_me_device *vdev) {

	DBG("[" VUART_NAME "] Frontend resume\n");
}

bool vuart_ready(void) {
	return __vuart_ready;
}

static void vuart_connected(struct vbus_me_device *vdev) {
	vuart_priv_t *vuart_priv = vdev->vdrv->priv;

	DBG("[" VUART_NAME "] Frontend connected\n");

	__vuart_ready = true;

	/* Force the processing of pending requests, if any */
	notify_remote_via_virq(vuart_priv->vuart.irq);
}

static vdrvfront_t vuartdrv = {
	.probe = vuart_probe,
	.reconfiguring = vuart_reconfiguring,
	.shutdown = vuart_shutdown,
	.closed = vuart_closed,
	.suspend = vuart_suspend,
	.resume = vuart_resume,
	.connected = vuart_connected
};

static int vuartrt_init(void) {
	vuart_priv_t *vuart_priv;
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "vuart,frontend");

	/* Check if DTS has vuihandler enabled */
	if (!of_device_is_available(np))
		return 0;

	vuart_priv = kzalloc(sizeof(vuart_priv_t), GFP_ATOMIC);
	BUG_ON(!vuart_priv);

	vuartdrv.vdrv.priv = vuart_priv;

	vdevfront_init(VUART_NAME, &vuartdrv);

	return 0;
}

/* Initcall to be called after a *standard* device_initcall, once all backends have been initalized */
device_initcall_sync(vuartrt_init);
