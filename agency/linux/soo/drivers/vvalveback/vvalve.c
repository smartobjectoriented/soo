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
#include <linux/completion.h>

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
#include <soo/dev/vvalve.h>
#include <linux/gpio/driver.h>
#include <linux/gpio/consumer.h> 

// extern ssize_t soo_heat_base_write_cmd(char *buffer, ssize_t len);
// extern int soo_heat_base_open(const char *port);

typedef struct {

	/* Must be the first field */
	vvalve_t vvalve;

	// struct completion wait_cmd;
	// struct completion wait_send_id;

	vvalve_desc_t vvalve_desc;

	struct gpio_desc *heat_gpio;

} vvalve_priv_t;

// /* Completions used for synchronization beetween callback and send thread */
DECLARE_COMPLETION(wait_cmd);
DECLARE_COMPLETION(wait_send_id);

/** List of all the connected vbus devices **/
static struct list_head *vdev_list;

/** List of all the domids of the connected vbus devices **/
static struct list_head *domid_list;


irqreturn_t vvalve_interrupt(int irq, void *dev_id)
{
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vvalve_priv_t *vvalve_priv = dev_get_drvdata(&vdev->dev);
	vvalve_request_t *ring_req;
	vvalve_response_t *ring_rsp;

	DBG("%d\n", vdev->otherend_id);

	while ((ring_req = vvalve_get_ring_request(&vvalve_priv->vvalve.ring)) != NULL) {

		ring_rsp = vvalve_new_ring_response(&vvalve_priv->vvalve.ring);

		switch (ring_req->action) {
		case VALVE_ACTION_CMD_VALVE:
			vvalve_priv->vvalve_desc.cmd_valve = ring_req->cmd_valve;
			// complete(&vvalve_priv->wait_cmd);
			complete(&wait_cmd);
			break;
		case VALVE_ACTION_ASK_ID:
			// complete(&vvalve_priv->wait_send_id);
			complete(&wait_send_id);
			break;
		default:
			BUG();
		}
	}

	return IRQ_HANDLED;
}


//called to open the valve
void vanalog_valve_open(void *arg) {
	vvalve_priv_t *vvalve_priv = (vvalve_priv_t *) arg;

	DBG(VVALVE_PREFIX "The valve is opened\n"); //print in kernel
	gpiod_set_value(vvalve_priv->heat_gpio, 1); //set the heat gpio to 1
}


//called to close the valve
void vanalog_valve_close(void *arg) {
	vvalve_priv_t *vvalve_priv = (vvalve_priv_t *) arg;

	DBG(VVALVE_PREFIX "The valve is closed\n"); //print in kernel
	gpiod_set_value(vvalve_priv->heat_gpio, 0); //set the heat gpio to 0
}


/**
 * @brief Allow ME to get ID of this SOO.heat Object
 **/
static int send_id_task_fn(void *arg) {

	vvalve_priv_t *vvalve_priv = (vvalve_priv_t *) arg;
	vvalve_response_t *ring_rsp;

	while(1) {
		// wait_for_completion(&vvalve_priv->wait_send_id);
		wait_for_completion(&wait_send_id);
		/* send self ID */
		ring_rsp = vvalve_new_ring_response(&vvalve_priv->vvalve.ring);

		ring_rsp->dev_id = vvalve_priv->vvalve_desc.dev_id;

		vvalve_ring_response_ready(&vvalve_priv->vvalve.ring);

		notify_remote_via_virq(vvalve_priv->vvalve.irq);		

	}

	return 0;	
}


/**
 * @brief Allow ME to get ID of this SOO.heat Object
 **/
static int cmd_valve_task_fn(void *arg) {

	vvalve_priv_t *vvalve_priv = (vvalve_priv_t *) arg;
#if 0
	while(1){
		vanalog_valve_open(vvalve_priv);
		msleep(2000);
		vanalog_valve_close(vvalve_priv);
		msleep(2000);
	}
#endif
	while(1) {
		// wait_for_completion(&vvalve_priv->wait_cmd);
		wait_for_completion(&wait_cmd);
		switch (vvalve_priv->vvalve_desc.cmd_valve)	{
		case VALVE_CMD_OPEN:
			printk("%s Call valve open\n", VVALVE_PREFIX);
			vanalog_valve_open(vvalve_priv);
			break;

		case VALVE_CMD_CLOSE:
			printk("%s Call valve close\n", VVALVE_PREFIX);
			vanalog_valve_close(vvalve_priv);
			break;
		
		default:
			break;
		}
	}

	return 0;	
}


//setup the gpios used for the vanalog backend
static void setup_vanalog_gpios(struct device *dev) {
	int ret;
	vvalve_priv_t *vvalve_priv = dev_get_drvdata(dev);
	
	//gpio used as output to open or close the valve 
	vvalve_priv->heat_gpio = gpiod_get(dev, "valve", GPIOD_OUT_LOW);

	if (IS_ERR(vvalve_priv->heat_gpio)) {
		ret = PTR_ERR(vvalve_priv->heat_gpio);
		dev_err(dev, "Failed to get HEAT GPIO: %d\n", ret);
		return;
	}

	DBG(VVALVE_PREFIX "Vavle setted\n"); 
}


void vvalve_probe(struct vbus_device *vdev) {
	int ret;
	const char *port_name = "ttyAMA5";
	struct device_node *np = vdev->dev.of_node;
	vvalve_priv_t *vvalve_priv;
	domid_priv_t *domid_priv;

	DBG(VVALVE_PREFIX "BACKEND PROBE CALLED\n");

	domid_priv = kzalloc(sizeof(domid_t), GFP_KERNEL);
    BUG_ON(!domid_priv);

    domid_priv->id = vdev->otherend_id;
    list_add(&domid_priv->list, domid_list);

	vvalve_priv = kzalloc(sizeof(vvalve_priv_t), GFP_ATOMIC);
	BUG_ON(!vvalve_priv);

	vdev->dev.of_node = of_find_compatible_node(NULL, NULL, "vvalve,backend");

	/* GET TTY PORT FROM DTB */
	ret = of_property_read_string(np, "tty_port", &port_name);

	if(ret == -EINVAL) {
		printk("%s property tty_port doesn't exist !\n", VVALVE_PREFIX);

	} else if (ret == -ENODATA) {
		printk("%s property port is null !\n", VVALVE_PREFIX);
	}

	dev_set_drvdata(&vdev->dev, vvalve_priv);
	vdevback_add_entry(vdev, vdev_list);

	setup_vanalog_gpios(&vdev->dev);

	kthread_run(send_id_task_fn, (void *)vvalve_priv, "send_id_task");
	kthread_run(cmd_valve_task_fn, (void *)vvalve_priv, "cmd_valve_task");

	DBG(VVALVE_PREFIX "Backend probed: %d\n", vdev->otherend_id);
}


void vvalve_remove(struct vbus_device *vdev) {
	vvalve_priv_t *vvalve_priv = dev_get_drvdata(&vdev->dev);
	domid_priv_t *domid_priv;

	/** Remove entry when frontend is leaving **/
	list_for_each_entry(domid_priv, domid_list, list) {
		if (domid_priv->id == vdev->otherend_id) {
				list_del(&domid_priv->list);
				kfree(domid_priv);
				break;
		}
	}

	DBG("%s: freeing the valve structure for %s\n", __func__,vdev->nodename);
	vdevback_del_entry(vdev, vdev_list);
	kfree(vvalve_priv);

	DBG(VVALVE_PREFIX "Backend removed: %d\n", vdev->otherend_id);
}


void vvalve_close(struct vbus_device *vdev) {
	vvalve_priv_t *vvalve_priv = dev_get_drvdata(&vdev->dev);

	DBG(VVALVE_PREFIX "Backend close: %d\n", vdev->otherend_id);

	/*
	 * Free the ring
	 */
	BACK_RING_INIT(&vvalve_priv->vvalve.ring, (&vvalve_priv->vvalve.ring)->sring, PAGE_SIZE);
	
	/* Unbind the irq */
	unbind_from_virqhandler(vvalve_priv->vvalve.irq, vdev);

	vbus_unmap_ring_vfree(vdev, vvalve_priv->vvalve.ring.sring);
	vvalve_priv->vvalve.ring.sring = NULL;
}


void vvalve_suspend(struct vbus_device *vdev) {

	DBG(VVALVE_PREFIX "Backend suspend: %d\n", vdev->otherend_id);
}


void vvalve_resume(struct vbus_device *vdev) {

	DBG(VVALVE_PREFIX "Backend resume: %d\n", vdev->otherend_id);
}


void vvalve_reconfigured(struct vbus_device *vdev) {
	int res;
	unsigned long ring_ref;
	unsigned int evtchn;
	vvalve_sring_t *sring;
	vvalve_priv_t *vvalve_priv = dev_get_drvdata(&vdev->dev);
	
	DBG(VVALVE_PREFIX "Backend reconfigured: %d\n",vdev->otherend_id);

	/*
	 * Set up a ring (shared page & event channel) between the agency and the ME.
	 */

	vbus_gather(VBT_NIL, vdev->otherend, "ring-ref", "%lu", &ring_ref, "ring-evtchn", "%u", &evtchn, NULL);

	DBG(VVALVE_PREFIX "BE: ring-ref=%lu, event-channel=%d\n", ring_ref, evtchn);

	vbus_map_ring_valloc(vdev, ring_ref, (void **) &sring);

	BACK_RING_INIT(&vvalve_priv->vvalve.ring, sring, PAGE_SIZE);

	/* No handler required, however used to notify the remote domain */
	res = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, evtchn, vvalve_interrupt, NULL, 0, VVALVE_NAME "-backend", vdev);

	BUG_ON(res < 0);

	vvalve_priv->vvalve.irq = res;
}


void vvalve_connected(struct vbus_device *vdev) {

	DBG(VVALVE_PREFIX "Backend connected: %d\n",vdev->otherend_id);
}


vdrvback_t vvalvedrv = {
	.probe = vvalve_probe,
	.remove = vvalve_remove,
	.close = vvalve_close,
	.connected = vvalve_connected,
	.reconfigured = vvalve_reconfigured,
	.resume = vvalve_resume,
	.suspend = vvalve_suspend
};


int vvalve_init(void) {
	struct device_node *np;

	DBG(VVALVE_PREFIX " start initialization\n");

	DBG("[ %s ] BACKEND INIT CALLED", VVALVE_NAME);

	np = of_find_compatible_node(NULL, NULL, "vvalve,backend");

	/* Check if DTS has vvalve enabled */
	if (!of_device_is_available(np)) {
		DBG(VVALVE_PREFIX " is disabled");
		return -1;
	}

	vdev_list = (struct list_head *)kzalloc(sizeof(struct list_head), GFP_ATOMIC);
    BUG_ON(!vdev_list);

    INIT_LIST_HEAD(vdev_list);

	domid_list = (struct list_head *)kzalloc(sizeof(struct list_head), GFP_KERNEL);
    BUG_ON(!domid_list);

	INIT_LIST_HEAD(domid_list);

	vdevback_init(VVALVE_NAME, &vvalvedrv);

	return 0;
}

device_initcall(vvalve_init);
