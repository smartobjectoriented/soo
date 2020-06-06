/*
 * Copyright (C) 2016 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2018 David Truan <david.truan@heig-vd.ch>
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
#include <linux/spinlock.h>
#include <linux/kfifo.h>
#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of.h>

#include <soo/core/device_access.h>

#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/uapi/console.h>
#include <soo/uapi/soo.h>
#include <soo/guest_api.h>
#include <soo/uapi/debug.h>

#include <stdarg.h>

#include <soo/dev/vdoga12v6nm.h>

vdoga12v6nm_t vdoga12v6nm;

/* Used to process the request from the vdoga12v6nm backend */
typedef struct {
	domid_t domid;
	unsigned int cmd;
	unsigned long arg;
} motor_arg_t;
static motor_arg_t motor_arg;
static struct completion cmd_complete;

/* 0 = no stop, 1 = up stop, 2 = down stop */
static int last_stop_type = 0;

#if !defined(CONFIG_ARCH_VEXPRESS) && defined(CONFIG_DS1050) && defined(CONFIG_DRV8703)

/* DRV8703_Q1 exported functions */
extern void drv8703q1_write_in1_ph(bool set);

/* DS1050 exported functions */
extern void ds1050_set_duty_cycle(uint8_t duty_cycle);
extern void ds1050_duty_cycle_full(void);
extern void ds1050_shutdown(void);
extern void ds1050_recall(void);

/**
 * Enable the motor by recalling from the shutdown mode. The speed after recalling
 * is the same as it was before entering shutdown mode if the speed wasn't changed.
 */
static void motor_enable(void) {
	DBG("%s\n", __func__);

	/* Reset the last saved stop type */
	last_stop_type = 0;

	ds1050_recall();
}

/**
 * Set the shutdown mode. It puts the PWM output into a low current state.
 */
static void motor_disable(void) {
	ds1050_shutdown();
}

/**
 * Set the percentage of PWM.
 */
static void motor_set_percentage_speed(uint8_t speed) {
	if (speed == 100) {
		ds1050_duty_cycle_full();

		return ;
	}

	ds1050_set_duty_cycle(speed);
}

/**
 * Set the rotation direction.
 * 1 = going up
 * 0 = going down
 */
static void motor_set_rotation_direction(uint8_t direction) {
	if (direction == 1)
		drv8703q1_write_in1_ph(true);

	if (direction == 0)
		drv8703q1_write_in1_ph(false);
}

#endif /* !CONFIG_ARCH_VEXPRESS && CONFIG_DS1050 && CONFIG_DRV8703 */

/**
 * Process a command coming from a frontend.
 */
irqreturn_t vdoga12v6nm_cmd_interrupt(int irq, void *dev_id) {
	struct vbus_device *dev = (struct vbus_device *) dev_id;
	RING_IDX i, rp;
	vdoga12v6nm_cmd_request_t *ring_req;

	if (!vdoga12v6nm_is_connected(dev->otherend_id))
		return IRQ_HANDLED;

	rp = vdoga12v6nm.cmd_rings[dev->otherend_id].ring.sring->req_prod;
	dmb();

	for (i = vdoga12v6nm.cmd_rings[dev->otherend_id].ring.sring->req_cons; i != rp; i++) {
		ring_req = RING_GET_REQUEST(&vdoga12v6nm.cmd_rings[dev->otherend_id].ring, i);

		DBG("0x%08x\n", ring_req->cmd);

		switch (ring_req->cmd) {
		case VDOGA12V6NM_ENABLE:
		case VDOGA12V6NM_DISABLE:
		case VDOGA12V6NM_SET_PERCENTAGE_SPEED:
		case VDOGA12V6NM_SET_ROTATION_DIRECTION:
			motor_arg.domid = dev->otherend_id;
			motor_arg.cmd = ring_req->cmd;
			motor_arg.arg = ring_req->arg;
			complete(&cmd_complete);
			break;

		default:
			BUG();
		}
	}

	vdoga12v6nm.cmd_rings[dev->otherend_id].ring.sring->req_cons = i;

	return IRQ_HANDLED;
}

/**
 * The up_notification should not be used in this direction.
 */
irqreturn_t vdoga12v6nm_up_interrupt(int irq, void *dev_id) {
	/* Nothing to do */

	return IRQ_HANDLED;
}

/**
 * The down_notification should not be used in this direction.
 */
irqreturn_t vdoga12v6nm_down_interrupt(int irq, void *dev_id) {
	/* Nothing to do */

	return IRQ_HANDLED;
}

/**
 * Send a return value back to a frontend after the execution of a command.
 */
static void do_cmd_feedback(domid_t domid, int ret) {
	vdoga12v6nm_cmd_response_t *ring_rsp;

	vdoga12v6nm_start(domid);

	ring_rsp = RING_GET_RESPONSE(&vdoga12v6nm.cmd_rings[domid].ring, vdoga12v6nm.cmd_rings[domid].ring.sring->rsp_prod);

	DBG("Ret=%d\n", ret);
	ring_rsp->ret = ret;

	dmb();

	vdoga12v6nm.cmd_rings[domid].ring.rsp_prod_pvt++;

	RING_PUSH_RESPONSES(&vdoga12v6nm.cmd_rings[domid].ring);

	notify_remote_via_virq(vdoga12v6nm.cmd_rings[domid].irq);

	vdoga12v6nm_end(domid);
}

/**
 * Deferred command processing task.
 */
static int vdoga12v6nm_task_fn(void *arg) {
	motor_arg_t motor_arg_cpy;
	int ret = 0;

	while (1) {

		wait_for_completion(&cmd_complete);

		memcpy(&motor_arg_cpy, &motor_arg, sizeof(motor_arg_t));

#if !defined(CONFIG_ARCH_VEXPRESS) && defined(CONFIG_DS1050) && defined(CONFIG_DRV8703)

		switch (motor_arg.cmd) {
		case VDOGA12V6NM_ENABLE:
			DBG("cmd: VDOGA12V6NM_ENABLE\n");
			motor_enable();
			break;

		case VDOGA12V6NM_DISABLE:
			DBG("cmd: VDOGA12V6NM_DISABLE\n");
			motor_disable();
			break;

		case VDOGA12V6NM_SET_PERCENTAGE_SPEED:
			DBG("cmd: VDOGA12V6NM_SET_PERCENTAGE_SPEED\n");
			motor_set_percentage_speed(motor_arg_cpy.arg);
			break;

		case VDOGA12V6NM_SET_ROTATION_DIRECTION:
			DBG("cmd: VDOGA12V6NM_SET_ROTATION_DIRECTION\n");
			motor_set_rotation_direction(motor_arg_cpy.arg);
			break;

		default:
			BUG();
		}

#else /* !CONFIG_ARCH_VEXPRESS && CONFIG_DS1050 && CONFIG_DRV8703 */

		switch (motor_arg.cmd) {
		case VDOGA12V6NM_ENABLE:
			DBG("cmd: VDOGA12V6NM_ENABLE\n");
			break;

		case VDOGA12V6NM_DISABLE:
			DBG("cmd: VDOGA12V6NM_DISABLE\n");
			break;

		case VDOGA12V6NM_SET_PERCENTAGE_SPEED:
			DBG("cmd: VDOGA12V6NM_SET_PERCENTAGE_SPEED\n");
			break;

		case VDOGA12V6NM_SET_ROTATION_DIRECTION:
			DBG("cmd: VDOGA12V6NM_SET_ROTATION_DIRECTION\n");
			break;

		default:
			BUG();
		}

#endif /* CONFIG_ARCH_VEXPRESS || (!CONFIG_DS1050 || !CONFIG_DRV8703) */

		do_cmd_feedback(motor_arg_cpy.domid, ret);

	}

	return 0;
}

#if 0 /* Poll instead of using IRQ */
/**
 * Up mechanical stop interrupt.
 * An interrupt is triggered when the blind reaches its fully open position.
 */
static irqreturn_t up_interrupt(int irq, void *dev_id) {
	uint32_t i;

	DBG0("Blind up interrupt\n");

	/* Forward a notification to the frontend(s) */
	for (i = 0; i < MAX_DOMAINS; i++) {
		if (vdoga12v6nm.rings[i].cmd_ring_desc.count == 0)
			continue;

		notify_remote_via_irq(vdoga12v6nm.notifications[i].up.irq);
	}

	return IRQ_HANDLED;
}
#endif /* 0 */

/**
 * Up mechanical stop event. We call it "event" because it is not based on any interrupt but polling.
 * An event is triggered when the blind reaches its fully opened position.
 */
static void up_event(void) {
	uint32_t i;

	DBG0("Blind up event\n");

	/* Forward a notification to the frontend(s) */
	for (i = 1; i < MAX_DOMAINS; i++) {
		if (!vdoga12v6nm_start(i))
			continue;

		notify_remote_via_virq(vdoga12v6nm.up_notifications[i].irq);
		vdoga12v6nm_end(i);
	}
}

/**
 * Down mechanical stop event. We call it "event" because it is not based on any interrupt but polling.
 * An event is triggered when the blind reaches its fully closed position.
 */
static void down_event(void) {
	uint32_t i;

	DBG0("Blind down event\n");

	/* Forward a notification to the frontend(s) */
	for (i = 1; i < MAX_DOMAINS; i++) {
		if (!vdoga12v6nm_start(i))
			continue;

		notify_remote_via_virq(vdoga12v6nm.down_notifications[i].irq);
		vdoga12v6nm_end(i);
	}
}

/**
 * Task that polls the mechanical stop GPIOs.
 */
static int vdoga12v6nm_stop_task_fn(void *arg) {
	int up_value, old_up_value = 1, down_value, old_down_value = 1;

	while (1) {
		msleep(GPIO_POLL_PERIOD);

		up_value = gpio_get_value(UP_GPIO);
		if ((up_value != old_up_value) && (!up_value)) {
			if (last_stop_type != 1) {
#if !defined(CONFIG_ARCH_VEXPRESS) && defined(CONFIG_DS1050) && defined(CONFIG_DRV8703)
				motor_disable();
#endif /* !CONFIG_ARCH_VEXPRESS && CONFIG_DS1050 && CONFIG_DRV8703 */
				up_event();
			}
			last_stop_type = 1;
		}
		old_up_value = up_value;

		down_value = gpio_get_value(DOWN_GPIO);
		if ((down_value != old_down_value) && (!down_value)) {
			if (last_stop_type != 2) {
#if !defined(CONFIG_ARCH_VEXPRESS) && defined(CONFIG_DS1050) && defined(CONFIG_DRV8703)
				motor_disable();
#endif /* !CONFIG_ARCH_VEXPRESS && CONFIG_DS1050 && CONFIG_DRV8703 */
				down_event();
			}
			last_stop_type = 2;
		}
		old_down_value = down_value;
	}

	return 0;
}

/**
 * Setup of the GPIOs required for the mechanical stop detection.
 */
static void setup_gpios(void) {
	int ret;
#if 0 /* Poll instead of using IRQ */
	int gpio_up_irq;
#endif /* 0 */

	/* Up mechanical stop detection GPIO */
	BUG_ON((ret = gpio_request(UP_GPIO, "vDoga12V6Nm_up")) < 0);
	gpio_direction_input(UP_GPIO);

#if 0 /* Poll instead of using IRQ */
	/* Request an IRQ bound to the GPIO */
	BUG_ON((gpio_up_irq = gpio_to_irq(UP_GPIO)) < 0);
	BUG_ON(request_irq(gpio_up_irq, up_interrupt, IRQF_TRIGGER_FALLING, "vDoga12V6Nm_up_irq", NULL) < 0);
#endif /* 0 */

	/* Down mechanical stop detection GPIO */
	BUG_ON((ret = gpio_request(DOWN_GPIO, "vDoga12V6Nm_down")) < 0);
	gpio_direction_input(DOWN_GPIO);

	/* We cannot request any IRQ on PD20. We must poll it. */
	kthread_run(vdoga12v6nm_stop_task_fn, NULL, "vdoga12v6nm_stop");
}


void vdoga12v6nm_probe(struct vbus_device *dev) {
	static bool vdoga12v6nm_initialized = false;

	DBG(VDOGA12V6NM_PREFIX "Backend probe: %d\n", dev->otherend_id);

	if (!vdoga12v6nm_initialized) {
		setup_gpios();

		kthread_run(vdoga12v6nm_task_fn, NULL, "vdoga12v6nm");

		vdoga12v6nm_initialized = true;
	}
}


void vdoga12v6nm_close(struct vbus_device *dev) {
	DBG(VDOGA12V6NM_PREFIX "Backend close: %d\n", dev->otherend_id);
}

void vdoga12v6nm_suspend(struct vbus_device *dev) {
	DBG(VDOGA12V6NM_PREFIX "Backend suspend: %d\n", dev->otherend_id);

}

void vdoga12v6nm_resume(struct vbus_device *dev) {
	DBG(VDOGA12V6NM_PREFIX "Backend resume: %d\n", dev->otherend_id);
}

void vdoga12v6nm_reconfigured(struct vbus_device *dev) {
	DBG(VDOGA12V6NM_PREFIX "Backend reconfigured: %d\n", dev->otherend_id);
}

void vdoga12v6nm_connected(struct vbus_device *dev) {
	DBG(VDOGA12V6NM_PREFIX "Backend connected: %d\n", dev->otherend_id);

	notify_remote_via_virq(vdoga12v6nm.cmd_rings[dev->otherend_id].irq);
}

int vdoga12v6nm_init(void) {
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "vdoga12v6nm,backend");

	/* Check if DTS has vuihandler enabled */
	if (!of_device_is_available(np))
		return 0;

	init_completion(&cmd_complete);

	vdoga12v6nm_vbus_init();

	/* Set the device capability associated to this interface */
	devaccess_set_devcaps(DEVCAPS_CLASS_DOMOTICS, DEVCAP_BLIND_MOTOR, true);

	return 0;
}

module_init(vdoga12v6nm_init);
