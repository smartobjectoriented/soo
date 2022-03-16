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

#if 1
#define DEBUG
#endif

#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/input.h>
#include <linux/kthread.h>

#include <soo/evtchn.h>
#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/uapi/console.h>

#include <stdarg.h>

#include <soo/vdevback.h>

#include <soo/dev/vwagoled.h>


typedef struct {

	/* Must be the first field */
	vwagoled_t vwagoled;

} vwagoled_priv_t;

static struct vbus_device *vwagoled_dev = NULL;

/* void j_handler(struct vbus_device *vdev, int key) {
	vsensej_priv_t *vsensej_priv = dev_get_drvdata(&vdev->dev);
	vsensej_response_t *vsensej_response;

	if (vdevfront_is_connected(vdev)) {

		vdevback_processing_begin(vdev);

		vsensej_response = vsensej_new_ring_response(&vsensej_priv->vsensej.ring);

		vsensej_response->type = EV_KEY;

		switch (key) {

		case RPISENSE_JS_UP:
			vsensej_response->code = KEY_UP;
			break;

		case RPISENSE_JS_DOWN:
			vsensej_response->code = KEY_DOWN;
			break;

		case RPISENSE_JS_RIGHT:
			vsensej_response->code = KEY_RIGHT;
			break;

		case RPISENSE_JS_LEFT:
			vsensej_response->code = KEY_LEFT;
			break;

		case RPISENSE_JS_CENTER:
			vsensej_response->code = KEY_ENTER;
			break;
		}

		vsensej_response->value = ((key == 0) ? 0 : 1);

		vsensej_ring_response_ready(&vsensej_priv->vsensej.ring);

		notify_remote_via_virq(vsensej_priv->vsensej.irq);

		vdevback_processing_end(vdev);
	}
} */

void vwagoled_probe(struct vbus_device *vdev) {
	vwagoled_priv_t *vwagoled_priv;

	vwagoled_priv = kzalloc(sizeof(vwagoled_priv_t), GFP_ATOMIC);
	BUG_ON(!vwagoled_priv);

	dev_set_drvdata(&vdev->dev, vwagoled_priv);

	vwagoled_dev = vdev;

	DBG(VWAGOLED_PREFIX "Backend probe: %d\n", vdev->otherend_id);
}

void vwagoled_remove(struct vbus_device *vdev) {
	vwagoled_priv_t *vwagoled_priv = dev_get_drvdata(&vdev->dev);

	DBG("%s: freeing the vwagoled structure for %s\n", __func__,vdev->nodename);
	kfree(vwagoled_priv);
}

void vwagoled_close(struct vbus_device *vdev) {
	vwagoled_priv_t *vwagoled_priv = dev_get_drvdata(&vdev->dev);

	DBG(VWAGOLED_PREFIX "Backend close: %d\n", vdev->otherend_id);

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

	DBG(VWAGOLED_PREFIX "Backend suspend: %d\n", vdev->otherend_id);
}

void vwagoled_resume(struct vbus_device *vdev) {

	DBG(VWAGOLED_PREFIX "Backend resume: %d\n", vdev->otherend_id);
}

void vwagoled_reconfigured(struct vbus_device *vdev) {
	int res;
	unsigned long ring_ref;
	unsigned int evtchn;
	vwagoled_sring_t *sring;
	vwagoled_priv_t *vwagoled_priv = dev_get_drvdata(&vdev->dev);

	DBG(VWAGOLED_PREFIX "Backend reconfigured: %d\n", vdev->otherend_id);

	/*
	 * Set up a ring (shared page & event channel) between the agency and the ME.
	 */

	vbus_gather(VBT_NIL, vdev->otherend, "ring-ref", "%lu", &ring_ref, "ring-evtchn", "%u", &evtchn, NULL);

	DBG("BE: ring-ref=%ld, event-channel=%d\n", ring_ref, evtchn);

	res = vbus_map_ring_valloc(vdev, ring_ref, (void **) &sring);
	BUG_ON(res < 0);

	BACK_RING_INIT(&vwagoled_priv->vwagoled.ring, sring, PAGE_SIZE);

	/* No handler required, however used to notify the remote domain */
	res = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, evtchn, NULL, NULL, 0, VWAGOLED_NAME "-backend", vdev);
	BUG_ON(res < 0);

	vwagoled_priv->vwagoled.irq = res;
}

void vwagoled_connected(struct vbus_device *vdev) {

	DBG(VWAGOLED_PREFIX "Backend connected: %d\n",vdev->otherend_id);
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

     DBG(VWAGOLED_PREFIX "Backend starting\n");

	np = of_find_compatible_node(NULL, NULL, "vwagoled,backend");

	/* Check if DTS has vuihandler enabled */
	if (!of_device_is_available(np))
		return 0;

	vdevback_init(VWAGOLED_NAME, &vwagoleddrv);

    DBG(VWAGOLED_PREFIX "Backend initialized\n");
	
    return 0;
}

device_initcall(vwagoled_init);
