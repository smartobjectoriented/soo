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

#include <linux/gpio/driver.h>
#include <linux/gpio/consumer.h> 

#include <stdarg.h>
#include <linux/kthread.h>

#include <soo/vdevback.h>

#include <soo/dev/vdoga.h>

static struct gpio_desc *sleep_gpio;
static struct gpio_desc *mode_gpio;
static struct gpio_desc *in1_ph_gpio;

static struct gpio_desc *up_end_gpio;
static struct gpio_desc *down_end_gpio;

/* DS1050 exported functions */
extern void ds1050_set_duty_cycle(uint8_t duty_cycle);
extern void ds1050_shutdown(void);
extern void ds1050_recall(void);

bool can_move_up = true;
bool can_move_down = true;
bool already_down = true;
bool already_up = true;

void vdoga_notify(struct vbus_device *vdev)
{
	vdoga_t *vdoga = to_vdoga(vdev);

	RING_PUSH_RESPONSES(&vdoga->ring);

	/* Send a notification to the frontend only if connected.
	 * Otherwise, the data remain present in the ring. */

	notify_remote_via_virq(vdoga->irq);
}

static int end_of_run_test(void *args) 
{
	while(1)
	{
		//test if the blind has reached the top end of run
		if(gpiod_get_value(up_end_gpio) == 0)
		{
			if(already_up == false)
			{
				can_move_up = false;
				already_up = true;
				ds1050_shutdown(); //stop the PWM generator
				gpiod_set_value(sleep_gpio, 0); //set the sleep gpio to 0
			}
		}
		else
		{
			can_move_up = true;
			already_up = false;
		}

		//test if the blind has reached the down end of run
		if(gpiod_get_value(down_end_gpio) == 0)
		{
			if(already_down == false)
			{
				can_move_down = false;
				already_down = true;
				ds1050_shutdown(); //stop the PWM generator
				gpiod_set_value(sleep_gpio, 0); //set the sleep gpio to 0
			}
		}
		else
		{
			can_move_down = true;
			already_down = false;
		}
		msleep(100);
	}
}

//called when the blind has to go up
void vdoga_blind_up(void)
{
	if(can_move_up == true)
	{
		printk("The blind goes up"); //print in kernel
		ds1050_recall(); //reactivate the PWM generator
		gpiod_set_value(in1_ph_gpio, 0); //set the way the motor has to go (To be checked)
		gpiod_set_value(sleep_gpio, 1); //set the sleep gpio to 1
	}
	else
	{
		printk("The blind is already up");	
	}
	
}

//called when the blind has to go down
void vdoga_blind_down(void)
{
	//test if the blind can go down
	if(can_move_down == true)
	{
		printk("The blind goes down"); //print in kernel
		ds1050_recall(); //reactivate the PWM generator
		gpiod_set_value(in1_ph_gpio, 1); //set the way the motor has to go (To be checked)
		gpiod_set_value(sleep_gpio, 1); //set the sleep gpio to 1
	}
	else
	{
		printk("The blind is already down");	
	}
	
}

//called when the blind has to stop
void vdoga_blind_stop(void)
{
	printk("The blind is stopped"); //print in kernel
	ds1050_shutdown(); //stop the PWM generator
	gpiod_set_value(sleep_gpio, 0); //set the sleep gpio to 0
}

irqreturn_t vdoga_interrupt(int irq, void *dev_id)
{
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vdoga_t *vdoga = to_vdoga(vdev);
	vdoga_request_t *ring_req;
	vdoga_response_t *ring_rsp;

	DBG("%d\n", dev->otherend_id);

	while ((ring_req = vdoga_ring_request(&vdoga->ring)) != NULL) {

		ring_rsp = vdoga_ring_response(&vdoga->ring);

		memcpy(ring_rsp->buffer, ring_req->buffer, VDOGA_PACKET_SIZE);

		vdoga_ring_response_ready(&vdoga->ring);

		notify_remote_via_virq(vdoga->irq);
	}

	return IRQ_HANDLED;
}

//setup the gpios used for the vdoga backend
static void setup_vdoga_gpios(struct device *dev) {
	int ret;

	//gpio used as output to drive the sleep input from the motor driver
	//0 = motor stopped / 1 = motor is able to move
	sleep_gpio = gpiod_get(dev, "sleep", GPIOD_OUT_HIGH);
	printk("sleep_gpio = 0x%08X\n", sleep_gpio);
	if (IS_ERR(sleep_gpio)) {
		ret = PTR_ERR(sleep_gpio);
		dev_err(dev, "Failed to get SLEEP GPIO: %d\n", ret);
		return;
	}
	gpiod_direction_output(sleep_gpio, 0); 

	//gpio used as output to set the motor driver function mode (default 0)
	mode_gpio = gpiod_get(dev, "mode", GPIOD_OUT_HIGH);
	printk("mode_gpio = 0x%08X\n", mode_gpio);
	if (IS_ERR(mode_gpio)) {
		ret = PTR_ERR(mode_gpio);
		dev_err(dev, "Failed to get MODE GPIO: %d\n", ret);
		return;
	}
	gpiod_direction_output(mode_gpio, 0);

	//gpio used as output to set the way the motor has to go
	in1_ph_gpio = gpiod_get(dev, "in1_ph", GPIOD_OUT_HIGH);
	printk("in1_ph_gpio = 0x%08X\n", in1_ph_gpio);
	if (IS_ERR(in1_ph_gpio)) {
		ret = PTR_ERR(in1_ph_gpio);
		dev_err(dev, "Failed to get IN1_PH GPIO: %d\n", ret);
		return;
	}
	gpiod_direction_output(in1_ph_gpio, 0); 

	//gpio used as input for the top end of run detector
	up_end_gpio = gpiod_get(dev, "up_end", GPIOD_IN);
	printk("up_end_gpio = 0x%08X\n", up_end_gpio);
	if (IS_ERR(up_end_gpio)) {
		ret = PTR_ERR(up_end_gpio);
		dev_err(dev, "Failed to get UP_END GPIO: %d\n", ret);
		return;
	}
	gpiod_direction_input(up_end_gpio);

	//gpio used as input for the down end of run detector
	down_end_gpio = gpiod_get(dev, "down_end", GPIOD_IN);
	printk("down_end_gpio = 0x%08X\n", down_end_gpio);
	if (IS_ERR(down_end_gpio)) {
		ret = PTR_ERR(down_end_gpio);
		dev_err(dev, "Failed to get DOWN_END GPIO: %d\n", ret);
		return;
	}
	gpiod_direction_input(down_end_gpio);
}

void vdoga_probe(struct vbus_device *vdev) {
	vdoga_t *vdoga;

	vdoga = kzalloc(sizeof(vdoga_t), GFP_ATOMIC);
	BUG_ON(!vdoga);

	vdev->dev.of_node = of_find_compatible_node(NULL, NULL, "vdoga,backend");

	dev_set_drvdata(&vdev->dev, &vdoga->vdevback);

	setup_vdoga_gpios(&vdev->dev);

	ds1050_set_duty_cycle(50); //set the PWM duty cycle to 50%

	//thread used to test the end of run
	kthread_run(end_of_run_test, NULL, "end_of_run_test");

	DBG(VDOGA_PREFIX "Backend probe: %d\n", vdev->otherend_id);
}


void vdoga_remove(struct vbus_device *vdev) {
	vdoga_t *vdoga = to_vdoga(vdev);

	DBG("%s: freeing the vdoga structure for %s\n", __func__,vdev->nodename);
	kfree(vdoga);
}


void vdoga_close(struct vbus_device *vdev) {
	vdoga_t *vdoga = to_vdoga(vdev);

	DBG(VDOGA_PREFIX "Backend close: %d\n", vdev->otherend_id);

	/*
	 * Free the ring and unbind evtchn.
	 */

	BACK_RING_INIT(&vdoga->ring, (&vdoga->ring)->sring, PAGE_SIZE);
	unbind_from_virqhandler(vdoga->irq, vdev);

	vbus_unmap_ring_vfree(vdev, vdoga->ring.sring);
	vdoga->ring.sring = NULL;
}

void vdoga_suspend(struct vbus_device *vdev) {

	DBG(VDOGA_PREFIX "Backend suspend: %d\n", vdev->otherend_id);
}

void vdoga_resume(struct vbus_device *vdev) {

	DBG(VDOGA_PREFIX "Backend resume: %d\n", vdev->otherend_id);
}

void vdoga_reconfigured(struct vbus_device *vdev) {
	int res;
	unsigned long ring_ref;
	unsigned int evtchn;
	vdoga_sring_t *sring;
	vdoga_t *vdoga = to_vdoga(vdev);

	DBG(VDOGA_PREFIX "Backend reconfigured: %d\n", vdev->otherend_id);

	/*
	 * Set up a ring (shared page & event channel) between the agency and the ME.
	 */

	vbus_gather(VBT_NIL, vdev->otherend, "ring-ref", "%lu", &ring_ref, "ring-evtchn", "%u", &evtchn, NULL);

	DBG("BE: ring-ref=%u, event-channel=%u\n", ring_ref, evtchn);

	res = vbus_map_ring_valloc(vdev, ring_ref, (void **) &sring);
	BUG_ON(res < 0);

	SHARED_RING_INIT(sring);
	BACK_RING_INIT(&vdoga->ring, sring, PAGE_SIZE);

	res = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, evtchn, vdoga_interrupt, NULL, 0, VDOGA_NAME "-backend", vdev);

	BUG_ON(res < 0);

	vdoga->irq = res;
}

void vdoga_connected(struct vbus_device *vdev) {

	DBG(VDOGA_PREFIX "Backend connected: %d\n",vdev->otherend_id);
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

			if (!vdummy_start(i))
				continue;

			vdummy_ring_response_ready()
			vdummy_notify(i);

			vdummy_end(i);
		}
	}

	return 0;
}
#endif

vdrvback_t vdogadrv = {
	.probe = vdoga_probe,
	.remove = vdoga_remove,
	.close = vdoga_close,
	.connected = vdoga_connected,
	.reconfigured = vdoga_reconfigured,
	.resume = vdoga_resume,
	.suspend = vdoga_suspend
};

int vdoga_init(void) {
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "vdoga,backend");

	/* Check if DTS has vuihandler enabled */
	if (!of_device_is_available(np))
		return 0;

#if 0
	kthread_run(generator_fn, NULL, "vDummy-gen");
#endif

	vdevback_init(VDOGA_NAME, &vdogadrv);

	return 0;
}

device_initcall(vdoga_init);
