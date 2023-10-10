/*
 * Copyright (C) 2021 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#include <soo/uapi/console.h>

#include "rpisense-joystick.h"

static joystick_handler_t __joystick_handler = NULL;
static struct vbus_device *__vdev;

#ifdef CONFIG_ARCH_VEXPRESS

#include <soo/core/sysfs.h>

/*
 * A joystick command simulation is performed via sysfs.
 * The joystick position is given by a number between 0 and 4 as follows:
 * - 0 for center
 * - 1 for right
 * - 2 for down
 * - 3 for left
 * - 4 for up
 */
void sysfs_sensej_store(char *str) {
	int key;

	switch (str[0]) {
	case '0':
		key = RPISENSE_JS_CENTER;
		break;
	case '1':
		key = RPISENSE_JS_RIGHT;
		break;
	case '2':
		key = RPISENSE_JS_DOWN;
		break;
	case '3':
		key = RPISENSE_JS_LEFT;
		break;
	case '4':
		key = RPISENSE_JS_UP;
		break;
	}

	__joystick_handler(__vdev, key);
}

void rpisense_joystick_handler_register(struct vbus_device *vdev, joystick_handler_t joystick_handler) {

	__joystick_handler = joystick_handler;
	__vdev = vdev;

}

void rpisense_joystick_handler_unregister(struct vbus_device *vdev) {
	__joystick_handler = NULL;
	__vdev = NULL;
}
EXPORT_SYMBOL(rpisense_joystick_handler_unregister);

#else

#include <linux/interrupt.h>

#include <linux/mfd/rpisense/core.h>

#include <linux/platform_device.h>

static struct rpisense *rpisense = NULL;

static irqreturn_t keys_irq_handler_bh(int irq, void *pdev) {
	int key;

	key = i2c_smbus_read_byte_data(rpisense->i2c_client, RPISENSE_JS_ADDR);

	if (__joystick_handler)
		__joystick_handler((struct vbus_device *) pdev, key);

	return IRQ_HANDLED;
}

static irqreturn_t keys_irq_handler(int irq, void *pdev)
{
	/*
	 * Nothing to do in the top half. Access to i2c requires
	 * the scheduler to be not in an atomic context.
	 */

	return IRQ_WAKE_THREAD;
}

void rpisense_joystick_handler_register(struct vbus_device *vdev, joystick_handler_t joystick_handler) {
	int ret;

	__joystick_handler = joystick_handler;
	__vdev = vdev;

	ret = request_threaded_irq(rpisense->joystick.keys_irq,
				  keys_irq_handler, keys_irq_handler_bh, IRQF_TRIGGER_RISING,
				  "keys", __vdev);
	if (ret) {
		lprintk("IRQ request failed ret = %d.\n", ret);
		BUG();
	}
}

void rpisense_joystick_handler_unregister(struct vbus_device *vdev) {
	__joystick_handler = NULL;
	__vdev = NULL;

	free_irq(rpisense->joystick.keys_irq, vdev);
}
EXPORT_SYMBOL(rpisense_joystick_handler_unregister);

#endif /* !CONFIG_ARCH_VEXPRESS */

EXPORT_SYMBOL(rpisense_joystick_handler_register);

void sensej_init(void) {

#ifdef CONFIG_ARCH_VEXPRESS

	soo_sysfs_register(vsensej_js, NULL, sysfs_sensej_store);

#else
	int ret;

	rpisense = rpisense_get_dev();
	ret = gpiod_direction_input(rpisense->joystick.keys_desc);
	if (ret) {
		printk("Could not set keys-int direction.\n");
		BUG();
	}

	rpisense->joystick.keys_irq = gpiod_to_irq(rpisense->joystick.keys_desc);
	if (rpisense->joystick.keys_irq < 0) {
		printk("Could not determine keys-int IRQ.\n");
		BUG();
	}

#endif /* !CONFIG_ARCH_VEXPRESS */

}
EXPORT_SYMBOL(sensej_init);
