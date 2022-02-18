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

#include <stdarg.h>

#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/kthread.h>
#include <linux/input.h>
#include <uapi/linux/input-event-codes.h>

#include <soo/evtchn.h>
#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/uapi/console.h>
#include <soo/vdevback.h>
#include <soo/dev/vinput.h>

#define VINPUT_COUNT 8

static vinput_t *vinputs[VINPUT_COUNT];
static domid_t current_vinput = 0;

/* When a touch event is received, each axis is bounded by min and max values
 * which are not related to the display resolution. These values are
 * communicated by the touchscreen device. */
static int32_t min[] = {0, 0};
static int32_t max[] = {1000, 1000}; /* TODO test values */

/*
 * Note: do not use DBG/printk, only lprintk (see avz_switch_console).
 */
void vinput_set_current(domid_t id)
{
	current_vinput = id;
}

void vinput_pass_event(unsigned int type, unsigned int code, int value)
{
	vinput_response_t *ring_rsp;
	vinput_t *vinput = vinputs[current_vinput];

	/* If front-end is not connected, skip. */
	if (!vinput) {
		return;
	}

	if (type == 0 && code == 0 && value == 0) {
		DBG(VINPUT_PREFIX "--- SYN report ---\n");
	}
	else {
		DBG(VINPUT_PREFIX "%u %u %d\n", type, code, value);
	}

	/* Modify touch events. Value remains to be multiplied by the screen
	 * resolution in the virtual driver. Doing this here avoids passing min
	 * and max to the front-end. */
	if (type == EV_ABS && (code == ABS_X || code == ABS_Y)) {
		value = 10000 * (value - min[code]) / (max[code] - min[code]);
	}

	/* Send event to front-end. */
	ring_rsp = vinput_ring_response(&vinput->ring);
	ring_rsp->type = type;
	ring_rsp->code = code;
	ring_rsp->value = value;

	vinput_ring_response_ready(&vinput->ring);
	notify_remote_via_virq(vinput->irq);
}

void vinput_set_touch_bounds(struct input_absinfo *info)
{
	min[0] = info[0].minimum; /* x */
	max[0] = info[0].maximum;
	min[1] = info[1].minimum; /* y */
	max[1] = info[1].maximum;
}

irqreturn_t vinput_interrupt(int irq, void *dev_id)
{
	/* Ignore interrupts from the front-end. */
	return IRQ_HANDLED;
}

void vinput_probe(struct vbus_device *vdev)
{
	vinput_t *vinput = kzalloc(sizeof(vinput_t), GFP_ATOMIC);
	BUG_ON(!vinput);
	dev_set_drvdata(&vdev->dev, &vinput->vdevback);

	DBG(VINPUT_PREFIX "Backend probe: %d\n", vdev->otherend_id);
}

void vinput_remove(struct vbus_device *vdev)
{
	DBG(VINPUT_PREFIX "Backend remove: %d\n", vdev->otherend_id);
}

void vinput_close(struct vbus_device *vdev)
{
	vinput_t *vinput = to_vinput(vdev);

	DBG(VINPUT_PREFIX "Backend close: %d\n", vdev->otherend_id);

	/* Free the ring and unbind evtchn. */
	BACK_RING_INIT(&vinput->ring, (&vinput->ring)->sring, PAGE_SIZE);
	unbind_from_virqhandler(vinput->irq, vdev);

	vbus_unmap_ring_vfree(vdev, vinput->ring.sring);
	vinput->ring.sring = NULL;
}

void vinput_suspend(struct vbus_device *vdev)
{
	DBG(VINPUT_PREFIX "Backend suspend: %d\n", vdev->otherend_id);
}

void vinput_resume(struct vbus_device *vdev)
{
	DBG(VINPUT_PREFIX "Backend resume: %d\n", vdev->otherend_id);
}

void vinput_reconfigured(struct vbus_device *vdev)
{
	int res;
	unsigned long ring_ref;
	unsigned int evtchn;
	vinput_sring_t *sring;
	vinput_t *vinput = to_vinput(vdev);

	DBG(VINPUT_PREFIX "Backend reconfigured: %d\n", vdev->otherend_id);

	/* Set up a ring (shared page & event channel) between the agency and the ME. */
	vbus_gather(VBT_NIL, vdev->otherend, "ring-ref", "%lu", &ring_ref, "ring-evtchn", "%u", &evtchn, NULL);

	DBG("BE: ring-ref=%lu, event-channel=%u\n", ring_ref, evtchn);

	res = vbus_map_ring_valloc(vdev, ring_ref, (void **) &sring);
	BUG_ON(res < 0);

	SHARED_RING_INIT(sring);
	BACK_RING_INIT(&vinput->ring, sring, PAGE_SIZE);

	res = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, evtchn, vinput_interrupt, NULL, 0, VINPUT_NAME "-backend", vdev);
	BUG_ON(res < 0);

	vinput->irq = res;
}

void vinput_connected(struct vbus_device *vdev)
{
	DBG(VINPUT_PREFIX "Backend connected: %d\n",vdev->otherend_id);
	vinputs[vdev->otherend_id] = to_vinput(vdev);
}

vdrvback_t vinputdrv = {
	.probe = vinput_probe,
	.remove = vinput_remove,
	.close = vinput_close,
	.connected = vinput_connected,
	.reconfigured = vinput_reconfigured,
	.resume = vinput_resume,
	.suspend = vinput_suspend
};

int vinput_init(void)
{
	struct device_node *np = of_find_compatible_node(NULL, NULL, "vinput,backend");
	if (!of_device_is_available(np)) {
		return 0;
	}

	vdevback_init(VINPUT_NAME, &vinputdrv);
	return 0;
}

device_initcall(vinput_init);
