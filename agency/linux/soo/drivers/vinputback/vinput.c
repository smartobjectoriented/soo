/*
 * Copyright (C) 2020 Nikolaos Garanis <nikolaos.garanis@heig-vd.ch>
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

#if 1
#define DEBUG
#endif

#include <stdarg.h>

#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/kthread.h>

#include <soo/evtchn.h>
#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/uapi/console.h>
#include <soo/vdevback.h>
#include <soo/dev/vinput.h>


void vinput_notify(struct vbus_device *vdev)
{
	vinput_t *vinput = to_vinput(vdev);

	RING_PUSH_RESPONSES(&vinput->ring);

	/* Send a notification to the frontend only if connected.
	 * Otherwise, the data remain present in the ring. */

	notify_remote_via_virq(vinput->irq);
}


irqreturn_t vinput_interrupt(int irq, void *dev_id)
{
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vinput_t *vinput = to_vinput(vdev);
	vinput_request_t *ring_req;
	vinput_response_t *ring_rsp;

	DBG("%d\n", vdev->otherend_id);

	while ((ring_req = vinput_ring_request(&vinput->ring)) != NULL) {
		ring_rsp = vinput_ring_response(&vinput->ring);
		memcpy(ring_rsp->buffer, ring_req->buffer, VINPUT_PACKET_SIZE);
		vinput_ring_response_ready(&vinput->ring);
		notify_remote_via_virq(vinput->irq);
	}

	return IRQ_HANDLED;
}

void vinput_probe(struct vbus_device *vdev)
{
	vinput_t *vinput;

	vinput = kzalloc(sizeof(vinput_t), GFP_ATOMIC);
	BUG_ON(!vinput);

	dev_set_drvdata(&vdev->dev, &vinput->vdevback);

	DBG(VINPUT_PREFIX "Backend probe: %d\n", vdev->otherend_id);
}

void vinput_remove(struct vbus_device *vdev)
{
	vinput_t *vinput = to_vinput(vdev);

	DBG("%s: freeing the vinput structure for %s\n", __func__,vdev->nodename);
	kfree(vinput);
}


void vinput_close(struct vbus_device *vdev)
{
	vinput_t *vinput = to_vinput(vdev);

	DBG(VINPUT_PREFIX "Backend close: %d\n", vdev->otherend_id);

	/*
	 * Free the ring and unbind evtchn.
	 */

	BACK_RING_INIT(&vinput->ring, (&vinput->ring)->sring, PAGE_SIZE);
	unbind_from_virqhandler(vinput->irq, vdev);

	vbus_unmap_ring_vfree(vdev, vinput->ring.sring);
	vinput->ring.sring = NULL;
}

void vinput_suspend(struct vbus_device *vdev)
{
	DBG(VINPUT_PREFIX "Backend suspend: %d\n", vdev->otherend_id);
}

void vinput_resume(struct vbus_device *vdev)
{
	DBG(VINPUT_PREFIX "Backend resume: %d\n", vdev->otherend_id);
}

void vinput_reconfigured(struct vbus_device *vdev)
{
	int res;
	unsigned long ring_ref;
	unsigned int evtchn;
	vinput_sring_t *sring;
	vinput_t *vinput = to_vinput(vdev);

	DBG(VINPUT_PREFIX "Backend reconfigured: %d\n", vdev->otherend_id);

	/*
	 * Set up a ring (shared page & event channel) between the agency and the ME.
	 */

	vbus_gather(VBT_NIL, vdev->otherend, "ring-ref", "%lu", &ring_ref, "ring-evtchn", "%u", &evtchn, NULL);

	DBG("BE: ring-ref=%lu, event-channel=%u\n", ring_ref, evtchn);

	res = vbus_map_ring_valloc(vdev, ring_ref, (void **) &sring);
	BUG_ON(res < 0);

	SHARED_RING_INIT(sring);
	BACK_RING_INIT(&vinput->ring, sring, PAGE_SIZE);

	res = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, evtchn, vinput_interrupt, NULL, 0, VINPUT_NAME "-backend", vdev);

	BUG_ON(res < 0);

	vinput->irq = res;
}

void vinput_connected(struct vbus_device *vdev)
{
	DBG(VINPUT_PREFIX "Backend connected: %d\n",vdev->otherend_id);
}

#if 0
/*
 * Testing code to analyze the behaviour of the ME during pre-suspend operations.
 */
int generator_fn(void *arg)
{
	uint32_t i;

	while (1) {
		msleep(50);

		for (i = 0; i < MAX_DOMAINS; i++) {

			if (!vinput_start(i))
				continue;

			vinput_ring_response_ready()
			vinput_notify(i);

			vinput_end(i);
		}
	}

	return 0;
}
#endif

vdrvback_t vinputdrv = {
	.probe = vinput_probe,
	.remove = vinput_remove,
	.close = vinput_close,
	.connected = vinput_connected,
	.reconfigured = vinput_reconfigured,
	.resume = vinput_resume,
	.suspend = vinput_suspend
};

int vinput_init(void)
{
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "vinput,backend");

	/* Check if DTS has vuihandler enabled */
	if (!of_device_is_available(np))
		return 0;

#if 0
	kthread_run(generator_fn, NULL, "vinput-gen");
#endif

	vdevback_init(VINPUT_NAME, &vinputdrv);
	return 0;
}

device_initcall(vinput_init);
