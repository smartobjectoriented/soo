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

#include "common.h"

#include <soo/evtchn.h>
#include <linux/wait.h>
#include <linux/slab.h>

#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/uapi/debug.h>

/* Give the possibility to maintain the number of frontend currently connected to this backend,
 * and to manage an associated waitqueue.
 */
void vinput_get(void)
{
	atomic_inc(&vinput.refcnt);
}

void vinput_put(void)
{
	if (atomic_dec_and_test(&vinput.refcnt))
		wake_up(&vinput.waiting_to_free);
}

/*
 * Set up a ring (shared page & event channel) between the agency and the ME.
 */
static int setup_sring(struct vbus_device *dev)
{
	int res;
	unsigned long ring_ref;
	unsigned int evtchn;
	vinput_sring_t *sring;
	vinput_ring_t *p_vinput_ring;

	p_vinput_ring = &vinput.rings[dev->otherend_id];

	vbus_gather(VBT_NIL, dev->otherend, "ring-ref", "%lu", &ring_ref, "ring-evtchn", "%u", &evtchn, NULL);

	res = vbus_map_ring_valloc(dev, ring_ref, (void **)&sring);
	if (res < 0)
	  return res;

	SHARED_RING_INIT(sring);
	BACK_RING_INIT(&p_vinput_ring->ring, sring, PAGE_SIZE);

	res = bind_interdomain_evtchn_to_irqhandler(dev->otherend_id, evtchn, vinput_interrupt, NULL, 0, "vinput-backend", dev);
  if (res < 0) {
	  vbus_unmap_ring_vfree(dev, sring);
	  (&p_vinput_ring->ring)->sring = NULL;

		return res;
	}

  p_vinput_ring->irq = res;

  /* Increment refcount for this vdev */
  vinput_get();

	return 0;
}


/*
 * Entry point to this code when a new device is created.  Allocate the basic
 * structures, and watch the store waiting for the hotplug scripts to tell us
 * the device's physical major and minor numbers.  Switch to InitWait.
 */
static int vinput_probe(struct vbus_device *dev, const struct vbus_device_id *id)
{
	DBG("Setting keyboard hw properties for %s\n", dev->nodename);

	vinput_subsys_init(dev);

	DBG("%s: SOO vinput driver probe done.\n", __func__);

	return 0;

}


static int frontend_resumed(struct vbus_device *dev)
{
	/* Resume Step 3 */
	DBG("%s: now is %s ...\n", __func__, dev->nodename);

	return 0;
}

/*
 * Callback received when the frontend's state changes.
 */
static void frontend_changed(struct vbus_device *dev, enum vbus_state frontend_state)
{
	int res = 0;
	vinput_ring_t *p_vinput_ring;
	int domid;

	p_vinput_ring = &vinput.rings[dev->otherend_id];

	DBG("%s\n", vbus_strstate(frontend_state));

	switch (frontend_state) {

	case VbusStateInitialised:
	case VbusStateReconfigured:

		res = setup_sring(dev);
		if (res) {
			lprintk("%s - line %d: Retrieval of ring info failed for device name %s\n", __func__, __LINE__, dev->nodename);
			BUG();
		}

		DBG0("SOO vinput: now connected...\n");

		break;

	case VbusStateConnected:

		DBG0("->vinput frontend connected, all right.\n");

		break;

	case VbusStateClosing:
		DBG0("Got that the virtual input frontend now closing...\n");

		vinput_put();

		/* Prepare to empty all buffers */
		BACK_RING_INIT(&p_vinput_ring->ring, (&p_vinput_ring->ring)->sring, PAGE_SIZE);

		unbind_from_irqhandler(p_vinput_ring->irq, dev);

		vbus_unmap_ring_vfree(dev, p_vinput_ring->ring.sring);
	  p_vinput_ring->ring.sring = NULL;

    p_vinput_ring->dev = NULL;

		domid = (vinput.domfocus == 1) ? 2 : 1;

		if (vinput.rings[domid].dev == NULL)
			vinput.domfocus = -1;
		else
			vinput.domfocus = domid;

		break;

	case VbusStateSuspended:

		/* Suspend Step 3 */
		DBG("frontend_suspended: %s ...\n", dev->nodename);

		break;

	case VbusStateUnknown:

	default:
		lprintk("%s - line %d: Unknown state %d (frontend) for device name %s\n", __func__, __LINE__, frontend_state, dev->nodename);
		BUG();
		break;
	}
}

/*
 * When the backend is being shutdown, we have to ensure that there is no still-using frontend.
 * 		wait_event(vinput.waiting_to_free, atomic_read(&vinput.refcnt) == 0);
 */

/* ** Driver Registration ** */

static const struct vbus_device_id vinput_ids[] = {
	{ "vinput" },
	{ "" }
};

static int vinput_suspend(struct vbus_device *dev)
{
	/* Suspend Step 1 */

	DBG0("Backend waiting for frontend now...\n");

	return 0;
}

static int vinput_resume(struct vbus_device *dev)
{
	/* Resume Step 1 */

	DBG("Backend resuming: %s ...\n", dev->nodename);

	return 0;
}

static struct vbus_driver vinput_drv = {
	.name = "vinput",
	.owner = THIS_MODULE,
	.ids = vinput_ids,
	.probe = vinput_probe,
	.otherend_changed = frontend_changed,
	.suspend = vinput_suspend,
	.resume = vinput_resume,
	.resumed = frontend_resumed,
};

int vinput_vbus_init(void) {
	init_waitqueue_head(&vinput.waiting_to_free);

  return vbus_register_backend(&vinput_drv);
}



