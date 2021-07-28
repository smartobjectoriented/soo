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

/* Reference to the shared content helpful during synergy with other MEs */
sh_ledctrl_t *sh_ledctrl;
common_data_t *shared_data;

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

	DBG(">> ME %d: cb_pre_activate...\n", ME_domID());


	/* Retrieve the agency UID of the Smart Object on which the ME has migrated */
	agency_ctl_args.cmd = AG_AGENCY_UID;
	args->__agency_ctl(&agency_ctl_args);
	lprintk("device rpi4 ID:");
	printID(agency_ctl_args.u.agencyUID_args.agencyUID.id);

	
	/*if id not init */
	if(shared_data->id[0] == 0){

		lprintk("init id\n");
		/*save the source uid*/
		memcpy(&shared_data->id, &agency_ctl_args.u.agencyUID_args.agencyUID.id, SOO_AGENCY_UID_SIZE);
	
		/*init the list of Visited Device*/
		memcpy(&shared_data->ID_device_visited[0], &agency_ctl_args.u.agencyUID_args.agencyUID.id, SOO_AGENCY_UID_SIZE);
		shared_data->nb_device_visited++;

	}else{

		/*ME dont need to be executed outside source (exepted for soo.agency)*/
		set_ME_state(ME_state_dormant);

		lprintk("check the vicious circle\n");
		/*check the vicious circle*/
		for(i = 0; i < shared_data->nb_device_visited; i++) {

			if(!memcmp(&shared_data->ID_device_visited[i],&agency_ctl_args.u.agencyUID_args.agencyUID.id,SOO_AGENCY_UID_SIZE)){

				/* Kill the ME to avoid circularity */
				lprintk("vicious circle detected\n");

			    lprintk("kill ME\n");
				agency_ctl_args.cmd = AG_FORCE_TERMINATE;
				agency_ctl_args.slotID = ME_domID();
				args->__agency_ctl(&agency_ctl_args);

				shared_data->killed =true;

				return 0;
			}

			
		} 
		shared_data->nb_device_visited++;
		lprintk("visited %d device \n",shared_data->nb_device_visited);

		//add the curent device ID to the list
		memcpy(&shared_data->ID_device_visited[shared_data->nb_device_visited - 1], &agency_ctl_args.u.agencyUID_args.agencyUID.id, SOO_AGENCY_UID_SIZE);

		

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

	/* Retrieve the agency UID of the Smart Object on which the ME has migrated */
	agency_ctl_args.cmd = AG_AGENCY_UID;
	args->__agency_ctl(&agency_ctl_args);
	
	pre_propagate_args->propagate_status = 0;

	/* Enable migration - here, we migrate 1 times before being killed. */
	if (sh_ledctrl->need_propagate) {
		pre_propagate_args->propagate_status = 1;
		shared_data->nb_jump++;
	}else if(memcmp(&shared_data->id,&agency_ctl_args.u.agencyUID_args.agencyUID.id,SOO_AGENCY_UID_SIZE)){
		/*if ME is not in source kill */
		shared_data->killed = true;
		agency_ctl_args.cmd = AG_FORCE_TERMINATE;
		agency_ctl_args.slotID = ME_domID();
		args->__agency_ctl(&agency_ctl_args);
	}
	
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
	unsigned int j;
	bool is_new_ID = false;
	unsigned char idexEnd;


	sh_ledctrl_t *incoming_sh_ledctrl;
	common_data_t *incoming_sh_data;

	uint32_t pfn;

	switch (cooperate_args->role) {
	case COOPERATE_INITIATOR:


		/*if the initator will be killed just skip the cooperate*/
		if(shared_data->killed == true){
			break;
		}

		/*
		 * If we are alone in this smart object or not in a well type, we stay here.
		 * The post_activate callback will update the LED.
		 */
		if (cooperate_args->alone) {

			//if ME alone jump to the next smart object
			sh_ledctrl->need_propagate = true;
			
			return 0;
		}

		shared_data->slotID =  ME_domID();

		for (i = 0; i < MAX_ME_DOMAINS; i++) {
			if (cooperate_args->u.target_coop_slot[i].spad.valid) {
				
				/* Collaboration ... */
				agency_ctl_args.u.target_cooperate_args.pfn.content =
					phys_to_pfn(virt_to_phys_pt((uint32_t) shared_data));

				/* This pattern enables the cooperation with the target ME */

				agency_ctl_args.cmd = AG_COOPERATE;
				agency_ctl_args.slotID = cooperate_args->u.target_coop_slot[i].slotID;

				/* Perform the cooperate in the target ME */
				args->__agency_ctl(&agency_ctl_args);
			}
		}

		break;

	case COOPERATE_TARGET:

		pfn = cooperate_args->u.initiator_coop.pfn.content;

		incoming_sh_data = (common_data_t *) io_map(pfn_to_phys(pfn), PAGE_SIZE);
		incoming_sh_ledctrl = &incoming_sh_data->ledctrl;


		/*if the target will be killed soon just skip the cooperate*/
		if(shared_data->killed == true){
			break;
		}

		/*if the tow ME have a differant type*/
		if(shared_data->type != incoming_sh_data->type){
			/*do the info exchange or not*/

			/*if ME com from a soo.outdoor update DATA*/
			if(incoming_sh_data->type == SOO_SIM_OUTDOOR){
				sh_ledctrl->incoming_nr = incoming_sh_data->ledctrl.local_nr;
				complete(&sh_ledctrl->upd_lock);
			}

			/*jump to the next smart object*/
			incoming_sh_ledctrl->need_propagate = true;
			break;
		}

		/*if the id source of both ME is not the same */
		if(memcmp(&shared_data->id,&incoming_sh_data->id,SOO_AGENCY_UID_SIZE)){

			/*if ME com from an other soo.blind*/
			sh_ledctrl->incoming_nr = incoming_sh_data->ledctrl.local_nr;
			complete(&sh_ledctrl->upd_lock);

			/*jump to the next smart object*/
			incoming_sh_ledctrl->need_propagate = true;
			break;
		}

		/*if ME have the same id and the same type kille the less recent*/
		if(incoming_sh_data->timeStamp < shared_data->timeStamp){
			agency_ctl_args.cmd = AG_FORCE_TERMINATE;
			agency_ctl_args.slotID = ME_domID();
			args->__agency_ctl(&agency_ctl_args);

		}else if(incoming_sh_data->timeStamp > shared_data->timeStamp){
			agency_ctl_args.cmd = AG_FORCE_TERMINATE;
			agency_ctl_args.slotID = incoming_sh_data->slotID;
			args->__agency_ctl(&agency_ctl_args);

		}else{
			/*if ME have the same timeStamp merge liste*/
			lprintk("ME have the same time stamp\n");
			lprintk("merge list and kill initator\n");

		
			/*end of list*/
			idexEnd = shared_data->nb_device_visited;

			/*update list*/
			for(i = 0; i < incoming_sh_data->nb_device_visited; i++){
				is_new_ID = true;
				
				/*find if id know*/
				for(j = 0; j < shared_data->nb_device_visited; j++){
					if(!memcmp(&shared_data->ID_device_visited[j],&incoming_sh_data->ID_device_visited[i],SOO_AGENCY_UID_SIZE)){
						is_new_ID = false;
					}
				}

				/*if id is new add to list*/
				if(is_new_ID){
					memcpy(&shared_data->ID_device_visited[idexEnd++], &incoming_sh_data->ID_device_visited[i], SOO_AGENCY_UID_SIZE);
				}
			}
			shared_data->nb_device_visited  = idexEnd;

			/*update local data or not*/
			sh_ledctrl->incoming_nr = incoming_sh_data->ledctrl.local_nr;
			complete(&sh_ledctrl->upd_lock);

			/*KILL initiatore ME*/
			agency_ctl_args.cmd = AG_FORCE_TERMINATE;
			agency_ctl_args.slotID = incoming_sh_data->slotID;
			args->__agency_ctl(&agency_ctl_args);
			incoming_sh_data->killed = true;

			/*jump to the next smart object*/
			shared_data->ledctrl.need_propagate = true;
		}
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
	set_ME_state(ME_state_killed);

	return 0;
}

void callbacks_init(void) {

	/* Allocate localinfo */
	shared_data = (common_data_t *) get_contig_free_vpages(1);

	sh_ledctrl = &shared_data->ledctrl;
	/* Initialize the shared content page used to exchange information between other MEs */
	memset(shared_data, 0, PAGE_SIZE);

	init_completion(&sh_ledctrl->upd_lock);

	sh_ledctrl->local_nr = -1;
	sh_ledctrl->incoming_nr = -1;

	/* Set the SPAD capabilities (currently not used) */
	memset(get_ME_desc()->spad.caps, 0, SPAD_CAPS_SIZE);

	shared_data->type = SOO_SIM_BLIND;


}


