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

#include <soo/evtchn.h>
#include <soo/vbus.h>
#include <soo/vbstore.h>

#include <soo/uapi/soo.h>
#include <soo/guest_api.h>
#include <soo/uapi/console.h>

#include <soo/guest_api.h>

#include <soo/core/device_access.h>
#include <soo/core/upgrader.h>

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
	int cpu = smp_processor_id();

	DBG("%s(%d, %d)\n", __func__, domID, dc_event);

	dc_event_args.domID = domID;
	dc_event_args.dc_event = dc_event;

	soo_hypercall(AVZ_DC_SET, NULL, NULL, &dc_event_args, NULL);
	while (dc_event_args.state == -EBUSY) {
		if (cpu == AGENCY_RT_CPU)
			xnsched_run();
		else
			schedule();

		soo_hypercall(AVZ_DC_SET, NULL, NULL, &dc_event_args, NULL);
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

	DBG("%s: notifying via evtchn %d...\n", __func__, dc_evtchn[domID]);
	notify_remote_via_evtchn(dc_evtchn[domID]);

	/* Wait for the response from the outgoing domain, and reset the barrier. */
	if (cpu == AGENCY_RT_CPU) {
		rtdm_event_wait(&rtdm_dc_stable_event[dc_event]);
		atomic_set(&rtdm_dc_outgoing_domID[dc_event], -1);
	} else {
		DBG("%s: waiting for completion on dc_event %d...\n", __func__, dc_event);
		wait_for_completion(&dc_stable_lock[dc_event]);

		DBG("%s: all right, got the completion, resetting the barrier.\n", __func__);
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

void soo_hypercall(int cmd, void *vaddr, void *paddr, void *p_val1, void *p_val2)
{
	soo_hyp_t soo_hyp;

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

	hypercall_trampoline(__HYPERVISOR_soo_hypercall, (long) &soo_hyp, 0, 0, 0);
}

void dump_threads(void)
{
	struct task_struct *p;

	for_each_process(p) {

		lprintk("  Backtrace for process pid: %d\n\n", p->pid);

		show_stack(p, NULL, KERN_INFO);
	}
}

/**
 * This callback is executed when the TRIGGER_DEV_PROBE dc_event is received.
 * All vbstore entries required by the frontends are created and ready to
 * proceed with backend initialization.
 *
 * @param dc_event	Normal callback argument, must be TRIGGER_DEV_PROBE
 */
void dc_trigger_dev_probe_fn(dc_event_t dc_event) {

	vbus_probe_backend(atomic_read(&dc_incoming_domID[dc_event]));

	tell_dc_stable(dc_event);
}

/**
 * Perform a local cooperation on target domain passed
 * in the dc_event.
 *
 * @param dc_event
 */
void dc_trigger_local_cooperation(dc_event_t dc_event) {
	unsigned int domID = atomic_read(&dc_incoming_domID[dc_event]);

	soo_hypercall(AVZ_TRIGGER_LOCAL_COOPERATION, NULL, NULL, &domID, NULL);

	tell_dc_stable(dc_event);
}

/*
 * do_soo_activity() may be called from the hypervisor as a DOMCALL, but not necessary.
 * The function may also be called as a deferred work during the ME kernel execution.
 */
void do_soo_activity(void *arg)
{
	soo_domcall_arg_t *args = (soo_domcall_arg_t *) arg;
	agency_ctl_args_t agency_ctl_args;

	switch (args->cmd) {

	case CB_DUMP_BACKTRACE: /* DOMCALL */

		dump_threads();
		break;

	case CB_DUMP_VBSTORE: /* DOMCALL */

		vbs_dump();
		break;

	case CB_AGENCY_CTL: /* DOMCALL */

		/* Prepare the arguments to pass to the agency ctl */
		memcpy(&agency_ctl_args, &args->u.agency_ctl_args, sizeof(agency_ctl_args_t));

		agency_ctl(&agency_ctl_args);

		/* Copy the agency ctl args back to retrieve the results (if relevant) */
		memcpy(&args->u.agency_ctl_args, &agency_ctl_args, sizeof(agency_ctl_args_t));
		break;

	}
}

/**
 * Agency_ctrl operations
 *
 * @param agency_ctl_args Agency control command
 */
void agency_ctl(agency_ctl_args_t *agency_ctl_args)
{

	switch (agency_ctl_args->cmd) {

	case AG_CHECK_DEVCAPS_CLASS:
		agency_ctl_args->u.devcaps_args.supported = devaccess_devcaps_class_supported(agency_ctl_args->u.devcaps_args.class);
		break;

	case AG_CHECK_DEVCAPS:
		agency_ctl_args->u.devcaps_args.supported = devaccess_devcaps_supported(agency_ctl_args->u.devcaps_args.class, agency_ctl_args->u.devcaps_args.devcaps);
		break;

	case AG_SOO_NAME:
		devaccess_get_soo_name(agency_ctl_args->u.soo_name_args.soo_name);
		break;

	case AG_AGENCY_UPGRADE:
		DBG("Upgrade buffer pfn: 0x%08X with size %lu\n", agency_ctl_args->u.agency_upgrade_args.buffer_pfn, agency_ctl_args->u.agency_upgrade_args.buffer_len);
		upg_store(agency_ctl_args->u.agency_upgrade_args.buffer_pfn, agency_ctl_args->u.agency_upgrade_args.buffer_len, agency_ctl_args->slotID);

		break;

	default:
		lprintk("%s: agency_ctl %d cmd not processed by the agency yet... \n", __func__, agency_ctl_args->cmd);

		BUG();
	}
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
	register_dc_event_callback(DC_TRIGGER_LOCAL_COOPERATION, dc_trigger_local_cooperation);
}

