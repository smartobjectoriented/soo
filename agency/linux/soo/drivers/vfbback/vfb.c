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
#include <linux/fb.h>
#include <linux/kthread.h>

#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/uapi/console.h>
#include <soo/dev/vfb.h>
#include <soo/vdevback.h>

#include <stdarg.h>

#define MIN_FB_HRES 800
#define MIN_FB_VRES 480

static uint32_t vfb_hres = 0;
static uint32_t vfb_vres = 0;

#define FB_SIZE  (vfb_hres * vfb_hres * 4) /* assume 24bpp */
#define FOUND_FB (FB_SIZE != 0)

#define VEXPRESS 1 /* 0: rpi4, 1: vexpress */
#define FB_COUNT 8


/* Array of registered front-end framebuffers. */
static struct vfb_fb *registered_fefb[FB_COUNT];
static domid_t current_fefb = 0;


#if !VEXPRESS /* Raspberry Pi 4 */

/* Thread to copy the front-end framebuffer into the Pi 4 framebuffer. */
static struct task_struct *ts1;

static int kthread_cp_fb(void *arg)
{
	uint32_t i, *addr_vc, *addr_fefb;
	struct fb_info *info;

	info = registered_fb[0];
	addr_vc = (uint32_t *) info->screen_base;
	addr_fefb = (uint32_t *) registered_fefb[current_fefb]->vaddr;

	DBG(VFB_PREFIX "Starting framebuffer copy thread.\n");

	while (true) {

		/* Copy current framebuffer. TODO avoid copy if current_fefb is 0. */
		for (i = 0; i < FB_SIZE / 4; i++) {
			addr_vc[i] = addr_fefb[i];
		}

		/* Update the current framebuffer, maybe it has changed. */
		addr_fefb = (uint32_t *) registered_fefb[current_fefb]->vaddr;
		msleep(50);
	}

	return 0;
}
#endif

/*
 * Callback called when a framebuffer is registered. Set value with
 * vfb_register_callback.
 */
static void (*callback)(struct vfb_fb *fb);

void vfb_register_callback(void (*cb)(struct vfb_fb *fb))
{
	callback = cb;
}

void vfb_register_fefb(struct vfb_fb *fb)
{
	DBG(VFB_PREFIX "Registered front-end framebuffer for domain id %d", fb->domid);
	registered_fefb[fb->domid] = fb;
}

struct vfb_fb *vfb_get_fefb(domid_t id)
{
	return registered_fefb[id];
}

domid_t vfb_current_fefb(void)
{
	return current_fefb;
}

/*
 * Reconfigure the framebuffer controller to display the front-end framebuffer
 * of the given domain id.
 *
 * Note: do not use DBG/printk, only lprintk (see avz_switch_console).
 */
void vfb_reconfig(domid_t id)
{
	struct fb_info *info;
	struct vfb_fb *fb;

	lprintk(VFB_PREFIX "Showing framebuffer of domain id %d\n", id);

	fb = registered_fefb[id];
	if (!fb) {
		return;
	}

#if VEXPRESS
	/* Currently, take the first registered framebuffer device. */
	info = registered_fb[0];
	lprintk(VFB_PREFIX "bpp %u\nres %u %u\nsmem 0x%08lx %u\nmmio 0x%08lx %u\nbase & size 0x%08x %lu\n",
			info->var.bits_per_pixel,
			info->var.xres, info->var.yres,
			info->fix.smem_start, info->fix.smem_len,
			info->fix.mmio_start, info->fix.mmio_len,
			info->screen_base, info->screen_size);

	info->fix.smem_start = fb->paddr;
	info->fix.smem_len = fb->size;
	info->screen_base = (char *) fb->vaddr;

	/* Call fb_set_par op that will write values to the controller's registers. */
	if (!info->fbops->fb_set_par || info->fbops->fb_set_par(info)) {
		lprintk("fb_set_par not found or call failed\n");
		return;
	}
#endif

	current_fefb = id;
}

/* Callback when a new front-end framebuffer is added. */
void fefb_callback(struct vbus_watch *watch)
{
	grant_ref_t fb_ref;
	struct vbus_transaction vbt;
	struct gnttab_map_grant_ref *op;
	struct vm_struct *area;
	struct vfb_fb *fb;
	char dir[21];
	unsigned long domid;
	char *node;

	/* Get domain id. */

	node = watch->node;
	while (*node && !isdigit(*node)) {
		node++;
	}

	domid = strtoul(node, NULL, 10);
	DBG(VFB_PREFIX "Domain id is %lu\n", domid);

	/* Retrieve grantref from vbstore. */

	vbus_transaction_start(&vbt);
	sprintf(dir, "device/%01lu/vfb/0/fe-fb", domid);
	vbus_scanf(vbt, dir, "value", "%u", &fb_ref);
	DBG(VFB_PREFIX "New value for %s: 0x%08x\n", watch->node, fb_ref);
	vbus_transaction_end(vbt);

	/* Allocate memory to map the ME framebuffer. */

	area = alloc_vm_area(FB_SIZE, NULL);
	DBG(VFB_PREFIX "Allocated area in vfb\n");

	/* Map the grantref area. */

	op = kzalloc(sizeof(struct gnttab_map_grant_ref), GFP_ATOMIC);
	BUG_ON(!op);

	gnttab_set_map_op(op, (phys_addr_t) area->addr, GNTMAP_host_map | GNTMAP_readonly, fb_ref, domid, 0, FB_SIZE);
	if (gnttab_map(op) || op->status != GNTST_okay) {
		free_vm_area(area);
		DBG(VFB_PREFIX "Mapping in shared page %d from domain %ld failed\n", fb_ref, domid);
		BUG();
	}

	/* Create the vfb_fb struct and register it. */

	fb = kzalloc(sizeof(struct vfb_fb), GFP_ATOMIC);
	fb->domid = (domid_t) domid;
	fb->paddr = op->dev_bus_addr << 12;
	fb->size = FB_SIZE;
	fb->vaddr = (uint32_t) area->addr;
	fb->area = area;
	fb->op = op;

	vfb_register_fefb(fb);

	/* If a callback has been registered, execute it. */
	if (callback) {
		callback(fb);
	}
}

void vfb_probe(struct vbus_device *vdev)
{
	vfb_t *vfb;
	struct vbus_transaction vbt;
	struct vbus_watch *watch;
	struct vfb_fb *fb;
	char dir[35];
	char *path;

	vfb = kzalloc(sizeof(vfb_t), GFP_ATOMIC);
	BUG_ON(!vfb);
	dev_set_drvdata(&vdev->dev, &vfb->vdevback);

	if (!FOUND_FB) {
		DBG(VFB_PREFIX "No framebuffer found!");
		return;
	}

	/* Create property in vbstore. */

	vbus_transaction_start(&vbt);

	sprintf(dir, "device/%01d/vfb/0", vdev->otherend_id);
	vbus_mkdir(vbt, dir, "fe-fb");
	vbus_mkdir(vbt, dir, "res");

	sprintf(dir, "device/%01d/vfb/0/fe-fb", vdev->otherend_id);
	vbus_write(vbt, dir, "value", "try");

	sprintf(dir, "device/%01d/vfb/0/res", vdev->otherend_id);
	vbus_printf(vbt, dir, "h", "%u", vfb_hres);
	vbus_printf(vbt, dir, "v", "%u", vfb_vres);

	vbus_transaction_end(vbt);

	/* Set a watch on fe-fb. */

	watch = kzalloc(sizeof(struct vbus_watch), GFP_ATOMIC);
	path = kzalloc(27 * sizeof(char), GFP_ATOMIC);
	BUG_ON(!watch || !path);

	sprintf(path, "device/%01d/vfb/0/fe-fb/value", vdev->otherend_id);
	vbus_watch_path(vdev, path, watch, fefb_callback);
	DBG(VFB_PREFIX "Watching %s\n", path);

	/*
	 * As is the case with most framebuffer drivers (according to the Linux
	 * documentation, see fbcons), the fb driver will initialize the
	 * related structure of the fb but will not actually write these values
	 * to the device registers if the Linux framebuffer console is
	 * deactivated.
	 *
	 * Here we assume the framebuffer console is inactive and so we must
	 * initialized the driver manually using the fb_set_par function. If
	 * framebuffer console is activated, it is necessary to unlink the
	 * framebuffer before using the vfb_reconfig (using the
	 * unlink_framebuffer function). This is necessary because fbcons sets
	 * up a thread to blink the console cursor and if the fb address
	 * changes this thread will crash.
	 *
	 * As we can switch between front-end framebuffers, we want to be able
	 * to return to the agency's framebuffer, thus we create a
	 * corresponding vfb_fb struct and register it (only once).
	 */
	if (!registered_fefb[0]) {

		fb = kzalloc(sizeof(struct vfb_fb), GFP_ATOMIC);
		fb->domid = 0;
		fb->paddr = registered_fb[0]->fix.smem_start;
		fb->size = registered_fb[0]->fix.smem_len;
		fb->vaddr = (uint32_t) registered_fb[0]->screen_base;

		vfb_register_fefb(fb);

		/* Activate it. */
		vfb_reconfig(0);
	}

#if !VEXPRESS /* Raspberry Pi 4 */
	ts1 = kthread_run(kthread_cp_fb, NULL, "thread-1");
	if (IS_ERR(ts1)) {
		DBG(VFB_PREFIX "Cannot create thread ts1.\n");
	}
#endif
}

void vfb_remove(struct vbus_device *vdev)
{
	DBG(VFB_PREFIX "Backend remove: %d\n", vdev->otherend_id);
}

void vfb_close(struct vbus_device *vdev)
{
	DBG(VFB_PREFIX "Backend close: %d\n", vdev->otherend_id);
}

void vfb_suspend(struct vbus_device *vdev)
{
	DBG(VFB_PREFIX "Backend suspend: %d\n", vdev->otherend_id);
}

void vfb_resume(struct vbus_device *vdev)
{
	DBG(VFB_PREFIX "Backend resume: %d\n", vdev->otherend_id);
}

void vfb_reconfigured(struct vbus_device *vdev)
{
	DBG(VFB_PREFIX "Backend reconfigured: %d\n", vdev->otherend_id);
}

void vfb_connected(struct vbus_device *vdev)
{
	DBG(VFB_PREFIX "Backend connected: %d\n", vdev->otherend_id);
}

static void vfb_register_fb(struct fb_info *info)
{
	if (vfb_hres || info->var.xres < MIN_FB_HRES || info->var.yres < MIN_FB_VRES) {
		DBG(VFB_PREFIX "Framebuffer already registered or resolution too small.\n");
		return;
	}

	vfb_hres = info->var.xres;
	vfb_vres = info->var.yres;

	DBG(VFB_PREFIX "Found framebuffer: %dx%d %dbpp\n",
		/*info->dev->driver->name,*/ vfb_hres, vfb_vres, info->var.bits_per_pixel);
}

static int vfb_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	if (event == FB_EVENT_FB_REGISTERED) {
		vfb_register_fb(((struct fb_event *) data)->info);
	}

	return 0;
}

static struct notifier_block vfb_fb_notif = {
	.notifier_call = vfb_fb_notifier_callback,
};

vdrvback_t vfbdrv = {
	.probe = vfb_probe,
	.remove = vfb_remove,
	.close = vfb_close,
	.connected = vfb_connected,
	.reconfigured = vfb_reconfigured,
	.resume = vfb_resume,
	.suspend = vfb_suspend
};

int vfb_init(void)
{
	struct device_node *np = of_find_compatible_node(NULL, NULL, "vfb,backend");

	if (!of_device_is_available(np)) {
		return 0;
	}

	vdevback_init(VFB_NAME, &vfbdrv);

	/* Try to find a proper framebuffer. */

	if (registered_fb[0]) {
		/* We check if there's already a registered framebuffer respecting an arbitrary minimal resolution. */
		vfb_register_fb(registered_fb[0]);
	}
	else {
		/* Otherwise, we register a client that will be notified when a framebuffer is registered. */
		DBG(VFB_PREFIX "No framebuffer registered, registering framebuffer client.\n");
		fb_register_client(&vfb_fb_notif);
	}

	return 0;
}

device_initcall(vfb_init);
