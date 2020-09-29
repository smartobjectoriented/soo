/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) March 2018 Baptiste Delporte <bonel@bonel.net>
 * Copyright (C) March 2028-2020 David Truan <david.truan@heig-vd.ch>
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

#if 1
#define DEBUG
#endif

#include <asm/mmu.h>

#include <memory.h>
#include <sync.h>

#include <soo/avz.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/soo.h>
#include <soo/console.h>
#include <soo/debug.h>

#include <apps/blind.h>

/* Cooperation with SOO.outdoor */
#include <apps/outdoor.h>

/* Localinfo buffer used during cooperation processing */
void *localinfo_data;

static bool blind_initialized = false;

/*
 * Agency UID history.
 * This array saves the Smart Object's agency UID from which the ME is sent. This is used to prevent a ME to
 * go back to the originater (vicious circle).
 * The agency UIDs are in inverted chronological order: the oldest value is at index 0.
 */
static agencyUID_t agencyUID_history[2];

static agencyUID_t last_agencyUID;

/* Limited number of migrations */
static bool limited_migrations = false;
static uint32_t migration_count = 0;

/* SPID of the SOO.outdoor ME */
uint8_t SOO_outdoor_spid[SPID_SIZE] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x61, 0xd0, 0x08 };

/**
 * PRE-ACTIVATE
 *
 * Should receive local information through args
 */
int cb_pre_activate(soo_domcall_arg_t *args) {

	agency_ctl_args_t agency_ctl_args;

	lprintk("Pre-activate %d\n", ME_domID());

	/* Retrieve the agency UID of the Smart Object on which the ME has migrated */
	agency_ctl_args.cmd = AG_AGENCY_UID;
	args->__agency_ctl(&agency_ctl_args);
	memcpy(&target_agencyUID, &agency_ctl_args.u.agencyUID_args.agencyUID, SOO_AGENCY_UID_SIZE);

	/* Detect if the ME has migrated, that is, it is now on another Smart Object */
	has_migrated = (agencyUID_is_valid(&last_agencyUID) && (memcmp(&target_agencyUID, &last_agencyUID, SOO_AGENCY_UID_SIZE) != 0));
	memcpy(&last_agencyUID, &target_agencyUID, SOO_AGENCY_UID_SIZE);

	blind_action_pre_activate();

	/* Retrieve the name of the Smart Object on which the ME has migrated */
	agency_ctl_args.cmd = AG_SOO_NAME;
	args->__agency_ctl(&agency_ctl_args);
	strcpy(target_soo_name, (const char *) agency_ctl_args.u.soo_name_args.soo_name);

	DBG("SOO." APP_NAME " ME now running on: ");
	DBG_BUFFER(&target_agencyUID, SOO_AGENCY_UID_SIZE);

	/* Check if the ME is not coming back to its source (vicious circle) */
	if (!memcmp(&target_agencyUID, &agencyUID_history[0], SOO_AGENCY_UID_SIZE)) {
		DBG("Back to the source\n");

		/* Kill the ME to avoid circularity */
		set_ME_state(ME_state_killed);

		return 0;
	}

	/* Ask if the Smart Object if a SOO.blind Smart Object */
	agency_ctl_args.u.devcaps_args.class = DEVCAPS_CLASS_DOMOTICS;
	agency_ctl_args.u.devcaps_args.devcaps = DEVCAP_BLIND_MOTOR;
	agency_ctl_args.cmd = AG_CHECK_DEVCAPS;
	args->__agency_ctl(&agency_ctl_args);

	if (agency_ctl_args.u.devcaps_args.supported) {
		DBG("SOO." APP_NAME ": This is a SOO." APP_NAME " Smart Object\n");

		/* Tell that the expected devcaps have been found on this Smart Object */
		available_devcaps = true;
	} else {
		DBG("SOO." APP_NAME ": This is not a SOO." APP_NAME " Smart Object\n");

		/* Tell that the expected devcaps have not been found on this Smart Object */
		available_devcaps = false;

		/* If the devcaps are not available, set the ID to 0xff */
		my_id = 0xff;

		/* Ask if the Smart Object offers the dedicated remote application devcap */
		agency_ctl_args.u.devcaps_args.class = DEVCAPS_CLASS_APP;
		agency_ctl_args.u.devcaps_args.devcaps = DEVCAP_APP_BLIND;
		agency_ctl_args.cmd = AG_CHECK_DEVCAPS;
		args->__agency_ctl(&agency_ctl_args);

		if (agency_ctl_args.u.devcaps_args.supported) {
			DBG("SOO." APP_NAME " remote application connected\n");

			/* Stay resident on the Smart Object */

			remote_app_connected = true;
			limited_migrations = false;
		} else {
			DBG("No SOO." APP_NAME " remote application connected\n");

			/* Go to dormant state and migrate once */
			//limited_migrations = true;
			migration_count = 0;

			set_ME_state(ME_state_dormant);
		}
    	}
	return 0;
}

/**
 * PRE-PROPAGATE
 *
 * The callback is executed in first stage to give a chance to a resident ME to stay or disappear, for example.
 */
int cb_pre_propagate(soo_domcall_arg_t *args) {
	agency_ctl_args_t agency_ctl_args;
	pre_propagate_args_t *pre_propagate_args = (pre_propagate_args_t *) &args->u.pre_propagate_args;

	DBG("Pre_propagate %d\n", ME_domID());

#if 1 /* dummy_activity */
	pre_propagate_args->propagate_status = 1;
#endif


	if (limited_migrations) {
		pre_propagate_args->propagate_status = 0;

		DBG("Limited number of migrations: %d/%d\n", migration_count, MAX_MIGRATION_COUNT);

		/* Enable migration - here, we migrate MAX_MIGRATION_COUNT times before being killed. */
		if ((get_ME_state() != ME_state_dormant) || (migration_count != MAX_MIGRATION_COUNT)) {
			pre_propagate_args->propagate_status = 1;
			//migration_count++;
		} else {
			set_ME_state(ME_state_killed);
		}
		goto propagate;
	}
	
	/* SOO.blind: increment the age counter and reset the inertia counter */
	inc_age_reset_inertia(&blind_lock, blind_data->info.presence);

	/* SOO.blind: watch the inactive ages */
	watch_ages(&blind_lock,
			blind_data->info.presence, &blind_data->info,
			tmp_blind_info->presence, tmp_blind_info,
			sizeof(blind_info_t));
	
propagate:	
	/* Save the previous source agency UID in the history */
	memcpy(&agencyUID_history[0], &agencyUID_history[1], SOO_AGENCY_UID_SIZE);

	/* Retrieve the agency UID of the Smart Object from which the ME will be sent */
	agency_ctl_args.cmd = AG_AGENCY_UID;
	args->__agency_ctl(&agency_ctl_args);
	memcpy(&agencyUID_history[1], &agency_ctl_args.u.agencyUID_args.agencyUID, SOO_AGENCY_UID_SIZE);

	DBG("SOO." APP_NAME " ME being sent by: ");
	DBG_BUFFER(&agencyUID_history[1], SOO_AGENCY_UID_SIZE);

	return 0;
}


/**
 * KILL_ME
 */
int cb_kill_me(soo_domcall_arg_t *args) {
	DBG("Kill-ME %d\n", ME_domID());

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
	DBG("Pre suspend %d\n", ME_domID());

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
	size_t recv_data_size;
	bool SOO_blind_present = false;
	uint32_t pfn;

	DBG0("Cooperate\n");

	switch (cooperate_args->role) {
	case COOPERATE_INITIATOR:
		DBG("Cooperate: Initiator %d\n", ME_domID());
		if (cooperate_args->alone)
			return 0;

		for (i = 0; i < MAX_ME_DOMAINS; i++) {
			/* Cooperation with a SOO.blind ME */
			if (!memcmp(cooperate_args->u.target_coop_slot[i].spid, SOO_blind_spid, SPID_SIZE)) {
				DBG("SOO." APP_NAME ": SOO.blind running on the Smart Object\n");
				SOO_blind_present = true;

				/* Only cooperate with MEs that accept to cooperate */
				if (!cooperate_args->u.target_coop_slot[i].spad.valid)
					continue;

				/*
				 * Prepare the arguments to transmit to the target ME:
				 * - Initiator's SPID
				 * - Initiator's SPAD capabilities
				 * - pfn of the localinfo
				 */
				agency_ctl_args.cmd = AG_COOPERATE;
				agency_ctl_args.slotID = cooperate_args->u.target_coop_slot[i].slotID;
				memcpy(agency_ctl_args.u.target_cooperate_args.spid, get_ME_desc()->spid, SPID_SIZE);
				memcpy(agency_ctl_args.u.target_cooperate_args.spad_caps, get_ME_desc()->spad.caps, SPAD_CAPS_SIZE);
				agency_ctl_args.u.target_cooperate_args.pfns.content = phys_to_pfn(virt_to_phys_pt((uint32_t) localinfo_data));
				args->__agency_ctl(&agency_ctl_args);

#if 0 /* Arrived ME disappears now... */
				set_ME_state(ME_state_killed);
#endif
			}

			/* Cooperation with a SOO.outdoor ME */
			if (!memcmp(cooperate_args->u.target_coop_slot[i].spid, SOO_outdoor_spid, SPID_SIZE))
				DBG("SOO." APP_NAME ": SOO.outdoor running on the Smart Object\n");
		}

		/*
		 * There are two cases in which we have to kill the initiator SOO.blind ME:
		 * - There is already a SOO.blind ME running on the Smart Object, independently of its nature.
		 * - There is no SOO.blind ME running on a non-SOO.blind Smart Object (the expected devcaps are not present) and there
		 *   is no connected SOO.blind application.
		 */
		if (SOO_blind_present || (!SOO_blind_present && !available_devcaps && !remote_app_connected)) {
			// agency_ctl_args.cmd = AG_KILL_ME;
			// agency_ctl_args.slotID = ME_domID();
			// DBG("Kill ME in slot ID: %d\n", ME_domID());
			// args->__agency_ctl(&agency_ctl_args);
			DBG("Kill ME in slot ID: %d\n", ME_domID());
			set_ME_state(ME_state_killed);
		}

		break;

	case COOPERATE_TARGET:
		DBG("Cooperate: SOO." APP_NAME " Target %d\n", ME_domID());

		DBG("SPID of the initiator: ");
		DBG_BUFFER(cooperate_args->u.initiator_coop.spid, SPID_SIZE);
		DBG("SPAD caps of the initiator: ");
		DBG_BUFFER(cooperate_args->u.initiator_coop.spad_caps, SPAD_CAPS_SIZE);

		/* Cooperation with a SOO.blind ME */
		if (!memcmp(cooperate_args->u.initiator_coop.spid, get_ME_desc()->spid, SPID_SIZE)) {
			DBG("Cooperation with SOO." APP_NAME "\n");

			pfn = cooperate_args->u.initiator_coop.pfns.content;

			recv_data_size = DIV_ROUND_UP(sizeof(blind_info_t), PAGE_SIZE) * PAGE_SIZE;
			recv_data = (void *) io_map(pfn_to_phys(pfn), recv_data_size);
#if 1
			 merge_info(&blind_lock,
					blind_data->info.presence, &blind_data->info,
					((blind_data_t *) recv_data)->info.presence, recv_data,
					tmp_blind_info->presence, tmp_blind_info,
					sizeof(blind_info_t));
#endif

			io_unmap((uint32_t) recv_data);
		}

		/* Cooperation with a SOO.outdoor ME */
		if (!memcmp(cooperate_args->u.initiator_coop.spid, SOO_outdoor_spid, SPID_SIZE)) {
			DBG("Cooperation with SOO.outdoor\n");

			pfn = cooperate_args->u.initiator_coop.pfns.content;

			recv_data_size = DIV_ROUND_UP(sizeof(outdoor_info_t), PAGE_SIZE) * PAGE_SIZE;
			recv_data = (void *) io_map(pfn_to_phys(pfn), recv_data_size);

			outdoor_merge_info(&((outdoor_data_t *) recv_data)->info);

			io_unmap((uint32_t) recv_data);
		}

#if 0 /* This pattern forces the termination of the residing ME (a kill ME is prohibited at the moment) */
		DBG("Force the termination of this ME #%d\n", ME_domID());
		agency_ctl_args.cmd = AG_FORCE_TERMINATE;
		agency_ctl_args.slotID = ME_domID();

		args->__agency_ctl(&agency_ctl_args);
#endif

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
	DBG0("pre_resume\n");

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
	DBG("Post activate %d\n", ME_domID());
	
	/*
	 * At the first post-activate, initialize the blind descriptor of this Smart Object and start the
	 * threads.
	 */
	if (unlikely(!blind_initialized)) {
		/* Save the agency UID of the Smart Object on which the ME has been injected */
		memcpy(&origin_agencyUID, &target_agencyUID, SOO_AGENCY_UID_SIZE);

		/* Save the name of the Smart Object on which the ME has been injected */
		strcpy(origin_soo_name, target_soo_name);

		create_my_desc();
		/* get_my_id(blind_data->info.presence); */

		/* DTN should reactivate this if it does not work in me.c */
		blind_start_threads();

		blind_initialized = true;

		blind_init_motor();
	}

	blind_action_post_activate();

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
	DBG("Localinfo update %d\n", ME_domID());

	return 0;
}

/**
 * FORCE_TERMINATE callback (async)
 *
 * Returns 0 if no propagation to the user space is required, 1 otherwise
 *
 */
int cb_force_terminate(void) {
	DBG("ME %d force terminate, state: %d\n", ME_domID(), get_ME_state());

	/* We do nothing particular here for this ME, however we proceed with the normal termination of execution */
	set_ME_state(ME_state_terminated);

	return 0;
}



/**
 * Initializations for callback handling.
 */
void callbacks_init(void) {
	int nr_pages = DIV_ROUND_UP(sizeof(blind_data_t), PAGE_SIZE);

	DBG("Localinfo: size=%d, nr_pages=%d\n", sizeof(blind_data_t), nr_pages);
	/* Allocate localinfo */
	localinfo_data = (void *) get_contig_free_vpages(nr_pages);

	memcpy(&last_agencyUID, &null_agencyUID, SOO_AGENCY_UID_SIZE);
	memcpy(&agencyUID_history[0], &null_agencyUID, SOO_AGENCY_UID_SIZE);
	memcpy(&agencyUID_history[1], &null_agencyUID, SOO_AGENCY_UID_SIZE);

	/* The ME accepts to collaborate */
	get_ME_desc()->spad.valid = true;

	/* Get ME SPID */
	memcpy(get_ME_desc()->spid, SOO_blind_spid, SPID_SIZE);
	lprintk("ME SPID: ");
	lprintk_buffer(SOO_blind_spid, SPID_SIZE);
}
