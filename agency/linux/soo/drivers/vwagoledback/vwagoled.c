/*
 * Copyright (C) 2022 Mattia Gallacchi <mattia.gallacchi@heig-vd.ch>
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

#include <linux/of.h>

#include <soo/evtchn.h>
#include <soo/dev/vwagoled.h>

#include "ledctrl.h"


typedef struct {
	/* Must be the first field */
	vwagoled_t vwagoled;

} vwagoled_priv_t;

irqreturn_t vwagoled_interrupt_bh(int irq, void *dev_id) {
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vwagoled_priv_t *vwagoled_priv = dev_get_drvdata(&vdev->dev);
	vwagoled_request_t *ring_req;

	vdevback_processing_begin(vdev);

	while ((ring_req = vwagoled_get_ring_request(&vwagoled_priv->vwagoled.ring)) != NULL) {
		ledctrl_process_request(ring_req->cmd, ring_req->ids, ring_req->ids_count);
	}
		
	vdevback_processing_end(vdev);

	return IRQ_HANDLED;
}

irqreturn_t vwagoled_interrupt(int irq, void *dev_id) {
	return IRQ_WAKE_THREAD;
}

void vwagoled_probe(struct vbus_device *vdev) {
	vwagoled_priv_t *vwagoled_priv;

	vwagoled_priv = kzalloc(sizeof(vwagoled_priv_t), GFP_ATOMIC);
	BUG_ON(!vwagoled_priv);

	dev_set_drvdata(&vdev->dev, vwagoled_priv);

	DBG(VWAGOLED_PREFIX "Probe: %d\n", vdev->otherend_id);
}

void vwagoled_remove(struct vbus_device *vdev) {
	vwagoled_priv_t *vwagoled_priv = dev_get_drvdata(&vdev->dev);

	DBG("%s: freeing the vwagoled structure for %s\n", __func__,vdev->nodename);
	kfree(vwagoled_priv);
}

void vwagoled_close(struct vbus_device *vdev) {
	vwagoled_priv_t *vwagoled_priv = dev_get_drvdata(&vdev->dev);

	DBG(VWAGOLED_PREFIX "Close: %d\n", vdev->otherend_id);

	/*
	 * Free the ring.
	 */

	BACK_RING_INIT(&vwagoled_priv->vwagoled.ring, (&vwagoled_priv->vwagoled.ring)->sring, PAGE_SIZE);

	/* Unbind the irq */
	unbind_from_virqhandler(vwagoled_priv->vwagoled.irq, vdev);

	vbus_unmap_ring_vfree(vdev, vwagoled_priv->vwagoled.ring.sring);
	vwagoled_priv->vwagoled.ring.sring = NULL;
}

void vwagoled_suspend(struct vbus_device *vdev) {

	DBG(VWAGOLED_PREFIX "Suspend: %d\n", vdev->otherend_id);
}

void vwagoled_resume(struct vbus_device *vdev) {

	DBG(VWAGOLED_PREFIX "Resume: %d\n", vdev->otherend_id);
}

void vwagoled_reconfigured(struct vbus_device *vdev) {
	int res;
	unsigned long ring_ref;
	unsigned int evtchn;
	vwagoled_sring_t *sring;
	vwagoled_priv_t *vwagoled_priv = dev_get_drvdata(&vdev->dev);

	DBG(VWAGOLED_PREFIX "Reconfigured: %d\n", vdev->otherend_id);

	/*
	 * Set up a ring (shared page & event channel) between the agency and the ME.
	 */

	vbus_gather(VBT_NIL, vdev->otherend, "ring-ref", "%lu", &ring_ref, "ring-evtchn", "%u", &evtchn, NULL);

	DBG(VWAGOLED_PREFIX "BE: ring-ref=%ld, event-channel=%d\n", ring_ref, evtchn);

	vbus_map_ring_valloc(vdev, ring_ref, (void **) &sring);

	BACK_RING_INIT(&vwagoled_priv->vwagoled.ring, sring, PAGE_SIZE);

	/* No handler required, however used to notify the remote domain */
	res = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, evtchn, vwagoled_interrupt, 
													vwagoled_interrupt_bh, 0, VWAGOLED_NAME "-backend", vdev);
	BUG_ON(res < 0);

	vwagoled_priv->vwagoled.irq = res;
}

void vwagoled_connected(struct vbus_device *vdev) {

	DBG(VWAGOLED_PREFIX "Connected: %d\n",vdev->otherend_id);
}


vdrvback_t vwagoleddrv = {
	.probe = vwagoled_probe,
	.remove = vwagoled_remove,
	.close = vwagoled_close,
	.connected = vwagoled_connected,
	.reconfigured = vwagoled_reconfigured,
	.resume = vwagoled_resume,
	.suspend = vwagoled_suspend
};

int vwagoled_init(void) {
	struct device_node *np;

    printk(VWAGOLED_PREFIX "Starting\n");

	np = of_find_compatible_node(NULL, NULL, "vwagoled,backend");

	/* Check if DTS has vwagoled enabled */
	if (!of_device_is_available(np))
		return 0;

	vdevback_init(VWAGOLED_NAME, &vwagoleddrv);

	if (ledctrl_init() < 0) {
		BUG();
	}

    printk(VWAGOLED_PREFIX "Initialized\n");
	
    return 0;
}

device_initcall(vwagoled_init);
