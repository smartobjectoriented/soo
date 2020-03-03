
/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
 * Copyright (C) 2016-2017 Baptiste Delporte <bonel@bonel.net>
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
#include <linux/atomic.h>
#include <linux/ipipe_base.h>
#include <linux/spinlock.h>

#include <xenomai/rtdm/driver.h>

#include <asm/cacheflush.h>

#include <soo/soolink/soolink.h>

#include <soo/soolink/plugin/common.h>
#include <soo/soolink/plugin/loopback.h>
#include <soo/soolink/plugin/ethernet.h>
#include <soo/soolink/plugin/bluetooth.h>
#include <soo/soolink/plugin/wlan.h>

#include <soo/vbstore.h>
#include <soo/hypervisor.h>
#include <soo/uapi/console.h>
#include <soo/uapi/debug.h>
#include <soo/hypervisor.h>
#include <soo/uapi/soo.h>
#include <soo/evtchn.h>
#include <soo/uapi/avz.h>

#include <rtdm/sdio_ops.h>
#include <soo/uapi/soo.h>

#include <linux/sched/task.h>

static rtdm_event_t rtdm_dc_stable_event[DC_EVENT_MAX];

static rtdm_event_t dc_isr_event;

static rtdm_irq_t vbus_vbstore_irq_handle, dc_irq_handle;

static rtdm_task_t rtdm_dc_isr_task;
static rtdm_task_t rtdm_vbus_task;

#ifdef CONFIG_SOOLINK_PLUGIN_WLAN

static rtdm_event_t rtdm_sl_wlan_send_event;
static rtdm_event_t rtdm_sl_wlan_recv_event;
static rtdm_event_t rtdm_sl_plugin_wlan_rx_event;

static rtdm_task_t rtdm_sl_wlan_send_task;
static rtdm_task_t rtdm_sl_wlan_recv_task;
static rtdm_task_t rtdm_sl_plugin_wlan_rx_task;

#endif /* CONFIG_SOOLINK_PLUGIN_WLAN */

#ifdef CONFIG_SOOLINK_PLUGIN_ETHERNET

static rtdm_event_t rtdm_sl_eth_send_event;
static rtdm_event_t rtdm_sl_eth_recv_event;
static rtdm_event_t rtdm_sl_plugin_ethernet_rx_event;
static rtdm_event_t rtdm_sl_plugin_tcp_rx_event;

static rtdm_task_t rtdm_sl_eth_send_task;
static rtdm_task_t rtdm_sl_eth_recv_task;
static rtdm_task_t rtdm_sl_plugin_eth_rx_task;

static rtdm_event_t rtdm_sl_tcp_send_event;
static rtdm_event_t rtdm_sl_tcp_recv_event;
static rtdm_task_t rtdm_sl_tcp_send_task;
static rtdm_task_t rtdm_sl_tcp_recv_task;
static rtdm_task_t rtdm_sl_plugin_tcp_rx_task;

#endif /* CONFIG_SOOLINK_PLUGIN_ETHERNET */

#ifdef CONFIG_SOOLINK_PLUGIN_BLUETOOTH

static rtdm_event_t rtdm_sl_bt_send_event;
static rtdm_event_t rtdm_sl_bt_recv_event;
static rtdm_event_t rtdm_sl_plugin_bluetooth_rx_event;

static rtdm_task_t rtdm_sl_bt_send_task;
static rtdm_task_t rtdm_sl_bt_recv_task;
static rtdm_task_t rtdm_sl_plugin_bt_rx_task;

#endif /* CONFIG_SOOLINK_PLUGIN_BLUETOOTH */

#ifdef CONFIG_SOOLINK_PLUGIN_LOOPBACK

static rtdm_event_t rtdm_sl_lo_send_event;
static rtdm_event_t rtdm_sl_lo_recv_event;
static rtdm_event_t rtdm_sl_plugin_loopback_rx_event;

static rtdm_task_t rtdm_sl_lo_send_task;
static rtdm_task_t rtdm_sl_lo_recv_task;
static rtdm_task_t rtdm_sl_plugin_loopback_rx_task;

#endif /* CONFIG_SOOLINK_PLUGIN_LOOPBACK */

/*
 * Used to keep track of the target domain for a certain (outgoing) dc_event.
 * Value -1 means no dc_event in progress.
 */
atomic_t rtdm_dc_outgoing_domID[DC_EVENT_MAX];

/*
 * Used to store the domID issuing a (incoming) dc_event
 */
atomic_t rtdm_dc_incoming_domID[DC_EVENT_MAX];

dc_event_fn_t *rtdm_dc_event_callback[DC_EVENT_MAX];

void rtdm_register_dc_event_callback(dc_event_t dc_event, dc_event_fn_t *callback) {
	rtdm_dc_event_callback[dc_event] = callback;
}

#ifdef CONFIG_ARCH_VEXPRESS
extern void propagate_interrupt_from_nonrt(void);
#endif

static void rtdm_dc_stable(int dc_event)
{
	atomic_set(&rtdm_dc_outgoing_domID[dc_event], -1);

	/* Wake up the waiter */
	rtdm_event_signal(&rtdm_dc_stable_event[dc_event]);
}

/*
 * Called to inform the non-RT side that we have completed a dc_event processing.
 */
void rtdm_tell_dc_stable(int dc_event)  {
	DBG("Now pinging domain %d back\n", DOMID_AGENCY);

	/* Make sure a previous transaction is not being processed */
	set_dc_event(AGENCY_CPU, dc_event);

	atomic_set(&rtdm_dc_incoming_domID[dc_event], -1);

	notify_remote_via_evtchn(dc_evtchn[DOMID_AGENCY]);

}

/*
 * Sends a ping event to a non-realtime agency in order to get synchronized.
 * Various types of event (dc_event) can be sent.
 *
 * @dc_event: type of event used in the synchronization
 */
void rtdm_do_sync_dom(domid_t domID, dc_event_t dc_event) {

	/* Ping the remote domain to perform the task associated to the DC event */
	DBG("%s: ping domain %d...\n", __func__, domID);

	/* Make sure a previous transaction is not being processed */
	while (atomic_cmpxchg(&rtdm_dc_outgoing_domID[dc_event], -1, domID) != -1) ;

	/* We avoid that a domain running on another CPU tries to update the dc_event field at the same time. */
	set_dc_event(domID, dc_event);

	notify_remote_via_evtchn(dc_evtchn[domID]);

	/* Wait for the response */
	rtdm_event_wait(&rtdm_dc_stable_event[dc_event]);
}

#ifdef CONFIG_SOOLINK_PLUGIN_WLAN

/*
 * Independent RT task for managing Soolink request (for sending).
 * We are out of the directcomm ISR (top & bottom half) context.
 * Since the rtdm_sl_send() function may interact with non-RT agency, and have a DC event ping-pong,
 * we keep free the non-RT to RT path.
 */
static void rtdm_sl_wlan_send_task_fn(void *arg)  {
	while (true) {
		rtdm_event_wait(&rtdm_sl_wlan_send_event);

		rtdm_propagate_sl_send();
		rtdm_tell_dc_stable(DC_SL_WLAN_SEND);
	}
}

static void rtdm_sl_wlan_recv_task_fn(void *arg)  {
	while (true) {
		rtdm_event_wait(&rtdm_sl_wlan_recv_event);

		rtdm_propagate_sl_recv();
		rtdm_tell_dc_stable(DC_SL_WLAN_RECV);
	}
}

static void rtdm_sl_plugin_wlan_rx_task_fn(void *arg)  {
	while (true) {
		rtdm_event_wait(&rtdm_sl_plugin_wlan_rx_event);
		rtdm_propagate_sl_plugin_wlan_rx();

		rtdm_tell_dc_stable(DC_PLUGIN_WLAN_RECV);
	}
}

#endif /* CONFIG_SOOLINK_PLUGIN_WLAN */

#ifdef CONFIG_SOOLINK_PLUGIN_ETHERNET
/*
 * Independent RT task for managing Soolink request (for sending).
 * We are out of the directcomm ISR (top & bottom half) context.
 * Since the rtdm_sl_send() function may interact with non-RT agency, and have a DC event ping-pong,
 * we keep free the non-RT to RT path.
 */

/* Low-level Ethernet handling */
static void rtdm_sl_eth_send_task_fn(void *arg)  {
	while (true) {
		rtdm_event_wait(&rtdm_sl_eth_send_event);

		rtdm_propagate_sl_send();
		rtdm_tell_dc_stable(DC_SL_ETH_SEND);
	}
}

static void rtdm_sl_eth_recv_task_fn(void *arg)  {
	while (true) {
		rtdm_event_wait(&rtdm_sl_eth_recv_event);

		rtdm_propagate_sl_recv();
		rtdm_tell_dc_stable(DC_SL_ETH_RECV);
	}
}

static void rtdm_sl_plugin_eth_rx_task_fn(void *arg)  {
	while (true) {
		rtdm_event_wait(&rtdm_sl_plugin_ethernet_rx_event);
		rtdm_propagate_sl_plugin_ethernet_rx();

		rtdm_tell_dc_stable(DC_PLUGIN_ETHERNET_RECV);
	}
}

/* TCP/IP over Ethernet */

static void rtdm_sl_tcp_send_task_fn(void *arg)  {
	while (true) {
		rtdm_event_wait(&rtdm_sl_tcp_send_event);

		rtdm_propagate_sl_send();
		rtdm_tell_dc_stable(DC_SL_TCP_SEND);
	}
}

static void rtdm_sl_tcp_recv_task_fn(void *arg)  {
	while (true) {
		rtdm_event_wait(&rtdm_sl_tcp_recv_event);

		rtdm_propagate_sl_recv();
		rtdm_tell_dc_stable(DC_SL_TCP_RECV);
	}
}

static void rtdm_sl_plugin_tcp_rx_task_fn(void *arg)  {
	while (true) {
		rtdm_event_wait(&rtdm_sl_plugin_tcp_rx_event);
		rtdm_propagate_sl_plugin_tcp_rx();

		rtdm_tell_dc_stable(DC_PLUGIN_TCP_RECV);
	}
}
#endif /* CONFIG_SOOLINK_PLUGIN_ETHERNET */

#ifdef CONFIG_SOOLINK_PLUGIN_BLUETOOTH
/*
 * Independent RT task for managing Soolink request (for sending).
 * We are out of the directcomm ISR (top & bottom half) context.
 * Since the rtdm_sl_send() function may interact with non-RT agency, and have a DC event ping-pong,
 * we keep free the non-RT to RT path.
 */
static void rtdm_sl_bt_send_task_fn(void *arg)  {
	while (true) {
		rtdm_event_wait(&rtdm_sl_bt_send_event);

		rtdm_propagate_sl_send();
		rtdm_tell_dc_stable(DC_SL_BT_SEND);
	}
}

static void rtdm_sl_bt_recv_task_fn(void *arg)  {
	while (true) {
		rtdm_event_wait(&rtdm_sl_bt_recv_event);

		rtdm_propagate_sl_recv();
		rtdm_tell_dc_stable(DC_SL_BT_RECV);
	}
}

static void rtdm_sl_plugin_bt_rx_task_fn(void *arg)  {
	while (true) {
		rtdm_event_wait(&rtdm_sl_plugin_bluetooth_rx_event);
		rtdm_propagate_sl_plugin_bluetooth_rx();

		rtdm_tell_dc_stable(DC_PLUGIN_BLUETOOTH_RECV);
	}
}
#endif /* CONFIG_SOOLINK_PLUGIN_BLUETOOTH */

#ifdef CONFIG_SOOLINK_PLUGIN_LOOPBACK
/*
 * Independent RT task for managing Soolink request (for sending).
 * We are out of the directcomm ISR (top & bottom half) context.
 * Since the rtdm_sl_send() function may interact with non-RT agency, and have a DC event ping-pong,
 * we keep free the non-RT to RT path.
 */
static void rtdm_sl_lo_send_task_fn(void *arg)  {
	while (true) {
		rtdm_event_wait(&rtdm_sl_lo_send_event);

		rtdm_propagate_sl_send();
		rtdm_tell_dc_stable(DC_SL_LO_SEND);
	}
}

static void rtdm_sl_lo_recv_task_fn(void *arg)  {
	while (true) {
		rtdm_event_wait(&rtdm_sl_lo_recv_event);

		rtdm_propagate_sl_recv()
		rtdm_tell_dc_stable(DC_SL_LO_RECV);
	}
}

static void rtdm_sl_plugin_loopback_rx_task_fn(void *arg)  {

	while (true) {
		rtdm_event_wait(&rtdm_sl_plugin_loopback_rx_event);
		rtdm_propagate_sl_plugin_loopback_rx();

		rtdm_tell_dc_stable(DC_PLUGIN_LOOPBACK_RECV);
	}
}
#endif /* CONFIG_SOOLINK_PLUGIN_LOOPBACK */



/*
 * Perform deferred processing related to directcomm DC_event processing
 */
static void rtdm_dc_isr_task_fn(void *arg) {
	dc_event_t dc_event;

	while (true) {
		rtdm_event_wait(&dc_isr_event);

		dc_event = atomic_read((const atomic_t *) &avz_shared_info->dc_event);

		/* Reset the dc_event now so that the domain can send another dc_event */
		atomic_set((atomic_t *) &avz_shared_info->dc_event, DC_NO_EVENT);

		/* Perform the associated callback function to this particular dc_event */
		if (rtdm_dc_event_callback[dc_event] != NULL)
			(*rtdm_dc_event_callback[dc_event])(dc_event);
		else {
			lprintk("%s: failure on dc_event %d, no callback function associated\n", __func__, dc_event);
			BUG();
		}

	}
}

/*
 * Retrieve the pointer to the directcomm ISR thread task
 */
rtdm_task_t *get_dc_isr(void) {
	return &rtdm_dc_isr_task;
}

/*
 * Proceed with the dc_event family dedeicated to the SL desc management.
 */
void rtdm_dc_sl_fn(dc_event_t dc_event) {

	switch (dc_event) {

#ifdef CONFIG_SOOLINK_PLUGIN_WLAN

	case DC_SL_WLAN_SEND:
		rtdm_event_signal(&rtdm_sl_wlan_send_event);
		break;

	case DC_SL_WLAN_RECV:
		rtdm_event_signal(&rtdm_sl_wlan_recv_event);
		break;
#endif

#ifdef CONFIG_SOOLINK_PLUGIN_ETHERNET

	case DC_SL_ETH_SEND:
		rtdm_event_signal(&rtdm_sl_eth_send_event);
		break;

	case DC_SL_ETH_RECV:
		rtdm_event_signal(&rtdm_sl_eth_recv_event);
		break;

	case DC_SL_TCP_SEND:
		rtdm_event_signal(&rtdm_sl_tcp_send_event);
		break;

	case DC_SL_TCP_RECV:
		rtdm_event_signal(&rtdm_sl_tcp_recv_event);
		break;
#endif

#ifdef CONFIG_SOOLINK_PLUGIN_BLUETOOTH

	case DC_SL_BT_SEND:
		rtdm_event_signal(&rtdm_sl_bt_send_event);
		break;

	case DC_SL_BT_RECV:
		rtdm_event_signal(&rtdm_sl_bt_recv_event);
		break;
#endif

#ifdef CONFIG_SOOLINK_PLUGIN_LOOPBACK

	case DC_SL_LO_SEND:
		rtdm_event_signal(&rtdm_sl_lo_send_event);
		break;

	case DC_SL_LO_RECV:
		rtdm_event_signal(&rtdm_sl_lo_recv_event);
		break;
#endif

	default:
		lprintk("%s: failure on dc_event %d\n", __func__, dc_event);
		BUG();

	}

}

/*
 * Proceed with the dc_event family dedeicated to the plugin management.
 */
void rtdm_dc_plugin_fn(dc_event_t dc_event) {

	switch(dc_event) {

#ifdef CONFIG_SOOLINK_PLUGIN_WLAN

	case DC_PLUGIN_WLAN_RECV:
		rtdm_event_signal(&rtdm_sl_plugin_wlan_rx_event);
		break;

#endif /* CONFIG_SOOLINK_PLUGIN_WLAN */

#ifdef CONFIG_SOOLINK_PLUGIN_ETHERNET
	case DC_PLUGIN_ETHERNET_RECV:
		rtdm_event_signal(&rtdm_sl_plugin_ethernet_rx_event);
		break;

	case DC_PLUGIN_TCP_RECV:
		rtdm_event_signal(&rtdm_sl_plugin_tcp_rx_event);
		break;
#endif /* CONFIG_SOOLINK_PLUGIN_ETHERNET */

#ifdef CONFIG_SOOLINK_PLUGIN_BLUETOOTH

	case DC_PLUGIN_BLUETOOTH_RECV:
		rtdm_event_signal(&rtdm_sl_plugin_bluetooth_rx_event);
		break;

#endif /* CONFIG_SOOLINK_PLUGIN_BLUETOOTH */

#ifdef CONFIG_SOOLINK_PLUGIN_LOOPBACK

	case DC_PLUGIN_LOOPBACK_RECV:
		rtdm_event_signal(&rtdm_sl_plugin_loopback_rx_event);
		break;

#endif /* CONFIG_SOOLINK_PLUGIN_LOOPBACK */

	default:
		lprintk("%s: failure on dc_event %d\n", __func__, dc_event);
		BUG();

	}
}

/*
 * IRQs off
 */
static int rtdm_dc_isr(rtdm_irq_t *unused) {
	dc_event_t dc_event;

	DBG("(ME domid %d): Received directcomm interrupt for event: %d\n", ME_domID(), avz_shared_info->dc_event);

	dc_event = atomic_read((const atomic_t *) &avz_shared_info->dc_event);

	/* Work to be done in ME */

	switch (dc_event) {

#ifdef CONFIG_SOOLINK_PLUGIN_WLAN
	case DC_SL_WLAN_SEND:
	case DC_SL_WLAN_RECV:
	case DC_PLUGIN_WLAN_RECV:
#endif /* CONFIG_SOOLINK_PLUGIN_WLAN */

#ifdef CONFIG_SOOLINK_PLUGIN_ETHERNET
	case DC_SL_ETH_SEND:
	case DC_SL_ETH_RECV:
	case DC_SL_TCP_SEND:
	case DC_SL_TCP_RECV:
	case DC_PLUGIN_ETHERNET_RECV:
	case DC_PLUGIN_TCP_RECV:
#endif /* CONFIG_SOOLINK_PLUGIN_ETHERNET */

#ifdef CONFIG_SOOLINK_PLUGIN_BLUETOOTH
	case DC_SL_BT_SEND:
	case DC_SL_BT_RECV:
	case DC_PLUGIN_BLUETOOTH_RECV:
#endif /* CONFIG_SOOLINK_PLUGIN_BLUETOOTH */

#ifdef CONFIG_SOOLINK_PLUGIN_LOOPBACK
	case DC_SL_LO_SEND:
	case DC_SL_LO_RECV:
	case DC_PLUGIN_LOOPBACK_RECV:
#endif /* CONFIG_SOOLINK_PLUGIN_LOOPBACK */

	/* These events are necessary a reply to a sync_dom from the non-RT agency. */
	case DC_PLUGIN_LOOPBACK_SEND:
	case DC_PLUGIN_ETHERNET_SEND:
	case DC_PLUGIN_TCP_SEND:
	case DC_PLUGIN_BLUETOOTH_SEND:
	case DC_PLUGIN_WLAN_SEND:

		if (atomic_read(&rtdm_dc_outgoing_domID[dc_event]) != -1)
			rtdm_dc_stable(dc_event);
		else {
			/* We should not receive twice a same dc_event, before it has been fully processed. */
			BUG_ON(atomic_read(&rtdm_dc_incoming_domID[dc_event]) != -1);
			atomic_set(&rtdm_dc_incoming_domID[dc_event], DOMID_AGENCY);

			/* Perform deferred processing for these events */
			rtdm_event_signal(&dc_isr_event);

			return RTDM_IRQ_HANDLED;
		}
		break;

	default:
		lprintk("%s: something weird happened on CPU %d, RT directcomm interrupt was triggered, but no DC event (%d) was configured !\n", __func__, smp_processor_id(), avz_shared_info->dc_event);
		break;
	}

	/* Reset the dc_event now so that the domain can send another dc_event */
	atomic_set((atomic_t *) &avz_shared_info->dc_event, DC_NO_EVENT);

	return RTDM_IRQ_HANDLED;
}

/*
 * Main realtime task which manages initialization of RT vbus and watch monitoring.
 */
void rtdm_vbus_task_fn(void *unused) {
	int vbus_irq;
	struct vbus_device dev;
	int nort_dc_evtchn, evtchn, i, res;
	char buf[20];
	struct vbus_transaction vbt;

	/*
	 * Bind evtchn for interdomain communication: must be executed from the agency or from a ME.
	 */
	dev.otherend_id = 0;

	DBG("%s: binding a local event channel (RT side) to the remote evtchn %d in Agency (intf: %lx) ...\n", __func__, __intf_rt->revtchn, __intf_rt);

	vbus_bind_evtchn(&dev, __intf_rt->revtchn, &evtchn);

	/* This is our local event channel */
	__intf_rt->levtchn = evtchn;

	DBG("Local vbstore_evtchn (RT side) is %d (remote is %d)\n", __intf_rt->levtchn, __intf_rt->revtchn);

	INIT_LIST_HEAD(&rtdm_vbs_state.reply_list);

	rtdm_mutex_init(&rtdm_vbs_state.transaction_mutex);
	rtdm_mutex_init(&rtdm_vbs_state.watch_mutex);
	rtdm_mutex_init(&rtdm_vbs_state.request_mutex);

	spin_lock_init(&rtdm_vbs_state.msg_list_lock);
	spin_lock_init(&rtdm_vbs_state.watch_list_lock);

	atomic_set(&rtdm_vbs_state.transaction_count, 0);

	rtdm_event_init(&rtdm_vbs_state.watch_wait, 0);
	rtdm_event_init(&rtdm_vbs_state.transaction_wq, 0);

	/* Perform the binding with the IRQ used for notification */
	/* Initialize the shared memory rings to talk to vbstore */
	vbus_irq = rtdm_bind_evtchn_to_virq_handler(&vbus_vbstore_irq_handle, __intf_rt->levtchn, rtdm_vbus_vbstore_isr, 0, "rtdm_vbus_vbstore_isr", NULL);
	if (vbus_irq < 0) {
		lprintk(KERN_ERR "RTDM vbus request irq failed %i\n", vbus_irq);
		BUG();
	}

	/* Bind the directcomm handler to a RT ISR which will perform the related work */
	for (i = 0; i < DC_EVENT_MAX; i++) {
		atomic_set(&rtdm_dc_outgoing_domID[i], -1);
		atomic_set(&rtdm_dc_incoming_domID[i], -1);

		rtdm_dc_event_callback[i] = NULL;

		rtdm_event_init(&rtdm_dc_stable_event[i], 0);
	}

	/* Initialize deferred processing outside the directcomm ISR bottom half */
#ifdef CONFIG_SOOLINK_PLUGIN_WLAN

	rtdm_event_init(&rtdm_sl_wlan_send_event, 0);
	rtdm_event_init(&rtdm_sl_wlan_recv_event, 0);
	rtdm_event_init(&rtdm_sl_plugin_wlan_rx_event, 0);

#endif /* CONFIG_SOOLINK_PLUGIN_WLAN */

#ifdef CONFIG_SOOLINK_PLUGIN_ETHERNET

	rtdm_event_init(&rtdm_sl_eth_send_event, 0);
	rtdm_event_init(&rtdm_sl_eth_recv_event, 0);

	rtdm_event_init(&rtdm_sl_tcp_send_event, 0);
	rtdm_event_init(&rtdm_sl_tcp_recv_event, 0);

	rtdm_event_init(&rtdm_sl_plugin_ethernet_rx_event, 0);
	rtdm_event_init(&rtdm_sl_plugin_tcp_rx_event, 0);

#endif /* CONFIG_SOOLINK_PLUGIN_ETHERNET */

#ifdef CONFIG_SOOLINK_PLUGIN_BLUETOOTH

	rtdm_event_init(&rtdm_sl_bt_send_event, 0);
	rtdm_event_init(&rtdm_sl_bt_recv_event, 0);
	rtdm_event_init(&rtdm_sl_plugin_bluetooth_rx_event, 0);

#endif /* CONFIG_SOOLINK_PLUGIN_BLUETOOTH */

#ifdef CONFIG_SOOLINK_PLUGIN_LOOPBACK

	rtdm_event_init(&rtdm_sl_lo_send_event, 0);
	rtdm_event_init(&rtdm_sl_lo_recv_event, 0);
	rtdm_event_init(&rtdm_sl_plugin_loopback_rx_event, 0);

#endif /* CONFIG_SOOLINK_PLUGIN_LOOPBACK */

	rtdm_register_dc_event_callback(DC_PLUGIN_WLAN_RECV, rtdm_dc_plugin_fn);
	rtdm_register_dc_event_callback(DC_PLUGIN_ETHERNET_RECV, rtdm_dc_plugin_fn);
	rtdm_register_dc_event_callback(DC_PLUGIN_TCP_RECV, rtdm_dc_plugin_fn);
	rtdm_register_dc_event_callback(DC_PLUGIN_BLUETOOTH_RECV, rtdm_dc_plugin_fn);
	rtdm_register_dc_event_callback(DC_PLUGIN_LOOPBACK_RECV, rtdm_dc_plugin_fn);

	rtdm_register_dc_event_callback(DC_SL_WLAN_SEND, rtdm_dc_sl_fn);
	rtdm_register_dc_event_callback(DC_SL_WLAN_RECV, rtdm_dc_sl_fn);
	rtdm_register_dc_event_callback(DC_SL_ETH_SEND, rtdm_dc_sl_fn);
	rtdm_register_dc_event_callback(DC_SL_ETH_RECV, rtdm_dc_sl_fn);
	rtdm_register_dc_event_callback(DC_SL_TCP_SEND, rtdm_dc_sl_fn);
	rtdm_register_dc_event_callback(DC_SL_TCP_RECV, rtdm_dc_sl_fn);
	rtdm_register_dc_event_callback(DC_SL_BT_SEND, rtdm_dc_sl_fn);
	rtdm_register_dc_event_callback(DC_SL_BT_RECV, rtdm_dc_sl_fn);
	rtdm_register_dc_event_callback(DC_SL_LO_SEND, rtdm_dc_sl_fn);
	rtdm_register_dc_event_callback(DC_SL_LO_RECV, rtdm_dc_sl_fn);


	/* Binding the irqhandler to the eventchannel */
	DBG("%s: setting up the directcomm event channel (%d) ...\n", __func__, nort_dc_evtchn);

	/* Set up the direct communication channel for post-migration activities
	 * previously established by dom0.
	 */

	vbus_transaction_start(&vbt);

	sprintf(buf, "soo/directcomm/%d", DOMID_AGENCY_RT);

	res = vbus_scanf(vbt, buf, "event-channel", "%d", &nort_dc_evtchn);

	if (res != 1) {
		printk("%s: reading soo/directcomm failed. Error code: %d\n", __func__, res);
		BUG();
	}

	vbus_transaction_end(vbt);

	vbus_irq = rtdm_bind_interdomain_evtchn_to_virqhandler(&dc_irq_handle, DOMID_AGENCY, nort_dc_evtchn, rtdm_dc_isr, 0, "rtdm_dc_isr", NULL);
	if (vbus_irq <= 0) {
		printk(KERN_ERR "Error: bind_evtchn_to_irqhandler failed");
		BUG();
	}

	dc_evtchn[0] = evtchn_from_virq(vbus_irq);

	DBG("%s: local event channel bound to directcomm towards non-RT Agency : %d\n", __func__, dc_evtchn[0]);

	/* Entering the watch monitor loop */

	rtdm_vbus_watch_monitor();
}

static int __rtdm_task_prologue(void *args) {
	rtdm_event_init(&dc_isr_event, 0);

	rtdm_task_init(&rtdm_dc_isr_task, "rtdm_dc_isr", rtdm_dc_isr_task_fn, NULL, DC_ISR_TASK_PRIO, 0);
	rtdm_task_init(&rtdm_vbus_task, "rtdm_vbus", rtdm_vbus_task_fn, NULL, VBUS_TASK_PRIO, 0);

#ifdef CONFIG_SOOLINK_PLUGIN_WLAN
	rtdm_task_init(&rtdm_sl_wlan_send_task, "rtdm_sl_wlan_send", rtdm_sl_wlan_send_task_fn, NULL, SL_SEND_TASK_PRIO, 0);
	rtdm_task_init(&rtdm_sl_wlan_recv_task, "rtdm_sl_wlan_recv", rtdm_sl_wlan_recv_task_fn, NULL, SL_RECV_TASK_PRIO, 0);
	rtdm_task_init(&rtdm_sl_plugin_wlan_rx_task, "rtdm_sl_plugin_wlan", rtdm_sl_plugin_wlan_rx_task_fn, NULL, SL_PLUGIN_WLAN_TASK_PRIO, 0);

#endif /* CONFIG_SOOLINK_PLUGIN_WLAN */

#ifdef CONFIG_SOOLINK_PLUGIN_ETHERNET
	rtdm_task_init(&rtdm_sl_eth_send_task, "rtdm_sl_eth_send", rtdm_sl_eth_send_task_fn, NULL, SL_SEND_TASK_PRIO, 0);
	rtdm_task_init(&rtdm_sl_eth_recv_task, "rtdm_sl_eth_recv", rtdm_sl_eth_recv_task_fn, NULL, SL_RECV_TASK_PRIO, 0);
	rtdm_task_init(&rtdm_sl_plugin_eth_rx_task, "rtdm_sl_plugin_eth", rtdm_sl_plugin_eth_rx_task_fn, NULL, SL_PLUGIN_ETHERNET_TASK_PRIO, 0);

	rtdm_task_init(&rtdm_sl_tcp_send_task, "rtdm_sl_tcp_send", rtdm_sl_tcp_send_task_fn, NULL, SL_SEND_TASK_PRIO, 0);
	rtdm_task_init(&rtdm_sl_tcp_recv_task, "rtdm_sl_tcp_recv", rtdm_sl_tcp_recv_task_fn, NULL, SL_RECV_TASK_PRIO, 0);
	rtdm_task_init(&rtdm_sl_plugin_tcp_rx_task, "rtdm_sl_plugin_tcp", rtdm_sl_plugin_tcp_rx_task_fn, NULL, SL_PLUGIN_TCP_TASK_PRIO, 0);
#endif /* CONFIG_SOOLINK_PLUGIN_ETHERNET */

#ifdef CONFIG_SOOLINK_PLUGIN_BLUETOOTH
	rtdm_task_init(&rtdm_sl_bt_send_task, "rtdm_sl_bt_send", rtdm_sl_bt_send_task_fn, NULL, SL_SEND_TASK_PRIO, 0);
	rtdm_task_init(&rtdm_sl_bt_recv_task, "rtdm_sl_bt_recv", rtdm_sl_bt_recv_task_fn, NULL, SL_RECV_TASK_PRIO, 0);
	rtdm_task_init(&rtdm_sl_plugin_bt_rx_task, "rtdm_sl_plugin_bt", rtdm_sl_plugin_bt_rx_task_fn, NULL, SL_PLUGIN_BLUETOOTH_TASK_PRIO, 0);
#endif /* CONFIG_SOOLINK_PLUGIN_BLUETOOTH */

#ifdef CONFIG_SOOLINK_PLUGIN_LOOPBACK
	rtdm_task_init(&rtdm_sl_lo_send_task, "rtdm_sl_lo_send", rtdm_sl_lo_send_task_fn, NULL, SL_SEND_TASK_PRIO, 0);
	rtdm_task_init(&rtdm_sl_lo_recv_task, "rtdm_sl_lo_recv", rtdm_sl_lo_recv_task_fn, NULL, SL_RECV_TASK_PRIO, 0);
	rtdm_task_init(&rtdm_sl_plugin_loopback_rx_task, "rtdm_sl_plugin_loopback", rtdm_sl_plugin_loopback_rx_task_fn, NULL, SL_PLUGIN_LOOPBACK_TASK_PRIO, 0);
#endif /* CONFIG_SOOLINK_PLUGIN_LOOPBACK */

	/* We can leave this thread die. Our system is living anyway... */
	do_exit(0);

	return 0;
}

int rtdm_vbus_init(void) {
	/* Prepare to initiate a Cobalt RT task */
	kernel_thread(__rtdm_task_prologue, NULL, 0);

	return 0;
}

subsys_initcall(rtdm_vbus_init);
