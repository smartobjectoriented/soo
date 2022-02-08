/*
 * Copyright (C) 2016-2019 Daniel Rossier <daniel.rossier@soo.tech>
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

#if 0
#define DEBUG
#endif

//#define VERBOSE_PENDING_UEVENT

#include <linux/version.h>
#include <linux/device.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>

#include <linux/sched/signal.h>
#include <linux/sched/debug.h>

#include <asm/cacheflush.h>
#include <asm/tlbflush.h>

#include <soo/vbus.h>
#include <soo/vbstore.h>

#include <soo/uapi/soo.h>
#include <soo/guest_api.h>
#include <soo/uapi/console.h>

#include <soo/guest_api.h>

#include <soo/core/device_access.h>

#define MAX_PENDING_UEVENT		10

/*
 * Used to keep track of the target domain for a certain (outgoing) dc_event.
 * Value -1 means no dc_event in progress.
 */
atomic_t dc_outgoing_domID[DC_EVENT_MAX];
extern atomic_t rtdm_dc_outgoing_domID[DC_EVENT_MAX];

/*
 * Used to store the domID issuing a (incoming) dc_event
 */
atomic_t dc_incoming_domID[DC_EVENT_MAX];
extern atomic_t rtdm_dc_incoming_domID[DC_EVENT_MAX];

static struct completion dc_stable_lock[DC_EVENT_MAX];
extern rtdm_event_t rtdm_dc_stable_event[DC_EVENT_MAX];

void dc_stable(int dc_event)
{
	/* It may happen that the thread which performs the down did not have time to perform the call and is not suspended.
	 * In this case, complete() will increment the count and wait_for_completion() will go straightforward.
	 */
	if (smp_processor_id() == AGENCY_RT_CPU) {
		rtdm_event_signal(&rtdm_dc_stable_event[dc_event]);
		atomic_set(&rtdm_dc_incoming_domID[dc_event], -1);
	} else {
		complete(&dc_stable_lock[dc_event]);
		atomic_set(&dc_incoming_domID[dc_event], -1);
	}
}

/*
 * Prepare a remote ME to react to a ping event.
 * @domID: the target ME
 */
void set_dc_event(domid_t domID, dc_event_t dc_event)
{
	soo_hyp_dc_event_t dc_event_args;
	int rc = -EBUSY;
	int cpu = smp_processor_id();

	DBG("%s(%d, %d)\n", __func__, domID, dc_event);

	dc_event_args.domID = domID;
	dc_event_args.dc_event = dc_event;

	rc = soo_hypercall(AVZ_DC_SET, NULL, NULL, &dc_event_args, NULL);
	while (rc == -EBUSY) {
		if (cpu == AGENCY_RT_CPU)
			xnsched_run();
		else
			schedule();
		rc = soo_hypercall(AVZ_DC_SET, NULL, NULL, &dc_event_args, NULL);
	}
}

/*
 * Sends a ping event to a remote domain in order to get synchronized.
 * Various types of event (dc_event) can be sent.
 *
 * @domID: the target domain
 * @dc_event: type of event used in the synchronization
 */
void do_sync_dom(int domID, dc_event_t dc_event)
{
	int cpu = smp_processor_id();

	/* Ping the remote domain to perform the task associated to the DC event */
	DBG("%s: ping domain %d...\n", __func__, domID);

	/* Make sure a previous transaction is not ongoing. */
	if (cpu == AGENCY_RT_CPU) {
		while (atomic_cmpxchg(&rtdm_dc_outgoing_domID[dc_event], -1, domID) != -1)
			xnsched_run();
	} else
		while (atomic_cmpxchg(&dc_outgoing_domID[dc_event], -1, domID) != -1)
			schedule();

	/* Configure the dc event on the target domain */

	set_dc_event(domID, dc_event);

	notify_remote_via_evtchn(dc_evtchn[domID]);

	/* Wait for the response from the outgoing domain, and reset the barrier. */
	if (cpu == AGENCY_RT_CPU) {
		rtdm_event_wait(&rtdm_dc_stable_event[dc_event]);
		atomic_set(&rtdm_dc_outgoing_domID[dc_event], -1);
	} else {
		wait_for_completion(&dc_stable_lock[dc_event]);
		atomic_set(&dc_outgoing_domID[dc_event], -1);
	}
}

/*
 * Tell a specific domain that we are now in a stable state regarding the dc_event actions.
 * Typically, this comes after receiving a dc_event leading to a dc_event related action.
 */
void tell_dc_stable(int dc_event)  {
	int domID;
	int cpu = smp_processor_id();

	if (cpu == AGENCY_RT_CPU) {
		domID = atomic_read(&rtdm_dc_incoming_domID[dc_event]);
		BUG_ON(domID != AGENCY_CPU);
	} else {
		domID = atomic_read(&dc_incoming_domID[dc_event]);
		BUG_ON(domID == -1);

	}

	DBG("vbus_stable: now pinging domain %d back\n", domID);
	set_dc_event(domID, dc_event);

	/* Ping the remote domain to perform the task associated to the DC event */
	DBG("%s: ping domain %d...\n", __func__, domID);

	if (cpu == AGENCY_RT_CPU) {
		atomic_set(&rtdm_dc_incoming_domID[dc_event], -1);
		notify_remote_via_evtchn(dc_evtchn[DOMID_AGENCY]);
	} else {
		atomic_set(&dc_incoming_domID[dc_event], -1);
		notify_remote_via_evtchn(dc_evtchn[domID]);
	}

}

/*
 * SOO hypercall
 *
 * Mandatory arguments:
 * - cmd: hypercall
 * - vaddr: a virtual address used within the hypervisor
 * - paddr: a physical address used within the hypervisor
 * - p_val1: a (virtual) address to a first value
 * - p_val2: a (virtual) address to a second value
 */

int soo_hypercall(int cmd, void *vaddr, void *paddr, void *p_val1, void *p_val2)
{
	soo_hyp_t soo_hyp;
	int ret;

	soo_hyp.cmd = cmd;
	soo_hyp.vaddr = (unsigned long) vaddr;
	soo_hyp.paddr = (unsigned long) paddr;
	soo_hyp.p_val1 = p_val1;
	soo_hyp.p_val2 = p_val2;

	/* AVZ_DC_SET and AVZ_GET_DOM_DESC are the only hypercalls that are allowed for CPU 1. */
	if ((smp_processor_id() == AGENCY_RT_CPU) && (cmd != AVZ_DC_SET)) {
		lprintk("%s: trying to unauthorized hypercall %d from the realtime CPU.\n", __func__, cmd);
		panic("Unauthorized.\n");
	}

	ret = hypercall_trampoline(__HYPERVISOR_soo_hypercall, (long) &soo_hyp, 0, 0, 0);

	if (ret < 0)
		goto out;

out:
	return ret;

}

void dump_threads(void)
{
	struct task_struct *p;

	for_each_process(p) {

		lprintk("  Backtrace for process pid: %d\n\n", p->pid);

		show_stack(p, NULL, KERN_INFO);
	}
}

void dc_trigger_dev_probe_fn(dc_event_t dc_event) {
	vbus_probe_backend(atomic_read(&dc_incoming_domID[dc_event]));
	tell_dc_stable(dc_event);
}


void soo_guest_activity_init(void)
{
	unsigned int i;


	for (i = 0; i < DC_EVENT_MAX; i++) {
		atomic_set(&dc_outgoing_domID[i], -1);
		atomic_set(&dc_incoming_domID[i], -1);

		init_completion(&dc_stable_lock[i]);
	}

	register_dc_event_callback(DC_TRIGGER_DEV_PROBE, dc_trigger_dev_probe_fn);

}

