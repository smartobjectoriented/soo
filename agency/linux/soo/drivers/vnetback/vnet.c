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

#include <soo/dev/vnet.h>

void vnet_notify(struct vbus_device *vdev)
{
	vnet_t *vnet = to_vnet(vdev);

	RING_PUSH_RESPONSES(&vnet->ring);

	/* Send a notification to the frontend only if connected.
	 * Otherwise, the data remain present in the ring. */

	notify_remote_via_virq(vnet->irq);

}


irqreturn_t vnet_interrupt(int irq, void *dev_id)
{
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vnet_t *vnet = to_vnet(vdev);
	vnet_request_t *ring_req;
	vnet_response_t *ring_rsp;

	//DBG("%d\n", dev->otherend_id);

	while ((ring_req = vnet_ring_request(&vnet->ring)) != NULL) {

		ring_rsp = vnet_ring_response(&vnet->ring);

		memcpy(ring_rsp->buffer, ring_req->buffer, VNET_PACKET_SIZE);

		vnet_ring_response_ready(&vnet->ring);

		notify_remote_via_virq(vnet->irq);
	}

	return IRQ_HANDLED;
}

void vnet_probe(struct vbus_device *vdev) {
	vnet_t *vnet;

	vnet = kzalloc(sizeof(vnet_t), GFP_ATOMIC);
	BUG_ON(!vnet);

	dev_set_drvdata(&vdev->dev, &vnet->vdevback);

	DBG(VNET_PREFIX "Backend probe: %d\n", vdev->otherend_id);
}

void vnet_remove(struct vbus_device *vdev) {
	vnet_t *vnet = to_vnet(vdev);

	DBG("%s: freeing the vnet structure for %s\n", __func__,vdev->nodename);
	kfree(vnet);
}


void vnet_close(struct vbus_device *vdev) {
	vnet_t *vnet = to_vnet(vdev);

	DBG(VNET_PREFIX "Backend close: %d\n", vdev->otherend_id);

	/*
	 * Free the ring and unbind evtchn.
	 */

	BACK_RING_INIT(&vnet->ring, (&vnet->ring)->sring, PAGE_SIZE);
	unbind_from_virqhandler(vnet->irq, vdev);

	vbus_unmap_ring_vfree(vdev, vnet->ring.sring);
	vnet->ring.sring = NULL;
}

void vnet_suspend(struct vbus_device *vdev) {

	DBG(VNET_PREFIX "Backend suspend: %d\n", vdev->otherend_id);
}

void vnet_resume(struct vbus_device *vdev) {

	DBG(VNET_PREFIX "Backend resume: %d\n", vdev->otherend_id);
}

void vnet_reconfigured(struct vbus_device *vdev) {
	int res;
	unsigned long ring_ref;
	unsigned int evtchn;
	vnet_sring_t *sring;
	vnet_t *vnet = to_vnet(vdev);

	DBG(VNET_PREFIX "Backend reconfigured: %d\n", vdev->otherend_id);

	/*
	 * Set up a ring (shared page & event channel) between the agency and the ME.
	 */

	vbus_gather(VBT_NIL, vdev->otherend, "ring-ref", "%lu", &ring_ref, "ring-evtchn", "%u", &evtchn, NULL);

	DBG("BE: ring-ref=%u, event-channel=%u\n", ring_ref, evtchn);

	res = vbus_map_ring_valloc(vdev, ring_ref, (void **) &sring);
	BUG_ON(res < 0);

	SHARED_RING_INIT(sring);
	BACK_RING_INIT(&vnet->ring, sring, PAGE_SIZE);

	res = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, evtchn, vnet_interrupt, NULL, 0, VNET_NAME "-backend", vdev);

	BUG_ON(res < 0);

	vnet->irq = res;
}

void vnet_connected(struct vbus_device *vdev) {

	DBG(VNET_PREFIX "Backend connected: %d\n",vdev->otherend_id);
}

#if 0
/*
 * Testing code to analyze the behaviour of the ME during pre-suspend operations.
 */
int generator_fn(void *arg) {
	uint32_t i;

	while (1) {
		msleep(50);

		for (i = 0; i < MAX_DOMAINS; i++) {

			if (!vnet_start(i))
				continue;

			vnet_ring_response_ready()
			vnet_notify(i);

			vnet_end(i);
		}
	}

	return 0;
}
#endif

vdrvback_t vnetdrv = {
	.probe = vnet_probe,
	.remove = vnet_remove,
	.close = vnet_close,
	.connected = vnet_connected,
	.reconfigured = vnet_reconfigured,
	.resume = vnet_resume,
	.suspend = vnet_suspend
};

int vnet_init(void) {
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "vnet,backend");

	/* Check if DTS has vuihandler enabled */
	if (!of_device_is_available(np))
		return 0;

#if 0
	kthread_run(generator_fn, NULL, "vDummy-gen");
#endif

	vdevback_init(VNET_NAME, &vnetdrv);

	return 0;
}

device_initcall(vnet_init);
