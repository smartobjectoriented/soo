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

#include <opencn/logfile.h>

typedef struct {

	/* Must be the first field */
	vuart_t vuart;

} vuart_priv_t;

void me_cons_sendc(domid_t domid, uint8_t ch) {
	/* Nothing in the RT domain */
}

/* Reference to the RT vuart */
static struct vbus_device *vuart_dev = NULL;

/*
 * In case of logging in a file, the processing is done in a bottom half interrupt
 * handler since VFS will enable interrupts.
 */
irqreturn_t vuart_interrupt_bh(int irq, void *dev_id) {
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vuart_priv_t *vuart_priv = dev_get_drvdata(&vdev->dev);
	vuart_request_t *ring_req;

	while ((ring_req = vuart_get_ring_request(&vuart_priv->vuart.ring)) != NULL)
		logfile_write(ring_req->str);

	return IRQ_HANDLED;
}

/*
 * Top half processing of the incoming data from the frontend.
 */
irqreturn_t vuart_interrupt(int irq, void *dev_id)
{
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vuart_priv_t *vuart_priv = dev_get_drvdata(&vdev->dev);
	vuart_request_t *ring_req;

	DBG("%d\n", vdev->otherend_id);

	/*
	 * Check if logfile is enabled because we need to switch to
	 * a bottom half processing since VFS will enable interrupts
	 * during the write operation.
	 */

	if (logfile_enabled())

		return IRQ_WAKE_THREAD;

	else {

		/* Output to the console */
		while ((ring_req = vuart_get_ring_request(&vuart_priv->vuart.ring)) != NULL)
			lprintk(ring_req->str);

	}

	return IRQ_HANDLED;
}

static void vuart_probe(struct vbus_device *vdev) {
	vuart_priv_t *vuart_priv;

	vuart_priv = kzalloc(sizeof(vuart_priv_t), GFP_ATOMIC);
	BUG_ON(!vuart_priv);

	dev_set_drvdata(&vdev->dev, vuart_priv);

	vuart_dev = vdev;

	DBG(VUART_PREFIX "Backend probe: %d\n", vdev->otherend_id);
}

void vuart_remove(struct vbus_device *vdev) {
	vuart_priv_t *vuart_priv = dev_get_drvdata(&vdev->dev);

	DBG("%s: freeing the vuart structure for %s\n", __func__,vdev->nodename);
	kfree(vuart_priv);
}

void vuart_close(struct vbus_device *vdev) {
	vuart_priv_t *vuart_priv = dev_get_drvdata(&vdev->dev);

	DBG("(vuart) Backend close: %d\n", vdev->otherend_id);

	/*
	 * Free the ring and unbind evtchn.
	 */

	BACK_RING_INIT(&vuart_priv->vuart.ring, (&vuart_priv->vuart.ring)->sring, PAGE_SIZE);
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
	int res;
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

	res = vbus_map_ring_valloc(vdev, ring_ref, (void **) &sring);
	BUG_ON(res < 0);

	BACK_RING_INIT(&vuart_priv->vuart.ring, sring, PAGE_SIZE);

	res = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, evtchn, vuart_interrupt, vuart_interrupt_bh, 0, VUART_NAME "-backend", vdev);

	BUG_ON(res < 0);

	vuart_priv->vuart.irq = res;

	vbus_gather(VBT_NIL, vdev->otherend, "ring-ref", "%lu", &ring_ref, "ring-evtchn", "%u", &evtchn, NULL);

}

void vuart_connected(struct vbus_device *vdev) {
	DBG(VUART_PREFIX "Backend connected: %d\n",vdev->otherend_id);
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

	/* Check if DTS has vuihandler enabled */
	if (!of_device_is_available(np))
		return 0;


	vdevback_init(VUART_NAME, &vuartdrv);

	return 0;
}

device_initcall(vuart_init);
