/*
 * Copyright (C) 2023 A.Gabriel Catel Torres <arzur.cateltorres@heig-vd.ch>
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

#if 1
#define DEBUG
#endif

#include <linux/of.h>

#include <soo/evtchn.h>

#include <soo/dev/viuoc.h>

/** List of all the connected vbus devices **/
static struct list_head *vdev_list;

/** List of all the domids of the connected vbus devices **/
static struct list_head *domid_list;

typedef struct {
	/* Must be the first field */
	viuoc_t viuoc;

} viuoc_priv_t;

irqreturn_t viuoc_interrupt_bh(int irq, void *dev_id) {
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	viuoc_priv_t *viuoc_priv = dev_get_drvdata(&vdev->dev);
	viuoc_request_t *ring_req;

	vdevback_processing_begin(vdev);

	while ((ring_req = viuoc_get_ring_request(&viuoc_priv->viuoc.ring)) != NULL) {
		DBG("[IUOC back] IRQ received element from RING\n");
		add_iuoc_element_to_queue(ring_req->me_data);
	}
		
	vdevback_processing_end(vdev);

	return IRQ_HANDLED;
}

irqreturn_t viuoc_interrupt(int irq, void *dev_id) {
	return IRQ_WAKE_THREAD;
}

void viuoc_send_data_to_fe(iuoc_data_t iuoc_data) {
	viuoc_priv_t  *viuoc_priv;
	viuoc_response_t *ring_resp;
    struct vbus_device *vdev;
    domid_priv_t *domid_priv;
	viuoc_response_t *res;

    res = kzalloc(sizeof(viuoc_response_t), GFP_KERNEL);
    BUG_ON(!res);

	res->me_data = iuoc_data;

	DBG("[IUOC back] Sending data to FE\n");

	list_for_each_entry(domid_priv, domid_list, list) {
		//DBG(VIUOC_PREFIX "Sending data to frontend: %d\n", domid_priv->id);
		
		vdev = vdevback_get_entry(domid_priv->id, vdev_list);
		vdevback_processing_begin(vdev);
		viuoc_priv = dev_get_drvdata(&vdev->dev);
		
		ring_resp = viuoc_new_ring_response(&viuoc_priv->viuoc.ring);
		memcpy(ring_resp, res, sizeof(viuoc_response_t));
		viuoc_ring_response_ready(&viuoc_priv->viuoc.ring);
		notify_remote_via_virq(viuoc_priv->viuoc.irq);

		vdevback_processing_end(vdev);
	}

	DBG("[IUOC back] Data sent to FE\n");
}

void viuoc_probe(struct vbus_device *vdev) {
	viuoc_priv_t *viuoc_priv;
	domid_priv_t *domid_priv;

	domid_priv = kzalloc(sizeof(domid_t), GFP_KERNEL);
	BUG_ON(!domid_priv);

	domid_priv->id = vdev->otherend_id;
	list_add(&domid_priv->list, domid_list);

	viuoc_priv = kzalloc(sizeof(viuoc_priv_t), GFP_ATOMIC);
	BUG_ON(!viuoc_priv);

	dev_set_drvdata(&vdev->dev, viuoc_priv);

	vdevback_add_entry(vdev, vdev_list);

	DBG(VIUOC_PREFIX "Probe: %d\n", vdev->otherend_id);
}

void viuoc_remove(struct vbus_device *vdev) {
	viuoc_priv_t *viuoc_priv = dev_get_drvdata(&vdev->dev);

	DBG("%s: freeing the viuoc structure for %s\n", __func__,vdev->nodename);
	kfree(viuoc_priv);
}

void viuoc_close(struct vbus_device *vdev) {
	viuoc_priv_t *viuoc_priv = dev_get_drvdata(&vdev->dev);

	DBG(VIUOC_PREFIX "Close: %d\n", vdev->otherend_id);

	/*
	 * Free the ring.
	 */

	BACK_RING_INIT(&viuoc_priv->viuoc.ring, (&viuoc_priv->viuoc.ring)->sring, PAGE_SIZE);

	/* Unbind the irq */
	unbind_from_virqhandler(viuoc_priv->viuoc.irq, vdev);

	vbus_unmap_ring_vfree(vdev, viuoc_priv->viuoc.ring.sring);
	viuoc_priv->viuoc.ring.sring = NULL;
}

void viuoc_suspend(struct vbus_device *vdev) {

	DBG(VIUOC_PREFIX "Suspend: %d\n", vdev->otherend_id);
}

void viuoc_resume(struct vbus_device *vdev) {

	DBG(VIUOC_PREFIX "Resume: %d\n", vdev->otherend_id);
}

void viuoc_reconfigured(struct vbus_device *vdev) {
	int res;
	unsigned long ring_ref;
	unsigned int evtchn;
	viuoc_sring_t *sring;
	viuoc_priv_t *viuoc_priv = dev_get_drvdata(&vdev->dev);

	DBG(VIUOC_PREFIX "Reconfigured: %d\n", vdev->otherend_id);

	/*
	 * Set up a ring (shared page & event channel) between the agency and the ME.
	 */

	vbus_gather(VBT_NIL, vdev->otherend, "ring-ref", "%lu", &ring_ref, "ring-evtchn", "%u", &evtchn, NULL);

	DBG(VIUOC_PREFIX "BE: ring-ref=%ld, event-channel=%d\n", ring_ref, evtchn);

	vbus_map_ring_valloc(vdev, ring_ref, (void **) &sring);

	BACK_RING_INIT(&viuoc_priv->viuoc.ring, sring, PAGE_SIZE);

	/* No handler required, however used to notify the remote domain */
	res = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, evtchn, viuoc_interrupt, 
													viuoc_interrupt_bh, 0, VIUOC_NAME "-backend", vdev);
	BUG_ON(res < 0);

	viuoc_priv->viuoc.irq = res;
}

void viuoc_connected(struct vbus_device *vdev) {

	DBG(VIUOC_PREFIX "Connected: %d\n",vdev->otherend_id);
}


vdrvback_t viuocdrv = {
	.probe = viuoc_probe,
	.remove = viuoc_remove,
	.close = viuoc_close,
	.connected = viuoc_connected,
	.reconfigured = viuoc_reconfigured,
	.resume = viuoc_resume,
	.suspend = viuoc_suspend
};

int viuoc_init(void) {
	struct device_node *np;

    DBG(VIUOC_PREFIX "Starting\n");

	np = of_find_compatible_node(NULL, NULL, "viuoc,backend");

	/* Check if DTS has viuoc enabled */
	if (!of_device_is_available(np))
		return 0;

	vdev_list = (struct list_head *)kzalloc(sizeof(struct list_head), GFP_ATOMIC);
    BUG_ON(!vdev_list);

    INIT_LIST_HEAD(vdev_list);

    domid_list = (struct list_head *)kzalloc(sizeof(struct list_head), GFP_KERNEL);
    BUG_ON(!domid_list);

    INIT_LIST_HEAD(domid_list);

	vdevback_init(VIUOC_NAME, &viuocdrv);

    DBG(VIUOC_PREFIX "Initialized\n");
	
    return 0;
}

device_initcall(viuoc_init);
