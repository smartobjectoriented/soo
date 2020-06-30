/*
 * Copyright (C) 2018-2019 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2018-2019 Baptiste Delporte <bonel@bonel.net>
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

#include <heap.h>
#include <mutex.h>
#include <delay.h>
#include <memory.h>
#include <asm/mmu.h>

#include <device/driver.h>

#include <soo/evtchn.h>
#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/console.h>
#include <soo/debug.h>

#include <device/network.h>
#include <network.h>

#include <soo/dev/vnet.h>

#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "lwip/ethip6.h"
#include "lwip/etharp.h"
#include <lwip/init.h>
#include <lwip/dhcp.h>
#include <netif/ethernet.h>
#include <lwip/netifapi.h>
#include <lwip/tcpip.h>
#include <lwip/ip_addr.h>


/* Our unique net instance. */
static struct vbus_device *vdev_net = NULL;

static bool thread_created = false;

irq_return_t vnet_interrupt(int irq, void *dev_id) {
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vnet_t *vnet = to_vnet(vdev);
	vnet_response_t *ring_rsp;

	DBG("%s, %d\n", __func__, ME_domID());

	while ((ring_rsp = vnet_ring_response(&vnet->ring)) != NULL) {

		DBG("%s, rsp=%p\n", __func__, ring_rsp);

		/* Do something with the response */
	}

	return IRQ_COMPLETED;
}


char vnet_buff[1514];
err_t vnet_lwip_send(struct netif *netif, struct pbuf *p) {
        vnet_t *vnet;
        char *data;
        char *buff;
        vnet_request_t *ring_req;
        printk("PKT1\n");

        if (!vdev_net){
                printk("ERR\n");
                return ERR_CONN;
        }

        vnet = to_vnet(vdev_net);



        printk("PKT\n");

        // TODO check null
        buff = malloc(p->tot_len);
        printk("1\n");
        data = (char *)pbuf_get_contiguous(p, buff, p->tot_len, p->tot_len, 0);
        printk("2\n");
        /*
         * If only one pbuf is used get contiguous returns the pbuf pointer and don't use buff
         * We want to have the frame in buff in any case
         */
        /*if(data != buff){
                memcpy(buff, data, p->tot_len);
        }*/

        // tmplen = (p->tot_len + VNET_PACKET_SIZE - 1) / VNET_PACKET_SIZE;

        while ((ring_req = vnet_ring_request(&vnet->ring)) == NULL){
                msleep(10);
                printk("NO RING\n");
        }
        printk("3\n");
        printk("%p\n", ring_req);


        //VBUFF_TX_PUT(ring_req, data, p->tot_len);

        /* Create a readonly grant */
        //ring_req->data_handle = gnttab_grant_foreign_access(ME_domID(), buff, 0);

        //ring_req->size = p->tot_len;


        /*while (tmplen--) {
                while ((ring_req = vnet_ring_request_next(&vnet.ring)) == NULL){
                        msleep(10);
                        printk("NO RING 2\n");
                }
                ring_req = vnet_ring_request_next(&vnet.ring);
                memcpy(&ring_req->data, data++, VNET_PACKET_SIZE);
        }*/

        vnet_ring_request_ready(&vnet->ring);
        notify_remote_via_irq(vnet->irq);

        printk("PKT END\n");

        //vnet_end();

        return ERR_OK;
}


/*
 * The following function is given as an example.
 *
 */
#if 0
void vnet_generate_request(char *buffer) {
	vnet_request_t *ring_req;

	vdevfront_processing_start();

	/*
	 * Try to generate a new request to the backend
	 */
	if (!RING_FULL(&vnet.ring)) {
		ring_req = RING_GET_REQUEST(&vnet.ring, vnet.ring.req_prod_pvt);

		memcpy(ring_req->buffer, buffer, VNET_PACKET_SIZE);

		/* Fill in the ring_req structure */

		/* Make sure the other end "sees" the request when updating the index */
		dmb();

		vnet.ring.req_prod_pvt++;

		RING_PUSH_REQUESTS(&vnet.ring);

		notify_remote_via_irq(vnet.irq);
	}

	vdevfront_processing_end();
}
#endif

void vnet_probe(struct vbus_device *vdev) {
	int res;
	unsigned int evtchn;
	vnet_sring_t *sring;
	struct vbus_transaction vbt;
	vnet_t *vnet;

	DBG0("[" VNET_NAME "] Frontend probe\n");

	if (vdev->state == VbusStateConnected)
		return ;


	vnet = malloc(sizeof(vnet_t));
	BUG_ON(!vnet);
	memset(vnet, 0, sizeof(vnet_t));

        /* Local instance */
        vdev_net = vdev;

	dev_set_drvdata(vdev->dev, &vnet->vdevfront);

	DBG("Frontend: Setup ring\n");

	/* Prepare to set up the ring. */

	vnet->ring_ref = GRANT_INVALID_REF;

	/* Allocate an event channel associated to the ring */
	res = vbus_alloc_evtchn(vdev, &evtchn);
	BUG_ON(res);

	res = bind_evtchn_to_irq_handler(evtchn, vnet_interrupt, NULL, vdev);
	if (res <= 0) {
		lprintk("%s - line %d: Binding event channel failed for device %s\n", __func__, __LINE__, vdev->nodename);
		BUG();
	}

	vnet->evtchn = evtchn;
	vnet->irq = res;

	/* Allocate a shared page for the ring */
	sring = (vnet_sring_t *) get_free_vpage();
	if (!sring) {
		lprintk("%s - line %d: Allocating shared ring failed for device %s\n", __func__, __LINE__, vdev->nodename);
		BUG();
	}

	SHARED_RING_INIT(sring);
	FRONT_RING_INIT(&vnet->ring, sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	res = vbus_grant_ring(vdev, phys_to_pfn(virt_to_phys_pt((uint32_t) vnet->ring.sring)));
	if (res < 0)
		BUG();

	vnet->ring_ref = res;

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "ring-ref", "%u", vnet->ring_ref);
	vbus_printf(vbt, vdev->nodename, "ring-evtchn", "%u", vnet->evtchn);

	vbus_transaction_end(vbt);

}

/* At this point, the FE is not connected. */
void vnet_reconfiguring(struct vbus_device *vdev) {
	int res;
	struct vbus_transaction vbt;
	vnet_t *vnet = to_vnet(vdev);

	DBG0("[" VNET_NAME "] Frontend reconfiguring\n");
	/* The shared page already exists */
	/* Re-init */

	gnttab_end_foreign_access_ref(vnet->ring_ref);

	DBG("Frontend: Setup ring\n");

	/* Prepare to set up the ring. */

	vnet->ring_ref = GRANT_INVALID_REF;

	SHARED_RING_INIT(vnet->ring.sring);
	FRONT_RING_INIT(&vnet->ring, (&vnet->ring)->sring, PAGE_SIZE);

	/* Prepare the shared to page to be visible on the other end */

	res = vbus_grant_ring(vdev, phys_to_pfn(virt_to_phys_pt((uint32_t) vnet->ring.sring)));
	if (res < 0)
		BUG();

	vnet->ring_ref = res;

	vbus_transaction_start(&vbt);

	vbus_printf(vbt, vdev->nodename, "ring-ref", "%u", vnet->ring_ref);
	vbus_printf(vbt, vdev->nodename, "ring-evtchn", "%u", vnet->evtchn);

	vbus_transaction_end(vbt);
}

void vnet_shutdown(struct vbus_device *vdev) {

	DBG0("[" VNET_NAME "] Frontend shutdown\n");
}

void vnet_closed(struct vbus_device *vdev) {
	vnet_t *vnet = to_vnet(vdev);

	DBG0("[" VNET_NAME "] Frontend close\n");

	/**
	 * Free the ring and deallocate the proper data.
	 */

	/* Free resources associated with old device channel. */
	if (vnet->ring_ref != GRANT_INVALID_REF) {
		gnttab_end_foreign_access(vnet->ring_ref);
		free_vpage((uint32_t) vnet->ring.sring);

		vnet->ring_ref = GRANT_INVALID_REF;
		vnet->ring.sring = NULL;
	}

	if (vnet->irq)
		unbind_from_irqhandler(vnet->irq);

	vnet->irq = 0;
}

void vnet_suspend(struct vbus_device *vdev) {

	DBG0("[" VNET_NAME "] Frontend suspend\n");
}

void vnet_resume(struct vbus_device *vdev) {

	DBG0("[" VNET_NAME "] Frontend resume\n");
}

void vnet_connected(struct vbus_device *vdev) {
	vnet_t *vnet = to_vnet(vdev);

	DBG0("[" VNET_NAME "] Frontend connected\n");

	/* Force the processing of pending requests, if any */
	notify_remote_via_irq(vnet->irq);

	if (!thread_created) {
		thread_created = true;
	}
}


err_t vnet_lwip_init(struct netif *netif) {
        eth_dev_t *eth_dev = netif->state;
        LWIP_ASSERT("netif != NULL", (netif != NULL));
        netif->name[0] = 'v';
        netif->name[1] = 'i';
        printk("vnet_lwip_init\n");
        netif->hwaddr_len = ARP_HLEN;
        netif->mtu = 1500;
        netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;


#if LWIP_IPV4
        netif->output = etharp_output;
#endif

        netif->linkoutput = vnet_lwip_send;

        memcpy(netif->hwaddr, eth_dev->enetaddr, ARP_HLEN);

        netif_set_default(netif);
        netif_set_link_up(netif);
        netif_set_up(netif);

        dhcp_start(netif);

        return ERR_OK;
}

int vnet_init(eth_dev_t *eth_dev) {
        struct netif *netif;
        printk("vnet_init\n");
        // Fake mac address
        eth_dev->enetaddr[0] = 0x12;
        eth_dev->enetaddr[1] = 0x12;
        eth_dev->enetaddr[2] = 0x12;
        eth_dev->enetaddr[3] = 0x12;
        eth_dev->enetaddr[4] = 0x12;
        eth_dev->enetaddr[5] = 0x12;

        netif = malloc(sizeof(struct netif));
        netif_add(netif, NULL, NULL, NULL, eth_dev, vnet_lwip_init, tcpip_input);

        return 1;
}

vdrvfront_t vnetdrv = {
	.probe = vnet_probe,
	.reconfiguring = vnet_reconfiguring,
	.shutdown = vnet_shutdown,
	.closed = vnet_closed,
	.suspend = vnet_suspend,
	.resume = vnet_resume,
	.connected = vnet_connected
};


static int vnet_register(dev_t *dev) {
        eth_dev_t *eth_dev = malloc(sizeof(eth_dev_t));
        memset(eth_dev, 0, sizeof(*eth_dev));

        vdevfront_init(VNET_NAME, &vnetdrv);

        //vnet_vbus_init();

        eth_dev->dev = dev;

        eth_dev->init = vnet_init;
        network_devices_register(eth_dev);

        printk("_____________________________________ vnet_register\n");


        //VBUFF_TX_INIT();

        return 0;
}

REGISTER_DRIVER_POSTCORE("vnet,frontend", vnet_register);
