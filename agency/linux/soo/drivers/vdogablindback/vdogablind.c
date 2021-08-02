/*
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

#include <soo/evtchn.h>
#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/uapi/console.h>

#include <stdarg.h>
#include <linux/kthread.h>

#include <soo/vdevback.h>

#include <linux/gpio/driver.h>
#include <linux/gpio/consumer.h> 

#include <soo/dev/vdogablind.h>


/* DS1050 exported functions */
extern void ds1050_set_duty_cycle(uint8_t duty_cycle);
extern void ds1050_shutdown(void);
extern void ds1050_recall(void);
extern void ds1050_duty_cycle_full(void);





typedef struct {

	/* Must be the first field */
	vdogablind_t vdogablind;
	bool can_move_up;
	bool can_move_down;
	bool already_down;
	bool already_up;

	struct gpio_desc *sleep_gpio;
	struct gpio_desc *mode_gpio;
	struct gpio_desc *in1_ph_gpio;
	struct gpio_desc *in2_ph_gpio;
	struct gpio_desc *up_end_gpio;
	struct gpio_desc *down_end_gpio;

} vdogablind_priv_t;

static struct vbus_device *vdogablind_dev = NULL;

void vdogablind_notify(struct vbus_device *vdev)
{
	vdogablind_priv_t *vdogablind_priv = dev_get_drvdata(&vdev->dev);

	vdogablind_ring_response_ready(&vdogablind_priv->vdogablind.ring);

	/* Send a notification to the frontend only if connected.
	 * Otherwise, the data remain present in the ring. */

	notify_remote_via_virq(vdogablind_priv->vdogablind.irq);
}


irqreturn_t vdogablind_interrupt(int irq, void *dev_id)
{
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vdogablind_priv_t *vdogablind_priv = dev_get_drvdata(&vdev->dev);
	vdogablind_request_t *ring_req;
	vdogablind_response_t *ring_rsp;

	DBG("%d\n", dev->otherend_id);

	while ((ring_req = vdogablind_get_ring_request(&vdogablind_priv->vdogablind.ring)) != NULL) {

		ring_rsp = vdogablind_new_ring_response(&vdogablind_priv->vdogablind.ring);

		memcpy(ring_rsp->buffer, ring_req->buffer, VDOGABLIND_PACKET_SIZE);

		vdogablind_ring_response_ready(&vdogablind_priv->vdogablind.ring);

		notify_remote_via_virq(vdogablind_priv->vdogablind.irq);
	}

	return IRQ_HANDLED;
}

void vdoga_motor_enable() {

	ds1050_recall(); //reactivate the PWM generator
}

void vdoga_motor_disable() {
	
	vdogablind_priv_t *vdogablind_priv = dev_get_drvdata(&vdogablind_dev->dev);
	
	ds1050_shutdown(); //stop the PWM generator	
	gpiod_set_value(vdogablind_priv->sleep_gpio, 0); //set the sleep gpio to 1
}

/**
 * speed must be [0:100]
 **/
void vdoga_motor_set_percentage_speed(int speed) {

	if(speed > 100 || speed < 0) {

		return;
	}

	if(speed == 100) {
		
		ds1050_duty_cycle_full();

	} else {

		ds1050_set_duty_cycle(50); //set the PWM duty cycle to 50%
	}
}

/**
 * dir: 0 => down
 * dir: 1 => up
 **/
void vdoga_motor_set_direction(int dir) {
	
	vdogablind_priv_t *vdogablind_priv = dev_get_drvdata(&vdogablind_dev->dev);
	
	if(dir == 0) {

		gpiod_set_value(vdogablind_priv->in1_ph_gpio, 1); //set the way the motor has to go (To be checked)
		gpiod_set_value(vdogablind_priv->sleep_gpio, 1); //set the sleep gpio to 1

	} else if (dir == 1) {

		gpiod_set_value(vdogablind_priv->in1_ph_gpio, 0); //set the way the motor has to go (To be checked)
		gpiod_set_value(vdogablind_priv->sleep_gpio, 1); //set the sleep gpio to 1
	}
}

static int end_of_run_test(void *args) 
{
	vdogablind_priv_t *vdogablind_priv = dev_get_drvdata(&vdogablind_dev->dev);

	while(1)
	{
		//test if the blind has reached the top end of run
		if(gpiod_get_value(vdogablind_priv->up_end_gpio) == 0)
		{
			if(vdogablind_priv->already_up == false)
			{
				printk("%s BLIND IS UP", VDOGABLIND_PREFIX);
				vdogablind_priv->can_move_up = false;
				vdogablind_priv->already_up = true;
				ds1050_shutdown(); //stop the PWM generator
				gpiod_set_value(vdogablind_priv->sleep_gpio, 0); //set the sleep gpio to 0
			}
		}
		else
		{
			vdogablind_priv->can_move_up = true;
			vdogablind_priv->already_up = false;
		}

		//test if the blind has reached the down end of run
		if(gpiod_get_value(vdogablind_priv->down_end_gpio) == 0)
		{
			if(vdogablind_priv->already_down == false)
			{
				printk("%s BLIND IS DOWN", VDOGABLIND_PREFIX);
				vdogablind_priv->can_move_down = false;
				vdogablind_priv->already_down = true;
				ds1050_shutdown(); //stop the PWM generator
				gpiod_set_value(vdogablind_priv->sleep_gpio, 0); //set the sleep gpio to 0
			}
		}
		else
		{
			vdogablind_priv->can_move_down = true;
			vdogablind_priv->already_down = false;
		}
		msleep(100);
	}

	return 0;
}

#if 1
//setup the gpios used for the vdoga backend
static void setup_vdoga_gpios(struct device *dev) {
	int ret;
	vdogablind_priv_t *vdogablind_priv = dev_get_drvdata(dev);

	//gpio used as output to drive the sleep input from the motor driver
	//0 = motor stopped / 1 = motor is able to move
	vdogablind_priv->sleep_gpio = gpiod_get(dev, "sleep", GPIOD_OUT_HIGH);
	// printk("sleep_gpio = 0x%08X\n", vdogablind_priv->sleep_gpio);
	if (IS_ERR(vdogablind_priv->sleep_gpio)) {
		ret = PTR_ERR(vdogablind_priv->sleep_gpio);
		dev_err(dev, "Failed to get SLEEP GPIO: %d\n", ret);
		return;
	}
	gpiod_direction_output(vdogablind_priv->sleep_gpio, 0); 

	//gpio used as output to set the motor driver function mode (default 0)
	vdogablind_priv->mode_gpio = gpiod_get(dev, "mode", GPIOD_OUT_HIGH);
	// printk("mode_gpio = 0x%08X\n", vdogablind_priv->mode_gpio);
	if (IS_ERR(vdogablind_priv->mode_gpio)) {
		ret = PTR_ERR(vdogablind_priv->mode_gpio);
		dev_err(dev, "Failed to get MODE GPIO: %d\n", ret);
		return;
	}
	gpiod_direction_output(vdogablind_priv->mode_gpio, 0);

	//gpio used as output to set the way the motor has to go
	vdogablind_priv->in1_ph_gpio = gpiod_get(dev, "in1_ph", GPIOD_OUT_HIGH);
	// printk("in1_ph_gpio = 0x%08X\n", vdogablind_priv->in1_ph_gpio);
	if (IS_ERR(vdogablind_priv->in1_ph_gpio)) {
		ret = PTR_ERR(vdogablind_priv->in1_ph_gpio);
		dev_err(dev, "Failed to get IN1_PH GPIO: %d\n", ret);
		return;
	}
	gpiod_direction_output(vdogablind_priv->in1_ph_gpio, 0); 

	//gpio used as output to set the way the motor has to go
	vdogablind_priv->in2_ph_gpio = gpiod_get(dev, "in2_ph", GPIOD_OUT_HIGH);
	// printk("in1_ph_gpio = 0x%08X\n", vdogablind_priv->in1_ph_gpio);
	if (IS_ERR(vdogablind_priv->in2_ph_gpio)) {
		ret = PTR_ERR(vdogablind_priv->in2_ph_gpio);
		dev_err(dev, "Failed to get IN2_PH GPIO: %d\n", ret);
		return;
	}
	gpiod_direction_output(vdogablind_priv->in2_ph_gpio, 0); 

	//gpio used as input for the top end of run detector
	vdogablind_priv->up_end_gpio = gpiod_get(dev, "up_end", GPIOD_IN);
	// printk("up_end_gpio = 0x%08X\n", vdogablind_priv->up_end_gpio);
	if (IS_ERR(vdogablind_priv->up_end_gpio)) {
		ret = PTR_ERR(vdogablind_priv->up_end_gpio);
		dev_err(dev, "Failed to get UP_END GPIO: %d\n", ret);
		return;
	}
	gpiod_direction_input(vdogablind_priv->up_end_gpio);

	//gpio used as input for the down end of run detector
	vdogablind_priv->down_end_gpio = gpiod_get(dev, "down_end", GPIOD_IN);
	// printk("down_end_gpio = 0x%08X\n", vdogablind_priv->down_end_gpio);
	if (IS_ERR(vdogablind_priv->down_end_gpio)) {
		ret = PTR_ERR(vdogablind_priv->down_end_gpio);
		dev_err(dev, "Failed to get DOWN_END GPIO: %d\n", ret);
		return;
	}
	gpiod_direction_input(vdogablind_priv->down_end_gpio);
}
#endif



void vdogablind_probe(struct vbus_device *vdev) {
	vdogablind_priv_t *vdogablind_priv;

	vdogablind_priv = kzalloc(sizeof(vdogablind_priv_t), GFP_ATOMIC);
	BUG_ON(!vdogablind_priv);

	vdev->dev.of_node = of_find_compatible_node(NULL, NULL, "vdogablind,backend");

	dev_set_drvdata(&vdev->dev, vdogablind_priv);

	vdogablind_dev = vdev;

	setup_vdoga_gpios(&vdev->dev);

	/* setup default blind configuration*/
	vdogablind_priv->can_move_up = true;
	vdogablind_priv->can_move_down = true;
	vdogablind_priv->already_down = true;
	vdogablind_priv->already_up = true;

	gpiod_set_value(vdogablind_priv->mode_gpio, 0); //set the mode to 0 (H bridge)


	// ds1050_duty_cycle_full();
	ds1050_set_duty_cycle(50); //set the PWM duty cycle to 50%

	//thread used to test the end of run
	kthread_run(end_of_run_test, NULL, "end_of_run_test");

	printk("%s BACKEND PROBE CALLED", VDOGABLIND_PREFIX);

	DBG(VDOGABLIND_PREFIX "Backend probe: %d\n", vdev->otherend_id);
}

void vdogablind_remove(struct vbus_device *vdev) {
	vdogablind_priv_t *vdogablind_priv = dev_get_drvdata(&vdev->dev);

	DBG("%s: freeing the vdogablind structure for %s\n", __func__,vdev->nodename);
	kfree(vdogablind_priv);
}


void vdogablind_close(struct vbus_device *vdev) {
	vdogablind_priv_t *vdogablind_priv = dev_get_drvdata(&vdev->dev);

	DBG(VDOGABLIND_PREFIX "Backend close: %d\n", vdev->otherend_id);

	/*
	 * Free the ring and unbind evtchn.
	 */

	BACK_RING_INIT(&vdogablind_priv->vdogablind.ring, (&vdogablind_priv->vdogablind.ring)->sring, PAGE_SIZE);
	unbind_from_virqhandler(vdogablind_priv->vdogablind.irq, vdev);

	vbus_unmap_ring_vfree(vdev, vdogablind_priv->vdogablind.ring.sring);
	vdogablind_priv->vdogablind.ring.sring = NULL;
}

void vdogablind_suspend(struct vbus_device *vdev) {

	DBG(VDOGABLIND_PREFIX "Backend suspend: %d\n", vdev->otherend_id);
}

void vdogablind_resume(struct vbus_device *vdev) {

	DBG(VDOGABLIND_PREFIX "Backend resume: %d\n", vdev->otherend_id);
}

void vdogablind_reconfigured(struct vbus_device *vdev) {
	int res;
	unsigned long ring_ref;
	unsigned int evtchn;
	vdogablind_sring_t *sring;
	vdogablind_priv_t *vdogablind_priv = dev_get_drvdata(&vdev->dev);

	DBG(VDOGABLIND_PREFIX "Backend reconfigured: %d\n", vdev->otherend_id);

	/*
	 * Set up a ring (shared page & event channel) between the agency and the ME.
	 */

	vbus_gather(VBT_NIL, vdev->otherend, "ring-ref", "%lu", &ring_ref, "ring-evtchn", "%u", &evtchn, NULL);

	DBG("BE: ring-ref=%u, event-channel=%u\n", ring_ref, evtchn);

	res = vbus_map_ring_valloc(vdev, ring_ref, (void **) &sring);
	BUG_ON(res < 0);

	BACK_RING_INIT(&vdogablind_priv->vdogablind.ring, sring, PAGE_SIZE);

	res = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, evtchn, vdogablind_interrupt, NULL, 0, VDOGABLIND_NAME "-backend", vdev);

	BUG_ON(res < 0);

	vdogablind_priv->vdogablind.irq = res;
}

void vdogablind_connected(struct vbus_device *vdev) {

	DBG(VDOGABLIND_PREFIX "Backend connected: %d\n",vdev->otherend_id);
}

#if 0
/*
 * Testing code to analyze the behaviour of the ME during pre-suspend operations.
 */
int generator_fn(void *arg) {
	uint32_t i;

	while (1) {
		msleep(50);

		for (i = 0; i < MAX_DOMAINS; i++) {

			if (!vdogablind_start(i))
				continue;

			vdogablind_ring_response_ready()

			vdogablind_notify(i);

			vdogablind_end(i);
		}
	}

	return 0;
}
#endif

vdrvback_t vdogablinddrv = {
	.probe = vdogablind_probe,
	.remove = vdogablind_remove,
	.close = vdogablind_close,
	.connected = vdogablind_connected,
	.reconfigured = vdogablind_reconfigured,
	.resume = vdogablind_resume,
	.suspend = vdogablind_suspend
};

int vdogablind_init(void) {
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "vdogablind,backend");

	/* Check if DTS has vuihandler enabled */
	if (!of_device_is_available(np))
		return 0;

#if 0
	kthread_run(generator_fn, NULL, "vdogablind-gen");
#endif

	vdevback_init(VDOGABLIND_NAME, &vdogablinddrv);

	return 0;
}

device_initcall(vdogablind_init);
