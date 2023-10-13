
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

#include <soo/uapi/soo.h>

#include <linux/sched/task.h>

rtdm_event_t rtdm_dc_stable_event[DC_EVENT_MAX];

static rtdm_event_t dc_isr_event;

static rtdm_irq_t vbus_vbstore_irq_handle, dc_irq_handle;

static rtdm_task_t rtdm_dc_isr_task;
static rtdm_task_t rtdm_vbus_task;

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

/*
 * Perform deferred processing related to directcomm DC_event processing
 */
static void rtdm_dc_isr_task_fn(void *arg) {
	dc_event_t dc_event;

	while (true) {
		rtdm_event_wait(&dc_isr_event);

		dc_event = atomic_read((const atomic_t *) &AVZ_shared->dc_event);

		/* Reset the dc_event now so that the domain can send another dc_event */
atomic_set((atomic_t *) &AVZ_shared->dc_event, DC_NO_EVENT);

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
 * Proceed with the dc_event family dedicated to the SL desc management.
 */
void rtdm_dc_sl_fn(dc_event_t dc_event) {

	switch (dc_event) {


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

	DBG("(ME domid %d): Received directcomm interrupt for event: %d\n", smp_processor_id(), AVZ_shared->dc_event);

	dc_event = atomic_read((const atomic_t *) &AVZ_shared->dc_event);

	/* We should not receive twice a same dc_event, before it has been fully processed. */
	BUG_ON(atomic_read(&rtdm_dc_incoming_domID[dc_event]) != -1);

	atomic_set(&rtdm_dc_incoming_domID[dc_event], DOMID_AGENCY);

	/* Work to be done on RT CPU */
	switch (dc_event) {

	case DC_TRIGGER_DEV_PROBE:

		/* FALLTHROUGH */

		if (atomic_read(&rtdm_dc_outgoing_domID[dc_event]) != -1)
			dc_stable(dc_event);
		else {

			/* Perform deferred processing for these events */
			rtdm_event_signal(&dc_isr_event);

			return RTDM_IRQ_HANDLED;
		}
		break;

	default:
		lprintk("%s: something weird happened on CPU %d, RT directcomm interrupt was triggered, but no DC event (%d) was configured !\n", __func__, smp_processor_id(), AVZ_shared->dc_event);
		break;
	}

	/* Reset the dc_event now so that the domain can send another dc_event */
	atomic_set((atomic_t *) &AVZ_shared->dc_event, DC_NO_EVENT);

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
