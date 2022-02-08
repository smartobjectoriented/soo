/*
 * Copyright (C) 2018-2019 Baptiste Delporte <bonel@bonel.net>
 * Copyright (C) 2018-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
 * Copyright (C) 2020 David Truan <david.truan@heig-vd.ch>
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
#include <soo/uapi/debug.h>
#include <soo/uapi/injector.h>

#include <soo/soolink/soolink.h>

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

/* Null SPID */
uint8_t vuihandler_null_spid[SPID_SIZE] = { 0 };

/* List that holds the connected remote applications */
static vuihandler_connected_app_t connected_app;
static spinlock_t connected_app_lock;

/*
 * PID of the process that holds /dev/rfcommX
 * It is also used to check if a remote tablet/smartphone is connected.
 * */
static int rfcomm_tty_pid = 0;
static struct mutex rfcomm_lock;

/* Received BT packet counter */
static uint32_t recv_count = 0;

static vuihandler_pkt_t *tx_vuihandler_pkt;
static size_t tx_pkt_size = 0;
static spinlock_t tx_lock;
static struct completion tx_completion;

static struct completion keepalive_completion;

static sl_desc_t *vuihandler_bt_sl_desc;
#if defined(CONFIG_SOOLINK_PLUGIN_ETHERNET)
static sl_desc_t *vuihandler_tcp_sl_desc;
#endif /* CONFIG_SOOLINK_PLUGIN_ETHERNET */

/* Beacon for the connect response operation */
static vuihandler_pkt_t *connect_rsp_pkt;

/* The payload of a beacon has only one byte */
static size_t connect_rsp_pkt_size = sizeof(vuihandler_pkt_t) + 1;

/* Beacon for the ping operation */
static vuihandler_pkt_t *ping_pkt;

/* The payload of a beacon has only one byte */
static size_t ping_pkt_size = sizeof(vuihandler_pkt_t) + 1;

/* Statically allocated array which saves the vbus_devices to be able to retrieve all
   vuihandler struct after they got created. */
static struct vbus_device *vdevs[MAX_DOMAINS] = { NULL };

/**
 * Return the SPID of the ME whose "otherend ID" is given as parameter.
 * Return the pointer to the SPID on success.
 * If there is no such ME in this slot, return NULL.
 */
static uint8_t *get_spid_from_otherend_id(int otherend_id) {
	vuihandler_t *vuihandler;

	if (vdevs[otherend_id] == NULL) {
		return NULL;
	}

	vuihandler = to_vuihandler(vdevs[otherend_id]);

	if (memcmp(vuihandler->spid, vuihandler_null_spid, SPID_SIZE) != 0)
		return vuihandler->spid;
	else
		return NULL;
}

/**
 * Return the "otherend ID" of the ME whose SPID is given as parameter.
 * Return the ID on success.
 * If there is no such ME, return -ENOENT.
 */
static int get_otherend_id_from_spid(uint8_t *spid) {
	uint32_t i;
	vuihandler_t *vuihandler;
	for (i = 1; i < MAX_DOMAINS; i++) {
		if (vdevs[i] == NULL)
			continue;
	

		vuihandler = to_vuihandler(vdevs[i]);

		if (!memcmp(spid, vuihandler->spid, SPID_SIZE))
			return i;
	}

	return -ENOENT;
}

/**
 * tx_ring interrupt.
 */
irqreturn_t vuihandler_tx_interrupt(int irq, void *dev_id) {
	struct vbus_device *dev = (struct vbus_device *) dev_id;
	vuihandler_t *vuihandler = to_vuihandler(dev);
	RING_IDX i, rp;
	vuihandler_tx_request_t *ring_req;
	uint8_t *spid;

	while ((ring_req = vuihandler_tx_ring_request(&vuihandler->tx_rings.ring)) != NULL) {
		//ring_req = RING_GET_REQUEST(&vuihandler->tx_rings.ring, i);

		DBG(VUIHANDLER_PREFIX "%d, %d\n", ring_req->id, ring_req->size);

		/* The slot ID is equal to the otherend ID */
		if ((spid = get_spid_from_otherend_id(dev->otherend_id)) == NULL)
			continue;

		/* Only send packets that are adapted to the tablet application */
		if (memcmp(connected_app.spid, spid, SPID_SIZE) != 0)
			continue;

		spin_lock(&tx_lock);
		tx_pkt_size = VUIHANDLER_BT_PKT_HEADER_SIZE + ring_req->size;
		memcpy(tx_vuihandler_pkt->spid, spid, SPID_SIZE);
		memcpy(tx_vuihandler_pkt->payload, vuihandler->tx_buffers.data + (ring_req->id % VUIHANDLER_MAX_PACKETS) * VUIHANDLER_MAX_PKT_SIZE, ring_req->size);
		tx_vuihandler_pkt->type = VUIHANDLER_DATA;
		spin_unlock(&tx_lock);

		complete(&tx_completion);
	}


	return IRQ_HANDLED;
}

/**
 * rx_ring interrupt. The RX ring should not be used in this direction.
 */
irqreturn_t vuihandler_rx_interrupt(int irq, void *dev_id) {
	struct vbus_device *dev = (struct vbus_device *) dev_id;
	vuihandler_t *vuihandler = to_vuihandler(dev);
	RING_IDX i, rp;
	vuihandler_rx_request_t *ring_req;

	/* Just consume the requests */
	while ((ring_req = vuihandler_rx_ring_request(&vuihandler->rx_rings.ring)) != NULL);

	return IRQ_HANDLED;
}

/**
 * Send a signal to the process holding the /dev/rfcommX entry.
 */
void rfcomm_send_sigterm(void) {
	mutex_lock(&rfcomm_lock);

	if (rfcomm_tty_pid) {
		DBG(VUIHANDLER_PREFIX "Send SIGTERM to rfcomm (%d) in user space\n", rfcomm_tty_pid);

		sys_kill(rfcomm_tty_pid, SIGTERM);
		rfcomm_tty_pid = 0;
	}

	mutex_unlock(&rfcomm_lock);
}

/**
 * Update the connected application SPID.
 */
void vuihandler_update_spid_vbstore(uint8_t spid[SPID_SIZE]) {
	uint32_t i;
	char connected_app_spid[3 * SPID_SIZE];
	char spid_digit[3];
	struct vbus_transaction vbt;

	for (i = 0; i < SPID_SIZE; i++) {
		sprintf(spid_digit, "%02x", spid[i]);
		memcpy(&connected_app_spid[3 * i], spid_digit, 2);
		connected_app_spid[3 * i + 2] = ':';
	}
	connected_app_spid[3 * SPID_SIZE - 1] = '\0';

	DBG(VUIHANDLER_PREFIX "New connected_app_spid: %s\n", connected_app_spid);

	vbus_transaction_start(&vbt);
	vbus_printf(vbt, VUIHANDLER_APP_VBSTORE_DIR, VUIHANDLER_APP_VBSTORE_NODE, "%s", connected_app_spid);
	vbus_transaction_end(vbt);

	/* Set the device capability associated to the connected remote application */
	devaccess_set_devcaps(DEVCAPS_CLASS_APP, DEVCAP_APP_BLIND, !memcmp(spid, SOO_blind_spid, SPID_SIZE));
	devaccess_set_devcaps(DEVCAPS_CLASS_APP, DEVCAP_APP_OUTDOOR, !memcmp(spid, SOO_outdoor_spid, SPID_SIZE));
}

/**
 * Process a received vUIHandler beacon.
 */
static void recv_beacon(vuihandler_pkt_t *vuihandler_pkt, size_t vuihandler_pkt_size) {
	unsigned long flags;

	DBG0(VUIHANDLER_PREFIX "Recv beacon\n");

	/* A beacon currently contains the SPID of the ME targeted by the remote application */

	/* IsSOO command */
	if (vuihandler_pkt->payload[0] == '?') {
		DBG0(VUIHANDLER_PREFIX "IsSOO beacon\n");

		/*
		 * Temporarily set the SPID to 0xff..0xff.
		 * - The SPID is not null so that the watchdog is active.
		 * - This is a way to tell there is a connected tablet/smartphone with no particular
		 *   app type (blind, outdoor).
		 */
		spin_lock_irqsave(&connected_app_lock, flags);
		memset(connected_app.spid, 0xff, SPID_SIZE);
		spin_unlock_irqrestore(&connected_app_lock, flags);

		spin_lock_irqsave(&tx_lock, flags);
		tx_pkt_size = connect_rsp_pkt_size;
		tx_vuihandler_pkt->type = connect_rsp_pkt->type;
		memcpy(tx_vuihandler_pkt->spid, connect_rsp_pkt->spid, SPID_SIZE);
		memcpy(tx_vuihandler_pkt->payload, connect_rsp_pkt->payload, connect_rsp_pkt_size - sizeof(vuihandler_pkt_t));
		spin_unlock_irqrestore(&tx_lock, flags);

		complete(&tx_completion);

		return ;
	}

	/* Ping response beacon (keepalive) */
	if (vuihandler_pkt->payload[0] == '!') {
		DBG0(VUIHANDLER_PREFIX "Ping response beacon\n");

		complete(&keepalive_completion);

		return ;
	}

	/* Explicit disconnection command */
	if (vuihandler_pkt->payload[0] == 'C') {
		DBG0(VUIHANDLER_PREFIX "Connect beacon\n");

		spin_lock_irqsave(&connected_app_lock, flags);
		memcpy(connected_app.spid, vuihandler_pkt->spid, SPID_SIZE);
		spin_unlock_irqrestore(&connected_app_lock, flags);

		/* Hard irqs must be enabled */
		vuihandler_update_spid_vbstore(connected_app.spid);

		return ;
	}

	/* Explicit disconnection command */
	if (vuihandler_pkt->payload[0] == 'D') {
		DBG0(VUIHANDLER_PREFIX "Disconnect beacon\n");

		spin_lock_irqsave(&connected_app_lock, flags);
		memcpy(connected_app.spid, vuihandler_null_spid, SPID_SIZE);
		spin_unlock_irqrestore(&connected_app_lock, flags);

		/* Hard irqs must be enabled */
		vuihandler_update_spid_vbstore(vuihandler_null_spid);

#if defined(CONFIG_BT_RFCOMM)
		/* Send SIGTERM to the current rfcomm instance if no application is detected */
		rfcomm_send_sigterm();
#endif /* CONFIG_BT_RFCOMM */

		return ;
	}

	lprintk("Unknown beacon\n");
	BUG();
}

static void rx_push_response(domid_t domid, vuihandler_pkt_t *vuihandler_pkt, size_t vuihandler_pkt_size) {
	vuihandler_t *vuihandler = to_vuihandler(vdevs[domid]); 
	vuihandler_rx_response_t *ring_rsp = vuihandler_rx_ring_response(&vuihandler->rx_rings.ring);
	size_t size = vuihandler_pkt_size - VUIHANDLER_BT_PKT_HEADER_SIZE;
	void *data = vuihandler_pkt->payload;

	ring_rsp->id = recv_count;
	ring_rsp->size = size;

	memcpy(vuihandler->rx_buffers.data + (ring_rsp->id % VUIHANDLER_MAX_PACKETS) * VUIHANDLER_MAX_PKT_SIZE, data, size);

	DBG(VUIHANDLER_PREFIX "id: %d\n", ring_rsp->id);

	vuihandler_rx_ring_response_ready(&vuihandler->rx_rings.ring);

	notify_remote_via_virq(vuihandler->rx_rings.irq);

	recv_count++;
}

void vuihandler_recv(vuihandler_pkt_t *vuihandler_pkt, size_t vuihandler_pkt_size) {
	size_t size, ME_size;
	int me_id;
	uint8_t *ME_pkt_payload;

	/* This is the ME size (1B type + 4B size) */
	if (vuihandler_pkt->type == VUIHANDLER_ME_SIZE) {
		ME_pkt_payload = (uint8_t *)vuihandler_pkt;

		DBG("ME size: %u\n", *((uint32_t *)(ME_pkt_payload+1)));
		ME_size = *((uint32_t *)(ME_pkt_payload+1));
		
#warning This may not be needed anymore but keep it for now as we may fallback to the non-block ME reception		
		/* Forward the size to the injector so it knows the ME size. */
		injector_prepare(ME_size);
		vfree((void *) ME_pkt_payload);
		return;
	}
	/* This is the ME data which needs to be forwarded to the Injector */
	if (vuihandler_pkt->type == VUIHANDLER_ME_INJECT) {
		/* As we bypass the full vuiHandler protocol, we first use a uint8_t array to
		easily access the data instead of the vuihandler_pkt */
		ME_pkt_payload = (uint8_t *)vuihandler_pkt;

		injector_receive_ME((void *)(ME_pkt_payload+1), vuihandler_pkt_size-1);

		/* We don't free the received buffer here as it the Core needs to read it 
		first. The free is done in the Injector once the userspace finished reading it. */
		return ;
	}


	if (vuihandler_pkt->type == VUIHANDLER_BEACON) {
		/* This is a vUIHandler beacon */
		recv_beacon(vuihandler_pkt, vuihandler_pkt_size);

		return ;
	}

	/* We expect the BT packet to be a vUIHandler data packet */
	if (vuihandler_pkt->type != VUIHANDLER_DATA)
		return ;

	size = vuihandler_pkt_size - VUIHANDLER_BT_PKT_HEADER_SIZE;

	if (unlikely(size > VUIHANDLER_MAX_PAYLOAD_SIZE))
		return ;

	DBG(VUIHANDLER_PREFIX "Size: %d\n", size);

	/* Find the ME targeted by the packet using its SPID */
	me_id = get_otherend_id_from_spid(vuihandler_pkt->spid);
	if (me_id < 0)
		return ;

	DBG(VUIHANDLER_PREFIX "ME ID: %d\n", me_id);

	rx_push_response(me_id, vuihandler_pkt, vuihandler_pkt_size);
}

/**
 * TX task.
 * BT and TCP/IP interfaces are mutually exclusive, thus a common thread is sufficient.
 */
static int tx_task_fn(void *arg) {
	vuihandler_pkt_t *vuihandler_pkt;
	size_t pkt_size;
	unsigned long flags;
	int rfcomm_pid;

	while (1) {
		wait_for_completion(&tx_completion);

		/* Retrieve parameters from TX ISR */
		spin_lock_irqsave(&tx_lock, flags);
		vuihandler_pkt = tx_vuihandler_pkt;
		pkt_size = tx_pkt_size;
		spin_unlock_irqrestore(&tx_lock, flags);

		/* Priority is not supported yet */
		mutex_lock(&rfcomm_lock);
		rfcomm_pid = rfcomm_tty_pid;
		mutex_unlock(&rfcomm_lock);

		if (rfcomm_pid) {
			lprintk("(B>%d)", pkt_size);
			sl_send(vuihandler_bt_sl_desc, vuihandler_pkt, pkt_size, get_null_agencyUID(), 0);
		}
#if defined(CONFIG_SOOLINK_PLUGIN_ETHERNET)
		else {
			lprintk("(T>%d)", pkt_size);
			sl_send(vuihandler_tcp_sl_desc, vuihandler_pkt, pkt_size, get_null_agencyUID(), 0);
		}
#endif /* CONFIG_SOOLINK_PLUGIN_ETHERNET */
	}

	return 0;
}

/**
 * RX task for the BT interface.
 */
static int rx_bt_task_fn(void *arg) {
	size_t size;
	void *priv_buffer = NULL;

	while (1) {
		size = sl_recv(vuihandler_bt_sl_desc, &priv_buffer);

		lprintk("(B<%d)", size);

		vuihandler_recv(priv_buffer, size);
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

	while (1) {
		size = sl_recv(vuihandler_tcp_sl_desc, &priv_buffer);

		lprintk("(T<%d)", size);

		vuihandler_recv(priv_buffer, size);
	}

	return 0;
}
#endif /* CONFIG_SOOLINK_PLUGIN_ETHERNET */

/**
 * Interface with RFCOMM.
 */
void vuihandler_open_rfcomm(pid_t pid) {

	mutex_lock(&rfcomm_lock);
	rfcomm_tty_pid = pid;
	mutex_unlock(&rfcomm_lock);
}

/**
 * Watchdog thread that detects a dead (disconnected) remote application and updates the connected
 * application SPID accordingly.
 */
static int connected_app_watchdog_fn(void *arg) {
	int ret;
	unsigned long flags;
	uint32_t failed_ping_count = 0;
	bool update_spid = false;
	uint8_t spid[SPID_SIZE];

	while (1) {
		msleep(VUIHANDLER_APP_WATCH_PERIOD);

		spin_lock_irqsave(&connected_app_lock, flags);
		memcpy(spid, connected_app.spid, SPID_SIZE);
		spin_unlock_irqrestore(&connected_app_lock, flags);

		/* If no tablet/smartphone is connected, just loop */
		if (!memcmp(spid, vuihandler_null_spid, SPID_SIZE))
			continue;

		failed_ping_count = 0;

send_ping:
		spin_lock_irqsave(&tx_lock, flags);
		tx_pkt_size = ping_pkt_size;
		tx_vuihandler_pkt->type = ping_pkt->type;
		memcpy(tx_vuihandler_pkt->spid, ping_pkt->spid, SPID_SIZE);
		memcpy(tx_vuihandler_pkt->payload, ping_pkt->payload, ping_pkt_size - sizeof(vuihandler_pkt_t));
		spin_unlock_irqrestore(&tx_lock, flags);

		complete(&tx_completion);

		DBG0("PING\n");

		if ((ret = wait_for_completion_interruptible_timeout(&keepalive_completion, msecs_to_jiffies(VUIHANDLER_APP_RSP_TIMEOUT))) <= 0) {
			DBG0(VUIHANDLER_PREFIX "No keepalive beacon received, count = %d\n", failed_ping_count);

			/* Send the ping again */
			if (failed_ping_count < MAX_FAILED_PING_COUNT) {
				failed_ping_count++;
				goto send_ping;
			}

			spin_lock_irqsave(&connected_app_lock, flags);

			/* Test if the current SPID is not NULL. If so, a notification will be sent to the ME. */
			update_spid = (memcmp(connected_app.spid, vuihandler_null_spid, SPID_SIZE) != 0);

			memcpy(connected_app.spid, vuihandler_null_spid, SPID_SIZE);
			memcpy(spid, vuihandler_null_spid, SPID_SIZE);

			spin_unlock_irqrestore(&connected_app_lock, flags);

			/* Hard irqs must be enabled */
			if (update_spid)
				vuihandler_update_spid_vbstore(spid);

#if defined(CONFIG_BT_RFCOMM)
			/* Send SIGTERM to the current rfcomm instance if no application is detected */
			rfcomm_send_sigterm();
#endif /* CONFIG_BT_RFCOMM */

			continue;
		}

		DBG0(VUIHANDLER_PREFIX "Keepalive beacon received\n");
	}

	return 0;
}


/**
 * Start the communication threads. 
 */
static void vuihandler_start_threads(void) {

	kthread_run(tx_task_fn, NULL, "vUIHandler-TX");
	kthread_run(rx_bt_task_fn, NULL, "vUIHandler-BT-RX");

#if defined(CONFIG_SOOLINK_PLUGIN_ETHERNET)
	kthread_run(rx_tcp_task_fn, NULL, "vUIHandler-TCP-RX");
#endif /* CONFIG_SOOLINK_PLUGIN_ETHERNET */
}

void vuihandler_probe(struct vbus_device *vdev) {
	vuihandler_t *vuihandler;

	vuihandler = kzalloc(sizeof(vuihandler_t), GFP_ATOMIC);
	BUG_ON(!vuihandler);

	dev_set_drvdata(&vdev->dev, &vuihandler->vdevback);

	vdevs[vdev->otherend_id] = vdev;

	DBG(VUIHANDLER_PREFIX "Backend probe: %d\n", vdev->otherend_id);
}

void vuihandler_remove(struct vbus_device *vdev) {
	vuihandler_t *vuihandler = to_vuihandler(vdev);

	DBG("%s: freeing the vuihandler structure for %s\n", __func__,vdev->nodename);
	kfree(vuihandler);
}

void vuihandler_close(struct vbus_device *vdev) {
	vuihandler_t *vuihandler = to_vuihandler(vdev);
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
	memcpy(vuihandler->spid, vuihandler_null_spid, SPID_SIZE);

}

void vuihandler_suspend(struct vbus_device *vdev) {
	DBG(VUIHANDLER_PREFIX "Backend suspend: %d\n", vdev->otherend_id);

}

void vuihandler_resume(struct vbus_device *vdev) {
	DBG(VUIHANDLER_PREFIX "Backend resume: %d\n", vdev->otherend_id);
}

void vuihandler_connected(struct vbus_device *vdev) {

	DBG(VDUMMY_PREFIX "Backend connected: %d\n",vdev->otherend_id);
}

void vuihandler_reconfigured(struct vbus_device *vdev) {
	vuihandler_t *vuihandler = to_vuihandler(vdev);
	int res;
	unsigned long tx_ring_ref, rx_ring_ref;
	unsigned int tx_evtchn, rx_evtchn;
	vuihandler_tx_sring_t *tx_sring;
	vuihandler_tx_ring_t *tx_ring = &vuihandler->tx_rings;
	vuihandler_rx_sring_t *rx_sring;
	vuihandler_rx_ring_t *rx_ring = &vuihandler->rx_rings;

	/* tx_ring */

	vbus_gather(VBT_NIL, vdev->otherend, "tx_ring-ref", "%lu", &tx_ring_ref, "tx_ring-evtchn", "%u", &tx_evtchn, NULL);

	res = vbus_map_ring_valloc(vdev, tx_ring_ref, (void **) &tx_sring);
	BUG_ON(res < 0);

	SHARED_RING_INIT(tx_sring);
	BACK_RING_INIT(&tx_ring->ring, tx_sring, PAGE_SIZE);

	res = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, tx_evtchn, vuihandler_tx_interrupt, NULL, 0, VUIHANDLER_NAME "-tx", vdev);
	BUG_ON(res < 0);

	tx_ring->irq = res;

	/* rx_ring */

	vbus_gather(VBT_NIL, vdev->otherend, "rx_ring-ref", "%lu", &rx_ring_ref, "rx_ring-evtchn", "%u", &rx_evtchn, NULL);

	res = vbus_map_ring_valloc(vdev, rx_ring_ref, (void **) &rx_sring);
	BUG_ON(res < 0);

	SHARED_RING_INIT(rx_sring);
	BACK_RING_INIT(&rx_ring->ring, rx_sring, PAGE_SIZE);

	res = bind_interdomain_evtchn_to_virqhandler(vdev->otherend_id, rx_evtchn, vuihandler_rx_interrupt, NULL, 0, VUIHANDLER_NAME "-rx", vdev);
	BUG_ON(res < 0);

	rx_ring->irq = res;
	DBG(VUIHANDLER_PREFIX "Backend reconfigured: %d\n", vdev->otherend_id);
}


vdrvback_t vuihandlerdrv = {
	.probe = vuihandler_probe,
	.remove = vuihandler_remove,
	.close = vuihandler_close,
	.connected = vuihandler_connected,
	.reconfigured = vuihandler_reconfigured,
	.resume = vuihandler_resume,
	.suspend = vuihandler_suspend
};

int vuihandler_init(void) {
	uint32_t i;
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "vuihandler,backend");

	/* Check if DTS has vuihandler enabled */
	if (!of_device_is_available(np))
		return 0;

	// for (i = 0; i < MAX_DOMAINS; i++)
	// 	memset(vuihandler.spid[i], 0, SPID_SIZE);

	memcpy(connected_app.spid, vuihandler_null_spid, SPID_SIZE);
	spin_lock_init(&connected_app_lock);

	kthread_run(connected_app_watchdog_fn, NULL, "vUIHandler-watch");

	//vuihandler_vbus_init();

	tx_vuihandler_pkt = (vuihandler_pkt_t *) kzalloc(sizeof(vuihandler_pkt_t) + VUIHANDLER_MAX_PKT_SIZE, GFP_KERNEL);
	spin_lock_init(&tx_lock);
	init_completion(&tx_completion);
	init_completion(&keepalive_completion);

	mutex_init(&rfcomm_lock);

	/* Connect response packet */
	connect_rsp_pkt = (vuihandler_pkt_t *) kzalloc(sizeof(vuihandler_pkt_t) + VUIHANDLER_MAX_PKT_SIZE, GFP_KERNEL);
	connect_rsp_pkt->type = VUIHANDLER_BEACON;
	memcpy(connect_rsp_pkt->spid, vuihandler_null_spid, SPID_SIZE);
	connect_rsp_pkt->payload[0] = '!';

	/* Ping packet */
	ping_pkt = (vuihandler_pkt_t *) kzalloc(sizeof(vuihandler_pkt_t) + VUIHANDLER_MAX_PKT_SIZE, GFP_KERNEL);
	ping_pkt->type = VUIHANDLER_BEACON;
	memcpy(ping_pkt->spid, vuihandler_null_spid, SPID_SIZE);
	ping_pkt->payload[0] = '?';

	/* Register with Soolink */
	vuihandler_bt_sl_desc = sl_register(SL_REQ_BT, SL_IF_BT, SL_MODE_UNICAST);

#if defined(CONFIG_SOOLINK_PLUGIN_ETHERNET)
	vuihandler_tcp_sl_desc = sl_register(SL_REQ_TCP, SL_IF_TCP, SL_MODE_UNICAST);
#endif /* CONFIG_SOOLINK_PLUGIN_ETHERNET */


	/* We need to start the threads here as they need a reference on vuihandler */
	vuihandler_start_threads();

	/* Set the associated dev capability */
	devaccess_set_devcaps(DEVCAPS_CLASS_COMM, DEVCAP_COMM_UIHANDLER, true);

	vdevback_init(VUIHANDLER_NAME, &vuihandlerdrv);

	return 0;
}

device_initcall(vuihandler_init);
