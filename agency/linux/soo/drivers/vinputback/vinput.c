/*
 * Copyright (C) 2015 Daniel Rossier <daniel.rossier@soo.tech>
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
#include <linux/hid.h>

#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>

#include <stdarg.h>
#include <linux/kthread.h>

#include <soo/dev/vinput.h>

#if 0
vinput_t vinput;

struct mutex sending;
struct spinlock sending_lock;

static void vinput_notify(struct vbus_device *dev)
{
	vinput_ring_t *p_vinput_ring = &vinput.rings[dev->otherend_id];

	RING_PUSH_RESPONSES(&p_vinput_ring->ring);

	/* Send a notification to the frontend only if connected.
	 * Otherwise, the data remain present in the ring. */

	if (dev->state == VbusStateConnected)
		notify_remote_via_irq(p_vinput_ring->irq);
}

irqreturn_t vinput_interrupt(int irq, void *dev_id)
{
	struct vbus_device *dev;

	dev = (struct vbus_device *) dev_id;

	DBG("Interrupt from domain: %d\n", dev->otherend_id);

	return IRQ_HANDLED;
}

/*
 * Input event comes from the Linux subsystem.
 */
int vinput_pass_event(struct input_dev *dev, unsigned int type, unsigned int code, int value)
{
	struct vinput_response *rsp;
	vinput_ring_t *p_vinput_ring;

	if (vinput.domfocus == -1)
		return 0;

	/* Currently we forward the event to domain owning the focus. */
	p_vinput_ring = &vinput.rings[vinput.domfocus];

	/* Fill in the ring... */

	/* If the frontend is disconnected for a while, we ensure the ring does not overflow. */
	if (RING_FREE_RESPONSES(&p_vinput_ring->ring)) {

		rsp = RING_GET_RESPONSE(&p_vinput_ring->ring, p_vinput_ring->ring.rsp_prod_pvt);

		rsp->type = type;
		rsp->code = code;
		rsp->value = value;

		dmb();

		p_vinput_ring->ring.rsp_prod_pvt++;

		vinput_notify(p_vinput_ring->dev);
	}

	return 0;
}

int set_input_property(unsigned int domid, struct vbus_transaction xbt, char *prop, unsigned int val) {
	int ret;
	char node[50];

	sprintf(node, "/backend/vinput/%d/0", domid);

	ret = vbus_printf(xbt, node, prop, "%u", val);

	return ret;
}

void vinput_connect(void) {

	if (vinput.domfocus != -1)
		set_input_property(vinput.domfocus, VBT_NIL, "kbd-present", 1);
}

void vinput_disconnect(void) {

	if (vinput.domfocus != -1)
		set_input_property(vinput.domfocus, VBT_NIL, "kbd-present", 0);
}

int vinput_subsys_enable(struct vbus_device *dev) {
	return 0;
}

void vinput_subsys_disable(struct vbus_device *dev) {

}

/*
 * Store device properties available in the local SOO.
 */
int vinput_subsys_init(struct vbus_device *dev) {
	int ret;
	struct vbus_transaction xbt;

	/* Keep a reference to the corresponding vbus dev */
	vinput.rings[dev->otherend_id].dev = dev;

	vbus_transaction_start(&xbt);

	if (vinput.domfocus == -1)
		vinput.domfocus = dev->otherend_id;

	DBG0("Writing input hw properties now...\n");

	/* The keyboard (__hiddev) might be plugged on before that. */
	ret = set_input_property(dev->otherend_id, xbt, "kbd-present", ((__hiddev == NULL) ? 0 : 1));

	if (ret)
		goto err_vbus;

	ret = set_input_property(dev->otherend_id, xbt, "bus_type", BUS_USB);
	if (ret)
		goto err_vbus;

	ret = set_input_property(dev->otherend_id, xbt, "vendorID", OLIMEX_KBD_VENDOR_ID);

	if (ret)
		goto err_vbus;

	ret = set_input_property(dev->otherend_id, xbt, "productID", OLIMEX_KBD_PRODUCT_ID);

	if (ret)
		goto err_vbus;

	vbus_transaction_end(xbt);

	return 0;

err_vbus:
	printk("%s:%d vbus_printf failed\n", __FUNCTION__, __LINE__);

	return ret;
}

void vkbd_subsys_remove(struct vbus_device *dev) {

	vinput.domfocus = -1;
}

/*
 * Initializing the vinput backend driver.
 * This driver interacts with the input Linux subsystem.
 */
int vinput_init(void)
{
	vinput.domfocus = -1;

	mutex_init(&sending);

	return vinput_vbus_init();
}


module_init(vinput_init);
#endif
