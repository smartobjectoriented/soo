/*
 * Copyright (C) 2021 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of.h>

#include <soo/evtchn.h>
#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/uapi/console.h>

#include <stdarg.h>
#include <linux/kthread.h>

#include <soo/vdevback.h>

#include <soo/dev/vsenseled.h>

#include "rpisense-led.h"

typedef struct {

	/* Must be the first field */
	vsenseled_t vsenseled;

} vsenseled_priv_t;

static struct vbus_device *vsenseled_dev = NULL;


irqreturn_t vsenseled_interrupt_bh(int irq, void *dev_id)
{
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vsenseled_priv_t *vsenseled_priv = dev_get_drvdata(&vdev->dev);
	vsenseled_request_t *ring_req;

	vdevback_processing_begin(vdev);

	while ((ring_req = vsenseled_get_ring_request(&vsenseled_priv->vsenseled.ring)) != NULL)
		display_led(ring_req->lednr, ring_req->ledstate);

	vdevback_processing_end(vdev);

	return IRQ_HANDLED;
}

irqreturn_t vsenseled_interrupt(int irq, void *dev_id)
{
	return IRQ_WAKE_THREAD;
}

void vsenseled_probe(struct vbus_device *vdev) {
	vsenseled_priv_t *vsenseled_priv;
	static bool rpisense = false;

	vsenseled_priv = kzalloc(sizeof(vsenseled_priv_t), GFP_ATOMIC);
	BUG_ON(!vsenseled_priv);

	dev_set_drvdata(&vdev->dev, vsenseled_priv);

	vsenseled_dev = vdev;

	if (!rpisense) {
		/* Initialize the RPi Sense HAT peripheral */
		senseled_init();

		rpisense = true;
	}

	DBG(VSENSELED_PREFIX "Backend probe: %d\n", vdev->otherend_id);
}

void vsenseled_remove(struct vbus_device *vdev) {
	vsenseled_priv_t *vsenseled_priv = dev_get_drvdata(&vdev->dev);

	DBG("%s: freeing the vsenseled structure for %s\n", __func__,vdev->nodename);
	kfree(vsenseled_priv);
}


void vsenseled_close(struct vbus_device *vdev) {
	vsenseled_priv_t *vsenseled_priv = dev_get_drvdata(&vdev->dev);

	DBG(VSENSELED_PREFIX "Backend close: %d\n", vdev->otherend_id);

	/*
	 * Free the ring and unbind evtchn.
	 */

	BACK_RING_INIT(&vsenseled_priv->vsenseled.ring, (&vsenseled_priv->vsenseled.ring)->sring, PAGE_SIZE);
	unbind_from_virqhandler(vsenseled_priv->vsenseled.irq, vdev);

	vbus_unmap_ring_vfree(vdev, vsenseled_priv->vsenseled.ring.sring);
	vsenseled_priv->vsenseled.ring.sring = NULL;
}

void vsenseled_suspend(struct vbus_device *vdev) {

	DBG(VSENSELED_PREFIX "Backend suspend: %d\n", vdev->otherend_id);
}

void vsenseled_resume(struct vbus_device *vdev) {

	DBG(VSENSELED_PREFIX "Backend resume: %d\n", vdev->otherend_id);
}

void vsenseled_reconfigured(struct vbus_device *vdev) {
	unsigned long ring_ref;
	unsigned int evtchn;
	vsenseled_sring_t *sring;
	vsenseled_priv_t *vsenseled_priv = dev_get_drvdata(&vdev->dev);

	DBG(VSENSELED_PREFIX "Backend reconfigured: %d\n", vdev->otherend_id);

	/*
	 * Set up a ring (shared page & event channel) between the agency and the ME.
	 */

	vbus_gather(VBT_NIL, vdev->otherend, "ring-ref", "%lu", &ring_ref, "ring-evtchn", "%u", &evtchn, NULL);

	DBG("BE: ring-ref=%u, event-channel=%u\n", ring_ref, evtchn);

	vbus_map_ring_valloc(vdev, ring_ref, (void **) &sring);

	BACK_RING_INIT(&vsenseled_priv->vsenseled.ring, sring, PAGE_SIZE);

	vsenseled_priv->vsenseled.irq = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, evtchn, vsenseled_interrupt, vsenseled_interrupt_bh,
					0, VSENSELED_NAME "-backend", vdev);
}

void vsenseled_connected(struct vbus_device *vdev) {

	DBG(VSENSELED_PREFIX "Backend connected: %d\n",vdev->otherend_id);
}


vdrvback_t vsenseleddrv = {
	.probe = vsenseled_probe,
	.remove = vsenseled_remove,
	.close = vsenseled_close,
	.connected = vsenseled_connected,
	.reconfigured = vsenseled_reconfigured,
	.resume = vsenseled_resume,
	.suspend = vsenseled_suspend
};

int vsenseled_init(void) {
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "vsenseled,backend");

	/* Check if DTS has vuihandler enabled */
	if (!of_device_is_available(np))
		return 0;

	vdevback_init(VSENSELED_NAME, &vsenseleddrv);

	return 0;
}

device_initcall(vsenseled_init);
