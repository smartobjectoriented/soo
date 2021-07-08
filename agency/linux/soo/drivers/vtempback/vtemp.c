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
#include <asm/termios.h>
#include <linux/serial_core.h>
#include <soo/dev/vtemp.h>
#include <linux/tty.h>

extern int soo_heat_base_open(void);
extern ssize_t soo_heat_base_read_temp(char *buffer);
extern void soo_heat_base_set_lora_rx(void);



typedef struct {

	/* Must be the first field */
	vtemp_t vtemp;

	/* contains data receives from LoRa */
	vtemp_data_t vtemp_data;

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

void send_data_to_front(vtemp_priv_t *vtemp_priv) {

	vtemp_response_t *ring_rsp;

	ring_rsp = vtemp_new_ring_response(&vtemp_priv->vtemp.ring);

	ring_rsp->temp = vtemp_priv->vtemp_data.temp;
	ring_rsp->dev_id = vtemp_priv->vtemp_data.dev_id;
	ring_rsp->dev_type = vtemp_priv->vtemp_data.dev_type;

	vtemp_ring_response_ready(&vtemp_priv->vtemp.ring);

	notify_remote_via_virq(vtemp_priv->vtemp.irq);
}

static int lora_monitor_fn(void *args) {


	char temp_char[TEMP_BLOCK_SIZE+1];
	char dev_id_char[DEV_ID_BLOCK_SIZE+1];
	char dev_type_char[DEV_TYPE_BLOCK_SIZE+1];

	char buffer[18];
    int  lora_padding = 10;

	vtemp_priv_t *vtemp_priv = (vtemp_priv_t *)args;

	soo_heat_base_set_lora_rx();

	while(true) {

		/* block until useful data from LoRa */
		soo_heat_base_read_temp(buffer);

		// if readed buffer contains radio_tx
		if(strstr(buffer, "radio_rx") != NULL) {

			/* copy temperature into local buffer*/
			memcpy(
				temp_char,
				buffer + lora_padding,
				TEMP_BLOCK_SIZE
			);
			temp_char[TEMP_BLOCK_SIZE] = '\0';
			// printk("%s temp : %s", VTEMP_PREFIX, temp_char);

			/* copy dev_id */
			memcpy(
				dev_id_char,
				buffer + lora_padding + TEMP_BLOCK_SIZE,
				DEV_ID_BLOCK_SIZE
			);
			dev_id_char[DEV_ID_BLOCK_SIZE] = '\0';
			// printk("%s dev_id : %s", VTEMP_PREFIX, dev_id_char);

			/* copy dev_type */
			memcpy(
				dev_type_char,
				buffer + lora_padding + TEMP_BLOCK_SIZE + DEV_ID_BLOCK_SIZE,
				DEV_TYPE_BLOCK_SIZE
			);
			dev_type_char[DEV_TYPE_BLOCK_SIZE] = '\0';
			// printk("%s dev_type : %s", VTEMP_PREFIX, dev_type_char);

			/* update local data */
			if(kstrtol(temp_char, 10, (long *)(&(vtemp_priv->vtemp_data.temp))) != 0){
				printk("%s ERROR CONV STR TO L tmeperature\n", VTEMP_PREFIX);

			}
			if(kstrtol(dev_id_char, 10, (long *)(&(vtemp_priv->vtemp_data.dev_id))) != 0){
				printk("%s ERROR CONV STR TO L dev_id\n", VTEMP_PREFIX);

			}
			if(kstrtol(dev_type_char, 10, (long *)(&(vtemp_priv->vtemp_data.dev_type))) != 0){
				printk("%s ERROR CONV STR TO L dev_type\n", VTEMP_PREFIX);

			}
			
			/* calculate real temperature */
			vtemp_priv->vtemp_data.temp /= 4;

			printk("%s DEV_ID: %d, DEV_TYPE: %d, TEMP: %d \n", VTEMP_PREFIX,
																vtemp_priv->vtemp_data.dev_id,
																vtemp_priv->vtemp_data.dev_type,
																vtemp_priv->vtemp_data.temp);
		}

		/* Send data to frontend */
		send_data_to_front(vtemp_priv);
	}

	return 0;
}

irqreturn_t vtemp_interrupt(int irq, void *dev_id)
{
	// struct vbus_device *vdev = (struct vbus_device *) dev_id;
	// vtemp_priv_t *vtemp_priv = dev_get_drvdata(&vdev->dev);
	// vtemp_request_t *ring_req;
	// vtemp_response_t *ring_rsp;

	// DBG("%d\n", dev->otherend_id);

	// while ((ring_req = vtemp_get_ring_request(&vtemp_priv->vtemp.ring)) != NULL) {

	// 	ring_rsp = vtemp_new_ring_response(&vtemp_priv->vtemp.ring);

	// 	memcpy(ring_rsp->buffer, ring_req->buffer, VTEMP_PACKET_SIZE);

	// 	vtemp_ring_response_ready(&vtemp_priv->vtemp.ring);

	// 	notify_remote_via_virq(vtemp_priv->vtemp.irq);
	// }

	return IRQ_HANDLED;
}

void vtemp_probe(struct vbus_device *vdev) {
	vtemp_priv_t *vtemp_priv;

	printk("%s BACKEND PROBE V2 CALLED\n", VTEMP_PREFIX);

	vtemp_priv = kzalloc(sizeof(vtemp_priv_t), GFP_ATOMIC);
	BUG_ON(!vtemp_priv);

	dev_set_drvdata(&vdev->dev, vtemp_priv);

	vtemp_dev = vdev;

#if 1
	kthread_run(lora_monitor_fn, (void *)vtemp_priv, "lora_monitor");
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
