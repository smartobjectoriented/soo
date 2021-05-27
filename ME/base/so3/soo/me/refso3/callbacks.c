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
#include <heap.h>

#if 0
#define DEBUG
#endif

#if 1 /*mode demo*/
#define NB_DEMO_DEVICE 4		
#endif


void printID(unsigned char* id){
	int i;
	
	for(i=0;i<SOO_AGENCY_UID_SIZE;i++){
		lprintk("%x ",id[i]);
	}

	lprintk("\n");

}


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
agencyUID_t listOfVisitedDevice;




/*
 * migrated_once allows the dormant ME to control its oneshot propagation, i.e.
 * the ME must be broadcast in the neighborhood, then disappear from the smart object.
 */
uint32_t migration_count;


/**
 * PRE-ACTIVATE
 *
 * Should receive local information through args
 */
int cb_pre_activate(soo_domcall_arg_t *args) {

	agency_ctl_args_t agency_ctl_args;
	struct list_head *list_it;
	agencyUID_t *id;

	

	DBG(">> ME %d: cb_pre_activate...\n", ME_domID());
	migration_count = 0;

	/* Retrieve the agency UID of the Smart Object on which the ME has migrated */
	agency_ctl_args.cmd = AG_AGENCY_UID;
	args->__agency_ctl(&agency_ctl_args);
	lprintk("device ID:");
	printID(agency_ctl_args.u.agencyUID_args.agencyUID.id);

	
	
	/*if id not init */
	if(localData->id[0] == 0){

		lprintk("init id and list\n");
		
		/*save the source uid*/
		memcpy(&localData->id, &agency_ctl_args.u.agencyUID_args.agencyUID.id, SOO_AGENCY_UID_SIZE);

		/*init the list of Visited Device*/
		memcpy(&listOfVisitedDevice.id, &agency_ctl_args.u.agencyUID_args.agencyUID.id, SOO_AGENCY_UID_SIZE);
		listOfVisitedDevice.list.next = NULL;
		listOfVisitedDevice.list.prev = NULL;

	}else{
		lprintk("check the vicious circle\n");
		list_it = &listOfVisitedDevice.list;

		/*check the vicious circle*/
		while(list_it->next){

			//get id
			id = container_of(list_it,agencyUID_t,list);
			printID(id->id);

			//if the device is know
			if(!memcmp(&id->id,&agency_ctl_args.u.agencyUID_args.agencyUID.id,SOO_AGENCY_UID_SIZE)){

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

			
			list_it = list_it->next;
		} 
		localData->nb_device_visited++;
		lprintk("visited %d device \n",localData->nb_device_visited);

		#if 1 /*mode demo*/

		if(localData->nb_device_visited == NB_DEMO_DEVICE){
			lprintk("\n\n\n\n\n!!!!!!!!!!!!!!! fin demo !!!!!!!!!!!!!!!!!!!!\n\n\n\n");
		}

		#endif

		//add the curent device ID to the list
		id = (agencyUID_t*) malloc(sizeof(agencyUID_t)); 
		memcpy(&id->id, &agency_ctl_args.u.agencyUID_args.agencyUID.id, SOO_AGENCY_UID_SIZE);
		list_it->next = &id->list;
		id->list.next = 0;

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
	agency_ctl_args_t agency_ctl_args;

	DBG(">> ME %d: cb_pre_propagate...\n", ME_domID());

	pre_propagate_args->propagate_status = 0;

	/* Enable migration - here, we migrate 1 times before being killed. */
	if ((migration_count < 1) && (localData->nb_jump < 7)) {
		pre_propagate_args->propagate_status = 1;
		localData->nb_jump++;
		migration_count++;
	} else{
		agency_ctl_args.cmd = AG_FORCE_TERMINATE;
		agency_ctl_args.slotID = ME_domID();
		args->__agency_ctl(&agency_ctl_args);
	}
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
	uint32_t pfn;



	lprintk("[soo:me:SOO.refSO3] ME %d: cb_cooperate...\n", ME_domID());

	switch (cooperate_args->role) {
	case COOPERATE_INITIATOR:

		if (cooperate_args->alone)
			return 0;

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
		


		/*if ME have the same device and have the same type*/
		if(!memcmp(&RxData->id,&localData->id,SOO_AGENCY_UID_SIZE) && (RxData->type == localData->type)){
				
			lprintk("same ME kill the less recent\n");

			if(RxData->timeStamp < localData->timeStamp){

				agency_ctl_args.cmd = AG_FORCE_TERMINATE;
				agency_ctl_args.slotID = ME_domID();
				args->__agency_ctl(&agency_ctl_args);
			}else if(RxData->timeStamp > localData->timeStamp){
				/*agency_ctl_args.cmd = AG_FORCE_TERMINATE;
				agency_ctl_args.slotID = cooperate_args->u.initiator_coop.;
				args->__agency_ctl(&agency_ctl_args);*/
			}	
		}


		/*TODO reste de la logique de colaboration*/



#if 0 /* This pattern forces the termination of the residing ME (a kill ME is prohibited at the moment) */
		DBG("Force the termination of this ME #%d\n", ME_domID());
		agency_ctl_args.cmd = AG_FORCE_TERMINATE;
		agency_ctl_args.slotID = ME_domID();

		args->__agency_ctl(&agency_ctl_args);
#endif

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

	set_ME_state(ME_state_terminated);

	return 0;
}

void callbacks_init(void) {

	/* Allocate localinfo */
	localinfo_data = (void *) get_contig_free_vpages(1);

	localData = (common_data* ) localinfo_data;

	/*init localData*/
	localData->id[0] = 0;
	localData->nb_jump = 0;
	localData->timeStamp = 0;
	localData->type = 0;
	localData->nb_device_visited = 1;


	/* Set the SPAD capabilities */
	memset(get_ME_desc()->spad.caps, 0, SPAD_CAPS_SIZE);
}