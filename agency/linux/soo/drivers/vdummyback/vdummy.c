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

vdummy_t vdummy;

void vdummy_notify(domid_t domid)
{
	vdummy_ring_t *p_vdummy_ring = &vdummy.rings[domid];

	RING_PUSH_RESPONSES(&p_vdummy_ring->ring);

	/* Send a notification to the frontend only if connected.
	 * Otherwise, the data remain present in the ring. */

	notify_remote_via_virq(p_vdummy_ring->irq);

}

irqreturn_t vdummy_interrupt(int irq, void *dev_id)
{
	struct vbus_device *dev = (struct vbus_device *) dev_id;
	RING_IDX i, rp;
	vdummy_request_t *ring_req;
	vdummy_response_t *ring_rsp;

	if (!vdummy_is_connected(dev->otherend_id))
		return IRQ_HANDLED;

	DBG("%d\n", dev->otherend_id);

	rp = vdummy.rings[dev->otherend_id].ring.sring->req_prod;
	dmb();

	for (i = vdummy.rings[dev->otherend_id].ring.sring->req_cons; i != rp; i++) {

		ring_req = RING_GET_REQUEST(&vdummy.rings[dev->otherend_id].ring, i);

		ring_rsp = RING_GET_RESPONSE(&vdummy.rings[dev->otherend_id].ring, vdummy.rings[dev->otherend_id].ring.rsp_prod_pvt);

		DBG("%s, cons=%d, prod=%d\n", __func__, i, vdummy.rings[dev->otherend_id].ring.rsp_prod_pvt);

		memcpy(ring_rsp->buffer, ring_req->buffer, VDUMMY_PACKET_SIZE);

		dmb();
		vdummy.rings[dev->otherend_id].ring.rsp_prod_pvt++;

		RING_PUSH_RESPONSES(&vdummy.rings[dev->otherend_id].ring);

		notify_remote_via_virq(vdummy.rings[dev->otherend_id].irq);
	}

	vdummy.rings[dev->otherend_id].ring.sring->req_cons = i;

	return IRQ_HANDLED;
}

void vdummy_probe(struct vbus_device *dev) {

	DBG(VDUMMY_PREFIX "Backend probe: %d\n", dev->otherend_id);
}

void vdummy_close(struct vbus_device *dev) {

	DBG(VDUMMY_PREFIX "Backend close: %d\n", dev->otherend_id);
}

void vdummy_suspend(struct vbus_device *dev) {

	DBG(VDUMMY_PREFIX "Backend suspend: %d\n", dev->otherend_id);
}

void vdummy_resume(struct vbus_device *dev) {

	DBG(VDUMMY_PREFIX "Backend resume: %d\n", dev->otherend_id);
}

void vdummy_reconfigured(struct vbus_device *dev) {

	DBG(VDUMMY_PREFIX "Backend reconfigured: %d\n", dev->otherend_id);
}

void vdummy_connected(struct vbus_device *dev) {

	DBG(VDUMMY_PREFIX "Backend connected: %d\n", dev->otherend_id);
}

int generator_fn(void *arg) {
	uint32_t i;

	while (1) {
		msleep(50);

		for (i = 0; i < MAX_DOMAINS; i++) {

			if (!vdummy_start(i))
				continue;

			vdummy_notify(i);

			vdummy_end(i);
		}
	}

	return 0;
}

int vdummy_init(void) {
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "vdummy,backend");

	/* Check if DTS has vuihandler enabled */
	if (!of_device_is_available(np))
		return 0;

#if 0
	kthread_run(generator_fn, NULL, "vDummy-gen");
#endif

	vdummy_vbus_init();

	return 0;
}

module_init(vdummy_init);
