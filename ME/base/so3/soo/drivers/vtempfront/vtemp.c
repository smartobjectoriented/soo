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
	/* wait data from backend */
	struct completion wait_temp;

} vtemp_priv_t;

static struct vbus_device *vtemp_dev = NULL;


/**
 * Get temperature data from temperature LoRa module (SOO.heat temp)
 * @return 0 if success, -1 if not ready yet
 */
int vtemp_get_temp_data(char *buf) {
	vtemp_priv_t *vtemp_priv;
	vtemp_response_t *ring_rsp;
	int len = 0;

	vtemp_priv = (vtemp_priv_t *) dev_get_drvdata(vtemp_dev->dev);

	/* Ask temperature to BE */
	DBG(VTEMP_PREFIX "Notify irq FE\n");
	DBG(VTEMP_PREFIX "%d vtemp.irq FE\n", vtemp_priv->vtemp.irq);
	notify_remote_via_virq(vtemp_priv->vtemp.irq);
	DBG(VTEMP_PREFIX "%d vtemp.irq FE\n", vtemp_priv->vtemp.irq);

	/* wait response from BE*/
	DBG(VTEMP_PREFIX "wait for completion get temp FE\n");
	wait_for_completion(&vtemp_priv->wait_temp);
	// wait_for_completion(&wait_data_sent_completion);
	DBG(VTEMP_PREFIX "After completion get temp FE\n");

	// while ((ring_rsp = vtemp_get_ring_response(&vtemp_priv->vtemp.ring)) != NULL) //{
	// 	DBG(VTEMP_PREFIX "After while(1) get temp FE\n");

	// 	temp_data->temp = ring_rsp->temp;
	// 	temp_data->dev_id = ring_rsp->dev_id;
	// }

	ring_rsp = vtemp_get_ring_response(&vtemp_priv->vtemp.ring);
	BUG_ON(!ring_rsp);
	DBG(VTEMP_PREFIX "After while(1) get temp FE\n");

	len = ring_rsp->len;
	BUG_ON(len < 1);

	BUG_ON(!buf);

	memcpy(buf, ring_rsp->buffer, len);
	DBG("venocean get data %d FE\n", len);

	return len;
}


irq_return_t vtemp_interrupt(int irq, void *dev_id) {
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vtemp_priv_t *vtemp_priv = (vtemp_priv_t *) dev_get_drvdata(vtemp_dev->dev);
	
	/* data receive from BE*/
	complete(&vtemp_priv->wait_temp);
	DBG(VTEMP_PREFIX " irq handled FE\n");
	DBG(VTEMP_PREFIX "%d vtemp.irq FE\n", vtemp_priv->vtemp.irq);
	return IRQ_COMPLETED;
}


static void vtemp_probe(struct vbus_device *vdev) {
	int res;
	unsigned int evtchn;
	vtemp_sring_t *sring;
	struct vbus_transaction vbt;
	vtemp_priv_t *vtemp_priv;

	DBG(VTEMP_PREFIX " Probe FE\n");

	if (vdev->state == VbusStateConnected)
		return ;

	vtemp_priv = dev_get_drvdata(vdev->dev);
	vtemp_dev = vdev;

	DBG(VTEMP_PREFIX " Setup ring FE\n");

	/* Prepare to set up the ring. */

	vtemp_priv->vtemp.ring_ref = GRANT_INVALID_REF;

	/* Allocate an event channel associated to the ring */
	vbus_alloc_evtchn(vdev, &evtchn);

	DBG(VTEMP_PREFIX "%d evtchn FE\n", evtchn);
	res = bind_evtchn_to_irq_handler(evtchn, vtemp_interrupt, NULL, vdev);
	if (res <= 0) {
		lprintk("%s - line %d: Binding event channel failed for device %s\n", __func__, __LINE__, vdev->nodename);
		BUG();
	}

	DBG(VTEMP_PREFIX "%d evtchn FE\n", evtchn);
	DBG(VTEMP_PREFIX "%d res FE\n", res);
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

	res = vbus_grant_ring(vdev, phys_to_pfn(virt_to_phys_pt((addr_t) vtemp_priv->vtemp.ring.sring)));
	if (res < 0)
		BUG();

	vtemp_priv->vtemp.ring_ref = res;

	init_completion(&vtemp_priv->wait_temp);

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "ring-ref", "%u", vtemp_priv->vtemp.ring_ref);
	vbus_printf(vbt, vdev->nodename, "ring-evtchn", "%u", vtemp_priv->vtemp.evtchn);

	vbus_transaction_end(vbt);

	DBG(VTEMP_PREFIX "  Probed successfully FE\n");
}


/* At this point, the FE is not connected. */
static void vtemp_reconfiguring(struct vbus_device *vdev) {
	int res;
	struct vbus_transaction vbt;
	vtemp_priv_t *vtemp_priv = dev_get_drvdata(vdev->dev);

	DBG(VTEMP_PREFIX  " Reconfiguring FE\n");
	/* The shared page already exists */
	/* Re-init */

	gnttab_end_foreign_access_ref(vtemp_priv->vtemp.ring_ref);

	DBG(VTEMP_PREFIX " Setup ring FE\n");

	/* Prepare to set up the ring. */

	vtemp_priv->vtemp.ring_ref = GRANT_INVALID_REF;

	SHARED_RING_INIT(vtemp_priv->vtemp.ring.sring);
	FRONT_RING_INIT(&vtemp_priv->vtemp.ring, (&vtemp_priv->vtemp.ring)->sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	res = vbus_grant_ring(vdev, phys_to_pfn(virt_to_phys_pt((addr_t) vtemp_priv->vtemp.ring.sring)));
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

	DBG(VTEMP_PREFIX " Close FE\n");

	/**
	 * Free the ring and deallocate the proper data.
	 */

	/* Free resources associated with old device channel. */
	if (vtemp_priv->vtemp.ring_ref != GRANT_INVALID_REF) {
		gnttab_end_foreign_access(vtemp_priv->vtemp.ring_ref);
		free_vpage((addr_t) vtemp_priv->vtemp.ring.sring);

		vtemp_priv->vtemp.ring_ref = GRANT_INVALID_REF;
		vtemp_priv->vtemp.ring.sring = NULL;
	}

	if (vtemp_priv->vtemp.irq)
		unbind_from_irqhandler(vtemp_priv->vtemp.irq);

	vtemp_priv->vtemp.irq = 0;
}


static void vtemp_suspend(struct vbus_device *vdev) {

	DBG(VTEMP_PREFIX " Suspend FE\n");
}


static void vtemp_resume(struct vbus_device *vdev) {

	DBG(VTEMP_PREFIX " Resume FE\n");
}


static void vtemp_connected(struct vbus_device *vdev) {
	vtemp_priv_t *vtemp_priv = dev_get_drvdata(vdev->dev);

	DBG(VTEMP_PREFIX " Connected FE\n");

	/* Force the processing of pending requests, if any */
	// notify_remote_via_virq(vtemp_priv->vtemp.irq);
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


static int vtemp_init(dev_t *dev, int fdt_offset) {
	vtemp_priv_t *vtemp_priv;

	vtemp_priv = malloc(sizeof(vtemp_priv_t));
	BUG_ON(!vtemp_priv);

	memset(vtemp_priv, 0, sizeof(vtemp_priv_t));

	dev_set_drvdata(dev, vtemp_priv);

	// lprintk("[ %s ] FRONTEND INIT CALLED \n", VTEMP_NAME);

	vdevfront_init(VTEMP_NAME, &vtempdrv);
	
	DBG(VTEMP_PREFIX " Initialized successfully FE\n");

	return 0;
}


REGISTER_DRIVER_POSTCORE("vtemp,frontend", vtemp_init);
