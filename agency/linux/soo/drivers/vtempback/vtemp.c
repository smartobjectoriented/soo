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

#include <linux/delay.h>
#include <linux/string.h>
#include <soo/dev/vtemp.h>


extern int hts221_get_temperature(void);


typedef struct {

	/* Must be the first field */
	vtemp_t vtemp;

	/* contains data receives from LoRa */
	int last_temp;

	struct completion wait_send_temp;

} vtemp_priv_t;

static struct vbus_device *vtemp_dev = NULL;

void vtemp_notify(struct vbus_device *vdev)
{
	vtemp_priv_t *vtemp_priv = dev_get_drvdata(&vdev->dev);

	vtemp_ring_response_ready(&vtemp_priv->vtemp.ring);

	/* Send a notification to the frontend only if connected.
	 * Otherwise, the data remain present in the ring. */

	notify_remote_via_virq(vtemp_priv->vtemp.irq);
}


int read_temperature(void) {

	return hts221_get_temperature();
}

static int send_temp_to_front(void *args) {

	vtemp_priv_t *vtemp_priv = (vtemp_priv_t *)args;
	vtemp_response_t *ring_rsp;

	printk(VTEMP_PREFIX "send temperature to FE\n");

	while(1) {
		
		
		/* wait notification from FE before sending temperature */
		wait_for_completion(&vtemp_priv->wait_send_temp);

		ring_rsp = vtemp_new_ring_response(&vtemp_priv->vtemp.ring);

		/* Read temperature using hts221_core driver */
		vtemp_priv->last_temp = read_temperature();

		printk(VTEMP_PREFIX "TEMP FROM SENSE HAT = %d\n", vtemp_priv->last_temp);

		ring_rsp->temp = vtemp_priv->last_temp;
		ring_rsp->dev_id = TEMP_DEV_ID;

		vtemp_ring_response_ready(&vtemp_priv->vtemp.ring);

		notify_remote_via_virq(vtemp_priv->vtemp.irq);
	}

	return 0;
}

irqreturn_t vtemp_interrupt(int irq, void *dev_id)
{
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vtemp_priv_t *vtemp_priv = dev_get_drvdata(&vdev->dev);
	
	complete(&vtemp_priv->wait_send_temp);

	return IRQ_HANDLED;
}



void vtemp_probe(struct vbus_device *vdev) {
	
	int ret;
	vtemp_priv_t *vtemp_priv;


	printk("%s BACKEND PROBE V2 CALLED\n", VTEMP_PREFIX);

	/* SETUP PRIVATE STRUCTURE */
	vtemp_priv = kzalloc(sizeof(vtemp_priv_t), GFP_ATOMIC);
	BUG_ON(!vtemp_priv);

	dev_set_drvdata(&vdev->dev, vtemp_priv);

	vtemp_dev = vdev;
	
	vtemp_priv->last_temp = 22;

	init_completion(&vtemp_priv->wait_send_temp);

#if 1
	kthread_run(send_temp_to_front, (void *)vtemp_priv, "lora_monitor");
#endif


	DBG(VTEMP_PREFIX "Backend probe: %d\n", vdev->otherend_id);
}

void vtemp_remove(struct vbus_device *vdev) {
	vtemp_priv_t *vtemp_priv = dev_get_drvdata(&vdev->dev);

	DBG("%s: freeing the vtemp structure for %s\n", __func__,vdev->nodename);

	
	kfree(vtemp_priv);
}


void vtemp_close(struct vbus_device *vdev) {
	vtemp_priv_t *vtemp_priv = dev_get_drvdata(&vdev->dev);

	DBG(VTEMP_PREFIX "Backend close: %d\n", vdev->otherend_id);


	/*
	 * Free the ring and unbind evtchn.
	 */
	BACK_RING_INIT(&vtemp_priv->vtemp.ring, (&vtemp_priv->vtemp.ring)->sring, PAGE_SIZE);
	unbind_from_virqhandler(vtemp_priv->vtemp.irq, vdev);

	vbus_unmap_ring_vfree(vdev, vtemp_priv->vtemp.ring.sring);
	vtemp_priv->vtemp.ring.sring = NULL;
}

void vtemp_suspend(struct vbus_device *vdev) {

	DBG(VTEMP_PREFIX "Backend suspend: %d\n", vdev->otherend_id);
}

void vtemp_resume(struct vbus_device *vdev) {

	DBG(VTEMP_PREFIX "Backend resume: %d\n", vdev->otherend_id);
}

void vtemp_reconfigured(struct vbus_device *vdev) {
	int res;
	unsigned long ring_ref;
	unsigned int evtchn;
	vtemp_sring_t *sring;
	vtemp_priv_t *vtemp_priv = dev_get_drvdata(&vdev->dev);

	DBG(VTEMP_PREFIX "Backend reconfigured: %d\n", vdev->otherend_id);

	/*
	 * Set up a ring (shared page & event channel) between the agency and the ME.
	 */

	vbus_gather(VBT_NIL, vdev->otherend, "ring-ref", "%lu", &ring_ref, "ring-evtchn", "%u", &evtchn, NULL);

	DBG("BE: ring-ref=%u, event-channel=%u\n", ring_ref, evtchn);

	res = vbus_map_ring_valloc(vdev, ring_ref, (void **) &sring);
	BUG_ON(res < 0);

	BACK_RING_INIT(&vtemp_priv->vtemp.ring, sring, PAGE_SIZE);

	res = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, evtchn, vtemp_interrupt, NULL, 0, VTEMP_NAME "-backend", vdev);

	BUG_ON(res < 0);

	vtemp_priv->vtemp.irq = res;
}

void vtemp_connected(struct vbus_device *vdev) {

	DBG(VTEMP_PREFIX "Backend connected: %d\n",vdev->otherend_id);
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

			if (!vtemp_start(i))
				continue;

			vtemp_ring_response_ready()

			vtemp_notify(i);

			vtemp_end(i);
		}
	}

	return 0;
}
#endif

vdrvback_t vtempdrv = {
	.probe = vtemp_probe,
	.remove = vtemp_remove,
	.close = vtemp_close,
	.connected = vtemp_connected,
	.reconfigured = vtemp_reconfigured,
	.resume = vtemp_resume,
	.suspend = vtemp_suspend
};

int vtemp_init(void) {
	struct device_node *np;

#if 0
	vtemp_priv_t *vtemp_priv;
	vtemp_priv = kzalloc(sizeof(vtemp_priv_t), GFP_ATOMIC);
#endif


	/* TODO: Change to vtemp -> edit DTS*/
	np = of_find_compatible_node(NULL, NULL, "vtemp,backend");

	/* Check if DTS has vuihandler enabled */
	if (!of_device_is_available(np))
		return 0;

#if 0
	kthread_run(lora_monitor_fn_te, NULL, "lora_monitor");
#endif

	printk("[ %s ] BACKEND INIT CALLED", VTEMP_NAME);

	vdevback_init(VTEMP_NAME, &vtempdrv);

	return 0;
}

device_initcall(vtemp_init);
