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

#include <soo/dev/vfb.h>

vfb_t vfb;

void vfb_notify(domid_t domid)
{
	vfb_ring_t *p_vfb_ring = &vfb.rings[domid];

	RING_PUSH_RESPONSES(&p_vfb_ring->ring);

	/* Send a notification to the frontend only if connected.
	 * Otherwise, the data remain present in the ring. */

	notify_remote_via_virq(p_vfb_ring->irq);

}

irqreturn_t vfb_interrupt(int irq, void *dev_id)
{
	struct vbus_device *dev = (struct vbus_device *) dev_id;
	RING_IDX i, rp;
	vfb_request_t *ring_req;
	vfb_response_t *ring_rsp;

	if (!vfb_is_connected(dev->otherend_id))
		return IRQ_HANDLED;

	DBG("%d\n", dev->otherend_id);

	rp = vfb.rings[dev->otherend_id].ring.sring->req_prod;
	dmb();

	for (i = vfb.rings[dev->otherend_id].ring.sring->req_cons; i != rp; i++) {

		ring_req = RING_GET_REQUEST(&vfb.rings[dev->otherend_id].ring, i);

		ring_rsp = RING_GET_RESPONSE(&vfb.rings[dev->otherend_id].ring, vfb.rings[dev->otherend_id].ring.rsp_prod_pvt);

		DBG("%s, cons=%d, prod=%d\n", __func__, i, vfb.rings[dev->otherend_id].ring.rsp_prod_pvt);

		memcpy(ring_rsp->buffer, ring_req->buffer, VFB_PACKET_SIZE);

		dmb();
		vfb.rings[dev->otherend_id].ring.rsp_prod_pvt++;

		RING_PUSH_RESPONSES(&vfb.rings[dev->otherend_id].ring);

		notify_remote_via_virq(vfb.rings[dev->otherend_id].irq);
	}

	vfb.rings[dev->otherend_id].ring.sring->req_cons = i;

	return IRQ_HANDLED;
}

void vfb_probe(struct vbus_device *dev) {

	DBG(VFB_PREFIX "Backend probe: %d\n", dev->otherend_id);
}

void vfb_close(struct vbus_device *dev) {

	DBG(VFB_PREFIX "Backend close: %d\n", dev->otherend_id);
}

void vfb_suspend(struct vbus_device *dev) {

	DBG(VFB_PREFIX "Backend suspend: %d\n", dev->otherend_id);
}

void vfb_resume(struct vbus_device *dev) {

	DBG(VFB_PREFIX "Backend resume: %d\n", dev->otherend_id);
}

void vfb_reconfigured(struct vbus_device *dev) {

	DBG(VFB_PREFIX "Backend reconfigured: %d\n", dev->otherend_id);
}

void vfb_connected(struct vbus_device *dev) {

	DBG(VFB_PREFIX "Backend connected: %d\n", dev->otherend_id);
}
/*
int generator_fn(void *arg) {
	uint32_t i;

	while (1) {
		msleep(50);

		for (i = 0; i < MAX_DOMAINS; i++) {

			if (!vfb_start(i))
				continue;

			vfb_notify(i);

			vfb_end(i);
		}
	}

	return 0;
}
*/

void fb_ph_addr(struct vbus_watch *watch) {

	DBG(VFB_PREFIX "ok ok\n");
}

int vfb_init(void) {

	struct device_node *np;
	struct vbus_watch watch;
	struct vbus_transaction vbt;

	DBG(VFB_PREFIX "vfb init backend 1\n");
	np = of_find_compatible_node(NULL, NULL, "vfb,backend");

	/* Check if DTS has vuihandler enabled */
	if (!of_device_is_available(np))
		return 0;

#if 0
	kthread_run(generator_fn, NULL, "vFb-gen");
#endif

	vfb_vbus_init();
	DBG(VFB_PREFIX "vfb init backend 2\n");

	/* add watch */
	vbus_transaction_start(&vbt);
	vbus_mkdir(vbt, "/backend/vfb", "fb-ph-addr");
	//vbus_write(vbt, vfb.dev->nodename, "fb-ph-addr", "a value");
	vbus_transaction_end(vbt);

	vbus_watch_path(vfb.vdev[0], "/backend/vfb/fb-ph-addr", &watch, fb_ph_addr);

	return 0;
}

module_init(vfb_init);
