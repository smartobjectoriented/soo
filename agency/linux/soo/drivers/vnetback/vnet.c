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
#include <linux/ethtool.h>
#include <linux/netdevice.h>


#include <soo/evtchn.h>
#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/uapi/console.h>

#include <stdarg.h>
#include <linux/kthread.h>

#include <soo/vdevback.h>

#include <soo/dev/vnetif.h>
#include <soo/dev/vnet.h>
#include <soo/dev/vnetbuff.h>
#include <soo/dev/vnetif.h>

#include "vnetbridge_priv.h"


struct net_device *net_devs[MAX_ME_DOMAINS];
bool net_devs_initialized = false;

void initialize_net_devs(void){
	int i = 0;
	//vnetbridge_add(SOO_BRIDGE_NAME);
	//vnetbridge_add_if(SOO_BRIDGE_NAME, "eth0");
	vnetbridge_if_conf(SOO_BRIDGE_NAME, IFF_PROMISC, 1);
	while(i < MAX_ME_DOMAINS){
		net_devs[i] = vnetif_init(i);
		i++;
	}

	//vnetbridge_add_if(SOO_BRIDGE_NAME, "vif0");

	net_devs_initialized = true;
}

void vnet_notify(struct vbus_device *vdev)
{
	vnet_t *vnet = to_vnet(vdev);

	RING_PUSH_RESPONSES(&vnet->ring_rx);

	/* Send a notification to the frontend only if connected.
	 * Otherwise, the data remain present in the ring. */

	notify_remote_via_virq(vnet->irq);

}

void vnet_process_ctrl(struct vbus_device *vdev){
	vnet_request_t *ring_req;
	vnet_response_t *ring_rsp;
	vnet_t *vnet = to_vnet(vdev);


	while ((ring_req = vnet_ctrl_ring_request(&vnet->ring_ctrl)) != NULL) {
		switch(ring_req->type){
		case ETHADDR:
			/* TODo improve, store the vif in a list */
			/*if (vnet->vif == NULL)
				vnet->vif = vnetif_init(0, ring_rsp->ethaddr);*/
			break;
		}
	}
}

void vnet_process_tx(struct vbus_device *vdev){
	vnet_request_t *ring_req = NULL;
	uint8_t* data;
	vnet_t *vnet = to_vnet(vdev);

	while ((ring_req = vnet_tx_ring_request(&vnet->ring_tx)) != NULL) {
		data = vbuff_get(&vnet->vbuff_tx, &ring_req->buff);
		//vbuff_print(&vnet->vbuff_tx, &ring_req->buff);
		netif_rx_packet(net_devs[vdev->otherend_id - 2], data, ring_req->buff.size);
	}
}

void vnet_send_rx(void* void_vnet, u8* data, int length){
	vnet_response_t *ring_rsp;
	struct vbuff_data vbuff_data;
	vnet_t* vnet = (vnet_t*)void_vnet;

	vbuff_put(&vnet->vbuff_rx, &vbuff_data, &data, length);

	if ((ring_rsp = vnet_rx_ring_response(&vnet->ring_rx)) != NULL) {
		ring_rsp->buff = vbuff_data;
		vnet_rx_ring_response_ready(&vnet->ring_rx);
		notify_remote_via_virq(vnet->irq);
	}
}


irqreturn_t vnet_interrupt(int irq, void *dev_id)
{
	struct vbus_device *vdev = (struct vbus_device *) dev_id;

	vnet_process_ctrl(vdev);

	vnet_process_tx(vdev);

	return IRQ_HANDLED;
}

void vnet_probe(struct vbus_device *vdev) {
	vnet_t *vnet;

	vnet = kzalloc(sizeof(vnet_t), GFP_ATOMIC);
	BUG_ON(!vnet);

	vnet->send = vnet_send_rx;

	dev_set_drvdata(&vdev->dev, &vnet->vdevback);

	DBG(VNET_PREFIX "Backend probe: %d\n", vdev->otherend_id);

	if(!net_devs_initialized){
		initialize_net_devs();
	}
}

void vnet_remove(struct vbus_device *vdev) {
	vnet_t *vnet = to_vnet(vdev);

	DBG("%s: freeing the vnet structure for %s\n", __func__,vdev->nodename);
	kfree(vnet);
}


void vnet_close(struct vbus_device *vdev) {
	vnet_t *vnet = to_vnet(vdev);

	DBG(VNET_PREFIX "Backend close: %d\n", vdev->otherend_id);

	/*
	 * Free the ring and unbind evtchn.
	 */

	BACK_RING_INIT(&vnet->ring_tx, (&vnet->ring_tx)->sring, PAGE_SIZE);
	BACK_RING_INIT(&vnet->ring_rx, (&vnet->ring_rx)->sring, PAGE_SIZE);
	BACK_RING_INIT(&vnet->ring_ctrl, (&vnet->ring_ctrl)->sring, PAGE_SIZE);
	unbind_from_virqhandler(vnet->irq, vdev);

	vbus_unmap_ring_vfree(vdev, vnet->ring_tx.sring);
	vbus_unmap_ring_vfree(vdev, vnet->ring_rx.sring);
	vbus_unmap_ring_vfree(vdev, vnet->ring_ctrl.sring);
	vnet->ring_tx.sring = NULL;
	vnet->ring_rx.sring = NULL;
	vnet->ring_ctrl.sring = NULL;

	unlink_vnet(net_devs[vdev->otherend_id - 2]);
}

void vnet_suspend(struct vbus_device *vdev) {

	DBG(VNET_PREFIX "Backend suspend: %d\n", vdev->otherend_id);
}

void vnet_resume(struct vbus_device *vdev) {

	DBG(VNET_PREFIX "Backend resume: %d\n", vdev->otherend_id);
}

void vnet_reconfigured(struct vbus_device *vdev) {
	int res, i=0;
	unsigned long ring_tx_ref, ring_rx_ref, ring_ctrl_ref, vbuff_tx_ref, vbuff_rx_ref;
	unsigned int evtchn;
	vnet_tx_sring_t *sring_tx;
	vnet_rx_sring_t *sring_rx;
	vnet_ctrl_sring_t *sring_ctrl;
	vnet_t *vnet = to_vnet(vdev);
	unsigned long grants_tx, grants_rx;

	DBG(VNET_PREFIX "Backend reconfigured: %d\n", vdev->otherend_id);

	/*
	 * Set up a ring (shared page & event channel) between the agency and the ME.
	 */

	vbus_gather(VBT_NIL, vdev->otherend, "ring-tx-ref", "%lu", &ring_tx_ref,
		    "ring-rx-ref", "%lu", &ring_rx_ref,
		    "ring-ctrl-ref", "%lu", &ring_ctrl_ref,
		    "ring-evtchn", "%u", &evtchn,
		    "grant-buff", "%lu", &vnet->grant_buff,
		    "vbuff-tx-ref", "%lu", &vbuff_tx_ref,
		    "vbuff-rx-ref", "%lu", &vbuff_rx_ref,
		    NULL );

	DBG("BE: ring-ref=%u, event-channel=%u\n", ring_ref, evtchn);

	res = vbus_map_ring_valloc(vdev, ring_tx_ref, (void **) &sring_tx);
	BUG_ON(res < 0);

	SHARED_RING_INIT(sring_tx);
	BACK_RING_INIT(&vnet->ring_tx, sring_tx, PAGE_SIZE);


	res = vbus_map_ring_valloc(vdev, ring_rx_ref, (void **) &sring_rx);
	BUG_ON(res < 0);

	SHARED_RING_INIT(sring_rx);
	BACK_RING_INIT(&vnet->ring_rx, sring_rx, PAGE_SIZE);


	res = vbus_map_ring_valloc(vdev, ring_ctrl_ref, (void **) &sring_ctrl);
	BUG_ON(res < 0);

	SHARED_RING_INIT(sring_ctrl);
	BACK_RING_INIT(&vnet->ring_ctrl, sring_ctrl, PAGE_SIZE);


	res = vbus_map_ring_valloc(vdev, vnet->grant_buff, (void **) &vnet->shared_data);
	BUG_ON(res < 0);
	/*vnet->vbuff_rx = vnet->vbuff_tx + PAGE_COUNT;
	vnet->vbuff_ethaddr = (unsigned char*)vnet->vbuff_rx + PAGE_COUNT;*/

	vbuff_init(&vnet->vbuff_tx, vbuff_tx_ref, vdev);
	vbuff_init(&vnet->vbuff_rx, vbuff_tx_ref, vdev);

	struct net_device *net_dev = net_devs[vdev->otherend_id - 2];
	link_vnet(net_dev, vnet);


	res = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, evtchn, vnet_interrupt, NULL, 0, VNET_NAME "-backend", vdev);
	BUG_ON(res < 0);

	vnet->irq = res;
}

void vnet_connected(struct vbus_device *vdev) {

	DBG(VNET_PREFIX "Backend connected: %d\n",vdev->otherend_id);
}


vdrvback_t vnetdrv = {
	.probe = vnet_probe,
	.remove = vnet_remove,
	.close = vnet_close,
	.connected = vnet_connected,
	.reconfigured = vnet_reconfigured,
	.resume = vnet_resume,
	.suspend = vnet_suspend
};

int vnet_init(void) {
	struct device_node *np;
	int i = 0;

	np = of_find_compatible_node(NULL, NULL, "vnet,backend");

	/* Check if DTS has vuihandler enabled */
	if (!of_device_is_available(np))
		return 0;

	vdevback_init(VNET_NAME, &vnetdrv);

	return 0;
}

device_initcall(vnet_init);
