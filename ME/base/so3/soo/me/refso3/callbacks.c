/*
 * Copyright (C) 2014-2022 Daniel Rossier <daniel.rossier@soo.tech>
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

#include <me/refso3.h>

static LIST_HEAD(visits);
static LIST_HEAD(known_soo_list);


/* Reference to the shared content helpful during synergy with other MEs */
sh_refso3_t *sh_refso3;

/**
 * PRE-ACTIVATE
 *
 * Should receive local information through args
 */
int cb_pre_activate(soo_domcall_arg_t *args) {

	DBG(">> ME %d: cb_pre_activate...\n", ME_domID());

#if 0 /* To be implemented... */
	logmsg("[soo:me:SOO.refSO3] ME %d: cb_pre_activate..\n", ME_domID());
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



	pre_propagate_args->propagate_status = 0;
#if 0
	/* Enable migration - here, we migrate 3 times before being killed. */
	if ((get_ME_state() != ME_state_dormant) || (migration_count != 3)) {
		pre_propagate_args->propagate_status = 1;
		migration_count++;
	} else
		set_ME_state(ME_state_killed);
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
	agency_ctl_args_t agency_ctl_args;
        
	lprintk("[soo:me:SOO.refSO3] ME %d: cb_cooperate...\n", ME_domID());

	switch (cooperate_args->role) {
	case COOPERATE_INITIATOR:

		if (cooperate_args->alone)
			return 0;

		/* Collaboration ... */

		/* Update the list of hosts */
		sh_refso3->me_common.soohost_nr = concat_hosts(&visits, (uint8_t *) sh_refso3->me_common.soohosts);

		agency_ctl_args.u.cooperate_args.pfn = phys_to_pfn(virt_to_phys_pt((uint32_t) sh_refso3));
		agency_ctl_args.u.cooperate_args.slotID = ME_domID(); /* Will be copied in initiator_cooperate_args */

		/* This pattern enables the cooperation with the target ME */

		agency_ctl_args.cmd = AG_COOPERATE;
		agency_ctl_args.slotID = cooperate_args->u.target_coop.slotID;

		/* Perform the cooperate in the target ME */
		args->__agency_ctl(&agency_ctl_args);


		break;

	case COOPERATE_TARGET:
		DBG("Cooperate: Target %d\n", ME_domID());

		/* In the SOO.refso3 ME, we simply destroy the initiator */
		agency_ctl_args.cmd = AG_KILL_ME;
		agency_ctl_args.slotID = cooperate_args->u.initiator_coop.slotID;
		args->__agency_ctl(&agency_ctl_args);

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

	/* Allocate the shared page. */
	sh_refso3 = (sh_refso3_t *) get_contig_free_vpages(1);

	/* Initialize the shared content page used to exchange information between other MEs */
	memset(sh_refso3, 0, PAGE_SIZE);

	/* Set the SPAD capabilities (currently not used) */
	memset(&get_ME_desc()->spad, 0, sizeof(spad_t));

}


