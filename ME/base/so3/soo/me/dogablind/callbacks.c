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
#include <me/dogablind/dogablind.h>

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


/* SPID of the SOO.dogablind ME */
uint8_t SOO_dogablind_spid[SPID_SIZE] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x61, 0xd0, 0x0d };


/* SPID of the SOO.indoor ME needed to cooperate with */
uint8_t SOO_knxblind_spid[SPID_SIZE] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x61, 0xd0, 0x0c };


#if 0
static int live_count = 0;
#endif

/*
 * migrated_once allows the dormant ME to control its oneshot propagation, i.e.
 * the ME must be broadcast in the neighborhood, then disappear from the smart object.
 */
#if 1
static uint32_t migration_count = 0;
#endif

void printID(unsigned char* id){
	int i;
	
	for(i=0;i<SOO_AGENCY_UID_SIZE;i++){
		lprintk("%x ",id[i]);
	}

	lprintk("\n");

}
void print_list_id(unsigned char list[][SOO_AGENCY_UID_SIZE],unsigned nb_ID){
	int i;
	lprintk("list of ID:\n");
	for(i = 0; i < nb_ID; i++){
		printID(list[i]);
	}

}
/**
 * PRE-ACTIVATE
 *
 * Should receive local information through args
 */
int cb_pre_activate(soo_domcall_arg_t *args) {


	agency_ctl_args_t agency_ctl_args;
	ssize_t i;	

	lprintk("SOO.dogablind cb_pre_activate\n");
	
	DBG(">> ME %d: cb_pre_activate...\n", ME_domID());
	migration_count = 0;

	/* Retrieve the agency UID of the Smart Object on which the ME has migrated */
	agency_ctl_args.cmd = AG_AGENCY_UID;
	args->__agency_ctl(&agency_ctl_args);
	lprintk("device rpi4 ID:");
	printID(agency_ctl_args.u.agencyUID_args.agencyUID.id);

	
	
	/*if id not init */
	if(localData->id[0] == 0){

		lprintk("init id\n");

		/*save the source uid*/
		memcpy(&localData->id, &agency_ctl_args.u.agencyUID_args.agencyUID.id, SOO_AGENCY_UID_SIZE);

		/*init the list of Visited Device*/
		memcpy(&localData->ID_device_visited[0], &agency_ctl_args.u.agencyUID_args.agencyUID.id, SOO_AGENCY_UID_SIZE);
		localData->nb_device_visited++;

	}else{

		lprintk("check the vicious circle\n");
		
		set_ME_state(ME_state_dormant);

		/*check the vicious circle*/
		for(i = 0; i < localData->nb_device_visited; i++) {

			if(!memcmp(&localData->ID_device_visited[i],&agency_ctl_args.u.agencyUID_args.agencyUID.id,SOO_AGENCY_UID_SIZE)){

				/* Kill the ME to avoid circularity */
				lprintk("vicious circle detected\n");
				

			    lprintk("kill ME\n");
				//delete the ME
				DBG("Force the termination of this ME #%d\n", ME_domID());
				agency_ctl_args.cmd = AG_FORCE_TERMINATE;
				agency_ctl_args.slotID = ME_domID();

				args->__agency_ctl(&agency_ctl_args);

				return 0;
			}

			
		} 
		localData->nb_device_visited++;
		lprintk("visited %d device \n",localData->nb_device_visited);

		//add the curent device ID to the list
		memcpy(&localData->ID_device_visited[localData->nb_device_visited - 1], &agency_ctl_args.u.agencyUID_args.agencyUID.id, SOO_AGENCY_UID_SIZE);
	
	}
	return 0;
}

/**
 * PRE-PROPAGATE
 *
 * The callback is executed in first stage to give a chance to a resident ME to stay or disappear, for example.
 */
int cb_pre_propagate(soo_domcall_arg_t *args) {
	
	pre_propagate_args_t *pre_propagate_args = (pre_propagate_args_t *) &args->u.pre_propagate_args;
	
	// lprintk("SOO.dogablind cb_pre_propagate\n");
	
	pre_propagate_args->propagate_status = 0;

	if(g_dogablind_data->need_propagate) {

		lprintk("ME SOO.dogablind migrate !!\n");

		pre_propagate_args->propagate_status = 1;
		localData->nb_jump++;
		migration_count++;
	}


	g_dogablind_data->need_propagate = false;

	return 0;
}

/**
 * Kill domcall - if another ME tries to kill us.
 */
int cb_kill_me(soo_domcall_arg_t *args) {

	lprintk("SOO.dogablind cb_kill_me\n");
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
	lprintk("SOO.dogablind cb_pre_suspend\n");
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

	// lprintk("SOO.dogablind cb_cooperate\n");


	lprintk("[soo:me:SOO.dogablind] ME %d: cb_cooperate...\n", ME_domID());

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


		/*KILL initiatore ME*/
		// agency_ctl_args.cmd = AG_FORCE_TERMINATE;
		// agency_ctl_args.slotID = RxData->slotID;
		// args->__agency_ctl(&agency_ctl_args);

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
	lprintk("SOO.dogablind cb_pre_resume\n");
	DBG(">> ME %d: cb_pre_resume...\n", ME_domID());

	return 0;
}

/**
 * POST_ACTIVATE callback (async)
 */
int cb_post_activate(soo_domcall_arg_t *args) {
	lprintk("SOO.dogablind cb_post_activate\n");
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

	lprintk("SOO.dogablind cb_localinfo_update\n");
	return 0;
}

/**
 * FORCE_TERMINATE callback (async)
 *
 * Returns 0 if no propagation to the user space is required, 1 otherwise
 *
 */

int cb_force_terminate(void) {
	lprintk("SOO.dogablind cb_force_terminate\n");
	DBG(">> ME %d: cb_force_terminate...\n", ME_domID());
	DBG("ME state: %d\n", get_ME_state());

	/* We do nothing particular here for this ME,
	 * however we proceed with the normal termination of execution.
	 */

	set_ME_state(ME_state_terminated);

	return 0;
}

void callbacks_init(void) {

	lprintk("SOO.dogablind callbacks_init\n");

	/* Allocate localinfo */
	localinfo_data = (void *) get_contig_free_vpages(1);
	localData = (common_data* ) localinfo_data;

	/*init localData*/
	localData->id[0] = 0;
	localData->nb_jump = 0;
	localData->timeStamp = 0;
	localData->nb_device_visited = 0;
	
	g_dogablind_data = &localData->data_dogablind;

	// init_completion(&g_dogablind_data->wait_me_indoor);

	memcpy(localData->type, SOO_dogablind_spid, SPID_SIZE);

	/* Set the SPAD capabilities */
	memset(get_ME_desc()->spad.caps, 0, SPAD_CAPS_SIZE);

}


