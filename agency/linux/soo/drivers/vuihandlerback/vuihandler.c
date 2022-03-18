/*
 * Copyright (C) 2018-2019 Baptiste Delporte <bonel@bonel.net>
 * Copyright (C) 2018-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
 * Copyright (C) 2020-2022 David Truan <david.truan@heig-vd.ch>
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

/*
 * TX path: from the Smart Object to the tablet/smartphone
 * RX path: from the tablet/smartphone to the Smart Object
 *
 */

#if 0
#define DEBUG
#endif

/* For debugging purposes: app presence simulator */
#if 0
#define APP_SIMULATOR
#endif

#include <stdarg.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/syscalls.h>
#include <linux/of.h>

#include <soo/core/device_access.h>

#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/uapi/console.h>
#include <soo/uapi/soo.h>
#include <soo/guest_api.h>
#include <soo/evtchn.h>

#include <soo/uapi/debug.h>
#include <soo/uapi/injector.h>

#include <soo/soolink/soolink.h>
#include <soo/sooenv.h>

#include <soo/vdevback.h>

#include <soo/dev/vuihandler.h>

/*
 * vuiHandler is dedicated to manage interactions between a (external) tablet/smartphone and the components
 * running in the agency (the agency itself or a ME).
 * For example, the injector can use vuiHandler to receive a ME from the tablet (dynamic injection).
 * Any GUI-capable ME can also resort to the vuiHandler.
 * The communication channel can be either Bluetooth or TCP; TCP means a network connection based on socket,
 * for example in the case of SOO.net connected to RJ-45, such a smart object can retrieve data from a remote tablet
 * connected to Internet.
 */

#if 0
#define TEST_RX 1
#endif

/* Max missed keepalive beacon count for disconnection */
#define MAX_FAILED_PING_COUNT	3

/* List of all vuihandler vbus_device managed by this BE */
static struct list_head *vdev_list;


/* vbus_device private structure */
typedef struct {
	vuihandler_t vuihandler;
} vuihandler_priv_t;


/* Private structure for the vbus_driver. It contains everything
   which is used only by the backend */
typedef struct {
	/* List that holds the connected remote applications */
	vuihandler_connected_app_t connected_app;
	spinlock_t connected_app_lock;

	/* Received BT packet counter */
	uint32_t recv_count;

	vuihandler_pkt_t *tx_vuihandler_pkt;
	vuihandler_tx_buf_t tx_buf;
	size_t tx_pkt_size;
	spinlock_t tx_lock;
	struct completion tx_completion;

	sl_desc_t *vuihandler_bt_sl_desc;
	
	#if defined(CONFIG_SOOLINK_PLUGIN_ETHERNET)
	sl_desc_t *vuihandler_tcp_sl_desc;
	#endif /* CONFIG_SOOLINK_PLUGIN_ETHERNET */

	/* RFCOMM interfacing members */
	int rfcomm_tty_pid;
	struct mutex rfcomm_lock;

} vuihandler_drv_priv_t;


/* We declare and define the driver here so we can access it
   in some functions directly */
vdrvback_t vuihandlerdrv = {
	.probe = vuihandler_probe,
	.remove = vuihandler_remove,
	.close = vuihandler_close,
	.connected = vuihandler_connected,
	.reconfigured = vuihandler_reconfigured,
	.resume = vuihandler_resume,
	.suspend = vuihandler_suspend
};


/**
 * Return the SPID of the ME whose "otherend ID" is given as parameter.
 * Return the pointer to the SPID on success.
 * If there is no such ME in this slot, return NULL.
 */
static uint64_t get_spid_from_otherend_id(int otherend_id) {
	vuihandler_t *vuihandler;
	vuihandler_priv_t *vuihandler_priv;
	struct vbus_device *vdev = vdevback_get_entry(otherend_id, vdev_list);

	vuihandler_priv = dev_get_drvdata(&vdev->dev);
	vuihandler = &vuihandler_priv->vuihandler;

	return vuihandler->spid;
}

/**
 * Return the "otherend ID" of the ME whose SPID is given as parameter.
 * Return the ID on success.
 * If there is no such ME, return -ENOENT.
 */
static int get_otherend_id_from_spid(uint64_t spid) {
	uint32_t i;
	vuihandler_t *vuihandler;
	vuihandler_priv_t *vuihandler_priv;
	struct vbus_device *vdev;
	
	soo_log("[soo:backend:vuihandler] Searching for ");
	soo_log_printlnUID(spid);


	for (i = 1; i < MAX_DOMAINS; i++) {
		vdev = vdevback_get_entry(i, vdev_list);
		if (vdev != NULL) {
			soo_log("[soo:backend:vuihandler] Slot %d is not empty!\n", i);

			vuihandler_priv = dev_get_drvdata(&vdev->dev);
			vuihandler = &vuihandler_priv->vuihandler;

			soo_log("[soo:backend:vuihandler] Getting ");
			soo_log_printlnUID(vuihandler->spid);

			if (spid ==  vuihandler->spid)
				return i;
		}
	}

	return -ENOENT;
}


/**
 * @brief Enqueue a packet to be sent in the internal circular buffer.
 * The fact we don't pass it a vuihandler_pkt_t is to avoid having to create one in the caller.
 * 
 * @param data Packet payload
 * @param size Payload size
 * @param spid The SPID of the original sender in case of an ME, NULL if comming from the agency itself
 * @param type The packet type (ex: VUIHANDLER_DATA)  
 * 
 * @return 0 on success, -1 on error
 */ 
int tx_buffer_put(uint8_t *data, uint32_t size, uint64_t spid, uint8_t type) {
	vuihandler_drv_priv_t *vdrv_priv = vdrv_get_priv(&vuihandlerdrv.vdrv);
	vuihandler_pkt_t *cur_elem = vdrv_priv->tx_buf.ring[vdrv_priv->tx_buf.put_index].pkt;

	soo_log("[soo:backend:vuihandler] Putting %dB of type %d in the TX buffer\n", size, type);

	/* abort if there are no place left on the circular buffer */
	if (vdrv_priv->tx_buf.cur_size == VUIHANDLER_TX_BUF_SIZE) 
		return -1;

	spin_lock(&vdrv_priv->tx_lock);

	/* Copy the data into the circular buffer */
	vdrv_priv->tx_buf.ring[vdrv_priv->tx_buf.put_index].size = VUIHANDLER_BT_PKT_HEADER_SIZE + size;

	/* In the case the packet is coming from the agency, there is no SPID */
	cur_elem->spid = spid;

	memcpy(cur_elem->payload, data, size);
	cur_elem->type = type;

	/* Update the circular buffer info */
	vdrv_priv->tx_buf.put_index = (vdrv_priv->tx_buf.put_index + 1) % VUIHANDLER_TX_BUF_SIZE;
	vdrv_priv->tx_buf.cur_size++;

	/* TODO: should we let this here or let the caller do the completion? */
	complete(&vdrv_priv->tx_completion);

	spin_unlock(&vdrv_priv->tx_lock);

	return 0;
}

/**
 * @brief Get the latest packet ready to be sent 
 * 
 * @return The tx_pkt_t pointer containing the next packet to be sent. NULL if no packet is ready.
 */
tx_pkt_t *tx_buffer_get(void) {
	tx_pkt_t *tx_pkt;
	vuihandler_drv_priv_t *vdrv_priv = vdrv_get_priv(&vuihandlerdrv.vdrv);

	if (vdrv_priv->tx_buf.cur_size == 0)
		return NULL;

	spin_lock(&vdrv_priv->tx_lock);
	tx_pkt = &vdrv_priv->tx_buf.ring[vdrv_priv->tx_buf.get_index];

	vdrv_priv->tx_buf.get_index = (vdrv_priv->tx_buf.get_index + 1) % VUIHANDLER_TX_BUF_SIZE;
	vdrv_priv->tx_buf.cur_size--;

	spin_unlock(&vdrv_priv->tx_lock);

	return tx_pkt;
}

/**
 * tx_ring interrupt.
 * It handles packet comming from the MEs. All packets comming from the MEs
 * destined to be sent to the tablet.
 */
irqreturn_t vuihandler_tx_interrupt(int irq, void *dev_id) {
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vuihandler_t *vuihandler = dev_get_drvdata(&vdev->dev);
	vuihandler_tx_request_t *ring_req;
	vuihandler_drv_priv_t *vdrv_priv = vdrv_get_vdevpriv(vdev);
	uint64_t spid;

	while ((ring_req = vuihandler_tx_get_ring_request(&vuihandler->tx_rings.ring)) != NULL) {

		DBG(VUIHANDLER_PREFIX "%d, %d\n", ring_req->id, ring_req->size);

		/* The slot ID is equal to the otherend ID */
		if ((spid = get_spid_from_otherend_id(vdev->otherend_id)) == 0)
			continue;

		/* Only send packets that are adapted to the tablet application */
		if (vdrv_priv->connected_app.spid != spid)
			continue;

		/* Let the circular buffer add the packet to itself */
		if (tx_buffer_put(ring_req->buf, ring_req->size, spid, VUIHANDLER_DATA) == -1) {
#warning Is it an error (BUG)  or acceptable condition?...
			printk("Error: could not put the TX packet in the circular buffer!\n");
			continue;
		}
	}

	return IRQ_HANDLED;
}

int vuihandler_send_from_agency(uint8_t *data, uint32_t size, uint8_t type) {

	if (tx_buffer_put(data, size, 0, type) == -1) {
#warning Is it an error (BUG) or acceptable condition?...
		printk("Error: could not put the TX packet in the circular buffer!\n");
		return -1;
	}
	return 0;
}

/**
 * rx_ring interrupt. The RX ring should not be used in this direction.
 */
irqreturn_t vuihandler_rx_interrupt(int irq, void *dev_id) {
	struct vbus_device *vdev = (struct vbus_device *) dev_id;
	vuihandler_t *vuihandler = dev_get_drvdata(&vdev->dev);
	vuihandler_rx_request_t *ring_req;

	/* Just consume the requests */
	while ((ring_req = vuihandler_rx_get_ring_request(&vuihandler->rx_rings.ring)) != NULL);

	return IRQ_HANDLED;
}


/**
 * Send a signal to the process holding the /dev/rfcommX entry.
 */
void rfcomm_send_sigterm(void) {
	vuihandler_drv_priv_t *vdrv_priv = vdrv_get_priv(&vuihandlerdrv.vdrv);
	mutex_lock(&vdrv_priv->rfcomm_lock);

	if (vdrv_priv->rfcomm_tty_pid) {
		DBG(VUIHANDLER_PREFIX "Send SIGTERM to rfcomm (%d) in user space\n", vdrv_priv->rfcomm_tty_pid);

		sys_kill(vdrv_priv->rfcomm_tty_pid, SIGTERM);
		vdrv_priv->rfcomm_tty_pid = 0;
	}

	mutex_unlock(&vdrv_priv->rfcomm_lock);
}


static void rx_push_response(domid_t domid, vuihandler_pkt_t *vuihandler_pkt, size_t vuihandler_pkt_size) {
	struct vbus_device *vdev = vdevback_get_entry(domid, vdev_list);
	vuihandler_priv_t *vuihandler_priv = dev_get_drvdata(&vdev->dev); 
	vuihandler_drv_priv_t *vdrv_priv = vdrv_get_vdevpriv(vdev);
	vuihandler_t *vuihandler = &vuihandler_priv->vuihandler;
	vuihandler_rx_response_t *ring_rsp = vuihandler_rx_new_ring_response(&vuihandler->rx_rings.ring);

	size_t size = vuihandler_pkt_size - VUIHANDLER_BT_PKT_HEADER_SIZE;
	void *data = vuihandler_pkt->payload;

	ring_rsp->id = vdrv_priv->recv_count;
	ring_rsp->size = size;

	memcpy(ring_rsp->buf, data, size);

	DBG(VUIHANDLER_PREFIX "id: %d\n", ring_rsp->id);

	vuihandler_rx_ring_response_ready(&vuihandler->rx_rings.ring);

	notify_remote_via_virq(vuihandler->rx_rings.irq);

	vdrv_priv->recv_count++;
}

/** Handles and route the packet coming from the tablet that are 
 * destined to the agency core, such as injector packets or update packets 
*/
void handle_agency_packet(vuihandler_pkt_t *vuihandler_pkt, size_t vuihandler_pkt_size) {
	uint8_t *ME_pkt_payload;
	size_t ME_size;

	switch (vuihandler_pkt->type) {
	/* This is the ME size (1B type + 4B size) */
	case VUIHANDLER_ME_SIZE:
		ME_pkt_payload = (uint8_t *)vuihandler_pkt;

		DBG("ME size: %u\n", *((uint32_t *)(ME_pkt_payload+1)));
		ME_size = *((uint32_t *)(ME_pkt_payload+1));
		
		/* Forward the size to the injector so it knows the ME size. */
		injector_prepare(ME_size);

		// vfree(vuihandler_pkt);
		break;

	/* This is the ME data which needs to be forwarded to the Injector */
	case VUIHANDLER_ME_INJECT:

		/* As we bypass the full vuiHandler protocol, we first use a uint8_t array to
		easily access the data instead of the vuihandler_pkt */
		ME_pkt_payload = (uint8_t *)vuihandler_pkt;

		/* The packet is freed in the injector agency_read callback */
		injector_receive_ME((void *)(ME_pkt_payload+1), vuihandler_pkt_size-1);
		break;
	}
}


/**
 * Ask the agency for the XML ME list and put it in the TX buffer
 * 
 * 
 * @return 0 on success, -1 on error
*/
int send_ME_list_xml(void) {
	ME_id_t *ME_buf_raw = kzalloc(MAX_ME_DOMAINS * sizeof(ME_id_t), GFP_ATOMIC);
	uint8_t *ME_buf_xml = NULL;

	get_ME_id_array(ME_buf_raw);
	ME_buf_xml = xml_prepare_id_array(ME_buf_raw);

	if (ME_buf_xml == NULL) {
		return -1;
	}

	tx_buffer_put(ME_buf_xml, strlen(ME_buf_xml)+1, 0, 0);

	kfree(ME_buf_raw);
	return 0;

}


void vuihandler_recv(vuihandler_pkt_t *vuihandler_pkt, size_t vuihandler_pkt_size) {
	size_t size;
	int me_id;

	/* Check for packet destinated to to agency, mainly ME injection related */
	if (vuihandler_pkt->type == VUIHANDLER_ME_SIZE || vuihandler_pkt->type == VUIHANDLER_ME_INJECT) {
		handle_agency_packet(vuihandler_pkt, vuihandler_pkt_size);
		return;
	}

	
	if (vuihandler_pkt->type == VUIHANDLER_ASK_LIST) {
		/* This is a vUIHandler beacon */
		send_ME_list_xml();

		return ;
	}
	
	if (vuihandler_pkt->type == VUIHANDLER_BEACON) {
		/* This is a vUIHandler beacon */
		// recv_beacon(vuihandler_pkt, vuihandler_pkt_size);

		return ;
	}

	/* We expect the BT packet to be a vUIHandler data packet or an event packet */
	if (!(vuihandler_pkt->type == VUIHANDLER_DATA || vuihandler_pkt->type == VUIHANDLER_SEND))
		return ;

	/* From there, the packet is forwarded to the corresponding ME */
	size = vuihandler_pkt_size - VUIHANDLER_BT_PKT_HEADER_SIZE;

	if (unlikely(size > VUIHANDLER_MAX_PAYLOAD_SIZE))
		return ;

	DBG(VUIHANDLER_PREFIX "Size: %d\n", size);

	// TODO: Change this in order to route the packet not using the SPID
	/* Find the ME targeted by the packet using its SPID */
	me_id = get_otherend_id_from_spid(vuihandler_pkt->spid);
#ifdef TEST_RX
	me_id = 2;
#else	
	if (me_id < 0) 
		return;
#endif /* TEST_RX */	

	DBG(VUIHANDLER_PREFIX "ME ID: %d\n", me_id);

	rx_push_response(me_id, vuihandler_pkt, vuihandler_pkt_size);

	// vfree(vuihandler_pkt);
}



/**
 * TX task.
 * BT and TCP/IP interfaces are mutually exclusive, thus a common thread is sufficient.
 */
static int tx_task_fn(void *arg) {
	vuihandler_pkt_t *vuihandler_pkt;
	tx_pkt_t *tx_pkt;
	size_t pkt_size;
	unsigned long flags;
	int rfcomm_pid;
	vuihandler_drv_priv_t *vdrv_priv = (vuihandler_drv_priv_t *) arg;

	while (1) {
		wait_for_completion(&vdrv_priv->tx_completion);

		tx_pkt = tx_buffer_get();

		if (tx_pkt == NULL) {
#warning Is it an error (BUG)  or acceptable condition?...
			lprintk("An error occured while getting the next TX packet!\n");
			continue;
		}

		/* Retrieve parameters from TX ISR */
		spin_lock_irqsave(&vdrv_priv->tx_lock, flags);
		vuihandler_pkt = tx_pkt->pkt;
		pkt_size = tx_pkt->size;
		spin_unlock_irqrestore(&vdrv_priv->tx_lock, flags);

		/* Priority is not supported yet */
		mutex_lock(&vdrv_priv->rfcomm_lock);
		rfcomm_pid = vdrv_priv->rfcomm_tty_pid;
		mutex_unlock(&vdrv_priv->rfcomm_lock);

		if (rfcomm_pid) {
			lprintk("(B>%d)", pkt_size);
			sl_send(vdrv_priv->vuihandler_bt_sl_desc, vuihandler_pkt, pkt_size, 0, 0);
		}
	}

	return 0;
}


/**
 * RX task for the BT interface.
 */
static int rx_bt_task_fn(void *arg) {
	size_t size;
	void *priv_buffer = NULL;
	vuihandler_drv_priv_t *vdrv_priv = (vuihandler_drv_priv_t *) arg;

	while (1) {
		size = sl_recv(vdrv_priv->vuihandler_bt_sl_desc, &priv_buffer);

		DBG("(B<%d)\n", size);

		/* We dont free the packet here, as its lifetime can be longer
		depending on the packet handler (ex: the injector must ensure it's data can
		be read from the userspace correctly). It implies that it is the handler 
		which MUST vfree the packet after using it */ 
		vuihandler_recv(priv_buffer, size);
		vfree(priv_buffer);
	}
	return 0;
}

#if defined(CONFIG_SOOLINK_PLUGIN_ETHERNET)
/**
 * RX task for the TCP interface.
 */
static int rx_tcp_task_fn(void *arg) {
	size_t size;
	void *priv_buffer = NULL;
	vuihandler_drv_priv_t *vdrv_priv = (vuihandler_drv_priv_t *) arg;

	while (1) {
		size = sl_recv(vdrv_priv->vuihandler_tcp_sl_desc, &priv_buffer);

		DBG("(T<%d)", size);

		vuihandler_recv(priv_buffer, size);
	}

	return 0;
}
#endif /* CONFIG_SOOLINK_PLUGIN_ETHERNET */

/**
 * Interface with RFCOMM.
 */
void vuihandler_open_rfcomm(pid_t pid) {
	vuihandler_drv_priv_t *vdrv_priv = vdrv_get_priv(&vuihandlerdrv.vdrv);
	mutex_lock(&vdrv_priv->rfcomm_lock);
	vdrv_priv->rfcomm_tty_pid = pid;
	mutex_unlock(&vdrv_priv->rfcomm_lock);
}


void vuihandler_probe(struct vbus_device *vdev) {
	vuihandler_priv_t *vuihandler_priv;

	vuihandler_priv = kzalloc(sizeof(vuihandler_priv_t), GFP_ATOMIC);
	BUG_ON(!vuihandler_priv);

	dev_set_drvdata(&vdev->dev, vuihandler_priv);

	vdevback_add_entry(vdev, vdev_list);

	DBG(VUIHANDLER_PREFIX "Backend probe: %d\n", vdev->otherend_id);
}


void vuihandler_remove(struct vbus_device *vdev) {
	vuihandler_priv_t *vuihandler_priv = dev_get_drvdata(&vdev->dev);

	vdevback_del_entry(vdev, vdev_list);

	DBG("%s: freeing the vuihandler structure for %s\n", __func__,vdev->nodename);
	kfree(vuihandler_priv);
}


void vuihandler_close(struct vbus_device *vdev) {
	vuihandler_priv_t *vuihandler_priv = dev_get_drvdata(&vdev->dev);
	vuihandler_t *vuihandler = &vuihandler_priv->vuihandler;
	vuihandler_tx_ring_t *tx_ring = &vuihandler->tx_rings;
	vuihandler_rx_ring_t *rx_ring = &vuihandler->rx_rings;

	/* tx_ring */
	BACK_RING_INIT(&tx_ring->ring, tx_ring->ring.sring, PAGE_SIZE);

	unbind_from_virqhandler(tx_ring->irq, vdev);

	vbus_unmap_ring_vfree(vdev, tx_ring->ring.sring);
	tx_ring->ring.sring = NULL;

	/* rx_ring */
	BACK_RING_INIT(&rx_ring->ring, rx_ring->ring.sring, PAGE_SIZE);

	unbind_from_virqhandler(rx_ring->irq, vdev);

	vbus_unmap_ring_vfree(vdev, rx_ring->ring.sring);
	rx_ring->ring.sring = NULL;

	/* Update the SPID in the SPID table */
	vuihandler->spid = 0;

	DBG(VUIHANDLER_PREFIX "Backend closed: %d\n", vdev->otherend_id);

}

void vuihandler_suspend(struct vbus_device *vdev) {
	DBG(VUIHANDLER_PREFIX "Backend suspend: %d\n", vdev->otherend_id);

}

ME_id_t me_id; /* Here because the short desc is a 1024-char buffer and exceeds the frame size */

void vuihandler_resume(struct vbus_device *vdev) {
	vuihandler_priv_t *vuihandler_priv;


	get_ME_id(vdev->otherend_id, &me_id);

	vuihandler_priv = (vuihandler_priv_t *) dev_get_drvdata(&vdev->dev);

	vuihandler_priv->vuihandler.spid = me_id.spid;

	DBG(VUIHANDLER_PREFIX "Backend resume: %d\n", vdev->otherend_id);
}


void vuihandler_connected(struct vbus_device *vdev) {

	DBG(VUIHANDLER_PREFIX "Backend connected: %d\n",vdev->otherend_id);
}


void vuihandler_reconfigured(struct vbus_device *vdev) {
	vuihandler_priv_t *vuihandler_priv = dev_get_drvdata(&vdev->dev);
	vuihandler_t *vuihandler = &vuihandler_priv->vuihandler;
	unsigned long tx_ring_ref, rx_ring_ref;
	unsigned int tx_evtchn, rx_evtchn;
	vuihandler_tx_sring_t *tx_sring;
	vuihandler_tx_ring_t *tx_ring = &vuihandler->tx_rings;
	vuihandler_rx_sring_t *rx_sring;
	vuihandler_rx_ring_t *rx_ring = &vuihandler->rx_rings;

	/* tx_ring */
	vbus_gather(VBT_NIL, vdev->otherend, "tx_ring-ref", "%lu", &tx_ring_ref, "tx_ring-evtchn", "%u", &tx_evtchn, NULL);

	vbus_map_ring_valloc(vdev, tx_ring_ref, (void **) &tx_sring);

	SHARED_RING_INIT(tx_sring);
	BACK_RING_INIT(&tx_ring->ring, tx_sring, PAGE_SIZE);

	tx_ring->irq = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, tx_evtchn, vuihandler_tx_interrupt, NULL, 0, VUIHANDLER_NAME "-tx", vdev);

	/* rx_ring */
	vbus_gather(VBT_NIL, vdev->otherend, "rx_ring-ref", "%lu", &rx_ring_ref, "rx_ring-evtchn", "%u", &rx_evtchn, NULL);

	vbus_map_ring_valloc(vdev, rx_ring_ref, (void **) &rx_sring);

	SHARED_RING_INIT(rx_sring);
	BACK_RING_INIT(&rx_ring->ring, rx_sring, PAGE_SIZE);

	rx_ring->irq = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, rx_evtchn, vuihandler_rx_interrupt, NULL, 0, VUIHANDLER_NAME "-rx", vdev);

	DBG(VUIHANDLER_PREFIX "Backend reconfigured: %d\n", vdev->otherend_id);
}




#ifdef TEST_RX
static int test_rx_fn(void *args) {
	vuihandler_pkt_t *test_pkt;
	char *msg = "Hello from BE!";

	msleep(20000);
	
	test_pkt = (vuihandler_pkt_t *) kzalloc(sizeof(vuihandler_pkt_t) + VUIHANDLER_MAX_PKT_SIZE, GFP_KERNEL);
	memset(test_pkt, 0, sizeof(vuihandler_pkt_t) + VUIHANDLER_MAX_PKT_SIZE);
	test_pkt->spid[0] = 0x02;
	test_pkt->spid[7] = 0x10;
	test_pkt->type = VUIHANDLER_DATA;
	memcpy(test_pkt->payload, msg, strlen(msg));
	while(1) {
		msleep(1000);
		vuihandler_recv(test_pkt, sizeof(vuihandler_pkt_t) + strlen(msg));
	}

	return 0;
}
#endif


/**
 * Start the communication threads. 
 */
static void vuihandler_start_threads(void *args) {

	kthread_run(tx_task_fn, args, "vUIHandler-TX");
	kthread_run(rx_bt_task_fn, args, "vUIHandler-BT-RX");

#ifdef TEST_RX
	kthread_run(test_rx_fn, args, "vuihandler-test-RX");
#endif

#if defined(CONFIG_SOOLINK_PLUGIN_ETHERNET)
	kthread_run(rx_tcp_task_fn, args, "vUIHandler-TCP-RX");
#endif /* CONFIG_SOOLINK_PLUGIN_ETHERNET */
}

void vuihandler_start_deferred(soo_env_t * sooenv, void *args) {
	vuihandler_drv_priv_t *vdrv_priv = (vuihandler_drv_priv_t *) args;

	vdrv_priv->vuihandler_bt_sl_desc = sl_register(SL_REQ_BT, SL_IF_BT, SL_MODE_UNICAST);
	vdrv_priv->vuihandler_tcp_sl_desc = sl_register(SL_REQ_TCP, SL_IF_TCP, SL_MODE_UNICAST);
	/* We need to start the threads here as they need a reference on vuihandler */
	vuihandler_start_threads(args);
}

int vuihandler_init(void) {
	struct device_node *np;
	int i;

	vuihandler_drv_priv_t *vdrv_priv = kzalloc(sizeof(vuihandler_drv_priv_t), GFP_ATOMIC);
	vdrv_set_priv(&vuihandlerdrv.vdrv, (void *)vdrv_priv);

	np = of_find_compatible_node(NULL, NULL, "vuihandler,backend");

	/* Check if DTS has vuihandler enabled */
	if (!of_device_is_available(np))
		return 0;

	vdev_list = (struct list_head *) kzalloc(sizeof(struct list_head), GFP_ATOMIC);
	INIT_LIST_HEAD(vdev_list);

	vdrv_priv->connected_app.spid = 0;

	spin_lock_init(&vdrv_priv->connected_app_lock);

	/* Allocate the circular TX buffer containers */
	for (i = 0; i < VUIHANDLER_TX_BUF_SIZE; ++i) {
		vdrv_priv->tx_buf.ring[i].pkt = (vuihandler_pkt_t *) kzalloc(sizeof(vuihandler_pkt_t) + VUIHANDLER_MAX_PKT_SIZE, GFP_KERNEL);
	}

	spin_lock_init(&vdrv_priv->tx_lock);
	init_completion(&vdrv_priv->tx_completion);

	mutex_init(&vdrv_priv->rfcomm_lock);

	/* Start the threads in a defferred way, to wait for the soolink to be ready */
	register_sooenv_up(vuihandler_start_deferred, vdrv_priv);

	/* Set the associated dev capability */
	devaccess_set_devcaps(DEVCAPS_CLASS_COMM, DEVCAP_COMM_UIHANDLER, true);

	vdevback_init(VUIHANDLER_NAME, &vuihandlerdrv);

	return 0;
}

late_initcall(vuihandler_init);
