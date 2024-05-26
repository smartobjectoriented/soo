/*
 * Copyright (C) 2015-2018 Daniel Rossier <daniel.rossier@soo.tech>
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

#include <linux/slab.h>
#include <linux/of.h>

#include <soo/evtchn.h>
#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/uapi/debug.h>

#include <soo/vdevback.h>

#include <soo/dev/vvext.h>

static vvext_t *__vvext = NULL;
static irq_handler_t vvext_interrupt;


static irqreturn_t __vvext_interrupt_wrapper_bh(int irq, void *dev_id) {
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vvext_t *vvext = dev_get_drvdata(&vdev->dev);

	return vvext_interrupt(irq, vvext);
}
static irqreturn_t __vvext_interrupt_wrapper(int irq, void *dev_id) {

	return IRQ_WAKE_THREAD;
}

void vvext_probe(struct vbus_device *vdev) {
	vvext_t *vvext;

	/*
	vvext = kzalloc(sizeof(vvext_t), GFP_ATOMIC);
	BUG_ON(!vvext);
*/

	BUG_ON(!__vvext);
	vvext = __vvext;

	spin_lock_init(&vvext->ring_lock);

	dev_set_drvdata(&vdev->dev, &vvext->vdevback);

	/* SEEE */
	vvext->vdev = vdev;

	DBG("Backend probe: %d\n", vdev->otherend_id);
}

void vvext_remove(struct vbus_device *vdev) {
	DBG("%s: freeing the vvext structure for %s\n", __func__,vdev->nodename);
	//kfree(vvext);
}

void vvext_close(struct vbus_device *vdev) {
	vvext_t *vvext = dev_get_drvdata(&vdev->dev);

	DBG("(vvext) Backend close: %d\n", vdev->otherend_id);

	/*
	 * Free the ring and unbind evtchn.
	 */

	BACK_RING_INIT(&vvext->ring, (&vvext->ring)->sring, PAGE_SIZE);
	unbind_from_virqhandler(vvext->irq, vdev);

	vbus_unmap_ring_vfree(vdev, vvext->ring.sring);
	vvext->ring.sring = NULL;
}

void vvext_suspend(struct vbus_device *vdev) {

	DBG("(vvext) Backend suspend: %d\n", vdev->otherend_id);
}

void vvext_resume(struct vbus_device *vdev) {

	DBG("(vvext) Backend resume: %d\n", vdev->otherend_id);
}

void vvext_reconfigured(struct vbus_device *vdev) {
	int res;
	unsigned long ring_ref;
	unsigned int evtchn;
	vvext_sring_t *sring;
	vvext_t *vvext = dev_get_drvdata(&vdev->dev);

	DBG(VVEXT_PREFIX "Backend reconfigured: %d\n", vdev->otherend_id);

	/*
	 * Set up a ring (shared page & event channel) between the agency and the ME.
	 */

	vbus_gather(VBT_NIL, vdev->otherend, "ring-ref", "%lu", &ring_ref, "ring-evtchn", "%u", &evtchn, NULL);

	DBG("BE: ring-ref=%lu, event-channel=%u\n", ring_ref, evtchn);

	vbus_map_ring_valloc(vdev, ring_ref, (void **) &sring);

	BACK_RING_INIT(&vvext->ring, sring, PAGE_SIZE);

	res = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, evtchn, __vvext_interrupt_wrapper,
			__vvext_interrupt_wrapper_bh, 0, VVEXT_NAME "-backend", vdev);

	BUG_ON(res < 0);

	vvext->irq = res;
}

void vvext_connected(struct vbus_device *vdev) {

	DBG(VVEXT_PREFIX "Backend connected: %d\n",vdev->otherend_id);

}

vdrvback_t vvextdrv = {
	.probe = vvext_probe,
	.remove = vvext_remove,
	.close = vvext_close,
	.connected = vvext_connected,
	.reconfigured = vvext_reconfigured,
	.resume = vvext_resume,
	.suspend = vvext_suspend
};


int vvext_init(vvext_t *vvext, irq_handler_t irq_handler) {
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "vvext,backend");

	/* Check if DTS has vvext enabled */
	if (!of_device_is_available(np))
		return -1;

	__vvext = vvext;
	vvext_interrupt = irq_handler;

	vdevback_init(VVEXT_NAME, &vvextdrv);

	return 0;
}
 
