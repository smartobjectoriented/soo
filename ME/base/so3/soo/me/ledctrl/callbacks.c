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

#include <soo/dev/vsenseled.h>

#include <me/ledctrl.h>

static LIST_HEAD(visits);
static LIST_HEAD(known_soo_list);

/* Reference to the shared content helpful during synergy with other MEs */
sh_ledctrl_t *sh_ledctrl;

/**
 * PRE-ACTIVATE
 *
 * Should receive local information through args
 */
int cb_pre_activate(soo_domcall_arg_t *args) {
	agency_ctl_args_t agency_ctl_args;
	host_entry_t *host_entry;

	/* Retrieve the agency UID of the Smart Object on which the ME is about to be activated. */
	agency_ctl_args.cmd = AG_AGENCY_UID;
	args->__agency_ctl(&agency_ctl_args);

	host_entry = find_host(&visits, &agency_ctl_args.u.agencyUID);
	if (host_entry) {

		/* We already visited this place. */

		/* If we are not returning in our origin, we kill ourself */

		if (cmpUID(&sh_ledctrl->initiator, &agency_ctl_args.u.agencyUID))
			set_ME_state(ME_state_killed); /* Will be removed by the agency */

	} else
		new_host(&visits, &agency_ctl_args.u.agencyUID, NULL, 0);


	return 0;
}

/**
 * PRE-PROPAGATE
 *
 * The callback is executed in first stage to give a chance to a resident ME to stay or disappear, for example.
 */
int cb_pre_propagate(soo_domcall_arg_t *args) {
	pre_propagate_args_t *pre_propagate_args = (pre_propagate_args_t *) &args->u.pre_propagate_args;

	pre_propagate_args->propagate_status = (sh_ledctrl->need_propagate ? PROPAGATE_STATUS_YES : PROPAGATE_STATUS_NO);

	/* Only once */
	sh_ledctrl->need_propagate = false;

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
	agency_ctl_args_t agency_ctl_args;
	unsigned int i;
	struct list_head incoming_hosts;
	sh_ledctrl_t *incoming_sh_ledctrl;

	uint32_t pfn;

	switch (cooperate_args->role) {
	case COOPERATE_INITIATOR:

		/*
		 * If we are alone in this smart object, we stay here.
		 * The post_activate callback will update the LED.
		 */
		if (cooperate_args->alone) {

			/* No LED currently switched on. */
			sh_ledctrl->local_nr = -1;

			/*
			 * We will stay resident in this smart object. We re-init the list of (visited) hosts.
			 * Only our resident SOO is at the first position.
			 */
			clear_hosts(&visits);

			agency_ctl_args.cmd = AG_AGENCY_UID;
			args->__agency_ctl(&agency_ctl_args);

			/* Store the agencyUID in our structure */
			memcpy(&sh_ledctrl->here, &agency_ctl_args.u.agencyUID, SOO_AGENCY_UID_SIZE);

			new_host(&visits, &sh_ledctrl->here, NULL, 0);

			complete(&sh_ledctrl->upd_lock);

			return 0;
		}

		for (i = 0; i < MAX_ME_DOMAINS; i++) {
			if (cooperate_args->u.target_coop_slot[i].spad.valid) {

				/* Collaboration ... */

				/* Update the list of hosts */
				sh_ledctrl->me_common.soohost_nr = concat_hosts(&visits, (uint8_t *) &sh_ledctrl->me_common.soohosts);

				agency_ctl_args.u.target_cooperate_args.pfn.content =
					phys_to_pfn(virt_to_phys_pt((uint32_t) sh_ledctrl));

				/* This pattern enables the cooperation with the target ME */

				agency_ctl_args.cmd = AG_COOPERATE;
				agency_ctl_args.slotID = cooperate_args->u.target_coop_slot[i].slotID;

				/* Perform the cooperate in the target ME */
				args->__agency_ctl(&agency_ctl_args);
			}
		}

		/* Can disappear...*/

		set_ME_state(ME_state_killed);

		break;

	case COOPERATE_TARGET:

#if 0 /* Will trigger a force_terminate on us */
		agency_ctl_args.cmd = AG_KILL_ME;
		agency_ctl_args.slotID = args->slotID;
		args->__agency_ctl(&agency_ctl_args);
#endif

		pfn = cooperate_args->u.initiator_coop.pfn.content;
		incoming_sh_ledctrl = (sh_ledctrl_t *) io_map(pfn_to_phys(pfn), PAGE_SIZE);

		sh_ledctrl->incoming_nr = incoming_sh_ledctrl->local_nr;

		memcpy(&sh_ledctrl->initiator, &incoming_sh_ledctrl->initiator, SOO_AGENCY_UID_SIZE);

		/* Check if we are in the initiator, if not we keep propagating */
		if (cmpUID(&sh_ledctrl->here, &sh_ledctrl->initiator))
			sh_ledctrl->need_propagate = true;
		else {

			/* Look for all known SOOs updated */

			/* At the beginning? */
			if (sh_ledctrl->incoming_nr == 0) {
				if (!find_host(&known_soo_list, &incoming_sh_ledctrl->here))
					/* Insert this new SOO.ledctrl smart object */
					new_host(&known_soo_list, &incoming_sh_ledctrl->here, NULL, 0);
			} else {

				/* We compare the list of visits of this incoming ME against
				 * our list of known SOOs.
				 */
				expand_hosts(&incoming_hosts, sh_ledctrl->me_common.soohosts, sh_ledctrl->me_common.soohost_nr);

				if (hosts_equals(&incoming_hosts, &known_soo_list)) {

					/* We can reset our state */
					sh_ledctrl->incoming_nr = 0;
				}
			}
		}

		io_unmap((uint32_t) incoming_sh_ledctrl);

		complete(&sh_ledctrl->upd_lock);

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

	set_ME_state(ME_state_terminated);

	return 0;
}

void callbacks_init(void) {

	/* Allocate localinfo */
	sh_ledctrl = (sh_ledctrl_t *) get_contig_free_vpages(1);

	/* Initialize the shared content page used to exchange information between other MEs */
	memset(sh_ledctrl, 0, PAGE_SIZE);

	init_completion(&sh_ledctrl->upd_lock);

	sh_ledctrl->local_nr = -1;
	sh_ledctrl->incoming_nr = -1;

	/* Set the SPAD capabilities (currently not used) */
	memset(get_ME_desc()->spad.caps, 0, SPAD_CAPS_SIZE);

}


