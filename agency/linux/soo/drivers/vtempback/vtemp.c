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

extern int hts221_get_temperature(void);


/* Completions used for synchronization beetween callback and send thread */
DECLARE_COMPLETION(wait_read_data_temp_completion);

typedef struct {
	/* Must be the first field */
	vtemp_t vtemp;
} vtemp_priv_t;

/** List of all the connected vbus devices **/
static struct list_head *vdev_list;

/** List of all the domids of the connected vbus devices **/
static struct list_head *domid_list;

int read_temperature(void) {
	return hts221_get_temperature();
}


static int send_temp_to_front(void *args){

	vtemp_priv_t *vtemp_priv;
	vtemp_response_t *ring_rsp;
	struct vbus_device *vdev;
    domid_priv_t *domid_priv;
	int temp;
	int test = 0;

	while(1) {
		
		DBG(VTEMP_PREFIX "wait for completion BE temp\n");
		/* wait notification from FE before sending temperature */
		wait_for_completion(&wait_read_data_temp_completion);
		temp = read_temperature();
		DBG(VTEMP_PREFIX "%d tempSensor BE\n", read_temperature());

		list_for_each_entry(domid_priv, domid_list, list) {
			vdev = vdevback_get_entry(domid_priv->id, vdev_list);
			vdevback_processing_begin(vdev);
			vtemp_priv = dev_get_drvdata(&vdev->dev);

			if (vdev->state == VbusStateConnected){
				DBG(VTEMP_PREFIX "create new ring BE\n");
				ring_rsp = vtemp_new_ring_response(&vtemp_priv->vtemp.ring);
				memcpy(ring_rsp->buffer, &temp, sizeof(byte));
				ring_rsp->len = sizeof(byte);

				vtemp_ring_response_ready(&vtemp_priv->vtemp.ring);
				DBG(VTEMP_PREFIX "Notify irq BE\n");
				DBG(VTEMP_PREFIX "%d vtemp.irq BE\n", vtemp_priv->vtemp.irq);
				notify_remote_via_virq(vtemp_priv->vtemp.irq);
			}else{
				DBG(VTEMP_PREFIX "frontend not found. Nothing will be sent BE\n");
			}
			vdevback_processing_end(vdev);
		}
		DBG(VTEMP_PREFIX "ring response sent BE\n");	
	}

	return 0;
}

irqreturn_t vtemp_interrupt_bh(int irq, void *dev_id)
{
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vtemp_priv_t *vtemp_priv = dev_get_drvdata(&vdev->dev);
	
	vdevback_processing_begin(vdev);
	complete(&wait_read_data_temp_completion);

	DBG(VTEMP_PREFIX "irq handler BE\n");
	DBG(VTEMP_PREFIX "%d vtemp.irq BE\n", vtemp_priv->vtemp.irq);
	vdevback_processing_end(vdev);
	return IRQ_HANDLED;
}

irqreturn_t vtemp_interrupt(int irq, void *dev_id)
{
	return IRQ_WAKE_THREAD;
}


void vtemp_probe(struct vbus_device *vdev) {
	
	int ret;
	vtemp_priv_t *vtemp_priv;
	domid_priv_t *domid_priv;

	pr_info(VTEMP_PREFIX " Starting Probe BE\n");

	domid_priv = kzalloc(sizeof(domid_t), GFP_KERNEL);
	BUG_ON(!domid_priv);

	domid_priv->id = vdev->otherend_id;
    list_add(&domid_priv->list, domid_list);

	vtemp_priv = kzalloc(sizeof(vtemp_priv_t), GFP_ATOMIC);
	BUG_ON(!vtemp_priv);

	dev_set_drvdata(&vdev->dev, vtemp_priv);
	vdevback_add_entry(vdev, vdev_list);

	DBG(VTEMP_PREFIX "Backend probe: %d BE\n", vdev->otherend_id);
	pr_info(VTEMP_PREFIX " Initialized probe successfully BE\n");
}


void vtemp_remove(struct vbus_device *vdev) {
	vtemp_priv_t *vtemp_priv = dev_get_drvdata(&vdev->dev);
	domid_priv_t *domid_priv;

	DBG("%s: freeing the vtemp structure for %s BE\n", __func__,vdev->nodename);
	/** Remove entry when frontend is leaving **/
	// list_for_each_entry(domid_priv, domid_list, list) {
	// 	if (domid_priv->id == vdev->otherend_id) {
	// 			list_del(&domid_priv->list);
	// 			kfree(domid_priv);
	// 			break;
	// 	}
	DBG("%s: freeing the vtemp structure for %s BE\n", __func__,vdev->nodename);
	vdevback_del_entry(vdev, vdev_list);
	kfree(vtemp_priv);

	DBG(VTEMP_PREFIX "Removed: %d BE\n", vdev->otherend_id);
}


void vtemp_close(struct vbus_device *vdev) {
	vtemp_priv_t *vtemp_priv = dev_get_drvdata(&vdev->dev);

	DBG(VTEMP_PREFIX "Backend close: %d BE\n", vdev->otherend_id);

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

	DBG(VTEMP_PREFIX "Backend reconfigured: %d BE\n", vdev->otherend_id);

	/*
	 * Set up a ring (shared page & event channel) between the agency and the ME.
	 */

	vbus_gather(VBT_NIL, vdev->otherend, "ring-ref", "%lu", &ring_ref, "ring-evtchn", "%u", &evtchn, NULL);

	DBG(VTEMP_PREFIX "BE: ring-ref=%u, event-channel=%u BE\n", ring_ref, evtchn);

	vbus_map_ring_valloc(vdev, ring_ref, (void **) &sring);

	BACK_RING_INIT(&vtemp_priv->vtemp.ring, sring, PAGE_SIZE);

	res = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, evtchn, vtemp_interrupt, vtemp_interrupt_bh, 0, VTEMP_NAME "-backend", vdev);

	BUG_ON(res < 0);
	DBG(VTEMP_PREFIX "%d res BE\n");
	vtemp_priv->vtemp.irq = res;
}


void vtemp_connected(struct vbus_device *vdev) {

	DBG(VTEMP_PREFIX "Backend connected: %d BE\n",vdev->otherend_id);
}


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

	DBG(VTEMP_PREFIX " start initialization BE\n");
	printk("%s start initialization BE\n", VTEMP_PREFIX);

	np = of_find_compatible_node(NULL, NULL, "vtemp,backend");

	/* Check if DTS has vuihandler enabled */
	if (!of_device_is_available(np)){
		DBG(VTEMP_PREFIX " is disabled BE");
		return 0;
	}

	vdev_list = (struct list_head *)kzalloc(sizeof(struct list_head), GFP_ATOMIC);
    BUG_ON(!vdev_list);

    INIT_LIST_HEAD(vdev_list);

	domid_list = (struct list_head *)kzalloc(sizeof(struct list_head), GFP_KERNEL);
    BUG_ON(!domid_list);

	INIT_LIST_HEAD(domid_list);

	vdevback_init(VTEMP_NAME, &vtempdrv);

	kthread_run(send_temp_to_front, NULL, "send_data_fn");

	printk("[ %s ] BACKEND INIT CALLED !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!", VTEMP_NAME);

	pr_info(VTEMP_PREFIX " Initialized successfully\n");

	return 0;
}

device_initcall(vtemp_init);
