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

#include <soo/dev/vsenseled.h>

typedef struct {

	/* Must be the first field */
	vsenseled_t vsenseled;

} vsenseled_priv_t;

static struct vbus_device *vsenseled_dev = NULL;

void vsenseled_notify(struct vbus_device *vdev)
{
	vsenseled_priv_t *vsenseled_priv = dev_get_drvdata(&vdev->dev);

	vsenseled_ring_response_ready(&vsenseled_priv->vsenseled.ring);

	/* Send a notification to the frontend only if connected.
	 * Otherwise, the data remain present in the ring. */

	notify_remote_via_virq(vsenseled_priv->vsenseled.irq);
}


irqreturn_t vsenseled_interrupt(int irq, void *dev_id)
{
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vsenseled_priv_t *vsenseled_priv = dev_get_drvdata(&vdev->dev);
	vsenseled_request_t *ring_req;
	vsenseled_response_t *ring_rsp;

	DBG("%d\n", dev->otherend_id);

	while ((ring_req = vsenseled_get_ring_request(&vsenseled_priv->vsenseled.ring)) != NULL) {

		ring_rsp = vsenseled_new_ring_response(&vsenseled_priv->vsenseled.ring);

		memcpy(ring_rsp->buffer, ring_req->buffer, VSENSELED_PACKET_SIZE);

		vsenseled_ring_response_ready(&vsenseled_priv->vsenseled.ring);

		notify_remote_via_virq(vsenseled_priv->vsenseled.irq);
	}

	return IRQ_HANDLED;
}

void vsenseled_probe(struct vbus_device *vdev) {
	vsenseled_priv_t *vsenseled_priv;

	vsenseled_priv = kzalloc(sizeof(vsenseled_priv_t), GFP_ATOMIC);
	BUG_ON(!vsenseled_priv);

	dev_set_drvdata(&vdev->dev, vsenseled_priv);

	vsenseled_dev = vdev;

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
	int res;
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

	res = vbus_map_ring_valloc(vdev, ring_ref, (void **) &sring);
	BUG_ON(res < 0);

	BACK_RING_INIT(&vsenseled_priv->vsenseled.ring, sring, PAGE_SIZE);

	res = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, evtchn, vsenseled_interrupt, NULL, 0, VSENSELED_NAME "-backend", vdev);

	BUG_ON(res < 0);

	vsenseled_priv->vsenseled.irq = res;
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

#if 0
	kthread_run(generator_fn, NULL, "vsenseled-gen");
#endif

	vdevback_init(VSENSELED_NAME, &vsenseleddrv);

	return 0;
}

device_initcall(vsenseled_init);
