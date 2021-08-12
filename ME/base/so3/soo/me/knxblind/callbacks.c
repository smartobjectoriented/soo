/*
 * Copyright (C) 2021 Thomas Rieder <thomas.rieder@heig-vd.ch>
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
#include <me/knxblind/knxblind.h>

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
common_data* localData;
common_data* RxData;


/* SPID of the SOO.knxblind ME */
uint8_t SOO_knxblind_spid[SPID_SIZE] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x61, 0xd0, 0x0c };


/* SPID of the SOO.indoor ME needed to cooperate with */
uint8_t SOO_dogablind_spid[SPID_SIZE] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x61, 0xd0, 0x0d };


#if 0
static int live_count = 0;
#endif

/*
 * migrated_once allows the dormant ME to control its oneshot propagation, i.e.
 * the ME must be broadcast in the neighborhood, then disappear from the smart object.
 */
#if 0
static uint32_t migration_count = 0;
#endif

/**
 * PRE-ACTIVATE
 *
 * Should receive local information through args
 */
int cb_pre_activate(soo_domcall_arg_t *args) {

	lprintk("SOO.knxblind cb_pre_activate\n");
	return 0;
}

/**
 * PRE-PROPAGATE
 *
 * The callback is executed in first stage to give a chance to a resident ME to stay or disappear, for example.
 */
int cb_pre_propagate(soo_domcall_arg_t *args) {
	
	pre_propagate_args_t *pre_propagate_args = (pre_propagate_args_t *) &args->u.pre_propagate_args;
	
	// lprintk("SOO.knxblind cb_pre_propagate\n");
	
	pre_propagate_args->propagate_status = 0;

	return 0;
}

/**
 * Kill domcall - if another ME tries to kill us.
 */
int cb_kill_me(soo_domcall_arg_t *args) {

	lprintk("SOO.knxblind cb_kill_me\n");
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
	lprintk("SOO.knxblind cb_pre_suspend\n");
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
	// unsigned int j;
	// bool is_new_ID = false;
	// unsigned char idexEnd;
	uint32_t pfn;

	// lprintk("SOO.knxblind cb_cooperate\n");


	lprintk("[soo:me:SOO.knxblind] ME %d: cb_cooperate...\n", ME_domID());

	switch (cooperate_args->role) {
	case COOPERATE_INITIATOR:

		if (cooperate_args->alone)
			return 0;

		localData->slotID = ME_domID();

		for (i = 0; i < MAX_ME_DOMAINS; i++) {
			if (cooperate_args->u.target_coop_slot[i].spad.valid) {

				/* Collaboration ... */
				agency_ctl_args.u.target_cooperate_args.pfn.content = phys_to_pfn(virt_to_phys_pt((uint32_t) localinfo_data));

				/* This pattern enables the cooperation with the target ME */
				agency_ctl_args.cmd = AG_COOPERATE;
				agency_ctl_args.slotID = cooperate_args->u.target_coop_slot[i].slotID;

				/* Perform the cooperate in the target ME */
				args->__agency_ctl(&agency_ctl_args);
			}
		}
		break;

	case COOPERATE_TARGET:
		DBG("Cooperate: Target %d\n", ME_domID());
		DBG("SPID of the initiator: ");
		DBG_BUFFER(cooperate_args->u.initiator_coop.spid, SPID_SIZE);
		DBG("SPAD caps of the initiator: ");
		DBG_BUFFER(cooperate_args->u.initiator_coop.spad_caps, SPAD_CAPS_SIZE);



		pfn = cooperate_args->u.initiator_coop.pfn.content;
		RxData = (common_data *) io_map(pfn_to_phys(pfn), sizeof(RxData));


		/* Cooperate with ME SOO.dogablind */
		if(memcmp(RxData->type, SOO_dogablind_spid, SPID_SIZE) == 0){
				
			// lprintk("SOO.knxblind detect SOO.indoor\n");

			lprintk("___ CALLBACK RxData : cmd = %d\n", RxData->data_dogablind.blind_cmd);

			localData->data_knxblind.blind_cmd = RxData->data_dogablind.blind_cmd;
			localData->data_knxblind.sw_id = RxData->data_dogablind.sw_id;

			// WAKE UP ME THREAD !
			complete(&g_knxblind_data->sw_cmd_receive);

		}

		/*KILL initiatore ME*/
		agency_ctl_args.cmd = AG_FORCE_TERMINATE;
		agency_ctl_args.slotID = RxData->slotID;
		args->__agency_ctl(&agency_ctl_args);

		/*TODO reste de la logique de propagation*/

		io_unmap((uint32_t) RxData);
	
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
	lprintk("SOO.knxblind cb_post_activate\n");
	DBG(">> ME %d: cb_pre_resume...\n", ME_domID());

	return 0;
}

/**
 * POST_ACTIVATE callback (async)
 */
int cb_post_activate(soo_domcall_arg_t *args) {
	lprintk("SOO.knxblind cb_post_activate\n");
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

	lprintk("SOO.knxblind cb_localinfo_update\n");
	return 0;
}

/**
 * FORCE_TERMINATE callback (async)
 *
 * Returns 0 if no propagation to the user space is required, 1 otherwise
 *
 */

int cb_force_terminate(void) {
	lprintk("SOO.knxblind cb_force_terminate\n");
	DBG(">> ME %d: cb_force_terminate...\n", ME_domID());
	DBG("ME state: %d\n", get_ME_state());

	/* We do nothing particular here for this ME,
	 * however we proceed with the normal termination of execution.
	 */

	set_ME_state(ME_state_terminated);

	return 0;
}

void callbacks_init(void) {

	lprintk("SOO.knxblind callbacks_init\n");

	/* Allocate localinfo */
	localinfo_data = (void *) get_contig_free_vpages(1);
	localData = (common_data* ) localinfo_data;

	/*init localData*/
	localData->id[0] = 0;
	localData->nb_jump = 0;
	localData->timeStamp = 0;
	localData->nb_device_visited = 0;
	
	g_knxblind_data = &localData->data_knxblind;

	init_completion(&g_knxblind_data->sw_cmd_receive);

	memcpy(localData->type, SOO_knxblind_spid, SPID_SIZE);

	/* Set the SPAD capabilities */
	memset(get_ME_desc()->spad.caps, 0, SPAD_CAPS_SIZE);

}


