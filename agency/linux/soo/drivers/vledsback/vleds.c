/*
 * Copyright (C) 2018,2019 Baptiste Delporte <bonel@bonel.net>
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
#include <linux/of.h>

#include <soo/core/device_access.h>

#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/uapi/console.h>
#include <soo/uapi/soo.h>
#include <soo/guest_api.h>

#include <stdarg.h>
#include <linux/kthread.h>

#include <soo/dev/vleds.h>

#if defined(CONFIG_LEDS_FAN5702)
/* Interface with the HW LED controller */
extern void led_ctrl_set_brightness(uint8_t id, uint8_t value);
extern void led_ctrl_blink(uint8_t id, uint8_t value);
#endif /* CONFIG_LEDS_FAN5702 */

vleds_t vleds;

/**
 * cmd_ring interrupt.
 */
irqreturn_t vleds_cmd_interrupt(int irq, void *dev_id) {
	struct vbus_device *dev = (struct vbus_device *) dev_id;
	RING_IDX i, rp;
	vleds_cmd_request_t *ring_req;
	uint8_t id, value;

	if (unlikely(!vleds_is_connected(dev->otherend_id)))
		return IRQ_HANDLED;

	rp = vleds.cmd_rings[dev->otherend_id].ring.sring->req_prod;
	dmb();

	for (i = vleds.cmd_rings[dev->otherend_id].ring.sring->req_cons ; i != rp ; i++) {
		ring_req = RING_GET_REQUEST(&vleds.cmd_rings[dev->otherend_id].ring, i);

		DBG(VLEDS_PREFIX "0x%08x, 0x%08x\n", ring_req->cmd, ring_req->arg);

		switch (ring_req->cmd) {
		case VLEDS_IOCTL_SET_BRIGHTNESS:
			id = (ring_req->arg >> 8) & 0xff;
			value = ring_req->arg & 0xff;

#if defined(CONFIG_LEDS_FAN5702)
			led_ctrl_set_brightness(id, value);
#else
			lprintk(VLEDS_PREFIX "%s: led_ctrl_set_brightness(id:%d, value:%d)\n", __func__, id, value);
#endif /* CONFIG_LEDS_FAN5702 */
			break;

		case VLEDS_IOCTL_SET_ON:
			id = ring_req->arg & 0xff;

#if defined(CONFIG_LEDS_FAN5702)
			led_ctrl_set_brightness(id, 0xff);
#else
			lprintk(VLEDS_PREFIX "%s: led_ctrl_set_brightness(id:%d, value:0xff)\n", __func__, id);
#endif /* CONFIG_LEDS_FAN5702 */
			break;

		case VLEDS_IOCTL_SET_OFF:
			id = ring_req->arg & 0xff;

#if defined(CONFIG_LEDS_FAN5702)
			led_ctrl_set_brightness(id, 0);
#else
			lprintk(VLEDS_PREFIX "%s: led_ctrl_set_brightness(id:%d, value:0x0)\n", __func__, id);
#endif /* CONFIG_LEDS_FAN5702 */

			break;

		case VLEDS_IOCTL_SET_BLINK:
			id = (ring_req->arg >> 8) & 0xff;
			value = ring_req->arg & 0xff;

#if defined(CONFIG_LEDS_FAN5702)
			led_ctrl_blink(id, value);
#else
			lprintk(VLEDS_PREFIX "%s: led_ctrl_blink(id:%d, value:%d)\n", __func__, id, value);
#endif /* CONFIG_LEDS_FAN5702 */

			break;

		default:
			lprintk(VLEDS_PREFIX "Invalid command\n");
			BUG();
		}
	}

	vleds.cmd_rings[dev->otherend_id].ring.sring->req_cons = i;

	return IRQ_HANDLED;
}

void vleds_probe(struct vbus_device *dev) {
	DBG(VLEDS_PREFIX "Backend probe: %d\n", dev->otherend_id);
}

void vleds_close(struct vbus_device *dev) {
	DBG(VLEDS_PREFIX "Backend close: %d\n", dev->otherend_id);
}

void vleds_suspend(struct vbus_device *dev) {
	DBG(VLEDS_PREFIX "Backend suspend: %d\n", dev->otherend_id);
}

void vleds_resume(struct vbus_device *dev) {
	DBG(VLEDS_PREFIX "Backend resume: %d\n", dev->otherend_id);
}

void vleds_reconfigured(struct vbus_device *dev) {
	DBG(VLEDS_PREFIX "Backend reconfigured: %d\n", dev->otherend_id);
}

void vleds_connected(struct vbus_device *dev) {
	DBG(VLEDS_PREFIX "Backend connected: %d\n", dev->otherend_id);
}

int vleds_init(void) {
	int ret;
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "vleds,backend");

	/* Check if DTS has vuihandler enabled */
	if (!of_device_is_available(np))
		return 0;

	vleds_vbus_init();

	/* Set the associated dev capability */
	devaccess_set_devcaps(DEVCAPS_CLASS_LED, DEVCAP_LED_6LED, true);

	return ret;
}

module_init(vleds_init);
