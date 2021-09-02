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
#include <linux/input.h>
#include <linux/kthread.h>

#include <soo/evtchn.h>
#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/uapi/console.h>

#include <stdarg.h>

#include <soo/vdevback.h>

#include <soo/dev/vsensej.h>

#include "rpisense-joystick.h"

typedef struct {

	/* Must be the first field */
	vsensej_t vsensej;

} vsensej_priv_t;

static struct vbus_device *vsensej_dev = NULL;

void j_handler(struct vbus_device *vdev, int key) {
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
}

void vsensej_probe(struct vbus_device *vdev) {
	vsensej_priv_t *vsensej_priv;
	static bool rpisense = false;

	vsensej_priv = kzalloc(sizeof(vsensej_priv_t), GFP_ATOMIC);
	BUG_ON(!vsensej_priv);

	dev_set_drvdata(&vdev->dev, vsensej_priv);

	vsensej_dev = vdev;

	if (!rpisense) {
		/* Initialize the RPi Sense HAT peripheral */
		sensej_init();

		rpisense = true;
	}

	rpisense_joystick_handler_register(vdev, j_handler);

	DBG(VSENSEJ_PREFIX "Backend probe: %d\n", vdev->otherend_id);
}

void vsensej_remove(struct vbus_device *vdev) {
	vsensej_priv_t *vsensej_priv = dev_get_drvdata(&vdev->dev);

	DBG("%s: freeing the vsensej structure for %s\n", __func__,vdev->nodename);
	kfree(vsensej_priv);
}

void vsensej_close(struct vbus_device *vdev) {
	vsensej_priv_t *vsensej_priv = dev_get_drvdata(&vdev->dev);

	DBG(VSENSEJ_PREFIX "Backend close: %d\n", vdev->otherend_id);

	/*
	 * Free the ring.
	 */

	BACK_RING_INIT(&vsensej_priv->vsensej.ring, (&vsensej_priv->vsensej.ring)->sring, PAGE_SIZE);

	/* Unbind the irq */
	unbind_from_virqhandler(vsensej_priv->vsensej.irq, vdev);

	vbus_unmap_ring_vfree(vdev, vsensej_priv->vsensej.ring.sring);
	vsensej_priv->vsensej.ring.sring = NULL;
}

void vsensej_suspend(struct vbus_device *vdev) {

	DBG(VSENSEJ_PREFIX "Backend suspend: %d\n", vdev->otherend_id);
}

void vsensej_resume(struct vbus_device *vdev) {

	DBG(VSENSEJ_PREFIX "Backend resume: %d\n", vdev->otherend_id);
}

void vsensej_reconfigured(struct vbus_device *vdev) {
	int res;
	unsigned long ring_ref;
	unsigned int evtchn;
	vsensej_sring_t *sring;
	vsensej_priv_t *vsensej_priv = dev_get_drvdata(&vdev->dev);

	DBG(VSENSEJ_PREFIX "Backend reconfigured: %d\n", vdev->otherend_id);

	/*
	 * Set up a ring (shared page & event channel) between the agency and the ME.
	 */

	vbus_gather(VBT_NIL, vdev->otherend, "ring-ref", "%lu", &ring_ref, "ring-evtchn", "%u", &evtchn, NULL);

	DBG("BE: ring-ref=%ld, event-channel=%d\n", ring_ref, evtchn);

	res = vbus_map_ring_valloc(vdev, ring_ref, (void **) &sring);
	BUG_ON(res < 0);

	BACK_RING_INIT(&vsensej_priv->vsensej.ring, sring, PAGE_SIZE);

	/* No handler required, however used to notify the remote domain */
	res = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, evtchn, NULL, NULL, 0, VSENSEJ_NAME "-backend", vdev);
	BUG_ON(res < 0);

	vsensej_priv->vsensej.irq = res;
}

void vsensej_connected(struct vbus_device *vdev) {

	DBG(VSENSEJ_PREFIX "Backend connected: %d\n",vdev->otherend_id);
}


vdrvback_t vsensejdrv = {
	.probe = vsensej_probe,
	.remove = vsensej_remove,
	.close = vsensej_close,
	.connected = vsensej_connected,
	.reconfigured = vsensej_reconfigured,
	.resume = vsensej_resume,
	.suspend = vsensej_suspend
};

int vsensej_init(void) {
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "vsensej,backend");

	/* Check if DTS has vuihandler enabled */
	if (!of_device_is_available(np))
		return 0;

	vdevback_init(VSENSEJ_NAME, &vsensejdrv);

	return 0;
}

device_initcall(vsensej_init);
