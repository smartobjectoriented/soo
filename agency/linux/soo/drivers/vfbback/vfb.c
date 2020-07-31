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

static struct vbus_watch *watches[DOMFB_COUNT];

/* Selected framebuffer device. */
static struct fb_info *vfb_info;

#define FB_HRES  (vfb_info->var.xres)
#define FB_VRES  (vfb_info->var.yres)
#define FB_SIZE  (FB_HRES * FB_VRES * 4) /* assume 24bpp */

/* Array of domain (ME or agency) framebuffers. */
static struct vfb_domfb *registered_domfb[DOMFB_COUNT];
static domid_t active_domfb = 0;

/* Callback to be called when a domain framebuffer is added. Allows others to
 * be notified of new domain framebuffers. */
static void (*callback_new_domfb)(struct vfb_domfb *, struct fb_info *);

/* Callback to be called when a domain framebuffer is removed. Allows others to
 * be notified when a domain framebuffer is removed */
static void (*callback_rm_domfb)(domid_t);

void vfb_set_callback_new_domfb(void (*cb)(struct vfb_domfb *, struct fb_info *))
{
	callback_new_domfb = cb;
}

void vfb_set_callback_rm_domfb(void (*cb)(domid_t id))
{
	callback_rm_domfb = cb;
}

struct vfb_domfb *vfb_get_domfb(domid_t id)
{
	return registered_domfb[id];
}

static void vfb_set_domfb(struct vfb_domfb *fb)
{
	DBG(VFB_PREFIX "Added domain framebuffer with id %d", fb->id);
	registered_domfb[fb->id] = fb;
}

/*
 * Sets the active domain framebuffer, i.e. the one that is supposed to be
 * displayed by the framebuffer device.
 *
 * Some controllers (e.g. PL111 in VExpress) allow for their framebuffer base
 * address to be changed. If that's the case we can just modify the fb_info
 * structure and call fb_set_par().
 *
 * Others (e.g. VideoCore in Raspberry) do not allow that, so we need to set up
 * a thread that will continuously copy the active domain framebuffer in the
 * memory controlled by the controller (see vfb_set_fbdev and kthread_cp_fb).
 *
 * Note: only use lprintk (see avz_switch_console).
 */
void vfb_set_active_domfb(domid_t domid)
{
	struct vfb_domfb *domfb = registered_domfb[domid];
	if (!domfb) {
		return;
	}

	lprintk(VFB_PREFIX "Domain framebuffer, current: %d, next: %d\n", active_domfb, domid);

#ifdef CONFIG_ARCH_VEXPRESS
	/* Change the physical and virtual addresses. */
	vfb_info->fix.smem_start = domfb->paddr;
	vfb_info->screen_base = domfb->vaddr;

	/* Updated the controller's registers. */
	if (!vfb_info->fbops->fb_set_par || vfb_info->fbops->fb_set_par(vfb_info)) {
		lprintk(VFB_PREFIX "fb_set_par not found or call failed\n");
		return;
	}
#endif

#ifdef CONFIG_ARCH_BCM2835
	/* At this point, the content of the VideoCore corresponds to the
	 * framebuffer of the agency. If we are switching from the agency
	 * and to an ME, we take a snapshot of the framebuffer so that it
	 * can be restored when coming back to the agency.
	 * See kthread_domfb_cpy */
	if (active_domfb == 0 && domid != 0) {
		memcpy(registered_domfb[0]->vaddr, vfb_info->screen_base, FB_SIZE);
	}
#endif

	active_domfb = domid;
}

/* Callback when an ME wants to add a domain framebuffer. */
static void callback_me_domfb(struct vbus_watch *watch)
{
	grant_ref_t fb_ref;
	struct vbus_transaction vbt;
	struct gnttab_map_grant_ref op;
	struct vm_struct *area;
	struct vfb_domfb *fb;
	char dir[40];
	domid_t domid;
	char *node;

	/* Extract domain id from node. */

	node = watch->node;
	while (*node && !isdigit(*node)) {
		node++;
	}

	domid = strtoul(node, NULL, 10);
	DBG(VFB_PREFIX "Domain id is %u\n", domid);

	/* Retrieve grantref from vbstore. */

	vbus_transaction_start(&vbt);
	sprintf(dir, "device/%01u/vfb/0/domfb-ref", domid);
	vbus_scanf(vbt, dir, "value", "%u", &fb_ref);
	vbus_transaction_end(vbt);

	/* Allocate memory to map the ME framebuffer. */

	area = alloc_vm_area(FB_SIZE, NULL);
	DBG(VFB_PREFIX "Allocated %u bytes\n", FB_SIZE);

	/* Map the grantref area. */

	gnttab_set_map_op(&op, (phys_addr_t) area->addr, GNTMAP_host_map | GNTMAP_readonly, fb_ref, domid, 0, FB_SIZE);
	if (gnttab_map(&op) || op.status != GNTST_okay) {
		free_vm_area(area);
		BUG();
	}

	/* Create the vfb_domfb struct and register it. */

	fb = kzalloc(sizeof(struct vfb_domfb), GFP_ATOMIC);
	fb->id = domid;
	fb->paddr = op.dev_bus_addr << 12;
	fb->vaddr = area->addr;
	fb->area = area;
	fb->gnt_handle = op.handle;
	vfb_set_domfb(fb);

	/* If a callback has been registered, execute it. */
	if (callback_new_domfb) {
		callback_new_domfb(fb, vfb_info);
	}
}

void vfb_probe(struct vbus_device *vdev)
{
	vfb_t *vfb;
	struct vbus_transaction vbt;
	char dir[35];

	vfb = kzalloc(sizeof(vfb_t), GFP_ATOMIC);
	BUG_ON(!vfb);
	dev_set_drvdata(&vdev->dev, &vfb->vdevback);

	if (!vfb_info) {
		DBG(VFB_PREFIX "No framebuffer device found!");
		BUG();
	}

	/* Create properties in vbstore. */

	vbus_transaction_start(&vbt);

	sprintf(dir, "device/%01d/vfb/0", vdev->otherend_id);
	vbus_mkdir(vbt, dir, "domfb-ref");
	vbus_mkdir(vbt, dir, "resolution");

	sprintf(dir, "device/%01d/vfb/0/domfb-ref", vdev->otherend_id);
	vbus_write(vbt, dir, "value", "");

	sprintf(dir, "device/%01d/vfb/0/resolution", vdev->otherend_id);
	vbus_printf(vbt, dir, "hor", "%u", FB_HRES);
	vbus_printf(vbt, dir, "ver", "%u", FB_VRES);

	vbus_transaction_end(vbt);

	/* Set a watch on domfb-ref. */

	watches[vdev->otherend_id] = kzalloc(sizeof(struct vbus_watch), GFP_ATOMIC);
	vbus_watch_pathfmt(vdev, watches[vdev->otherend_id], callback_me_domfb, "device/%01d/vfb/0/domfb-ref/value", vdev->otherend_id);
}

void vfb_remove(struct vbus_device *vdev)
{
	DBG(VFB_PREFIX "Backend remove: %d\n", vdev->otherend_id);
}

/* Cleaning-up resources when a ME has been shutdown. */
void vfb_close(struct vbus_device *vdev)
{
	struct gnttab_unmap_grant_ref op;
	DBG(VFB_PREFIX "Backend close: %d\n", vdev->otherend_id);

	/* Unregister and free the watch. */
	unregister_vbus_watch(watches[vdev->otherend_id]);
	kfree(watches[vdev->otherend_id]);
	watches[vdev->otherend_id] = NULL;

	/* Unmap the grantref. */
	gnttab_set_unmap_op(
		&op,
		(phys_addr_t) registered_domfb[vdev->otherend_id]->vaddr,
		GNTMAP_host_map | GNTMAP_readonly,
		registered_domfb[vdev->otherend_id]->gnt_handle);
	gnttab_unmap(&op);

	/* Free framebuffer area. */
	free_vm_area(registered_domfb[vdev->otherend_id]->area);

	/* Free vfb_domfb. */
	kfree(registered_domfb[vdev->otherend_id]);
	registered_domfb[vdev->otherend_id] = NULL;

	if (callback_rm_domfb) {
		callback_rm_domfb(vdev->otherend_id);
	}
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

#ifdef CONFIG_ARCH_BCM2835 /* Raspberry Pi 4 */
static int kthread_domfb_cpy(void *arg)
{
	void *addr_domfb, *addr_vc = vfb_info->screen_base;
	uint32_t previous_domfb = 0;

	DBG(VFB_PREFIX "Starting framebuffer copy thread.\n");
	while (true) {
		/* Due to the limitations of the VideoCore explained above, we
		 * use this thread to copy the content of the active domain fb
		 * into the memory of the VideoCore. We should not copy the
		 * active domfb if it belongs to the agency. But only when we
		 * are switching back to the agency. */
		if (active_domfb != 0 || previous_domfb != 0) {
			previous_domfb = active_domfb;
			addr_domfb = registered_domfb[previous_domfb]->vaddr;
			memcpy(addr_vc, addr_domfb, FB_SIZE);
		}

		msleep(50);
	}

	return 0;
}
#endif

static int vfb_set_agencyfb(struct fb_info *info)
{
	/* Domain framebuffer for the agency. */
	struct vfb_domfb *fb;

	if (vfb_info || !info || info->var.xres < MIN_FB_HRES || info->var.yres < MIN_FB_VRES) {
		DBG(VFB_PREFIX "Framebuffer device already registered or resolution too small.\n");
		return -1;
	}

	vfb_info = info;

	/* Set vfb_domfb struct. */
	fb = kzalloc(sizeof(struct vfb_domfb), GFP_ATOMIC);
	fb->id = 0;

#ifdef CONFIG_ARCH_VEXPRESS
	/* The size could be smaller if the bpp is less than 24. But it's
	 * probably bigger because the whole VRAM is attribute to the fb. */
	BUG_ON(info->fix.smem_len < FB_SIZE);
	info->fix.smem_len = FB_SIZE;

	/* The physical address will be used to reconfigure the fb device when
	 * switching back to the agency. */
	fb->paddr = info->fix.smem_start;
	fb->vaddr = info->screen_base;

	/* Register and set the current domain fb. */
	vfb_set_domfb(fb);
	vfb_set_active_domfb(fb->id);
#endif

#ifdef CONFIG_ARCH_BCM2835
	/* With the VideoCore of the Pi 4, we can't change its base address to
	 * make it display another memory region. So, we need to allocate some
	 * memory in which the content of the VideoCore will be copied when
	 * switching to an ME and restored when coming back to the agency. */
	fb->vaddr = vmalloc(FB_SIZE);

	/* Register and set the current front-end fb. */
	vfb_set_domfb(fb);
	vfb_set_active_domfb(fb->id);

	/* Start the framebuffer copy thread. */
	BUG_ON(!kthread_run(kthread_domfb_cpy, NULL, "fb-copy-thread"));
#endif

	DBG(VFB_PREFIX "Found framebuffer: %dx%d %dbpp\n", FB_HRES, FB_VRES, info->var.bits_per_pixel);
	return 0;
}

static int vfb_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	if (event == FB_EVENT_FB_REGISTERED) {
		vfb_set_agencyfb(((struct fb_event *) data)->info);
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
	int i = 0;

	if (!of_device_is_available(np)) {
		return 0;
	}

	vdevback_init(VFB_NAME, &vfbdrv);

	/* We check if there's already a registered framebuffer device. */
	while (i < FB_MAX && vfb_set_agencyfb(registered_fb[i])) {
		i++;
	}

	/* Otherwise, we register a client that will be notified when a
	 * framebuffer is registered. */
	if (i == FB_MAX) {
		DBG(VFB_PREFIX "No framebuffer registered, registering framebuffer client.\n");
		fb_register_client(&vfb_fb_notif);
	}

	return 0;
}

device_initcall(vfb_init);
