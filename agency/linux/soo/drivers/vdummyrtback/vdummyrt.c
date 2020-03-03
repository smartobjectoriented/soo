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
#include <linux/of.h>

#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/uapi/console.h>

#include <stdarg.h>
#include <linux/kthread.h>

#include "common.h"

vdummyrt_t vdummyrt;

void vdummyrt_notify(struct vbus_device *dev) {
	vdummyrt_ring_t *p_vdummy_ring = &vdummyrt.rings[dev->otherend_id];

	if (!vdummyrt_start(dev->otherend_id))
		return ;

	RING_PUSH_RESPONSES(&p_vdummy_ring->ring);

	/* Send a notification to the frontend only if connected.
	 * Otherwise, the data remain present in the ring. */

	notify_remote_via_virq(p_vdummy_ring->irq_handle.irq);

	vdummyrt_end(dev->otherend_id);
}

int vdummyrt_interrupt(rtdm_irq_t *dummy) {
	struct vbus_device *dev = (struct vbus_device *) dummy->cookie;
	RING_IDX i, rp;
	vdummyrt_request_t *ring_req;
	vdummyrt_response_t *ring_rsp;

	DBG("Interrupt from domain: %d\n", dev->otherend_id);

	if (!vdummyrt_is_connected(dev->otherend_id))
		return RTDM_IRQ_HANDLED;

	DBG("%s(%d)\n", __func__, dev->otherend_id);

	rp = vdummyrt.rings[dev->otherend_id].ring.sring->req_prod;
	dmb();

	for (i = vdummyrt.rings[dev->otherend_id].ring.sring->req_cons; i != rp; i++) {
		ring_req = RING_GET_REQUEST(&vdummyrt.rings[dev->otherend_id].ring, i);

		ring_rsp = RING_GET_RESPONSE(&vdummyrt.rings[dev->otherend_id].ring, vdummyrt.rings[dev->otherend_id].ring.rsp_prod_pvt);

		DBG("%s, cons=%d, prod=%d\n", __func__, i, vdummyrt.rings[dev->otherend_id].ring.rsp_prod_pvt);

		memcpy(ring_rsp->buffer, ring_req->buffer, VDUMMYRT_PACKET_SIZE);

		dmb();
		vdummyrt.rings[dev->otherend_id].ring.rsp_prod_pvt++;

		RING_PUSH_RESPONSES(&vdummyrt.rings[dev->otherend_id].ring);

		notify_remote_via_virq(vdummyrt.rings[dev->otherend_id].irq_handle.irq);
	}

	vdummyrt.rings[dev->otherend_id].ring.sring->req_cons = i;

	return RTDM_IRQ_HANDLED;
}

void vdummyrt_probe(struct vbus_device *dev) {
	DBG(VDUMMYRT_PREFIX " Backend probe: %d\n", dev->otherend_id);

}

void vdummyrt_close(struct vbus_device *dev) {
	DBG(VDUMMYRT_PREFIX " Backend close: %d\n", dev->otherend_id);
}

void vdummyrt_suspend(struct vbus_device *dev) {
	DBG(VDUMMYRT_PREFIX " Backend suspend: %d\n", dev->otherend_id);
}

void vdummyrt_resume(struct vbus_device *dev) {
	DBG(VDUMMYRT_PREFIX " Backend resume: %d\n", dev->otherend_id);;
}

void vdummyrt_reconfigured(struct vbus_device *dev) {
	DBG(VDUMMYRT_PREFIX " Backend reconfigured: %d\n", dev->otherend_id);
}

void vdummyrt_connected(struct vbus_device *dev) {
	DBG(VDUMMYRT_PREFIX " Backend connected: %d\n", dev->otherend_id);
}

int vdummyrt_init(void) {
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "soo,vdummyrt");

	/* Check if DTS has vuihandler enabled */
	if (!of_device_is_available(np))
		return 0;	


	vdummyrt_vbus_init();

	return 0;
}

module_init(vdummyrt_init);
