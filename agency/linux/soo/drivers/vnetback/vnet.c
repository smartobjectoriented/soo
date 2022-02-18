/*
 * Copyright (C) 2020 Julien Quartier <julien.quartier@bluewin.ch>
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

#include <soo/dev/vnet.h>

#include "vnetbuff.h"
#include "vnetif.h"
#include "vnetifutil_priv.h"

struct net_device *net_devs[MAX_ME_DOMAINS];
bool net_devs_initialized = false;

/* This store which dom can forward frames to all MEs */
int connected_dom = INVALID_DOM;

int if_running = 0;

void initialize_net_devs(void){
	int i = 0;
	struct vnetif *vif;

	while(i < MAX_ME_DOMAINS){
		net_devs[i] = vnetif_init(i);
		vif = netdev_priv(net_devs[i]);
		i++;
	}

	net_devs_initialized = true;
}

void vnet_process_ctrl(struct vbus_device *vdev){
	vnet_request_t *ring_req;
	vnet_response_t *ring_rsp;
	vnet_t *vnet = to_vnet(vdev);
	int can_access = 0;

	while ((ring_req = vnet_ctrl_ring_request(&vnet->ring_ctrl)) != NULL) {
		switch(ring_req->type){
		case NET_STATUS:

			/* The network access token if free or we already are the owner */
			if(connected_dom == INVALID_DOM || connected_dom == vdev->otherend_id) {
				/* Set the other end as token owner */
				connected_dom = vdev->otherend_id;
				can_access = 1;
			}

			can_access &= if_running;

			vnet->has_connected_token = can_access;

			if ((ring_rsp = vnet_ctrl_ring_response(&vnet->ring_ctrl)) != NULL) {
				ring_rsp->type = NET_STATUS;
				ring_rsp->network.broadcast_token = can_access;
				ring_rsp->network.connected = if_running;

				vnet_ctrl_ring_response_ready(&vnet->ring_ctrl);
				notify_remote_via_virq(vnet->irq);
			}

			break;
		}
	}
}

void vnet_process_data(struct vbus_device *vdev){
	vnet_request_t *ring_req = NULL;
	uint8_t* data;
	vnet_t *vnet = to_vnet(vdev);

	while ((ring_req = vnet_data_ring_request(&vnet->ring_data)) != NULL) {
		data = vbuff_get(&vnet->vbuff_tx, &ring_req->buff);
		netif_rx_packet(net_devs[vdev->otherend_id - 2], data, ring_req->buff.size);
	}
}

inline static int vnet_is_eth_dest(vnet_t* vnet, u8* data){
	/* Is ethernet multicast or broadcast */
	if((data[0] & 0b1) == 0b1)
		return 1;

	return memcmp(vnet->shared_data->ethaddr, data, ETH_ALEN) == 0;
}

void vnet_send_data(void* void_vnet, u8* data, int length){
	vnet_response_t *ring_rsp;
	struct vbuff_data vbuff_data;
	vnet_t* vnet = (vnet_t*)void_vnet;
	u8* buff_data = NULL;

	DBG(VNET_PREFIX "Send data length: %d\n", length);

	/* The frame is not for us and we can't broadcast to other */
	if(!vnet->has_connected_token && !vnet_is_eth_dest(vnet, data))
		return;

	vbuff_put(&vnet->vbuff_rx, &vbuff_data, &buff_data, length);
	memcpy(buff_data, data, length);

	if ((ring_rsp = vnet_data_ring_response(&vnet->ring_data)) != NULL) {
		ring_rsp->buff = vbuff_data;
		vnet_data_ring_response_ready(&vnet->ring_data);
		notify_remote_via_virq(vnet->irq);
	}
}


irqreturn_t vnet_interrupt(int irq, void *dev_id)
{
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	DBG(VNET_PREFIX "Interrupt start: %d\n", irq);

	vnet_process_ctrl(vdev);

	vnet_process_data(vdev);

	DBG(VNET_PREFIX "Interrupt end: %d\n", irq);

	return IRQ_HANDLED;
}

void vnet_probe(struct vbus_device *vdev) {
	vnet_t *vnet;

	vnet = kzalloc(sizeof(vnet_t), GFP_ATOMIC);
	BUG_ON(!vnet);

	vnet->send = vnet_send_data;

	dev_set_drvdata(&vdev->dev, &vnet->vdevback);

	DBG(VNET_PREFIX "Backend probe: %d\n", vdev->otherend_id);
}

void vnet_remove(struct vbus_device *vdev) {
	vnet_t *vnet = to_vnet(vdev);

	DBG("%s: freeing the vnet structure for %s\n", __func__,vdev->nodename);
	kfree(vnet);
}


void vnet_close(struct vbus_device *vdev) {
	vnet_t *vnet = to_vnet(vdev);

	DBG(VNET_PREFIX "Backend close: %d\n", vdev->otherend_id);

	if(connected_dom == vdev->otherend_id){
		vnet->has_connected_token = 0;
		connected_dom = INVALID_DOM;
	}

	/*
	 * Free the ring and unbind evtchn.
	 */
	BACK_RING_INIT(&vnet->ring_data, (&vnet->ring_data)->sring, PAGE_SIZE);
	BACK_RING_INIT(&vnet->ring_ctrl, (&vnet->ring_ctrl)->sring, PAGE_SIZE);
	unbind_from_virqhandler(vnet->irq, vdev);

	vbus_unmap_ring_vfree(vdev, vnet->ring_data.sring);
	vbus_unmap_ring_vfree(vdev, vnet->ring_ctrl.sring);
	vnet->ring_data.sring = NULL;
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
	int res;
	unsigned long ring_data_ref, ring_ctrl_ref, vbuff_tx_ref, vbuff_rx_ref;
	unsigned int evtchn;
	vnet_data_sring_t *sring_data;
	vnet_ctrl_sring_t *sring_ctrl;
	vnet_t *vnet = to_vnet(vdev);
	struct net_device *net_dev;

	DBG(VNET_PREFIX "Backend reconfigured: %d\n", vdev->otherend_id);

	/*
	 * Set up a ring (shared page & event channel) between the agency and the ME.
	 */

	vbus_gather(VBT_NIL, vdev->otherend,
		    "ring-data-ref", "%lu", &ring_data_ref,
		    "ring-ctrl-ref", "%lu", &ring_ctrl_ref,
		    "ring-evtchn", "%u", &evtchn,
		    "grant-buff", "%lu", &vnet->grant_buff,
		    "vbuff-tx-ref", "%lu", &vbuff_tx_ref,
		    "vbuff-rx-ref", "%lu", &vbuff_rx_ref,
		    NULL );

	DBG("BE: ring-ref=%u, event-channel=%u\n", ring_data_ref, evtchn);

	res = vbus_map_ring_valloc(vdev, ring_data_ref, (void **) &sring_data);
	BUG_ON(res < 0);

	SHARED_RING_INIT(sring_data);
	BACK_RING_INIT(&vnet->ring_data, sring_data, PAGE_SIZE);


	res = vbus_map_ring_valloc(vdev, ring_ctrl_ref, (void **) &sring_ctrl);
	BUG_ON(res < 0);

	SHARED_RING_INIT(sring_ctrl);
	BACK_RING_INIT(&vnet->ring_ctrl, sring_ctrl, PAGE_SIZE);


	res = vbus_map_ring_valloc(vdev, vnet->grant_buff, (void **) &vnet->shared_data);
	BUG_ON(res < 0);


	/* init circualar buffers used for rx/tx frames */
	vbuff_init(&vnet->vbuff_tx, vbuff_tx_ref, vdev);
	vbuff_init(&vnet->vbuff_rx, vbuff_rx_ref, vdev);

	/* Get the vif attributed to this domain */
	net_dev = net_devs[vdev->otherend_id - 2];
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

int vnet_beat_thread(void * args){
	while(1){
		if_running = vnetifutil_if_running(VNET_INTERFACE);
		msleep(2000);
	}
}

int vnet_init(void) {
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "vnet,backend");

	/* Check if DTS has vuihandler enabled */
	if (!of_device_is_available(np))
		return 0;

	vdevback_init(VNET_NAME, &vnetdrv);

	initialize_net_devs();


	kernel_thread(vnet_beat_thread, NULL, 0);


	return 0;
}

device_initcall(vnet_init);
