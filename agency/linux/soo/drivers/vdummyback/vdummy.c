/*
 * Copyright (C) 2016-2018 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2016 Baptiste Delporte <bonel@bonel.net>
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

#include <soo/dev/vdummy.h>

typedef struct {

	/* Must be the first field */
	vdummy_t vdummy;

} vdummy_priv_t;

static struct vbus_device *vdummy_dev = NULL;

static irqreturn_t vdummy_interrupt(int irq, void *dev_id)
{
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vdummy_priv_t *vdummy_priv = dev_get_drvdata(&vdev->dev);
	vdummy_request_t *ring_req;
	vdummy_response_t *ring_rsp;

	DBG("%d\n", vdev->otherend_id);

	while ((ring_req = vdummy_get_ring_request(&vdummy_priv->vdummy.ring)) != NULL) {

		ring_rsp = vdummy_new_ring_response(&vdummy_priv->vdummy.ring);

		memcpy(ring_rsp->buffer, ring_req->buffer, VDUMMY_PACKET_SIZE);

		vdummy_ring_response_ready(&vdummy_priv->vdummy.ring);

		notify_remote_via_virq(vdummy_priv->vdummy.irq);
	}

	return IRQ_HANDLED;
}

static void vdummy_probe(struct vbus_device *vdev) {
	vdummy_priv_t *vdummy_priv;

	vdummy_priv = kzalloc(sizeof(vdummy_priv_t), GFP_ATOMIC);
	BUG_ON(!vdummy_priv);

	dev_set_drvdata(&vdev->dev, vdummy_priv);

	vdummy_dev = vdev;

	DBG(VDUMMY_PREFIX "Backend probe: %d\n", vdev->otherend_id);
}

static void vdummy_remove(struct vbus_device *vdev) {
	vdummy_priv_t *vdummy_priv = dev_get_drvdata(&vdev->dev);

	DBG("%s: freeing the vdummy structure for %s\n", __func__,vdev->nodename);
	kfree(vdummy_priv);
}


static void vdummy_close(struct vbus_device *vdev) {
	vdummy_priv_t *vdummy_priv = dev_get_drvdata(&vdev->dev);

	DBG(VDUMMY_PREFIX "Backend close: %d\n", vdev->otherend_id);

	/*
	 * Free the ring and unbind evtchn.
	 */

	BACK_RING_INIT(&vdummy_priv->vdummy.ring, (&vdummy_priv->vdummy.ring)->sring, PAGE_SIZE);
	unbind_from_virqhandler(vdummy_priv->vdummy.irq, vdev);

	vbus_unmap_ring_vfree(vdev, vdummy_priv->vdummy.ring.sring);
	vdummy_priv->vdummy.ring.sring = NULL;
}

static void vdummy_suspend(struct vbus_device *vdev) {

	DBG(VDUMMY_PREFIX "Backend suspend: %d\n", vdev->otherend_id);
}

static void vdummy_resume(struct vbus_device *vdev) {

	DBG(VDUMMY_PREFIX "Backend resume: %d\n", vdev->otherend_id);
}

static void vdummy_reconfigured(struct vbus_device *vdev) {
	int res;
	unsigned long ring_ref;
	unsigned int evtchn;
	vdummy_sring_t *sring;
	vdummy_priv_t *vdummy_priv = dev_get_drvdata(&vdev->dev);

	DBG(VDUMMY_PREFIX "Backend reconfigured: %d\n", vdev->otherend_id);

	/*
	 * Set up a ring (shared page & event channel) between the agency and the ME.
	 */

	vbus_gather(VBT_NIL, vdev->otherend, "ring-ref", "%lu", &ring_ref, "ring-evtchn", "%u", &evtchn, NULL);

	DBG("BE: ring-ref=%u, event-channel=%u\n", ring_ref, evtchn);

	res = vbus_map_ring_valloc(vdev, ring_ref, (void **) &sring);
	BUG_ON(res < 0);

	BACK_RING_INIT(&vdummy_priv->vdummy.ring, sring, PAGE_SIZE);

	res = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, evtchn, vdummy_interrupt, NULL, 0, VDUMMY_NAME "-backend", vdev);

	BUG_ON(res < 0);

	vdummy_priv->vdummy.irq = res;
}

static void vdummy_connected(struct vbus_device *vdev) {

	DBG(VDUMMY_PREFIX "Backend connected: %d\n",vdev->otherend_id);
}


static vdrvback_t vdummydrv = {
	.probe = vdummy_probe,
	.remove = vdummy_remove,
	.close = vdummy_close,
	.connected = vdummy_connected,
	.reconfigured = vdummy_reconfigured,
	.resume = vdummy_resume,
	.suspend = vdummy_suspend
};

int vdummy_init(void) {
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "vdummy,backend");

	/* Check if DTS has vuihandler enabled */
	if (!of_device_is_available(np))
		return 0;

#if 0
	kthread_run(generator_fn, NULL, "vdummy-gen");
#endif

	vdevback_init(VDUMMY_NAME, &vdummydrv);

	return 0;
}

device_initcall(vdummy_init);
