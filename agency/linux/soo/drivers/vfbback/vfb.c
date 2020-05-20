
/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>


#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/uapi/console.h>

#include <stdarg.h>
#include <linux/kthread.h>

#include <soo/dev/vfb.h>

vfb_t vfb;

/*
 * Interrupt routine called when a notification from the frontend is received.
 * All processing within this function is performed with IRQs disabled (top half processing).
 */
irqreturn_t vfb_interrupt(int irq, void *dev_id)
{
	/* Not used ... */

	return IRQ_HANDLED;
}

/*
 * Set a framebuffer property in vbstore for MEs.
 */
int set_fb_property(unsigned int domid, struct vbus_transaction vbt, char *prop, unsigned int val)
{
	char node[50];

	sprintf(node, "/backend/vfb/%d/0", domid);

	vbus_printf(vbt, node, prop, "%u", val);

	return 0;
}


void vfb_probe(struct vbus_device *dev) {
	struct vbus_transaction vbt;
	int ret = 0;

	DBG(VFB_PREFIX "Backend probe: %d\n", dev->otherend_id);

	vfb.domfocus = dev->otherend_id;

	ret = vfb_get_params(&vfb.data[1].fb_hw);
	if (ret)
		BUG();

	vbus_transaction_start(&vbt);

	ret = set_fb_property(1, vbt, "width", vfb.data[1].fb_hw.width);
	if (ret)
		BUG();

	ret = set_fb_property(1, vbt, "height", vfb.data[1].fb_hw.height);
	if (ret)
		BUG();

	ret = set_fb_property(1, vbt, "depth", vfb.data[1].fb_hw.depth);
	if (ret)
		BUG();

	ret = set_fb_property(1, vbt, "red_length", vfb.data[1].fb_hw.red.length);
	if (ret)
		BUG();

	ret = set_fb_property(1, vbt, "red_offset", vfb.data[1].fb_hw.red.offset);
	if (ret)
		BUG();

	ret = set_fb_property(1, vbt, "red_msb_right", vfb.data[1].fb_hw.red.msb_right);
	if (ret)
		BUG();

	ret = set_fb_property(1, vbt, "green_length", vfb.data[1].fb_hw.green.length);
	if (ret)
		BUG();

	ret = set_fb_property(1, vbt, "green_offset", vfb.data[1].fb_hw.green.offset);
	if (ret)
		BUG();

	ret = set_fb_property(1, vbt, "green_msb_right", vfb.data[1].fb_hw.green.msb_right);
	if (ret)
		BUG();

	ret = set_fb_property(1, vbt, "blue_length", vfb.data[1].fb_hw.blue.length);
	if (ret)
		BUG();

	ret = set_fb_property(1, vbt, "blue_offset", vfb.data[1].fb_hw.blue.offset);
	if (ret)
		BUG();

	ret = set_fb_property(1, vbt, "blue_msb_right", vfb.data[1].fb_hw.blue.msb_right);
	if (ret)
		BUG();

	ret = set_fb_property(1, vbt, "transp_length", vfb.data[1].fb_hw.transp.length);
	if (ret)
		BUG();

	ret = set_fb_property(1, vbt, "transp_offset", vfb.data[1].fb_hw.transp.offset);
	if (ret)
		BUG();

	ret = set_fb_property(1, vbt, "transp_msb_right", vfb.data[1].fb_hw.transp.msb_right);
	if (ret)
		BUG();

	ret = set_fb_property(1, vbt, "line_length", vfb.data[1].fb_hw.line_length);
	if (ret)
		BUG();

	ret = set_fb_property(1, vbt, "fb_mem_len", vfb.data[1].fb_hw.fb_mem_len);
	if (ret)
		BUG();

	vbus_transaction_end(vbt);

}

void vfb_close(struct vbus_device *dev) {

	DBG(VFB_PREFIX "Backend close: %d\n", dev->otherend_id);

	/* Unmap reviously mapped fb */
	vunmap(vfb.data[dev->otherend_id].fb);
}

void vfb_suspend(struct vbus_device *dev) {

	DBG(VFB_PREFIX "Backend suspend: %d\n", dev->otherend_id);
}

void vfb_resume(struct vbus_device *dev) {

	DBG(VFB_PREFIX "Backend resume: %d\n", dev->otherend_id);
}

void vfb_reconfigured(struct vbus_device *dev) {

	vbus_gather(VBT_NIL, dev->otherend, "vfb_fb_pfn", "%u", &vfb.data[dev->otherend_id].fb_pfn, NULL);

	DBG(VFB_PREFIX "Backend reconfigured: %d\n", dev->otherend_id);
}

void vfb_connected(struct vbus_device *dev) {
	int  nr_pages, i;
	struct page **phys_pages;
	vfb_info_t dom_info;
	struct fb_event event;

	DBG(VFB_PREFIX "Backend connected: %d\n", dev->otherend_id);

	/* Map the virtual framebuffer pixel array */

	/*
	 * Allocate an array of struct page pointers. map_vm_area() wants
	 * this, rather than just an array of pages.
	 */

	/* Propagate the register event to the framebuffer driver */
	dom_info.domid = dev->otherend_id;
	dom_info.paddr = vfb.data[dev->otherend_id].fb_pfn << PAGE_SHIFT;
	dom_info.len = vfb.data[dev->otherend_id].fb_hw.fb_mem_len;
	event.data = &dom_info;

	fb_notifier_call_chain(VFB_EVENT_DOM_REGISTER, &event);

}

int vfb_init(void) {
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "vfb,backend");

	/* Check if DTS has vuihandler enabled */
	if (!of_device_is_available(np))
		return 0;

	vfb_vbus_init();

	return 0;
}

device_initcall(vfb_init);



