/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) March 2018 Baptiste Delporte <bonel@bonel.net>
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

#include <asm/mmu.h>

#include <memory.h>
#include <completion.h>

#include <soo/avz.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/soo.h>
#include <soo/console.h>
#include <soo/debug.h>

/*
 * ME Description:
 * The ME resides in one (and only one) Smart Object.
 * It is propagated to other Smart Objects until it meets the one with the UID 0x08.
 * At this moment, the ME keeps the info (localinfo_data+1) until the ME comes back to its origin (the running ME instance).
 * The letter is then incremented and the ME is ready for a new trip.
 * The ME must stay dormant in the Smart Objects different than the origin and the one with UID 0x08.
 */

/* Localinfo buffer used during cooperation processing */
void *localinfo_data;

#if 0
static int live_count = 0;
#endif

/*
 * migrated_once allows the dormant ME to control its oneshot propagation, i.e.
 * the ME must be broadcast in the neighborhood, then disappear from the smart object.
 */
static uint32_t migration_count = 0;

/**
 * PRE-ACTIVATE
 *
 * Should receive local information through args
 */
int cb_pre_activate(soo_domcall_arg_t *args) {
#if 1 /* alphabet */
	agency_ctl_args_t agency_ctl_args;

	agencyUID_t refUID = {
		.id = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08}
	};
#endif /* 0 */
	char target_soo_name[SOO_NAME_SIZE];

	DBG(">> ME %d: cb_pre_activate..\n", ME_domID());

	/* Retrieve the name of the Smart Object on which the ME has migrated */
	agency_ctl_args.cmd = AG_SOO_NAME;
	args->__agency_ctl(&agency_ctl_args);
	strcpy(target_soo_name, (const char *) agency_ctl_args.u.soo_name_args.soo_name);

#if 0 /* dummy_activity */
	/* Kill MEs that are in slot 3 or beyond to keep only 2 MEs */
	if (ME_domID() > 2) {
		lprintk("> kill\n");
		set_ME_state(ME_state_killed);
	}
#endif


#if 1 /* alphabet */
	lprintk("## (slotID: %d) bringing value %c (found: %d)\n", args->slotID, *((char *) localinfo_data), *((char *) localinfo_data+1));
	if (get_ME_state() != ME_state_preparing) {

		/* Keep the ME in dormant state; the ME is temporary here in order to be propagated. */
		migration_count = 0;
		set_ME_state(ME_state_dormant);
	}

	/* Retrieve the agency UID of the Smart Object on which the ME has migrated */
	agency_ctl_args.cmd = AG_AGENCY_UID;
	args->__agency_ctl(&agency_ctl_args);

	if (!memcmp(&refUID, &agency_ctl_args.u.agencyUID_args.agencyUID, SOO_AGENCY_UID_SIZE)) {
		if (*((char *) localinfo_data+1) == 1) /* already ? */ {

			lprintk("## already found: killing...\n");
			set_ME_state(ME_state_killed);
		} else {
			/* Second byte of localinfo_data tells we found the smart object with UID 0x08. */
			*((char *) localinfo_data+1) = 1;
			lprintk("##################################### (slotID: %d) found with %c\n", args->slotID, *((char *) localinfo_data));
		}
	}
#endif
	return 0;
}

/**
 * PRE-PROPAGATE
 *
 * The callback is executed in first stage to give a chance to a resident ME to stay or disappear, for example.
 */
int cb_pre_propagate(soo_domcall_arg_t *args) {

	pre_propagate_args_t *pre_propagate_args = (pre_propagate_args_t *) &args->u.pre_propagate_args;

	DBG(">> ME %d: cb_pre_propagate...\n", ME_domID());

#if 0 /* dummy_activity */
	pre_propagate_args->propagate_status = 1;
#endif

#if 1 /* Alphabet */

	pre_propagate_args->propagate_status = 0;

	/* Enable migration - here, we migrate 3 times before being killed. */
	if ((get_ME_state() != ME_state_dormant) || (migration_count != 3)) {
		pre_propagate_args->propagate_status = 1;
		migration_count++;
	} else
		set_ME_state(ME_state_killed);

#endif

#if 0
	live_count++;

	if (live_count == 5) {
		lprintk("##################### ME %d disappearing..\n", ME_domID());
		set_ME_state(ME_state_killed);
	}

#endif

	return 0;
}

/**
 * Kill domcall - if another ME tries to kill us.
 */
int cb_kill_me(soo_domcall_arg_t *args) {

	DBG(">> ME %d: cb_kill_me...\n", ME_domID());

	/* Do we accept to be killed? yes... */
	set_ME_state(ME_state_killed);

	return 0;
}

/**
 * PRE_SUSPEND
 *
 * This callback is executed right before suspending the state of frontend drivers, before migrating
 *
 * Returns 0 if no propagation to the user space is required, 1 otherwise
 */
int cb_pre_suspend(soo_domcall_arg_t *args) {
	DBG(">> ME %d: cb_pre_suspend...\n", ME_domID());

	/* No propagation to the user space */
	return 0;
}

/**
 * COOPERATE
 *
 * This callback is executed when an arriving ME (initiator) decides to cooperate with a residing ME (target).
 */
int cb_cooperate(soo_domcall_arg_t *args) {
	cooperate_args_t *cooperate_args = (cooperate_args_t *) &args->u.cooperate_args;
#if 1
	agency_ctl_args_t agency_ctl_args;
#endif
	unsigned char me_spad_caps[SPAD_CAPS_SIZE];
	unsigned int i;
	void *recv_data;
	uint32_t pfn;
	bool target_found, initiator_found;
	char target_char, initiator_char;

	DBG(">> ME %d: cb_cooperate...\n", ME_domID());

	switch (cooperate_args->role) {
	case COOPERATE_INITIATOR:
		DBG("Cooperate: Initiator %d\n", ME_domID());
		if (cooperate_args->alone)
			return 0;

		for (i = 0; i < MAX_ME_DOMAINS; i++) {
			if (cooperate_args->u.target_coop_slot[i].spad.valid) {

				memcpy(me_spad_caps, cooperate_args->u.target_coop_slot[i].spad.caps, SPAD_CAPS_SIZE);

#if 0
				agency_ctl_args.cmd = AG_FORCE_TERMINATE;
				agency_ctl_args.slotID = cooperate_args->u.target_coop_slot[i].slotID;
				lprintk("## ************** killing %d\n", agency_ctl_args.slotID);

				/* Perform the cooperate in the target ME */
				args->__agency_ctl(&agency_ctl_args);
#endif /* 0 */

#if 1 /* Alphabet */
				/* Collaboration ... */
				agency_ctl_args.u.target_cooperate_args.pfns.content = phys_to_pfn(virt_to_phys_pt((uint32_t) localinfo_data));
				/* This pattern enables the cooperation with the target ME */


				agency_ctl_args.cmd = AG_COOPERATE;
				agency_ctl_args.slotID = cooperate_args->u.target_coop_slot[i].slotID;
				//memcpy(agency_ctl_args.u.target_cooperate_args.spid, get_ME_desc()->spid, SPID_SIZE);
				//memcpy(agency_ctl_args.u.target_cooperate_args.spad_caps, get_ME_desc()->spad.caps, SPAD_CAPS_SIZE);

				/* Perform the cooperate in the target ME */
				args->__agency_ctl(&agency_ctl_args);

#if 0
				/* Now incrementing us */
				*((char *) localinfo_data) = *((char *) localinfo_data) + 1;
#endif

#endif
#if 0 /* Arrived ME disappears now... */
				set_ME_state(ME_state_killed);
#endif
			}
		}

#if 0 /* This pattern is used to remove this (just arrived) ME even before its activation. */
		if (!cooperate_args->alone) {

			DBG("Killing ME #%d\n", ME_domID());

			set_ME_state(ME_state_killed);
		}
#endif

		break;

	case COOPERATE_TARGET:
		DBG("Cooperate: Target %d\n", ME_domID());

		DBG("SPID of the initiator: ");
		DBG_BUFFER(cooperate_args->u.initiator_coop.spid, SPID_SIZE);
		DBG("SPAD caps of the initiator: ");
		DBG_BUFFER(cooperate_args->u.initiator_coop.spad_caps, SPAD_CAPS_SIZE);

		pfn = cooperate_args->u.initiator_coop.pfns.content;
		recv_data = (void *) io_map(pfn_to_phys(pfn), PAGE_SIZE);

		lprintk("## in-cooperate received : %c\n", *((char *) recv_data));

		target_found = *((char *) localinfo_data+1);
		initiator_found = *((char *) recv_data+1);

		target_char = *((char *) localinfo_data);
		initiator_char = *((char *) recv_data);

#if 1 /* Alphabet - Increment the alphabet in this case. */
		if (get_ME_state() != ME_state_dormant)  {
			lprintk("## Not dormant: ");
			if (initiator_found)
			{
				lprintk("got the target :-)\n");

				(*((char *) localinfo_data))++;
				if (*((char *) localinfo_data) > 'Z')
					*((char *) localinfo_data) = 'A';
				*((char *) localinfo_data+1) = 0; /* Reset */
			} else
				lprintk("not bringing valuable value, killing ME %d\n", args->slotID);

			/* In any case, the arrived ME must disappeared */
			agency_ctl_args.cmd = AG_KILL_ME;
			agency_ctl_args.slotID = args->slotID;

			args->__agency_ctl(&agency_ctl_args);


		} else {
			lprintk("## Target has %c and arrived has %c\n", *((char *) localinfo_data), *((char *) recv_data));
			if (*((char *) localinfo_data) > (*((char *) recv_data))) {
				lprintk("## I'm dormant and I'm killing slotID %d\n", args->slotID);
				agency_ctl_args.cmd = AG_KILL_ME;
				agency_ctl_args.slotID = args->slotID;

				args->__agency_ctl(&agency_ctl_args);

			} else {

				target_found = *((char *) localinfo_data+1);
				initiator_found = *((char *) recv_data+1);

				target_char = *((char *) localinfo_data);
				initiator_char = *((char *) recv_data);

				if ((target_char < initiator_char) ||
				    (initiator_found && (!target_found || (initiator_char >= target_char))))
				{
					lprintk("## Killing myself\n");
					set_ME_state(ME_state_killed);
				} else {
					lprintk("## Killing the arrived (initiator) \n");
					agency_ctl_args.cmd = AG_KILL_ME;
					agency_ctl_args.slotID = args->slotID;

					args->__agency_ctl(&agency_ctl_args);
				}
			}
		}

#endif

#if 0 /* This pattern forces the termination of the residing ME (a kill ME is prohibited at the moment) */
		DBG("Force the termination of this ME #%d\n", ME_domID());
		agency_ctl_args.cmd = AG_FORCE_TERMINATE;
		agency_ctl_args.slotID = ME_domID();

		args->__agency_ctl(&agency_ctl_args);
#endif
		io_unmap((uint32_t) recv_data);

		break;

	default:
		lprintk("Cooperate: Bad role %d\n", cooperate_args->role);
		BUG();
	}

	return 0;
}

/**
 * PRE_RESUME
 *
 * This callback is executed right before resuming the frontend drivers, right after ME activation
 *
 * Returns 0 if no propagation to the user space is required, 1 otherwise
 */
int cb_pre_resume(soo_domcall_arg_t *args) {
	DBG(">> ME %d: cb_pre_resume...\n", ME_domID());

	return 0;
}

/**
 * POST_ACTIVATE callback (async)
 */
int cb_post_activate(soo_domcall_arg_t *args) {
#if 0
	agency_ctl_args_t agency_ctl_args;
	static uint32_t count = 0;
#endif

	DBG(">> ME %d: cb_post_activate...\n", ME_domID());

	return 0;
}

/**
 * LOCALINFO_UPDATE callback (async)
 *
 * This callback is executed when a localinfo_update DC event is received (normally async).
 *
 * Returns 0 if no propagation to the user space is required, 1 otherwise
 */
int cb_localinfo_update(void) {

	return 0;
}

/**
 * FORCE_TERMINATE callback (async)
 *
 * Returns 0 if no propagation to the user space is required, 1 otherwise
 *
 */

int cb_force_terminate(void) {
	DBG(">> ME %d: cb_force_terminate...\n", ME_domID());
	DBG("ME state: %d\n", get_ME_state());

	/* We do nothing particular here for this ME,
	 * however we proceed with the normal termination of execution.
	 */
	lprintk("###################### FORCE terminate me %d\n", ME_domID());
	set_ME_state(ME_state_terminated);

	return 0;
}

void callbacks_init(void) {

	/* Allocate localinfo */
	localinfo_data = (void *) get_contig_free_vpages(1);

	*((char *) localinfo_data) = 'A';
	*((char *) localinfo_data+1) = 0;

	/* The ME accepts to collaborate */
	get_ME_desc()->spad.valid = true;

	/* Set the SPAD capabilities */
	memset(get_ME_desc()->spad.caps, 0, SPAD_CAPS_SIZE);


}

/*** THIS IS OLD STUFF TO BE GRADUALLY IMPORTED IN THE NEW SCHEME. ESPECIALLY THE IMEC-RELATED STUFF. ***/

#if 0

/*
 * PRE_SUSPEND
 *
 * This callback is executed right before suspending the state of frontend drivers, before migrating
 *
 * Returns 0 if no propagation to the user space is required, 1 otherwise
 */
int cb_pre_suspend(soo_domcall_arg_t *args) {
	int rc;

	area = alloc_vm_area(PAGE_SIZE, NULL);

	/* Currently, never freed */
	ME_data = (void *) __get_free_page(GFP_NOIO | __GFP_HIGH);

	imec_channel = (imec_channel_t *) area->addr;

	rc = ioremap_page((unsigned long) imec_channel, virt_to_phys(ME_data), get_mem_type(MT_HIGH_VECTORS));

	if (rc) {
		lprintk("%s failed with rc = %d\n", __func__, rc);
		return -1;
	}

	flush_all();

	memset(imec_channel, 0, sizeof(imec_channel_t));

	imec_init_channel(imec_channel, imec_interrupt);
	/*
	 * Hold the RT tasks for example...
	 * Linux provides a nice mechanism to do that thanks to kthread_park() functions.
	 * These functions wait until the threads have been parked. So, it is not necessary
	 * to use other synchronization mechanisms.
	 */

	if (t1_started) {
		kthread_park(t1);

		kthread_park(t2);
	}
	tick_suspend();

	return 0;
}

/*
 * PRE-ACTIVATE
 *
 * Should receive local information through args
 */
int cb_pre_activate(soo_domcall_arg_t *args) {
#ifdef SOOTEST_AVZ_MEMORY
	//agency_ctl_args_t agency_ctl_args;
#endif

/*#ifdef SOOTEST_AVZ_MEMORY
	lprintk("SOOTEST: kill %d\n", ME_domID());

	agency_ctl_args.cmd = AG_KILL_ME;
	agency_ctl_args.slotID = ME_domID();

	args->__agency_ctl(&agency_ctl_args);
#endif*/

	return 0;
}

/*
 * COOPERATE
 *
 */
int cb_cooperate(soo_domcall_arg_t *args) {
#if defined(SOOTEST_ME_IMEC) || defined(SOOTEST_AVZ_MEMORY)
	unsigned char spad_capabilities[SPAD_CAPABILITIES_SIZE];
	cooperate_args_t *cooperate_args = (cooperate_args_t *) &args->u.cooperate_args;
	agency_ctl_args_t agency_ctl_args;
	int i;
#endif

#ifdef SOOTEST_ME_IMEC
	int rc;
	unsigned int revtchn, rirq;
	irq_handler_t handler;
#endif

#ifdef SOOTEST_AVZ_MEMORY
	char kill_me = 0;
#endif

#if defined(SOOTEST_ME_RTNORT) || defined(SOOTEST_ME_RTRT)
	return 0;
#endif

#if defined(SOOTEST_ME_IMEC) || defined(SOOTEST_AVZ_MEMORY)
	switch (cooperate_args->role) {

	case COOPERATE_INITIATOR:

		DBG("INITIATER currently in slot ID: %d\n", ME_domID());

		if (cooperate_args->alone)
			return 0;

		/* We prepare to transfer information about our IMEC structure */

		/*
		 * We get the set of MEs which are ready to collaborate. We can exchange information based on their aptitudes.
		 */
		for (i = 0; i < MAX_ME_DOMAINS; i++) {

			if (!cooperate_args->u.target_coop_slot[i].spad.valid)
				continue;

			memcpy(spad_capabilities, cooperate_args->u.target_coop_slot[i].spad.caps, SPAD_CAPABILITIES_SIZE);

			/* Same SPID: a residing ME is already here */
			if (!memcmp(cooperate_args->u.target_coop_slot[i].spid, get_ME_desc()->spid, SPID_SPID_SIZE)) {
#ifdef SOOTEST_AVZ_MEMORY
				kill_me = 1;
				break;
#endif

#ifdef SOOTEST_ME_IMEC
				//if (cooperate_args->u.target_coop_slot[i].spad.valid) {
				DBG("### %s 1\n", __func__);
				agency_ctl_args.cmd = AG_COOPERATE;
				agency_ctl_args.slotID = cooperate_args->u.target_coop_slot[i].slotID;

				imec_channel->initiator_slotID = ME_domID();
				imec_channel->peer_slotID = agency_ctl_args.slotID;

				agency_ctl_args.u.target_cooperate_args.pfns.imec = (unsigned int) virt_to_pfn(ME_data);

				args->__agency_ctl(&agency_ctl_args);

				/* Refuse subsequent cooperate */

				//get_ME_desc()->spad.valid = false;
#endif
			}

			//return 0;
			//}
		}

#ifdef SOOTEST_AVZ_MEMORY
		if (kill_me) {
			DBG("Killing ME #%d\n", ME_domID());
			lprintk("SOOTEST: kill RT %d\n", ME_domID());

			agency_ctl_args.cmd = AG_KILL_ME;
			agency_ctl_args.slotID = ME_domID();

			args->__agency_ctl(&agency_ctl_args);
		}
#endif

		break;

	case COOPERATE_TARGET:

		DBG("TARGET currently in slot ID: %d\n", ME_domID());

#if 0 /* PATTERN: Only if we refuse to cooperate with a ME of our family (same SPID) */
	  	if (memcmp(get_ME_desc()->spid, cooperate_args->spid, SPID_SPID_SIZE))
	  		break;
#endif


#ifdef SOOTEST_ME_IMEC
		/* We access the existing virtual mapping in order to reach the pfn passed by the other ME.
		 * From now on, we refer to the imec_channel belonging to the initiator.
		 */
		revtchn = imec_channel->levtchn;
		rirq = imec_channel->lirq;
		handler = imec_channel->initiator_handler;

		vunmap_page_range((unsigned long) imec_channel, ((unsigned long) imec_channel) + PAGE_SIZE);

		flush_all();

		rc = ioremap_page((unsigned long) imec_channel, __pfn_to_phys(cooperate_args->u.pfns.imec), get_mem_type(MT_HIGH_VECTORS));

		if (rc) {
			lprintk("%s failed with rc = %d\n", __func__, rc);
			return -1;
		}

		flush_all();

		/* We set up the communication channel */
		imec_channel->revtchn = revtchn;
		imec_channel->rirq = rirq;
		imec_channel->peer_handler = handler;

		/* Refuse subsequent cooperate */

		get_ME_desc()->spad.valid = false;
#endif

		break;

	}
#endif

	return 0;
}

#if 0
bool sent = false;

irqreturn_t imec_interrupt(int irq, void *dev_id) {
	imec_content_t *content;

	if (imec_peer(imec_channel)) {
		content = imec_cons_request(imec_channel);

		DBG("received: %s\n", content->content.data);
		lprintk("received: %s\n", content->content.data);

		content = imec_prod_response(imec_channel);

		strcpy(content->content.data, "Good\n");

		imec_notify(imec_channel);

		sent = true;

	}
	else if (imec_initiator(imec_channel)) {

		content = imec_cons_response(imec_channel);

		DBG("recvd: %s\n", content->content.data);
		lprintk("received: %s\n", content->content.data);

		sent = true;

	}

	return IRQ_HANDLED;
}
#endif

#ifdef SOOTEST_ME_IMEC
bool sent = false;

irqreturn_t imec_interrupt(int irq, void *dev_id) {
	imec_content_t *content;
	int val;
	bool (*availability_test)(imec_channel_t *imec_channel) = (imec_peer(imec_channel)) ? &imec_available_request : &imec_available_response;
	int k;
	static int recv_count = 0;
	static bool recv_count_end = false;

	while ((*availability_test)(imec_channel)) {
		if (recv_count < SOOTEST_N_PACKETS) {
			if (imec_peer(imec_channel)) {

				content = imec_cons_request(imec_channel);

				memcpy(&val, content->content.data, sizeof(int));

				//lprintk("(R%d)", val);

				recv_vals[val / 8] |= 1 << (val % 8);

				if (val == SOOTEST_N_REQS - 1) {
					//lprintk("\n"); // DEBUG
					lprintk("Recv Peer: ");
					for (k = SOOTEST_N_REQS / 8 - 1 ; k >= 0 ; k--)
						lprintk("%02x-", recv_vals[k]);
					lprintk("\n");
					memset(recv_vals, 0, sizeof(recv_vals));

					recv_count++;
				}
			}

			if (imec_initiator(imec_channel)) {

				content = imec_cons_response(imec_channel);

				memcpy(&val, content->content.data, sizeof(int));
				recv_vals[val / 8] |= 1 << (val % 8);

				if (recv_count < SOOTEST_N_PACKETS) {
					if (val == SOOTEST_N_REQS - 1) {
						lprintk("Recv Init: ");
						for (k = SOOTEST_N_REQS / 8 - 1 ; k >= 0 ; k--)
							lprintk("%02x-", recv_vals[k]);
						lprintk("\n");
						memset(recv_vals, 0, sizeof(recv_vals));

						recv_count++;
					}
				}
			}

			if (recv_count == SOOTEST_N_PACKETS - 1) {
				if (!recv_count_end)
					lprintk("SOOTEST: end IMEC %d\n", ME_domID());
				recv_count_end = true;
			}
		}
	}

	return IRQ_HANDLED;
}
#endif


/*
 * post_activate callback is called in both client ME (which stays in the SOO) and server ME (after migration).
 */

int cb_post_activate(soo_domcall_arg_t *args) {
	/* Open the IMEC channel with the other ME */

	/*
	 * Here, peer_slotID is not zero in the migrated ME and a setup can be done followed
	 * by unparking the parked threads.
	 * In the client ME, peer_slotID remains at 0 and the condition is false.
	 *
	 */

	tick_resume();

	if (!t1_started) {

		rtapp_main();

	} else {

		kthread_unpark(t1);
		kthread_unpark(t2);

	}

#ifdef SOOTEST_ME_IMEC
	if ((imec_channel->peer_slotID != 0) && !imec_ready(imec_channel))
		imec_initiator_setup(imec_channel);
#endif

	/* Returning 1 for propagating to the user space */

	return 0;
}

#endif /* 0 */
