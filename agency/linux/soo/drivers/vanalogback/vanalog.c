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

#include <soo/dev/vanalog.h>

static struct gpio_desc *heat_gpio;

//called to open the valve
void vanalog_valve_open(void)
{
	printk("The valve is opened"); //print in kernel
	gpiod_set_value(heat_gpio, 1); //set the heat gpio to 1
}

//called to close the valve
void vanalog_valve_close(void)
{
	printk("The valve is closed"); //print in kernel
	gpiod_set_value(heat_gpio, 0); //set the heat gpio to 0
}

void vanalog_notify(struct vbus_device *vdev)
{
	vanalog_t *vanalog = to_vanalog(vdev);

	RING_PUSH_RESPONSES(&vanalog->ring);

	/* Send a notification to the frontend only if connected.
	 * Otherwise, the data remain present in the ring. */

	notify_remote_via_virq(vanalog->irq);

}

irqreturn_t vanalog_interrupt(int irq, void *dev_id)
{
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vanalog_t *vanalog = to_vanalog(vdev);
	vanalog_request_t *ring_req;
	vanalog_response_t *ring_rsp;

	DBG("%d\n", dev->otherend_id);

	while ((ring_req = vanalog_ring_request(&vanalog->ring)) != NULL) {

		ring_rsp = vanalog_ring_response(&vanalog->ring);

		memcpy(ring_rsp->buffer, ring_req->buffer, VANALOG_PACKET_SIZE);

		vanalog_ring_response_ready(&vanalog->ring);

		notify_remote_via_virq(vanalog->irq);
	}

	return IRQ_HANDLED;
}

//setup the gpios used for the vanalog backend
static void setup_vanalog_gpios(struct device *dev) {
	int ret;

	//gpio used as output to open or close the valve 
	heat_gpio = gpiod_get(dev, "heat", GPIOD_OUT_HIGH);
	printk("heat_gpio = 0x%08X\n", heat_gpio);
	if (IS_ERR(heat_gpio)) {
		ret = PTR_ERR(heat_gpio);
		dev_err(dev, "Failed to get HEAT GPIO: %d\n", ret);
		return;
	}
	gpiod_direction_output(heat_gpio, 0); 
}

void vanalog_probe(struct vbus_device *vdev) {
	vanalog_t *vanalog;

	vanalog = kzalloc(sizeof(vanalog_t), GFP_ATOMIC);
	BUG_ON(!vanalog);

	vdev->dev.of_node = of_find_compatible_node(NULL, NULL, "vanalog,backend");

	dev_set_drvdata(&vdev->dev, &vanalog->vdevback);

	setup_vanalog_gpios(&vdev->dev);

	DBG(VANALOG_PREFIX "Backend probe: %d\n", vdev->otherend_id);
}

void vanalog_remove(struct vbus_device *vdev) {
	vanalog_t *vanalog = to_vanalog(vdev);

	DBG("%s: freeing the vanalog structure for %s\n", __func__,vdev->nodename);
	kfree(vanalog);
}


void vanalog_close(struct vbus_device *vdev) {
	vanalog_t *vanalog = to_vanalog(vdev);

	DBG(VANALOG_PREFIX "Backend close: %d\n", vdev->otherend_id);

	/*
	 * Free the ring and unbind evtchn.
	 */

	BACK_RING_INIT(&vanalog->ring, (&vanalog->ring)->sring, PAGE_SIZE);
	unbind_from_virqhandler(vanalog->irq, vdev);

	vbus_unmap_ring_vfree(vdev, vanalog->ring.sring);
	vanalog->ring.sring = NULL;
}

void vanalog_suspend(struct vbus_device *vdev) {

	DBG(VANALOG_PREFIX "Backend suspend: %d\n", vdev->otherend_id);
}

void vanalog_resume(struct vbus_device *vdev) {

	DBG(VANALOG_PREFIX "Backend resume: %d\n", vdev->otherend_id);
}

void vanalog_reconfigured(struct vbus_device *vdev) {
	int res;
	unsigned long ring_ref;
	unsigned int evtchn;
	vanalog_sring_t *sring;
	vanalog_t *vanalog = to_vanalog(vdev);

	DBG(VANALOG_PREFIX "Backend reconfigured: %d\n", vdev->otherend_id);

	/*
	 * Set up a ring (shared page & event channel) between the agency and the ME.
	 */

	vbus_gather(VBT_NIL, vdev->otherend, "ring-ref", "%lu", &ring_ref, "ring-evtchn", "%u", &evtchn, NULL);

	DBG("BE: ring-ref=%u, event-channel=%u\n", ring_ref, evtchn);

	res = vbus_map_ring_valloc(vdev, ring_ref, (void **) &sring);
	BUG_ON(res < 0);

	SHARED_RING_INIT(sring);
	BACK_RING_INIT(&vanalog->ring, sring, PAGE_SIZE);

	res = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, evtchn, vanalog_interrupt, NULL, 0, VANALOG_NAME "-backend", vdev);

	BUG_ON(res < 0);

	vanalog->irq = res;
}

void vanalog_connected(struct vbus_device *vdev) {

	DBG(VANALOG_PREFIX "Backend connected: %d\n",vdev->otherend_id);
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

vdrvback_t vanalogdrv = {
	.probe = vanalog_probe,
	.remove = vanalog_remove,
	.close = vanalog_close,
	.connected = vanalog_connected,
	.reconfigured = vanalog_reconfigured,
	.resume = vanalog_resume,
	.suspend = vanalog_suspend
};

int vanalog_init(void) {
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "vanalog,backend");

	/* Check if DTS has vuihandler enabled */
	if (!of_device_is_available(np))
		return 0;

#if 0
	kthread_run(generator_fn, NULL, "vDummy-gen");
#endif

	vdevback_init(VANALOG_NAME, &vanalogdrv);

	return 0;
}

device_initcall(vanalog_init);
