/*
 * Copyright (C) 2016-2018 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2016 Baptiste Delporte <bonel@bonel.net>
 * Copyright (C) 2022 Mattia Gallacchi <mattia.gallacchi@heig-vd.ch>
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
#include <linux/of_device.h>

#include <linux/gpio/driver.h>
#include <linux/gpio/consumer.h> 

#include <linux/completion.h>

#include <soo/evtchn.h>
#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/uapi/console.h>

#include <stdarg.h>
#include <linux/kthread.h>

#include <soo/vdevback.h>
#include <soo/dev/venocean.h>
#include <soo/device/tcm515.h>

/* Store last ESP3 packet received */
esp3_packet_t *last_packet = NULL;

/* Completions used for synchronization beetween callback and send thread */
DECLARE_COMPLETION(send_data_completion);
DECLARE_COMPLETION(data_sent_completion);

typedef struct {
	venocean_t venocean;
} venocean_priv_t;

/** List of all the connected vbus devices **/
static struct list_head *vdev_list;

/** List of all the domids of the connected vbus devices **/
static struct list_head *domid_list;

/**
 * @brief This function is used as a callback when a new ESP3 packet is received by tcm515.
 * 
 * @param packet New ESP3 packet. Will replace the values of last packet.
 */
void tcm515_callback(esp3_packet_t *packet) {

	DBG(VENOCEAN_PREFIX " New data form tcm515\n");
	if (!packet)
		return;
	
	/*** free memory if packet has not been sent ***/
	if (last_packet) {
		if (last_packet->data)
			kfree(last_packet->data);

		if (last_packet->optional_data)
			kfree(last_packet->optional_data);
		
		kfree(last_packet);
		last_packet = NULL;
	}

	last_packet = kzalloc(sizeof(esp3_packet_t), GFP_KERNEL);
	if (!last_packet) {
		DBG(VENOCEAN_PREFIX " failed to allocate last packet\n");
		goto packet_alloc_err;
	}
	memcpy(&last_packet->header, &packet->header, sizeof(esp3_header_t));

	last_packet->data = kzalloc(last_packet->header.data_len * sizeof(byte), GFP_KERNEL);
	if (!last_packet->data) {
		DBG(VENOCEAN_PREFIX " failed to allocate last packet data\n");
		goto data_alloc_err;
	}
	memcpy(last_packet->data, packet->data, last_packet->header.data_len * sizeof(byte));

	last_packet->optional_data = kzalloc(last_packet->header.optional_len * sizeof(byte), GFP_KERNEL);
	if (!last_packet->optional_data) {
		DBG(VENOCEAN_PREFIX " failed to allocate last packet optional data\n");
		goto optional_alloc_err;
	}
	memcpy(last_packet->optional_data, packet->optional_data, last_packet->header.optional_len * sizeof(byte));

	DBG(VENOCEAN_PREFIX " new esp3 packet received");

	DBG(VENOCEAN_PREFIX " sending data to frontend\n");
	/*** trigger data send ***/
	complete(&send_data_completion);

	/*** wait for data to be sent ***/
	wait_for_completion(&data_sent_completion);

	DBG(VENOCEAN_PREFIX " ready for new data\n");

	return;

	optional_alloc_err:
		kfree(last_packet->data);
	data_alloc_err:
		kfree(last_packet);
	packet_alloc_err:
		last_packet = NULL;
		return;
}

/**
 * @brief This thread function put last packet in the ring. If frontend is not connected packet is just dropped.
 * 			Waits on completion sent by tmc515 callback.
 * 
 * @param data Vbus device
 * @return int 0 for success
 */
static int venocean_send_data_fn(void *data) {
	venocean_priv_t  *venocean_priv;
	venocean_response_t *ring_resp;
	struct vbus_device *vdev;
    domid_priv_t *domid_priv;

	while(!kthread_should_stop()) {
		wait_for_completion(&send_data_completion);

		list_for_each_entry(domid_priv, domid_list, list) {
			vdev = vdevback_get_entry(domid_priv->id, vdev_list);
			vdevback_processing_begin(vdev);
			venocean_priv = dev_get_drvdata(&vdev->dev);

			if (vdev->state == VbusStateConnected){
			
				DBG(VENOCEAN_PREFIX " creating new ring response\n");
				if (last_packet->header.data_len < BUFFER_SIZE - 1) {
					
					ring_resp = venocean_new_ring_response(&venocean_priv->venocean.ring);
					memcpy(ring_resp->buffer, last_packet->data, last_packet->header.data_len);
					ring_resp->len = last_packet->header.data_len;

					DBG(VENOCEAN_PREFIX " data: %s, len: %d\n", last_packet->data, last_packet->header.data_len);
					
					venocean_ring_response_ready(&venocean_priv->venocean.ring);
					notify_remote_via_virq(venocean_priv->venocean.irq);
					
				} else {
					DBG(VENOCEAN_PREFIX " last packet data is too big\n");
				}
			} else {
				DBG(VENOCEAN_PREFIX "fronted not found. Nothing will be sent\n");
			}
			vdevback_processing_end(vdev);
		}


		DBG(VENOCEAN_PREFIX " freeing last packet\n");
		
		kfree(last_packet->optional_data);
		kfree(last_packet->data);
		kfree(last_packet);
		last_packet = NULL;

		complete(&data_sent_completion);
		DBG(VENOCEAN_PREFIX " ring response sent\n");
	}

	return 0;
}

irqreturn_t venocean_interrupt_bh(int irq, void *dev_id) {
	struct vbus_device *vdev = (struct vbus_device *) dev_id;

	vdevback_processing_begin(vdev);

	/*** Do somenthing with the request ***/

	vdevback_processing_end(vdev);

	return IRQ_HANDLED;
}

irqreturn_t venocean_interrupt(int irq, void *dev_id) {
	return IRQ_WAKE_THREAD;
}

void venocean_probe(struct vbus_device *vdev) {
	venocean_priv_t *venocean_priv;
	domid_priv_t *domid_priv;

	domid_priv = kzalloc(sizeof(domid_t), GFP_KERNEL);
    BUG_ON(!domid_priv);

    domid_priv->id = vdev->otherend_id;
    list_add(&domid_priv->list, domid_list);

	venocean_priv = kzalloc(sizeof(venocean_priv_t), GFP_ATOMIC);
	BUG_ON(!venocean_priv);

	dev_set_drvdata(&vdev->dev, venocean_priv);
	vdevback_add_entry(vdev, vdev_list);
	
	DBG(VENOCEAN_PREFIX "Backend probe: %d\n", vdev->otherend_id);
}

void venocean_remove(struct vbus_device *vdev) {
	venocean_priv_t *venocean_priv = dev_get_drvdata(&vdev->dev);
	domid_priv_t *domid_priv;

	/** Remove entry when frontend is leaving **/
	list_for_each_entry(domid_priv, domid_list, list) {
		if (domid_priv->id == vdev->otherend_id) {
				list_del(&domid_priv->list);
				kfree(domid_priv);
				break;
		}
	}

	DBG("%s: freeing the venocean structure for %s\n", __func__,vdev->nodename);
	vdevback_del_entry(vdev, vdev_list);
	kfree(venocean_priv);

	DBG(VENOCEAN_PREFIX "Removed: %d\n", id);
}

void venocean_close(struct vbus_device *vdev) {
	venocean_priv_t *venocean_priv = dev_get_drvdata(&vdev->dev);

	DBG(VENOCEAN_PREFIX " Backend close: %d\n", vdev->otherend_id);

	/*
	 * Free the ring
	 */

	BACK_RING_INIT(&venocean_priv->venocean.ring, (&venocean_priv->venocean.ring)->sring, PAGE_SIZE);
	
	/* Unbind the irq */
	unbind_from_virqhandler(venocean_priv->venocean.irq, vdev);

	vbus_unmap_ring_vfree(vdev, venocean_priv->venocean.ring.sring);
	venocean_priv->venocean.ring.sring = NULL;
}

void venocean_suspend(struct vbus_device *vdev) {

	DBG(VENOCEAN_PREFIX " Backend suspend: %d\n", vdev->otherend_id);
}

void venocean_resume(struct vbus_device *vdev) {

	DBG(VENOCEAN_PREFIX " Backend resume: %d\n", vdev->otherend_id);
}

void venocean_reconfigured(struct vbus_device *vdev) {
	int res;
	unsigned long ring_ref;
	unsigned int evtchn;
	venocean_sring_t *sring;
	venocean_priv_t *venocean_priv = dev_get_drvdata(&vdev->dev);

	DBG(VENOCEAN_PREFIX "Backend reconfigured: %d\n", vdev->otherend_id);

	/*
	 * Set up a ring (shared page & event channel) between the agency and the ME.
	 */

	vbus_gather(VBT_NIL, vdev->otherend, "ring-ref", "%lu", &ring_ref, "ring-evtchn", "%u", &evtchn, NULL);

	DBG(VENOCEAN_PREFIX "BE: ring-ref=%lu, event-channel=%d\n", ring_ref, evtchn);

	vbus_map_ring_valloc(vdev, ring_ref, (void **) &sring);

	BACK_RING_INIT(&venocean_priv->venocean.ring, sring, PAGE_SIZE);

	/* No handler required, however used to notify the remote domain */
	res = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, evtchn, venocean_interrupt,
													venocean_interrupt_bh, 0, VENOCEAN_NAME "-backend", vdev);

	BUG_ON(res < 0);

	venocean_priv->venocean.irq = res;
}

void venocean_connected(struct vbus_device *vdev) {

	DBG(VENOCEAN_PREFIX "Backend connected: %d\n",vdev->otherend_id);
}

vdrvback_t venoceandrv = {
	.probe = venocean_probe,
	.remove = venocean_remove,
	.close = venocean_close,
	.connected = venocean_connected,
	.reconfigured = venocean_reconfigured,
	.resume = venocean_resume,
	.suspend = venocean_suspend
};

static int venocean_init(void) {
	struct device_node *np;

	DBG(VENOCEAN_PREFIX " start initialization\n");

	np = of_find_compatible_node(NULL, NULL, "venocean,backend");

	/* Check if DTS has venocean enabled */
	if (!of_device_is_available(np)) {
		DBG(VENOCEAN_PREFIX " is disabled");
		return -1;
	}

	vdev_list = (struct list_head *)kzalloc(sizeof(struct list_head), GFP_ATOMIC);
    BUG_ON(!vdev_list);

    INIT_LIST_HEAD(vdev_list);

    domid_list = (struct list_head *)kzalloc(sizeof(struct list_head), GFP_KERNEL);
    BUG_ON(!domid_list);

	INIT_LIST_HEAD(domid_list);

	vdevback_init(VENOCEAN_NAME, &venoceandrv);

	/* Add callback function to the list of callback of tcm515-serdev */
	if (tcm515_subscribe(tcm515_callback) < 0) {
		DBG(VENOCEAN_PREFIX " failed to subscribe to tcm515");
		BUG();
	}

		/* Modify once we have multiple FE **/
	kthread_run(venocean_send_data_fn, NULL, "send_data_fn");

	pr_info(VENOCEAN_PREFIX " Initialized successfully\n");

	return 0;
}
device_initcall(venocean_init);
