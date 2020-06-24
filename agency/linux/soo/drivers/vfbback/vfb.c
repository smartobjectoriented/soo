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
#include <linux/fb.h>

#include <soo/evtchn.h>
#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/uapi/console.h>

#include <stdarg.h>
#include <linux/kthread.h>

#include <soo/vdevback.h>

#include <soo/dev/vfb.h>


static struct vbus_watch me_watch;

void vfb_notify(struct vbus_device *vdev)
{
	vfb_t *vfb = to_vfb(vdev);

	RING_PUSH_RESPONSES(&vfb->ring);

	/* Send a notification to the frontend only if connected.
	 * Otherwise, the data remain present in the ring. */

	notify_remote_via_virq(vfb->irq);
}


irqreturn_t vfb_interrupt(int irq, void *dev_id)
{
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vfb_t *vfb = to_vfb(vdev);
	vfb_request_t *ring_req;
	vfb_response_t *ring_rsp;

	DBG("%d\n", vdev->otherend_id);

	while ((ring_req = vfb_ring_request(&vfb->ring)) != NULL) {

		ring_rsp = vfb_ring_response(&vfb->ring);

		memcpy(ring_rsp->buffer, ring_req->buffer, VFB_PACKET_SIZE);

		vfb_ring_response_ready(&vfb->ring);

		notify_remote_via_virq(vfb->irq);
	}

	return IRQ_HANDLED;
}

/* Call back when value of fb-ph-addr changes. */
void fb_ph_addr(struct vbus_watch *watch) {

	int res;
	grant_ref_t fb_ref;
	uint32_t *fb_space;
	struct vbus_transaction vbt;
	struct gnttab_map_grant_ref op;
	struct vm_struct *area;
	struct fb_info *fbinfo;

	vbus_transaction_start(&vbt);
	vbus_scanf(vbt, "backend/vfb/fb-ph-addr", "value", "%u", &fb_ref);
	vbus_transaction_end(vbt);

	DBG(VFB_PREFIX "New value for %s: 0x%08x\n", watch->node, fb_ref);

	/* Test */

	op.flags = GNTMAP_host_map;
	op.ref = fb_ref;
	op.dom = watch->dev->otherend_id;

	area = alloc_vm_area(1024 * 768 * 4, NULL);
	op.host_addr = (unsigned long) area->addr;
	if (!area) {
		BUG();
	}

	if (grant_table_op(GNTTABOP_map_grant_ref, &op, 1)) { /* TODO use gnttab_map directly */
		BUG();
	}

	if (op.status != GNTST_okay) {
		free_vm_area(area);
		DBG(VFB_PREFIX "mapping in shared page %d from domain %d failed for device %s\n", fb_ref, watch->dev->otherend_id, watch->dev->nodename);
		BUG();
	}

	area->phys_addr = (unsigned long) op.handle;

	fb_space = area->addr;
	DBG(VFB_PREFIX "First pixel: 0x%08x, addr: 0x%08x, area: 0x%08x", *fb_space, fb_space, area->addr);

	/* Reconfigure the framebuffer. */
	DBG(VFB_PREFIX "reg fb: %d, 0x%08x\n", num_registered_fb, registered_fb[0]->screen_base);
	fbinfo = registered_fb[0];
	fbinfo->screen_base = (char *) fb_space;
	fbinfo->fix.smem_start = op.dev_bus_addr << 12;
	fbinfo->var.bits_per_pixel = 32;
	res = fbinfo->fbops->fb_set_par(fbinfo);
	DBG(VFB_PREFIX "res: %d, dev bus addr: 0x%08llx\n", res, op.dev_bus_addr << 12);
}

void vfb_probe(struct vbus_device *vdev) {
	vfb_t *vfb;
	struct vbus_transaction vbt;

	vfb = kzalloc(sizeof(vfb_t), GFP_ATOMIC);
	BUG_ON(!vfb);

	dev_set_drvdata(&vdev->dev, &vfb->vdevback);

	/* Create property in vbstore. */
	vbus_transaction_start(&vbt);
	vbus_mkdir(vbt, "backend/vfb", "fb-ph-addr");
	vbus_write(vbt, "backend/vfb/fb-ph-addr", "value", "try");
	vbus_transaction_end(vbt);

	/* Set a watch on fb-ph-addr. */
	vbus_watch_path(vdev, "backend/vfb/fb-ph-addr/value", &me_watch, fb_ph_addr);

	DBG(VFB_PREFIX "Backend probe: %d\n", vdev->otherend_id);

	/* Unlink the framebuffer console from the framebuffer driver. */
	unlink_framebuffer(registered_fb[0]);
	DBG(VFB_PREFIX "Unlinked framebuffer.\n");
}

void vfb_remove(struct vbus_device *vdev) {
	vfb_t *vfb = to_vfb(vdev);

	DBG("%s: freeing the vfb structure for %s\n", __func__,vdev->nodename);
	kfree(vfb);
}


void vfb_close(struct vbus_device *vdev) {
	vfb_t *vfb = to_vfb(vdev);

	DBG(VFB_PREFIX "Backend close: %d\n", vdev->otherend_id);

	/*
	 * Free the ring and unbind evtchn.
	 */

	BACK_RING_INIT(&vfb->ring, (&vfb->ring)->sring, PAGE_SIZE);
	unbind_from_virqhandler(vfb->irq, vdev);

	vbus_unmap_ring_vfree(vdev, vfb->ring.sring);
	vfb->ring.sring = NULL;
}

void vfb_suspend(struct vbus_device *vdev) {

	DBG(VFB_PREFIX "Backend suspend: %d\n", vdev->otherend_id);
}

void vfb_resume(struct vbus_device *vdev) {

	DBG(VFB_PREFIX "Backend resume: %d\n", vdev->otherend_id);
}

void vfb_reconfigured(struct vbus_device *vdev) {
	int res;
	unsigned long ring_ref;
	unsigned int evtchn;
	vfb_sring_t *sring;
	vfb_t *vfb = to_vfb(vdev);

	DBG(VFB_PREFIX "Backend reconfigured: %d\n", vdev->otherend_id);

	/*
	 * Set up a ring (shared page & event channel) between the agency and the ME.
	 */

	vbus_gather(VBT_NIL, vdev->otherend, "ring-ref", "%lu", &ring_ref, "ring-evtchn", "%u", &evtchn, NULL);

	DBG("BE: ring-ref=%lu, event-channel=%u\n", ring_ref, evtchn);

	res = vbus_map_ring_valloc(vdev, ring_ref, (void **) &sring);
	BUG_ON(res < 0);

	SHARED_RING_INIT(sring);
	BACK_RING_INIT(&vfb->ring, sring, PAGE_SIZE);

	res = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, evtchn, vfb_interrupt, NULL, 0, VFB_NAME "-backend", vdev);

	BUG_ON(res < 0);

	vfb->irq = res;
}

void vfb_connected(struct vbus_device *vdev) {

	DBG(VFB_PREFIX "Backend connected: %d\n",vdev->otherend_id);
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

			if (!vfb_start(i))
				continue;

			vfb_ring_response_ready()
			vfb_notify(i);

			vfb_end(i);
		}
	}

	return 0;
}
#endif

vdrvback_t vfbdrv = {
	.probe = vfb_probe,
	.remove = vfb_remove,
	.close = vfb_close,
	.connected = vfb_connected,
	.reconfigured = vfb_reconfigured,
	.resume = vfb_resume,
	.suspend = vfb_suspend
};

int vfb_init(void) {
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "vfb,backend");

	/* Check if DTS has vuihandler enabled */
	if (!of_device_is_available(np))
		return 0;

#if 0
	kthread_run(generator_fn, NULL, "vfb-gen");
#endif

	vdevback_init(VFB_NAME, &vfbdrv);

	return 0;
}

device_initcall(vfb_init);
