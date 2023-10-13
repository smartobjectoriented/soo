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

/* For testing purpose, if set to 1, launch a thread which continually sends
 data to the first ME which vuihandler FE is reconfigured */
#if 0
#define TEST_RX 1
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


/* Max missed keepalive beacon count for disconnection */
#define MAX_FAILED_PING_COUNT	3

/* List of all vuihandler vbus_device managed by this BE */
static struct list_head *vdev_list;

ME_id_t me_id; /* Here because the short desc is a 1024-char buffer and exceeds the frame size */

#ifdef TEST_RX
bool test_thread_launched = false;
#endif

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
int tx_buffer_put(uint8_t *data, uint32_t size, int32_t slotID, uint8_t type) {
	vuihandler_drv_priv_t *vdrv_priv = vdrv_get_priv(&vuihandlerdrv.vdrv);
	vuihandler_pkt_t *cur_elem = vdrv_priv->tx_buf.ring[vdrv_priv->tx_buf.put_index].pkt;

	DBG("[soo:backend:vuihandler] Putting %dB of type %d in the TX buffer\n", size, type);

	/* abort if there are no place left on the circular buffer */
	if (vdrv_priv->tx_buf.cur_size == VUIHANDLER_TX_BUF_SIZE) 
		return -1;

	spin_lock(&vdrv_priv->tx_lock);

	/* Copy the data into the circular buffer */
	vdrv_priv->tx_buf.ring[vdrv_priv->tx_buf.put_index].size = VUIHANDLER_BT_PKT_HEADER_SIZE + size;

	/* In the case the packet is coming from the agency, there is no slotID (-1) */
	cur_elem->slotID = slotID;

	memcpy(cur_elem->payload, data, size);
	cur_elem->type = type;

	/* Update the circular buffer info */
	vdrv_priv->tx_buf.put_index = (vdrv_priv->tx_buf.put_index + 1) % VUIHANDLER_TX_BUF_SIZE;
	vdrv_priv->tx_buf.cur_size++;


	spin_unlock(&vdrv_priv->tx_lock);
	complete(&vdrv_priv->tx_completion);

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
	int32_t slotID = -1;

	while ((ring_req = vuihandler_tx_get_ring_request(&vuihandler->tx_rings.ring)) != NULL) {

		DBG(VUIHANDLER_PREFIX "%d, %d\n", ring_req->id, ring_req->size);
	
		slotID = vuihandler->otherend_id;		

		/* Let the circular buffer add the packet to itself */
		if (tx_buffer_put(ring_req->buf, ring_req->size, slotID, ring_req->type) == -1) {
			lprintk("Error: could not put the TX packet in the circular buffer!\n");
			BUG();
		}
	}
	

	return IRQ_HANDLED;
}

int vuihandler_send_from_agency(uint8_t *data, uint32_t size, uint8_t type) {

	if (tx_buffer_put(data, size, 0, type) == -1) {
		lprintk("Error: could not put the TX packet in the circular buffer!\n");
		BUG();
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
 * Called by the TTY RFCOMM driver once the connection is closed 
 */
void vuihandler_close_rfcomm(void) {
	vuihandler_drv_priv_t *vdrv_priv = vdrv_get_priv(&vuihandlerdrv.vdrv);

	mutex_lock(&vdrv_priv->rfcomm_lock);
	vdrv_priv->rfcomm_tty_pid = 0;
	mutex_unlock(&vdrv_priv->rfcomm_lock);
}


static void rx_push_response(domid_t domid, vuihandler_pkt_t *vuihandler_pkt, size_t vuihandler_pkt_size) {
	struct vbus_device *vdev = vdevback_get_entry(domid, vdev_list);
	vuihandler_priv_t *vuihandler_priv = dev_get_drvdata(&vdev->dev); 
	vuihandler_drv_priv_t *vdrv_priv = vdrv_get_vdevpriv(vdev);
	vuihandler_t *vuihandler = &vuihandler_priv->vuihandler;
	vuihandler_rx_response_t *ring_rsp = vuihandler_rx_new_ring_response(&vuihandler->rx_rings.ring);

	size_t size = vuihandler_pkt_size - VUIHANDLER_BT_PKT_HEADER_SIZE;

	ring_rsp->id = vdrv_priv->recv_count;
	ring_rsp->size = size;
	ring_rsp->type = vuihandler_pkt->type;

	DBG(VUIHANDLER_PREFIX "id: %d, type %d, size %d\n", ring_rsp->id, ring_rsp->type, ring_rsp->size);

	memset(ring_rsp->buf, '\0', RING_BUF_SIZE);
	if (size != 0)
		memcpy(ring_rsp->buf, vuihandler_pkt->payload, size);


	vuihandler_rx_ring_response_ready(&vuihandler->rx_rings.ring);

	notify_remote_via_virq(vuihandler->rx_rings.irq);

	vdrv_priv->recv_count++;
}

/** Handles and route the packet coming from the tablet that are 
 * destined to the agency core, such as injector packets or update packets 
*/
void handle_agency_packet(vuihandler_pkt_t *vuihandler_pkt, size_t vuihandler_pkt_size) {
	uint32_t ME_size;
	uint8_t *payload = (uint8_t *) &(vuihandler_pkt->payload);
	
	switch (vuihandler_pkt->type) {
	case VUIHANDLER_ME_SIZE:
		ME_size = *((uint32_t *)(payload));
		DBG("ME size: %u\n", ME_size);
		
		/* Forward the size to the injector so it knows the ME size. */
		injector_prepare(ME_size);
		break;

	/* This is the ME data which needs to be forwarded to the Injector */
	case VUIHANDLER_ME_INJECT:
		DBG("Injecting a %u (%u) ME chunk\n", vuihandler_pkt_size, vuihandler_pkt_size-VUIHANDLER_BT_PKT_HEADER_SIZE);
		injector_receive_ME((void *)(payload), vuihandler_pkt_size-VUIHANDLER_BT_PKT_HEADER_SIZE);
		break;
	}
}

/**
 * Ask the agency for the XML ME list and put it in the TX buffer
 * 
 * 
 * @return 0 on success, -1 on error
*/
int send_ME_model_xml(int slotID) {
	vuihandler_pkt_t vuihandler_pkt;

	vuihandler_pkt.type = VUIHANDLER_SELECT;
	vuihandler_pkt.slotID = slotID;

	rx_push_response(slotID, &vuihandler_pkt, VUIHANDLER_BT_PKT_HEADER_SIZE);

	return 0;
}

/**
 * Ask the agency for the XML ME list and put it in the TX buffer
 * 
 * 
 * @return 0 on success, -1 on error
*/
int send_ME_list_xml(void) {
	ME_id_t *ME_buf_raw = kzalloc(MAX_ME_DOMAINS * sizeof(ME_id_t), GFP_ATOMIC);
	uint8_t *ME_buf_xml = NULL; /* will be allocated by xml_prepare_id_array */

	get_ME_id_array(ME_buf_raw);
	ME_buf_xml = xml_prepare_id_array(ME_buf_raw);

	if (ME_buf_xml == NULL) {
		return -1;
	}
	
	tx_buffer_put(ME_buf_xml, strlen(ME_buf_xml)+1, 0, VUIHANDLER_ASK_LIST);

	/* release the buffers */
	kfree(ME_buf_xml);
	kfree(ME_buf_raw);

	return 0;
}


void vuihandler_recv(vuihandler_pkt_t *vuihandler_pkt, size_t vuihandler_pkt_size) {
	size_t size;
	int32_t me_id;

	DBG("Receieved a packet of type %d for slotID %d\n", vuihandler_pkt->type, vuihandler_pkt->slotID);

	/* Check for packet destinated to to agency, mainly ME injection related */
	if (vuihandler_pkt->type == VUIHANDLER_ME_SIZE || vuihandler_pkt->type == VUIHANDLER_ME_INJECT) {
		handle_agency_packet(vuihandler_pkt, vuihandler_pkt_size);
		return;
	}

	/* Ask for the ME list */
	if (vuihandler_pkt->type == VUIHANDLER_ASK_LIST) {
		/* This is a vUIHandler beacon */
		send_ME_list_xml();
		return ;
	}

	/* Ask for a ME model */
	if (vuihandler_pkt->type == VUIHANDLER_SELECT) {
		/* This is a vUIHandler select ME  */
		send_ME_model_xml(vuihandler_pkt->slotID);
		return ;
	}

	/* We expect the BT packet to be a vUIHandler data packet or an event packet */
	if (!(vuihandler_pkt->type == VUIHANDLER_DATA || vuihandler_pkt->type == VUIHANDLER_POST))
		return ;

	/* From there, the packet is forwarded to the corresponding ME */
	size = vuihandler_pkt_size - VUIHANDLER_BT_PKT_HEADER_SIZE;

	if (unlikely(size > VUIHANDLER_MAX_PAYLOAD_SIZE))
		return ;

	me_id = vuihandler_pkt->slotID;

	/* Here we want to ensure that a correct ME is targeted */
	if (me_id < 0) 
		return;
	/* Here, the packet is sent to the ME, which will process it accordingly */
	rx_push_response(me_id, vuihandler_pkt, vuihandler_pkt_size);
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
			lprintk("An error occured while getting the next TX packet!\n");
			BUG();
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
			DBG("(B>%d)", pkt_size);
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

		vuihandler_recv((vuihandler_pkt_t *)priv_buffer, size);
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



#ifdef TEST_RX
static int test_rx_fn(void *args) {
	vuihandler_pkt_t *test_pkt;
	vuihandler_t *vuihandler = (vuihandler_t *) args;
	char *msg = "Hello from BE!";
	
	test_pkt = (vuihandler_pkt_t *) kzalloc(sizeof(vuihandler_pkt_t) + VUIHANDLER_MAX_PKT_SIZE, GFP_KERNEL);
	memset(test_pkt, 0, sizeof(vuihandler_pkt_t) + VUIHANDLER_MAX_PKT_SIZE);

	test_pkt->slotID = vuihandler->otherend_id;
	test_pkt->type = VUIHANDLER_DATA;
	memcpy(test_pkt->payload, msg, strlen(msg));

	while(1) {
		msleep(2000);
		vuihandler_recv(test_pkt, sizeof(vuihandler_pkt_t) + strlen(msg));
	}

	return 0;
}
#endif


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
	vuihandler->otherend_id = 0;

	DBG(VUIHANDLER_PREFIX "Backend closed: %d\n", vdev->otherend_id);
}

void vuihandler_suspend(struct vbus_device *vdev) {
	DBG(VUIHANDLER_PREFIX "Backend suspend: %d\n", vdev->otherend_id);

}


void vuihandler_resume(struct vbus_device *vdev) {
	DBG(VUIHANDLER_PREFIX "Backend resume: %d\n", vdev->otherend_id);
}


void vuihandler_connected(struct vbus_device *vdev) {
	vuihandler_priv_t *vuihandler_priv = (vuihandler_priv_t *) dev_get_drvdata(&vdev->dev);
	
	vuihandler_priv->vuihandler.otherend_id = vdev->otherend_id;

#ifdef TEST_RX
	if (!test_thread_launched) {
		test_thread_launched = true;
		kthread_run(test_rx_fn, &vuihandler_priv->vuihandler, "vuihandler-test-RX");
	}
#endif
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


/**
 * Start the communication threads. 
 */
static void vuihandler_start_threads(void *args) {

	kthread_run(tx_task_fn, args, "vUIHandler-TX");
	kthread_run(rx_bt_task_fn, args, "vUIHandler-BT-RX");

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
