/*
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

#include <linux/of.h>
#include <linux/kthread.h>
#include <linux/slab.h>

#include <soo/evtchn.h>
#include <soo/dev/vknx.h>

#include <soo/vdevback.h>
#include <soo/device/baos_client.h>

typedef struct {
	/* Must be the first field */
	vknx_t vknx;
    vknx_request_t *req;

} vknx_priv_t;

static vknx_response_t *indication = NULL;

DECLARE_COMPLETION(send_data);
DECLARE_COMPLETION(data_sent);

/** List of all the connected vbus devices **/
static struct list_head *vdev_list;

/** List of all the domids of the connected vbus devices **/
static struct list_head *domid_list;

struct mutex list_mutex;

#if 0
static void vknx_print_request(vknx_request_t *req) {
    int i, j;

    DBG(VKNX_PREFIX "Request type: %d\n", req->type);
    DBG(VKNX_PREFIX "dp count %d\n", req->dp_count);

    for (i = 0 ; i < req->dp_count; i++) {
        DBG(VKNX_PREFIX "Datapoints:\n");
        DBG(VKNX_PREFIX "ID: 0x%02X\n", req->datapoints[i].id);
        DBG(VKNX_PREFIX "cmd/state: 0x%02X\n", req->datapoints[i].cmd);
        DBG(VKNX_PREFIX "Data length: %d\n", req->datapoints[i].data_len);
        DBG(VKNX_PREFIX "Data:\n");
        for (j = 0; j < req->datapoints[i].data_len; j++) {
            DBG(VKNX_PREFIX "[%d]: 0x%02X\n", j, req->datapoints[i].data[j]);
        }
    }
}
#endif

static vknx_response_t *vknx_baos_to_response(baos_frame_t *frame) {
    vknx_response_t *res;
    int i;

    res = kzalloc(sizeof(vknx_response_t), GFP_KERNEL);
    BUG_ON(!res);

    res->dp_count = frame->obj_count.val;
    for (i = 0; i < res->dp_count; i++) {
        res->datapoints[i].id = frame->datapoints[i]->id.val;
        res->datapoints[i].state = frame->datapoints[i]->state;
        res->datapoints[i].data_len = frame->datapoints[i]->length;
        memcpy(res->datapoints[i].data, frame->datapoints[i]->data, res->datapoints[i].data_len);
    }

    return res;
}

static baos_datapoint_t *vknx_request_to_baos(vknx_request_t *req) {
    baos_datapoint_t *datapoints;
    int i;

    datapoints = kzalloc(sizeof(baos_datapoint_t) * req->dp_count, GFP_KERNEL);
    BUG_ON(!datapoints);

    for (i = 0; i < req->dp_count; i++) {
        datapoints[i].id.val = req->datapoints[i].id;
        datapoints[i].command = req->datapoints[i].cmd;
        datapoints[i].length = req->datapoints[i].data_len;
        datapoints[i].data = kzalloc(datapoints[i].length * sizeof(byte), GFP_KERNEL);
        BUG_ON(!datapoints[i].data);

        memcpy(datapoints[i].data, req->datapoints[i].data, datapoints[i].length);
    }

    return datapoints;
}

void vknx_baos_indication_process(baos_frame_t *frame) {
    DBG(VKNX_PREFIX "Got a new indication:\n");

    indication = vknx_baos_to_response(frame);
    indication->event = KNX_INDICATION;

    complete(&send_data);
    wait_for_completion(&data_sent);

    kfree(indication);

    indication = NULL;
}

static void vknx_get_dp_value(void *data) {
	struct vbus_device *vdev = (struct vbus_device *)data;
	vknx_priv_t *vknx_priv = dev_get_drvdata(&vdev->dev);
	vknx_request_t *req = vknx_priv->req;
	vknx_response_t *res;
	vknx_response_t *ring_resp;
	baos_frame_t *frame;

	BUG_ON(!req->datapoints);

	frame = baos_get_datapoint_value(req->datapoints[0].id, req->dp_count);
	res = vknx_baos_to_response(frame);
	res->event = KNX_RESPONSE;

	vdevback_processing_begin(vdev);

	ring_resp = vknx_new_ring_response(&vknx_priv->vknx.ring);
	memcpy(ring_resp, res, sizeof(vknx_response_t));
	vknx_ring_response_ready(&vknx_priv->vknx.ring);
	notify_remote_via_virq(vknx_priv->vknx.irq);

	vdevback_processing_end(vdev);

	baos_free_frame(frame);
	kfree(res->datapoints);
	kfree(res);
}

static void vknx_set_dp_value(vknx_request_t *req) {
	baos_datapoint_t *datapoints;

	datapoints = vknx_request_to_baos(req);
	BUG_ON(!datapoints);

	baos_set_datapoint_value(datapoints, req->dp_count);

	kfree(datapoints);
}

static int vknx_send_indication_fn(void *data) {
    vknx_priv_t *vknx_priv;
    vknx_response_t *ring_resp;
    struct vbus_device *vdev;
    domid_priv_t *domid_priv;

    while(!kthread_should_stop()) {
        wait_for_completion(&send_data);

        list_for_each_entry(domid_priv, domid_list, list) {
            DBG(VKNX_PREFIX "Sending data to frontend: %d\n", domid_priv->id);
            
            vdev = vdevback_get_entry(domid_priv->id, vdev_list);
            vdevback_processing_begin(vdev);
            vknx_priv = dev_get_drvdata(&vdev->dev);
            
            ring_resp = vknx_new_ring_response(&vknx_priv->vknx.ring);
            memcpy(ring_resp, indication, sizeof(vknx_response_t));
            vknx_ring_response_ready(&vknx_priv->vknx.ring);
            notify_remote_via_virq(vknx_priv->vknx.irq);

            vdevback_processing_end(vdev);
        }

        DBG(VKNX_PREFIX "Data sent\n");

        complete(&data_sent);
    }

    return 0;
}

irqreturn_t vknx_interrupt_bh(int irq, void *dev_id) {
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vknx_priv_t *vknx_priv = dev_get_drvdata(&vdev->dev);
	vknx_request_t *ring_req;

	vdevback_processing_begin(vdev);

	DBG(VKNX_PREFIX "New data from frontend: %d\n", vdev->otherend_id);

	while ((ring_req = vknx_get_ring_request(&vknx_priv->vknx.ring)) != NULL) {
		switch(ring_req->type) {
		case GET_DP_VALUE:
			DBG(VKNX_PREFIX "Getting datapoint values\n");
			vknx_priv->req = ring_req;
			vknx_get_dp_value(vdev);
			break;

		case SET_DP_VALUE:
			DBG(VKNX_PREFIX "Setting datapoint values\n");
			vknx_set_dp_value(ring_req);
			break;
		}
	}

	vdevback_processing_end(vdev);

	return IRQ_HANDLED;
}

irqreturn_t vknx_interrupt(int irq, void *dev_id) {
	return IRQ_WAKE_THREAD;
}

void vknx_probe(struct vbus_device *vdev) {
	vknx_priv_t *vknx_priv;
	domid_priv_t *domid_priv;

	DBG(VKNX_PREFIX "Probe: %d\n", vdev->otherend_id);

	domid_priv = kzalloc(sizeof(domid_t), GFP_KERNEL);
	BUG_ON(!domid_priv);

	domid_priv->id = vdev->otherend_id;
	list_add(&domid_priv->list, domid_list);

	vknx_priv = kzalloc(sizeof(vknx_priv_t), GFP_KERNEL);
	BUG_ON(!vknx_priv);

	dev_set_drvdata(&vdev->dev, vknx_priv);

	vdevback_add_entry(vdev, vdev_list);

	DBG(VKNX_PREFIX "Probed: %d\n", vdev->otherend_id);
}

void vknx_remove(struct vbus_device *vdev) {
	vknx_priv_t *vknx_priv = dev_get_drvdata(&vdev->dev);
	domid_priv_t *domid_priv;
	int32_t id = vdev->otherend_id;

	DBG(VKNX_PREFIX "Remove: %d\n", id);

	/** Remove entry when frontend is leaving **/
	list_for_each_entry(domid_priv, domid_list, list) {
		if (domid_priv->id == id) {
			list_del(&domid_priv->list);
			kfree(domid_priv);
			break;
		}
	}

	DBG("%s: freeing the vknx structure for %s\n", __func__,vdev->nodename);
	vdevback_del_entry(vdev, vdev_list);
	kfree(vknx_priv);

	DBG(VKNX_PREFIX "Removed: %d\n", id);
}

void vknx_close(struct vbus_device *vdev) {
	vknx_priv_t *vknx_priv = dev_get_drvdata(&vdev->dev);

	DBG(VKNX_PREFIX "Close: %d\n", vdev->otherend_id);

	/*
	 * Free the ring.
	 */
	BACK_RING_INIT(&vknx_priv->vknx.ring, (&vknx_priv->vknx.ring)->sring, PAGE_SIZE);

	/* Unbind the irq */
	unbind_from_virqhandler(vknx_priv->vknx.irq, vdev);

	vbus_unmap_ring_vfree(vdev, vknx_priv->vknx.ring.sring);
	vknx_priv->vknx.ring.sring = NULL;

    DBG(VKNX_PREFIX "Closed: %d\n", vdev->otherend_id);
}

void vknx_suspend(struct vbus_device *vdev) {

	DBG(VKNX_PREFIX "Suspend: %d\n", vdev->otherend_id);
}

void vknx_resume(struct vbus_device *vdev) {

	DBG(VKNX_PREFIX "Resume: %d\n", vdev->otherend_id);
}

void vknx_reconfigured(struct vbus_device *vdev) {
	int res;
	unsigned long ring_ref;
	unsigned int evtchn;
	vknx_sring_t *sring;
	vknx_priv_t *vknx_priv = dev_get_drvdata(&vdev->dev);

	DBG(VKNX_PREFIX "Reconfigured: %d\n", vdev->otherend_id);

	/*
	 * Set up a ring (shared page & event channel) between the agency and the ME.
	 */
	vbus_gather(VBT_NIL, vdev->otherend, "ring-ref", "%lu", &ring_ref, "ring-evtchn", "%u", &evtchn, NULL);

	DBG(VKNX_PREFIX "BE: ring-ref=%ld, event-channel=%d\n", ring_ref, evtchn);

	vbus_map_ring_valloc(vdev, ring_ref, (void **) &sring);

	BACK_RING_INIT(&vknx_priv->vknx.ring, sring, PAGE_SIZE);

	/* No handler required, however used to notify the remote domain */
	res = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, evtchn, vknx_interrupt, 
													vknx_interrupt_bh, 0, VKNX_NAME "-backend", vdev);
	BUG_ON(res < 0);

	vknx_priv->vknx.irq = res;
}

void vknx_connected(struct vbus_device *vdev) {

	DBG(VKNX_PREFIX "Connected: %d\n",vdev->otherend_id);
}

vdrvback_t vknxdrv = {
	.probe = vknx_probe,
	.remove = vknx_remove,
	.close = vknx_close,
	.connected = vknx_connected,
	.reconfigured = vknx_reconfigured,
	.resume = vknx_resume,
	.suspend = vknx_suspend
};

int vknx_init(void) {
	struct device_node *np;

    DBG(VKNX_PREFIX "Starting\n");

	np = of_find_compatible_node(NULL, NULL, "vknx,backend");

	/* Check if DTS has vknx enabled */
	if (!of_device_is_available(np))
		return 0;

    vdev_list = (struct list_head *)kzalloc(sizeof(struct list_head), GFP_ATOMIC);
    BUG_ON(!vdev_list);

    INIT_LIST_HEAD(vdev_list);

    domid_list = (struct list_head *)kzalloc(sizeof(struct list_head), GFP_KERNEL);
    BUG_ON(!domid_list);

    INIT_LIST_HEAD(domid_list);

	vdevback_init(VKNX_NAME, &vknxdrv);

    baos_client_subscribe_to_indications(vknx_baos_indication_process);

    // init_completion(&data_sent);
    // init_completion(&send_data);

    kthread_run(vknx_send_indication_fn, NULL, "send_indication_fn");

    DBG(VKNX_PREFIX "Initialized\n");
	
    return 0;
}

device_initcall(vknx_init);
