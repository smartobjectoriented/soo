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

static DEFINE_SEMAPHORE(sooeventd_lock);
DEFINE_SEMAPHORE(usr_feedback);

struct list_head uevents;
static char uevent_str[80];

static unsigned int current_pending_uevent = 0;
static volatile pending_uevent_request_t pending_uevent_req[MAX_PENDING_UEVENT];

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


int sooeventd_resume(void)
{
	/* Increment the semaphore */
	up(&sooeventd_lock);

	return 0;
}


void wait_for_usr_feedback(void)
{
	down(&usr_feedback);
}

void usr_feedback_ready(void)
{
	up(&usr_feedback);
}

int soo_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	return add_uevent_var(env, uevent_str);
}

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
 * Check if an uevent already exists for a specific slotID.
 */
static bool check_uevent(unsigned int uevent_type, unsigned int slotID) {
	soo_uevent_t *cur;

	list_for_each_entry(cur, &uevents, list)
	if ((cur->uevent_type == uevent_type) && (cur->slotID == slotID))
		return true;

	return false;
}

/*
 * Propagate the cooperate callback in the user space if necessary.
 */
static void add_soo_uevent(unsigned int uevent_type, unsigned int slotID)
{
	soo_uevent_t *cur;

	/* Consider only one uevent_type for a given slotID */
	if (!check_uevent(uevent_type, slotID)) {

		cur = (soo_uevent_t *) kmalloc(sizeof(soo_uevent_t), GFP_ATOMIC);

		cur->uevent_type = uevent_type;
		cur->slotID = slotID;

		/* Insert the thread at the end of the list */
		list_add_tail(&cur->list, &uevents);

#ifdef CONFIG_SOO_ME
		/* Resume the eventd daemon */
		sooeventd_resume();
#endif
	}
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

	/* AVZ_DC_SET and AVZ_GET_DOM_DESC are the only hypercalls that are allowed for CPU 1. */
	if ((smp_processor_id() == 1) && (cmd != AVZ_DC_SET) && (cmd != AVZ_GET_DOM_DESC)) {
		lprintk("%s: trying to unauthorized hypercall %d from the realtime CPU.\n", __func__, cmd);
		panic("Unauthorized.\n");
	}

	ret = hypercall_trampoline(__HYPERVISOR_soo_hypercall, (long) &soo_hyp, 0, 0, 0);

	if (ret < 0)
		goto out;

	/* Complete possible pending uevent request done during the work performed at the hypervisor level. */
#ifdef VERBOSE_PENDING_UEVENT
	lprintk("current_pending_uevent=%d\n", current_pending_uevent);
#endif
	for (i = 0; i < current_pending_uevent; i++)
		if (pending_uevent_req[i].pending) {
			add_soo_uevent(pending_uevent_req[i].uevent_type, pending_uevent_req[i].slotID);
			pending_uevent_req[i].pending = false;
		}

	current_pending_uevent = 0;

out:
	return ret;

}

void dump_threads(void)
{
	struct task_struct *p;

	for_each_process(p) {

		lprintk("  Backtrace for process pid: %d\n\n", p->pid);

		show_stack(p, NULL);
	}
}

/*
 * Check for any available uevent in the agency.
 * This version is sequential.
 * Returns 0 if no uevent is present, 1 if any.
 */
int pick_next_uevent(void)
{
	soo_uevent_t *cur;

	/*
	 * If a uevent is available, it will be retrieved and released to the user space.
	 * If no uevent is available, the thread is suspended.
	 */

	if (list_empty(&uevents))
		return 0;

	cur = list_first_entry(&uevents, soo_uevent_t, list);

	/* Process usr space notification & tasks */
	/* Prepare a uevent for further activities in user space if any... */
	set_uevent(cur->uevent_type, cur->slotID);

	list_del(&cur->list);

	kfree(cur);

	return 1;
}

int get_ME_state(unsigned int ME_slotID)
{
	int rc;
	int val;

	val = ME_slotID;

	rc = soo_hypercall(AVZ_GET_ME_STATE, NULL, NULL, &val, NULL);
	if (rc != 0) {
		printk("%s: failed to get the ME state from the hypervisor (%d)\n", __func__, rc);
		return rc;
	}

	return val;
}

/*
 * Setting the ME state to the specific ME_slotID.
 * The hypercall args is passed by 2 contiguous (unsigned) int, the first one is
 * used for slotID, the second for the state
 */
int set_ME_state(unsigned int ME_slotID, ME_state_t state)
{
	int rc;
	int _state[2];

	_state[0] = ME_slotID;
	_state[1] = state;

	rc = soo_hypercall(AVZ_SET_ME_STATE, NULL, NULL, _state, NULL);
	if (rc != 0) {
		printk("%s: failed to set the ME state from the hypervisor (%d)\n", __func__, rc);
		return rc;
	}

	return rc;
}

void dc_trigger_dev_probe_fn(dc_event_t dc_event) {
	vbus_probe_backend(atomic_read(&dc_incoming_domID[dc_event]));
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

		rc = agency_ctl(&agency_ctl_args);

		/* Copy the agency ctl args back to retrieve the results (if relevant) */
		memcpy(&args->u.agency_ctl_args, &agency_ctl_args, sizeof(agency_ctl_args_t));

		break;

	}

	return rc;
}

/*
 * Agency ctl operations
 */

int agency_ctl(agency_ctl_args_t *agency_ctl_args)
{

	switch (agency_ctl_args->cmd) {

	case AG_FORCE_TERMINATE:
		queue_uevent(ME_FORCE_TERMINATE, agency_ctl_args->slotID);
		break;

	case AG_LOCALINFO_UPDATE:
		/* Send a DC event to the corresponding ME to trigger a localinfo update */
		queue_uevent(ME_LOCALINFO_UPDATE, agency_ctl_args->slotID);
		break;

	case AG_IMEC_SETUP_PEER:
		DBG0("Before do_sync_ME DC_IMEC\n");
		queue_uevent(ME_IMEC_SETUP_PEER, agency_ctl_args->slotID);
		break;

	case AG_CHECK_DEVCAPS_CLASS:
		agency_ctl_args->u.devcaps_args.supported = devaccess_is_devcaps_class_supported(agency_ctl_args->u.devcaps_args.class);
		break;

	case AG_CHECK_DEVCAPS:
		agency_ctl_args->u.devcaps_args.supported = devaccess_is_devcaps_supported(agency_ctl_args->u.devcaps_args.class, agency_ctl_args->u.devcaps_args.devcaps);
		break;

	case AG_SOO_NAME:
		devaccess_get_soo_name(agency_ctl_args->u.soo_name_args.soo_name);
		break;

	case AG_AGENCY_UPGRADE:
		DBG("Upgrade buffer pfn: 0x%08X with size %lu\n", agency_ctl_args->u.agency_upgrade_args.buffer_pfn, agency_ctl_args->u.agency_upgrade_args.buffer_len);
		devaccess_store_upgrade(agency_ctl_args->u.agency_upgrade_args.buffer_pfn, agency_ctl_args->u.agency_upgrade_args.buffer_len, agency_ctl_args->slotID);

		break;

	default:
		lprintk("%s: agency_ctl %d cmd not processed by the agency yet... \n", __func__, agency_ctl_args->cmd);

		BUG();
	}


	return 0;
}

/*
 * Retrieve the agency descriptor.
 */
int get_agency_desc(agency_desc_t *agency_desc)
{
	int rc;
	dom_desc_t dom_desc;
	unsigned int slotID;

	slotID = 1;  /* Agency slot */

	rc = soo_hypercall(AVZ_GET_DOM_DESC, NULL, NULL, &slotID, &dom_desc);
	if (rc != 0) {
		printk("%s: failed to retrieve the SOO descriptor for slot ID %d.\n", __func__, rc);
		return rc;
	}

	memcpy(agency_desc, &dom_desc.u.agency, sizeof(agency_desc_t));

	return 0;
}

/**
 * Retrieve the ME descriptor including the SPID, the state and the SPAD.
 */
void get_ME_desc(unsigned int slotID, ME_desc_t *ME_desc) {
	int rc;
	dom_desc_t dom_desc;

	rc = soo_hypercall(AVZ_GET_DOM_DESC, NULL, NULL, &slotID, &dom_desc);
	if (rc != 0) {
		printk("%s: failed to retrieve the SOO descriptor for slot ID %d.\n", __func__, rc);
		BUG();
	}

	memcpy(ME_desc, &dom_desc.u.ME, sizeof(ME_desc_t));
}

/*
 * Retrieve the SPID of a ME.
 *
 * Return 0 if success.
 */
void get_ME_spid(unsigned int slotID, unsigned char *spid) {
	ME_desc_t ME_desc;

	get_ME_desc(slotID, &ME_desc);
	memcpy(spid, ME_desc.spid, SPID_SIZE);
}

void cache_flush_all(void)
{
	/* Flush all cache */
	flush_cache_all();
	flush_tlb_all();
}

void soo_guest_activity_init(void)
{
	unsigned int i;

	sema_init(&sooeventd_lock, 0);
	sema_init(&usr_feedback, 0);

	for (i = 0; i < DC_EVENT_MAX; i++) {
		atomic_set(&dc_outgoing_domID[i], -1);
		atomic_set(&dc_incoming_domID[i], -1);

		init_completion(&dc_stable_lock[i]);
	}

	for (i = 0; i < MAX_PENDING_UEVENT; i++)
		pending_uevent_req[i].pending = false;

	register_dc_event_callback(DC_TRIGGER_DEV_PROBE, dc_trigger_dev_probe_fn);

	INIT_LIST_HEAD(&uevents);

}

