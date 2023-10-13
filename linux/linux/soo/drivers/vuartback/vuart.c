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

#include <soo/dev/vuart.h>

typedef struct {

	/* Must be the first field */
	vuart_t vuart;

} vuart_priv_t;

/*
 * We maintain a list of vdev_console
 */

struct vdev_console {
	struct list_head list;
	struct vbus_device *vdev_console;
};

struct list_head vdev_consoles;

/*
 * Add a new console for a new ME.
 */
void add_console(struct vbus_device *vdev_console) {
	struct vdev_console *console;

	console = kzalloc(sizeof(vdev_console), GFP_ATOMIC);
	BUG_ON(!console);

	console->vdev_console = vdev_console;

	list_add(&console->list, &vdev_consoles);
}

/*
 * Search for a console related to a specific ME according to its domid.
 */
struct vbus_device *get_console(uint32_t domid) {
	struct vdev_console *entry;

	list_for_each_entry(entry, &vdev_consoles, list)
		if (entry->vdev_console->otherend_id == domid)
			return entry->vdev_console;
	return NULL;
}

/*
 * Remove a console attached to a specific ME
 */
void del_console(struct vbus_device *vdev_console) {
	struct vdev_console *entry;

	list_for_each_entry(entry, &vdev_consoles, list)
		if (entry->vdev_console == vdev_console) {
			list_del(&entry->list);
			kfree(entry);
			return ;
		}
	BUG();
}

void process_response(struct vbus_device *vdev) {
	vuart_priv_t *vuart_priv = dev_get_drvdata(&vdev->dev);
	vuart_request_t *ring_req;
	vuart_response_t *ring_rsp;
	struct winsize wsz;

	vdevback_processing_begin(vdev);

	while ((ring_req = vuart_get_ring_request(&vuart_priv->vuart.ring)) != NULL) {

		if (ring_req->c == SERIAL_GWINSZ) {
			/* Process the window size info */

			/* At the moment, we hardcode these values */
			wsz.ws_col = 80;
			wsz.ws_row = 25;

			ring_rsp = vuart_new_ring_response(&vuart_priv->vuart.ring);
			ring_rsp->c = wsz.ws_row;
			ring_rsp = vuart_new_ring_response(&vuart_priv->vuart.ring);
			ring_rsp->c = wsz.ws_col;
			vuart_ring_response_ready(&vuart_priv->vuart.ring);

			notify_remote_via_virq(vuart_priv->vuart.irq);

		} else
			lprintch(ring_req->c);
	}

	vdevback_processing_end(vdev);
}

irqreturn_t vuart_interrupt_bh(int irq, void *dev_id) {
	struct vbus_device *vdev = (struct vbus_device *) dev_id;

	process_response(vdev);

	return IRQ_HANDLED;
}

irqreturn_t vuart_interrupt(int irq, void *dev_id)
{
	/* All processing in the bottom half */

	return IRQ_WAKE_THREAD;
}

/**
 * This function is called in interrupt context.
 * - If the state is Connected, the character can directly be pushed in the ring.
 * - If the state is not Connected, the character is pushed into a circular buffer that
 *   will be flushed at the next call to the Connected callback.
 */
void me_cons_sendc(domid_t domid, uint8_t ch) {
	struct vbus_device *console;
	vuart_priv_t *vuart_priv;
	vuart_response_t *vuart_rsp;

	console = get_console(domid);

	if (!vdevfront_is_connected(console))
		return ;

	vdevback_processing_begin(console);

	vuart_priv = dev_get_drvdata(&console->dev);

	spin_lock(&vuart_priv->vuart.ring_lock);
	vuart_rsp = vuart_new_ring_response(&vuart_priv->vuart.ring);

	vuart_rsp->c = ch;

	spin_unlock(&vuart_priv->vuart.ring_lock);

	vuart_ring_response_ready(&vuart_priv->vuart.ring);

	if (console->state == VbusStateConnected)
		notify_remote_via_virq(vuart_priv->vuart.irq);

	vdevback_processing_end(console);
}

void vuart_probe(struct vbus_device *vdev) {
	vuart_priv_t *vuart_priv;

	vuart_priv = kzalloc(sizeof(vuart_priv_t), GFP_ATOMIC);
	BUG_ON(!vuart_priv);

	spin_lock_init(&vuart_priv->vuart.ring_lock);

	dev_set_drvdata(&vdev->dev, vuart_priv);

	add_console(vdev);

	DBG(VUART_PREFIX "Backend probe: %d\n", vdev->otherend_id);
}

void vuart_remove(struct vbus_device *vdev) {
	vuart_priv_t *vuart_priv = dev_get_drvdata(&vdev->dev);

	del_console(vdev);

	DBG("%s: freeing the vuart structure for %s\n", __func__,vdev->nodename);
	kfree(vuart_priv);
}

void vuart_close(struct vbus_device *vdev) {
	vuart_priv_t *vuart_priv = dev_get_drvdata(&vdev->dev);

	DBG("(vuart) Backend close: %d\n", vdev->otherend_id);

	/*
	 * Free the ring and unbind evtchn.
	 */

	BACK_RING_INIT(&vuart_priv->vuart.ring, vuart_priv->vuart.ring.sring, PAGE_SIZE);
	unbind_from_virqhandler(vuart_priv->vuart.irq, vdev);

	vbus_unmap_ring_vfree(vdev, vuart_priv->vuart.ring.sring);
	vuart_priv->vuart.ring.sring = NULL;
}

void vuart_suspend(struct vbus_device *vdev) {

	DBG("(vuart) Backend suspend: %d\n", vdev->otherend_id);
}

void vuart_resume(struct vbus_device *vdev) {

	DBG("(vuart) Backend resume: %d\n", vdev->otherend_id);
}

void vuart_reconfigured(struct vbus_device *vdev) {
	unsigned long ring_ref;
	unsigned int evtchn;
	vuart_sring_t *sring;
	vuart_priv_t *vuart_priv = dev_get_drvdata(&vdev->dev);

	DBG(VUART_PREFIX "Backend reconfigured: %d\n", vdev->otherend_id);

	/*
	 * Set up a ring (shared page & event channel) between the agency and the ME.
	 */

	vbus_gather(VBT_NIL, vdev->otherend, "ring-ref", "%lu", &ring_ref, "ring-evtchn", "%u", &evtchn, NULL);

	DBG("BE: ring-ref=%lu, event-channel=%u\n", ring_ref, evtchn);

	vbus_map_ring_valloc(vdev, ring_ref, (void **) &sring);

	BACK_RING_INIT(&vuart_priv->vuart.ring, sring, PAGE_SIZE);

	vuart_priv->vuart.irq = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, evtchn, vuart_interrupt, vuart_interrupt_bh, 0, VUART_NAME "-backend", vdev);
}

void vuart_connected(struct vbus_device *vdev) {

	DBG(VUART_PREFIX "Backend connected: %d\n",vdev->otherend_id);

	process_response(vdev);
}

vdrvback_t vuartdrv = {
	.probe = vuart_probe,
	.remove = vuart_remove,
	.close = vuart_close,
	.connected = vuart_connected,
	.reconfigured = vuart_reconfigured,
	.resume = vuart_resume,
	.suspend = vuart_suspend
};

int vuart_init(void) {
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "vuart,backend");

	/* Check if DTS has vuart enabled */
	if (!of_device_is_available(np))
		return 0;

	INIT_LIST_HEAD(&vdev_consoles);

	vdevback_init(VUART_NAME, &vuartdrv);

	return 0;
}

device_initcall(vuart_init);
