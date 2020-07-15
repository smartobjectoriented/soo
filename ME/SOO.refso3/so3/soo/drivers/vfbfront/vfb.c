/*
 * Copyright (C) 2020 Nikolaos Garanis <nikolaos.garanis@heig-vd.ch>
 * Copyright (C) 2018-2019 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2018-2019 Baptiste Delporte <bonel@bonel.net>
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

#include <heap.h>
#include <mutex.h>
#include <delay.h>
#include <memory.h>
#include <asm/mmu.h>

#include <device/driver.h>
#include <device/fb/so3virt_fb.h>

#include <soo/evtchn.h>
#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/console.h>
#include <soo/debug.h>

#include <soo/dev/vfb.h>

#define FB_SIZE_VEXPRESS (1024 * 768 * 4)
#define FB_SIZE_52PI     (1024 * 600 * 4)
#define FB_SIZE_RPI4     ( 800 * 480 * 4)
#define FB_SIZE          FB_SIZE_VEXPRESS /* change this */


void vfb_probe(struct vbus_device *vdev)
{
	int i, res;
	uint32_t fb_base;
	unsigned int evtchn;
	vfb_sring_t *sring;
	struct vbus_transaction vbt;
	vfb_t *vfb;
	grant_ref_t fb_ref;
	char dir[40];

	DBG0("[" VFB_NAME "] Frontend probe\n");

	if (vdev->state == VbusStateConnected)
		return ;

	vfb = malloc(sizeof(vfb_t));
	BUG_ON(!vfb);
	memset(vfb, 0, sizeof(vfb_t));

	dev_set_drvdata(vdev->dev, &vfb->vdevfront);

	DBG("Frontend: Setup ring\n");

	/* Prepare to set up the ring. */

	vfb->ring_ref = GRANT_INVALID_REF;

	/* Allocate an event channel associated to the ring */
	res = vbus_alloc_evtchn(vdev, &evtchn);
	BUG_ON(res);

	vfb->evtchn = evtchn;

	/* Allocate a shared page for the ring */
	sring = (vfb_sring_t *) get_free_vpage();
	if (!sring) {
		lprintk("%s - line %d: Allocating shared ring failed for device %s\n", __func__, __LINE__, vdev->nodename);
		BUG();
	}

	SHARED_RING_INIT(sring);
	FRONT_RING_INIT(&vfb->ring, sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	res = vbus_grant_ring(vdev, phys_to_pfn(virt_to_phys_pt((uint32_t) vfb->ring.sring)));
	if (res < 0)
		BUG();

	vfb->ring_ref = res;

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "ring-ref", "%u", vfb->ring_ref);
	vbus_printf(vbt, vdev->nodename, "ring-evtchn", "%u", vfb->evtchn);

	/*
	 * Grant access to every necessary page of the framebuffer, write the
	 * first grantref to vbstore.
	 */

	fb_base = get_fb_base();
	BUG_ON(!fb_base);

	for (i = 0; i < FB_SIZE / PAGE_SIZE; i++) {
		res = gnttab_grant_foreign_access(vdev->otherend_id, phys_to_pfn(fb_base + i * PAGE_SIZE), 0);
		BUG_ON(res < 0);

		if (i == 0) {
			fb_ref = res;
		}
	}

	sprintf(dir, "device/%01d/vfb/0/fe-fb", ME_domID());
	vbus_printf(vbt, dir, "value", "%u", fb_ref);
	DBG("dir: %s, fb_phys: 0x%08x, fb_ref: 0x%08x\n", dir, get_fb_base(), fb_ref);

	vbus_transaction_end(vbt);
}

/* At this point, the FE is not connected. */
void vfb_reconfiguring(struct vbus_device *vdev)
{
	int res;
	struct vbus_transaction vbt;
	vfb_t *vfb = to_vfb(vdev);

	DBG0("[" VFB_NAME "] Frontend reconfiguring\n");
	/* The shared page already exists */
	/* Re-init */

	gnttab_end_foreign_access_ref(vfb->ring_ref);

	DBG("Frontend: Setup ring\n");

	/* Prepare to set up the ring. */

	vfb->ring_ref = GRANT_INVALID_REF;

	SHARED_RING_INIT(vfb->ring.sring);
	FRONT_RING_INIT(&vfb->ring, (&vfb->ring)->sring, PAGE_SIZE);

	/* Prepare the shared page to be visible on the other end. */

	res = vbus_grant_ring(vdev, phys_to_pfn(virt_to_phys_pt((uint32_t) vfb->ring.sring)));
	BUG_ON(res < 0);

	vfb->ring_ref = res;

	vbus_transaction_start(&vbt);
	vbus_printf(vbt, vdev->nodename, "ring-ref", "%u", vfb->ring_ref);
	vbus_printf(vbt, vdev->nodename, "ring-evtchn", "%u", vfb->evtchn);
	vbus_transaction_end(vbt);
}

void vfb_shutdown(struct vbus_device *vdev)
{
	DBG0("[" VFB_NAME "] Frontend shutdown\n");
}

void vfb_closed(struct vbus_device *vdev)
{
	vfb_t *vfb = to_vfb(vdev);

	DBG0("[" VFB_NAME "] Frontend close\n");

	/* Free resources associated with old device channel. */
	if (vfb->ring_ref != GRANT_INVALID_REF) {
		gnttab_end_foreign_access(vfb->ring_ref);
		free_vpage((uint32_t) vfb->ring.sring);

		vfb->ring_ref = GRANT_INVALID_REF;
		vfb->ring.sring = NULL;
	}
}

void vfb_suspend(struct vbus_device *vdev)
{
	DBG0("[" VFB_NAME "] Frontend suspend\n");
}

void vfb_resume(struct vbus_device *vdev)
{
	DBG0("[" VFB_NAME "] Frontend resume\n");
}

void vfb_connected(struct vbus_device *vdev)
{
	DBG0("[" VFB_NAME "] Frontend connected\n");
}

vdrvfront_t vfbdrv = {
	.probe = vfb_probe,
	.reconfiguring = vfb_reconfiguring,
	.shutdown = vfb_shutdown,
	.closed = vfb_closed,
	.suspend = vfb_suspend,
	.resume = vfb_resume,
	.connected = vfb_connected
};

static int vfb_init(dev_t *dev)
{
	vdevfront_init(VFB_NAME, &vfbdrv);
	return 0;
}

REGISTER_DRIVER_POSTCORE("vfb,frontend", vfb_init);
