/*
 * Copyright (C) 2016-2019 Daniel Rossier <daniel.rossier@soo.tech>
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

#include <common.h>
#include <thread.h>
#include <sync.h>
#include <heap.h>
#include <list.h>
#include <schedule.h>
#include <errno.h>

#include <device/irq.h>

#include <soo/evtchn.h>
#include <soo/vbus.h>
#include <soo/vbstore.h>
#include <soo/soo.h>
#include <soo/console.h>
#include <soo/debug/logbool.h>

#define MAX_PENDING_UEVENT 	10

/*
 * Used to keep track of the target domain for a certain (outgoing) dc_event.
 * Value -1 means no dc_event in progress.
 */
atomic_t dc_outgoing_domID[DC_EVENT_MAX];

/*
 * Used to store the domID issuing a (incoming) dc_event
 */
atomic_t dc_incoming_domID[DC_EVENT_MAX];

static struct completion dc_stable_lock[DC_EVENT_MAX];

static struct completion sooeventd_lock;
static struct completion usr_feedback_lock;

static soo_uevent_t uevents;
static char uevent_str[80];

static unsigned int current_pending_uevent = 0;
static volatile pending_uevent_request_t pending_uevent_req[MAX_PENDING_UEVENT];

int __pfn_offset = 0;

void dc_stable(int dc_event)
{
	/* It may happen that the thread which performs the down did not have time to perform the call and is not suspended.
	 * In this case, complete() will increment the count and wait_for_completion() will go straightforward.
	 */

	complete(&dc_stable_lock[dc_event]);
}


/*
 * Sends a ping event to a remote domain in order to get synchronized.
 * Various types of event (dc_event) can be sent.
 *
 * To perform a ping from the RT domain, please use rtdm_do_sync_agency() in rtdm_vbus.c
 *
 * As for the domain table, the index 0 and 1 are for the agency and the indexes 2..MAX_DOMAINS
 * are for the MEs. If a ME_slotID is provided, the proper index is given by ME_slotID.
 *
 * @domID: the target domain
 * @dc_event: type of event used in the synchronization
 */
void do_sync_dom(int domID, dc_event_t dc_event)
{
	/* Ping the remote domain to perform the task associated to the DC event */
	DBG("%s: ping domain %d...\n", __func__, domID);

	/* Make sure a previous transaction is not ongoing. */
	while (atomic_cmpxchg(&dc_outgoing_domID[dc_event], -1, domID) != -1)
		schedule();

	set_dc_event(domID, dc_event);

	notify_remote_via_evtchn(dc_evtchn);

	/* Wait for the response from the outgoing domain */
	wait_for_completion(&dc_stable_lock[dc_event]);

	/* Now, reset the barrier. */
	atomic_set(&dc_outgoing_domID[dc_event], -1);
}

/*
 * Tell a specific domain that we are now in a stable state regarding the dc_event actions.
 * Typically, this comes after receiving a dc_event leading to a dc_event related action.
 */
void tell_dc_stable(int dc_event)  {
	int domID;

	domID = atomic_read(&dc_incoming_domID[dc_event]);

	BUG_ON(domID == -1);

	DBG("vbus_stable: now pinging domain %d back\n", domID);

	set_dc_event(domID, dc_event);

	/* Ping the remote domain to perform the task associated to the DC event */
	DBG("%s: ping domain %d...\n", __func__, dc_incoming_domID[dc_event]);

	atomic_set(&dc_incoming_domID[dc_event], -1);

	notify_remote_via_evtchn(dc_evtchn);
}


/*
 * Prepare a remote ME to react to a ping event.
 * @domID: the target ME
 */
int set_dc_event(unsigned int domID, dc_event_t dc_event)
{
	soo_hyp_dc_event_t dc_event_args;
	int rc;

	DBG("%s(%d, %d)\n", __func__, domID, dc_event);

	dc_event_args.domID = domID;
	dc_event_args.dc_event = dc_event;

	rc = soo_hypercall(AVZ_DC_SET, NULL, NULL, &dc_event_args, NULL);
	while (rc == -EBUSY) {
		schedule();
		rc = soo_hypercall(AVZ_DC_SET, NULL, NULL, &dc_event_args, NULL);
	}

	return 0;
}

int sooeventd_resume(void)
{
	/* Increment the semaphore */
	complete(&sooeventd_lock);

	return 0;
}


void wait_for_usr_feedback(void)
{
	wait_for_completion(&usr_feedback_lock);
}

void usr_feedback_ready(void)
{
	complete(&usr_feedback_lock);
}

#if 0
int soo_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	return add_uevent_var(env, uevent_str);
}
#endif

/*
 * Configure the uevent which will be retrieved from the user space with the sooeventd utility
 */
void set_uevent(unsigned int uevent_type, unsigned int ME_slotID)
{
	char strSlotID[3];

	switch (uevent_type) {

	case ME_FORCE_TERMINATE:
		strcpy(uevent_str, "SOO_CALLBACK=FORCE_TERMINATE:");
		break;

	case ME_POST_ACTIVATE:
		strcpy(uevent_str, "SOO_CALLBACK=POST_ACTIVATE:");
		break;

	case ME_PRE_SUSPEND:
		strcpy(uevent_str, "SOO_CALLBACK=PRE_SUSPEND:");
		break;

	case ME_LOCALINFO_UPDATE:
		strcpy(uevent_str, "SOO_CALLBACK=LOCALINFO_UPDATE:");
		break;

	}

	sprintf(strSlotID, "%d", ME_slotID);
	strcat(uevent_str, strSlotID);

	/* uevent entries should be processed by means of netlink sockets.
	 * However, at the moment, we just read the /sys file, and we do not
	 * get the right size of the string. So, that's why we put a delimiter at the end of our string.
	 */

	strcat(uevent_str, ":");
}

/*
 * Propagate the cooperate callback in the user space if necessary.
 */
static void add_soo_uevent(unsigned int uevent_type, unsigned int slotID)
{
	soo_uevent_t *cur;

	cur = (soo_uevent_t *) malloc(sizeof(soo_uevent_t));

	cur->uevent_type = uevent_type;
	cur->slotID = slotID;

	/* Insert the thread at the end of the list */
	list_add_tail(&cur->list, &uevents.list);

	/* Resume the eventd daemon */
	sooeventd_resume();

}

/*
 * queue_uevent is used by both agency and ME.
 *
 * At the agency side, most of callback processing is performed by the agency (CPU #0), hence
 * in the context of a hypercall. It means that during the final upcall in the agency,
 * the uevent will be inserted in the
 */
void queue_uevent(unsigned int uevent_type, unsigned int slotID)
{
	pending_uevent_req[current_pending_uevent].slotID = slotID;
	pending_uevent_req[current_pending_uevent].uevent_type = uevent_type;
	pending_uevent_req[current_pending_uevent].pending = true;

	current_pending_uevent++;
}

/*
 * SOO Migration hypercall
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
	int ret, i;

	soo_hyp.cmd = cmd;
	soo_hyp.vaddr = (unsigned long) vaddr;
	soo_hyp.paddr = (unsigned long) paddr;
	soo_hyp.p_val1 = p_val1;
	soo_hyp.p_val2 = p_val2;

	ret = hypercall_trampoline(__HYPERVISOR_soo_hypercall, (long) &soo_hyp, 0, 0, 0);

	if (ret < 0)
		goto out;

	/* Complete possible pending uevent request done during the work performed at the hypervisor level. */
	DBG("current_pending_uevent=%d\n", current_pending_uevent);
	for (i = 0; i < current_pending_uevent; i++)
		if (pending_uevent_req[i].pending) {
			add_soo_uevent(pending_uevent_req[i].uevent_type, pending_uevent_req[i].slotID);
			pending_uevent_req[i].pending = false;
		}

	current_pending_uevent = 0;

out:
	return ret;

}

/*
 * Check for any available uevent. If not, the running thread (eventd daemon) is suspended.
 */
int pick_next_uevent(void)
{
	soo_uevent_t *cur;

	/*
	 * If a uevent is available, it will be retrieved and released to the user space.
	 * If no uevent is available, the thread is suspended.
	 */

	wait_for_completion(&sooeventd_lock);

	cur = list_first_entry(&uevents.list, soo_uevent_t, list);

	/* Process usr space notification & tasks */
	/* Prepare a uevent for further activities in user space if any... */
	set_uevent(cur->uevent_type, cur->slotID);

	list_del(&cur->list);

	free(cur);

	return 0;
}


/*
 * Set the pfn offset after migration
 */
void set_pfn_offset(int pfn_offset)
{
	__pfn_offset = pfn_offset;
}

int get_pfn_offset(void)
{
	return __pfn_offset;
}

/*
 * Get the state of a ME.
 */
int get_ME_state(void)
{
	return avz_shared_info->dom_desc.u.ME.state;
}

void set_ME_state(ME_state_t state)
{
	avz_shared_info->dom_desc.u.ME.state = state;
}

void set_ME_type(ME_type_t ME_type) {
	avz_shared_info->dom_desc.u.ME.type = ME_type;
}

void perform_task(dc_event_t dc_event)
{
	soo_domcall_arg_t args;
	int rc;

	DBG("ME\n");

	switch (dc_event) {

	case DC_FORCE_TERMINATE:
		/* The ME will initiate the force_terminate processing on its own. */

		/* The sooeventd process must access the SOO driver so that it can initiate
		 * the device shutdown properly, and once all devices are stable, we can
		 * ask the hypervisor to kill ourselves. This has to be performed by the this daemon.
		 *
		 * Subsequently, the daemon will be periodically ping by the agency so that the agency
		 * can decide to interrupt in a brute force way the running ME (something weird happens)
		 *
		 */

		DBG("perform a CB_FORCE_TERMINATE on this ME %d\n", ME_domID());
		rc = cb_force_terminate();

		if (get_ME_state() == ME_state_terminated) {

			if (rc) {
				/* Process usr space notification & tasks */

				add_soo_uevent(ME_FORCE_TERMINATE, ME_domID());

				/* Synchronous waiting until the user space has finished its work. */
				wait_for_usr_feedback();
			}

			/* Prepare vbus to stop everything with the frontend */
			/* (interactions with vbus) */
			DBG("Device shutdown...\n");
			device_shutdown();

			/* Remove all devices */
			DBG("Removing devices ...\n");
			remove_devices();

			/* Remove vbstore entries related to this ME */
			DBG("Removing vbstore entries ...\n");
			remove_vbstore_entries();

			/* Remove grant table entries */
			DBG("Removing grant references ...\n");
			gnttab_remove(true);
		}

		break;

	case DC_RESUME:

		DBG("resuming vbstore...\n");

		/* Giving a chance to perform actions before resuming devices */
		args.cmd = CB_PRE_RESUME;
		do_soo_activity(&args);

		DBG("Now resuming vbstore...\n");
		vbs_resume();

		/* Re-init watch for device/<domID> */
		if (soo_get_personality() == SOO_PERSONALITY_TARGET) {
			postmig_setup();
			soo_set_personality(SOO_PERSONALITY_INITIATOR);
		}

		DBG("vbstore resumed.\n");

		break;

	case DC_SUSPEND:

		DBG("Suspending vbstore...\n");

		vbs_suspend();
		DBG("vbstore suspended.\n");

		break;

	case DC_PRE_SUSPEND:
		DBG("Pre-suspending...\n");

		/* Giving a chance to perform actions before resuming devices */
		args.cmd = CB_PRE_SUSPEND;
		do_soo_activity(&args);
		break;

	case DC_POST_ACTIVATE:
		DBG("Post_activate...\n");

		args.cmd = CB_POST_ACTIVATE;
		do_soo_activity(&args);
		break;

	case DC_LOCALINFO_UPDATE:

		DBG("Localinfo update callback for ME %d\n", ME_domID());

		rc = cb_localinfo_update();
		if (rc > 0) {
			/* Asynchronous processing for localinfo_update */
			add_soo_uevent(ME_LOCALINFO_UPDATE, ME_domID());

			wait_for_usr_feedback();
		}
		break;

	default:
		lprintk("Wrong DC event %d\n", avz_shared_info->dc_event);
	}

	tell_dc_stable(dc_event);
}


/*
 * do_soo_activity() may be called from the hypervisor as a DOMCALL, but not necessary.
 * The function may also be called as a deferred work during the ME kernel execution.
 */
int do_soo_activity(void *arg)
{
	int rc = 0;
	soo_domcall_arg_t *args = (soo_domcall_arg_t *) arg;

	switch (args->cmd) {

	case CB_PRE_SUSPEND: /* Called by perform_pre_suspend */
		DBG("Pre-suspend callback for ME %d\n", ME_domID());

		rc = cb_pre_suspend(arg);

		if (rc > 0) {
			add_soo_uevent(ME_PRE_SUSPEND, ME_domID());

			wait_for_usr_feedback();
		}

		break;

	case CB_PRE_RESUME: /* Called from vbus/vbs.c */
		DBG("Pre-resume callback for ME %d\n", ME_domID());
		rc = cb_pre_resume(arg);
		if (rc > 0) {
			add_soo_uevent(ME_PRE_RESUME, ME_domID());

			wait_for_usr_feedback();
		}
		break;

	case CB_PRE_ACTIVATE: /* DOMCALL */

		/* Allow to pass local information of this SOO to this ME
		 * and to decide what to do next...
		 */
		rc = cb_pre_activate(arg);
		break;

	case CB_PRE_PROPAGATE: /* DOMCALL */

		rc = cb_pre_propagate(arg);
		break;

	case CB_KILL_ME: /* Kill domcall */

		/* If the ME agrees to be killed (immediately being shutdown, it has to change its state to killed) */
		rc = cb_kill_me(arg);
		break;

	case CB_COOPERATE: /* Both DOMCALL and called by perform_cooperate() */

		/*
		 * Enable possible exchange of data between MEs
		 * and make further actions
		 */

		rc = cb_cooperate(arg);
		break;

	case CB_POST_ACTIVATE: /* Called by perform_post_activate() */

		DBG("Post_activate callback for ME %d\n", ME_domID());

		rc = cb_post_activate(args);
		if (rc > 0) {
			/* Asynchronous processing for localinfo_update */
			add_soo_uevent(ME_POST_ACTIVATE, ME_domID());

			wait_for_usr_feedback();
		}

		break;

	case CB_DUMP_BACKTRACE: /* DOMCALL */

		dump_sched();
		break;
	}

	return rc;
}

/*
 * Agency ctl operations
 */

int agency_ctl(agency_ctl_args_t *agency_ctl_args)
{
	int rc;

	agency_ctl_args->slotID = ME_domID();

	rc = soo_hypercall(AVZ_AGENCY_CTL, NULL, NULL, agency_ctl_args, NULL);
	if (rc != 0) {
		printk("Failed to set directcomm event from hypervisor (%d)\n", rc);
		return rc;
	}

	return 0;
}

bool me_realtime(void) {
	return avz_shared_info->dom_desc.realtime;
}

ME_desc_t *get_ME_desc(void)
{
	return &avz_shared_info->dom_desc.u.ME;
}

agency_desc_t *get_agency_desc(void)
{
	return &avz_shared_info->dom_desc.u.agency;
}

void soo_guest_activity_init(void)
{
	unsigned int i;

	init_completion(&sooeventd_lock);
	init_completion(&usr_feedback_lock);

	for (i = 0; i < DC_EVENT_MAX; i++) {
		atomic_set(&dc_outgoing_domID[i], -1);
		atomic_set(&dc_incoming_domID[i], -1);

		init_completion(&dc_stable_lock[i]);
	}

	for (i = 0; i < MAX_PENDING_UEVENT; i++)
		pending_uevent_req[i].pending = false;

	INIT_LIST_HEAD(&uevents.list);

}

