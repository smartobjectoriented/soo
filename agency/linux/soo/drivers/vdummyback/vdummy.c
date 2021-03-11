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

#include <soo/dev/vdummy.h>

void vdummy_notify(struct vbus_device *vdev)
{
	vdummy_t *vdummy = to_vdummy(vdev);

	RING_PUSH_RESPONSES(&vdummy->ring);

	/* Send a notification to the frontend only if connected.
	 * Otherwise, the data remain present in the ring. */

	notify_remote_via_virq(vdummy->irq);

}


irqreturn_t vdummy_interrupt(int irq, void *dev_id)
{
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vdummy_t *vdummy = to_vdummy(vdev);
	vdummy_request_t *ring_req;
	vdummy_response_t *ring_rsp;

	DBG("%d\n", dev->otherend_id);

	while ((ring_req = vdummy_get_ring_request(&vdummy->ring)) != NULL) {

		ring_rsp = vdummy_new_ring_response(&vdummy->ring);

		memcpy(ring_rsp->buffer, ring_req->buffer, VDUMMY_PACKET_SIZE);

		vdummy_ring_response_ready(&vdummy->ring);

		notify_remote_via_virq(vdummy->irq);
	}

	return IRQ_HANDLED;
}

void vdummy_probe(struct vbus_device *vdev) {
	vdummy_t *vdummy;

	vdummy = kzalloc(sizeof(vdummy_t), GFP_KERNEL);
	BUG_ON(!vdummy);

	dev_set_drvdata(&vdev->dev, &vdummy->vdevback);

	DBG(VDUMMY_PREFIX "Backend probe: %d\n", vdev->otherend_id);
}

void vdummy_remove(struct vbus_device *vdev) {
	vdummy_t *vdummy = to_vdummy(vdev);

	DBG("%s: freeing the vdummy structure for %s\n", __func__,vdev->nodename);
	kfree(vdummy);
}


void vdummy_close(struct vbus_device *vdev) {
	vdummy_t *vdummy = to_vdummy(vdev);

	DBG(VDUMMY_PREFIX "Backend close: %d\n", vdev->otherend_id);

	/*
	 * Free the ring and unbind evtchn.
	 */

	BACK_RING_INIT(&vdummy->ring, (&vdummy->ring)->sring, PAGE_SIZE);
	unbind_from_virqhandler(vdummy->irq, vdev);

	vbus_unmap_ring_vfree(vdev, vdummy->ring.sring);
	vdummy->ring.sring = NULL;
}

void vdummy_suspend(struct vbus_device *vdev) {

	DBG(VDUMMY_PREFIX "Backend suspend: %d\n", vdev->otherend_id);
}

void vdummy_resume(struct vbus_device *vdev) {

	DBG(VDUMMY_PREFIX "Backend resume: %d\n", vdev->otherend_id);
}

void vdummy_reconfigured(struct vbus_device *vdev) {
	int res;
	unsigned long ring_ref;
	unsigned int evtchn;
	vdummy_sring_t *sring;
	vdummy_t *vdummy = to_vdummy(vdev);

	DBG(VDUMMY_PREFIX "Backend reconfigured: %d\n", vdev->otherend_id);

	/*
	 * Set up a ring (shared page & event channel) between the agency and the ME.
	 */

	vbus_gather(VBT_NIL, vdev->otherend, "ring-ref", "%lu", &ring_ref, "ring-evtchn", "%u", &evtchn, NULL);

	DBG("BE: ring-ref=%u, event-channel=%u\n", ring_ref, evtchn);

	res = vbus_map_ring_valloc(vdev, ring_ref, (void **) &sring);
	BUG_ON(res < 0);

	BACK_RING_INIT(&vdummy->ring, sring, PAGE_SIZE);

	res = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, evtchn, vdummy_interrupt, NULL, 0, VDUMMY_NAME "-backend", vdev);

	BUG_ON(res < 0);

	vdummy->irq = res;
}

void vdummy_connected(struct vbus_device *vdev) {

	DBG(VDUMMY_PREFIX "Backend connected: %d\n",vdev->otherend_id);
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

			if (!vdummy_start(i))
				continue;

			vdummy_ring_response_ready()
			vdummy_notify(i);

			vdummy_end(i);
		}
	}

	return 0;
}
#endif

vdrvback_t vdummydrv = {
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
	kthread_run(generator_fn, NULL, "vDummy-gen");
#endif

	vdevback_init(VDUMMY_NAME, &vdummydrv);

	return 0;
}

device_initcall(vdummy_init);
