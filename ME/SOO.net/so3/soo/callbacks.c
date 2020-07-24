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

#include <soo/evtchn.h>
#include <asm/mmu.h>

#include <memory.h>
#include <completion.h>
#include <timer.h>

#include <soo/avz.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/soo.h>
#include <soo/console.h>
#include <soo/debug.h>

#include <soo/dev/vnet.h>
#include <soo/dev/vnetbuff.h>

/*
 * ME Description:
 * The ME resides in one (and only one) Smart Object.
 * It is propagated to other Smart Objects until it meets the one with the UID 0x08.
 * At this moment, the ME keeps the info (localinfo_data+1) until the ME comes back to its origin (the running ME instance).
 * The letter is then incremented and the ME is ready for a new trip.
 * The ME must stay dormant in the Smart Objects different than the origin and the one with UID 0x08.
 */


struct localinfo {
        uint32_t data_ring_pfn;
        uint32_t data_sring_pfn;
        uint32_t buff_tx_pfn;
        uint32_t buff_rx_pfn;

        uint64_t last_rx_timestamp;
        uint64_t last_tx_timestamp;

        uint32_t rx_start, rx_end;
        uint32_t tx_start, tx_end;

};

/* Localinfo buffer used during cooperation processing */
struct localinfo *localinfo_data;

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
        vnet_t *vnet;
        pre_propagate_args_t *pre_propagate_args = (pre_propagate_args_t *) &args->u.pre_propagate_args;

        DBG(">> ME %d: cb_pre_propagate...\n", ME_domID());

        vnet = vnet_get_vnet();

        if(vnet->connected) {
                /* Connected so we don't need to transmit */
                localinfo_data->tx_start = localinfo_data->tx_end = 0;

                localinfo_data->rx_start = localinfo_data->rx_end;
                localinfo_data->rx_end = vnet->ring_data.sring->rsp_prod;

                printk("I'm connected share %d RX frames with other MEs\n", localinfo_data->rx_start - localinfo_data->rx_end );

        } else {
                /* Not connected so we don't have any outside frame to share */
                localinfo_data->rx_start = localinfo_data->rx_end = 0;

                localinfo_data->tx_start = localinfo_data->tx_end;
                localinfo_data->tx_end = vnet->ring_data.sring->req_prod;

                printk("I'm NOT connected share %d TX frames with other MEs\n", localinfo_data->tx_start - localinfo_data->tx_end );
        }

        /* propagate only if we have something to share */
        pre_propagate_args->propagate_status =
                localinfo_data->rx_start < localinfo_data->rx_end
                || localinfo_data->tx_start < localinfo_data->tx_end;


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
        return 1;
}

/**
 * COOPERATE
 *
 * This callback is executed when an arriving ME (initiator) decides to cooperate with a residing ME (target).
 */
int cb_cooperate(soo_domcall_arg_t *args) {
        cooperate_args_t *cooperate_args = (cooperate_args_t *) &args->u.cooperate_args;
        agency_ctl_args_t agency_ctl_args;

        unsigned char me_spad_caps[SPAD_CAPS_SIZE];
        unsigned int i;
        struct localinfo *recv_localinfo;
        struct vnet_data_front_ring data_ring;
        struct vnet_data_front_ring *data_ring_initiator;
        uint8_t *vbuff_rx, *vbuff_tx;
        uint32_t pfn;
        bool target_found, initiator_found;
        vnet_t *vnet;

        DBG(">> ME %d: cb_cooperate...\n", ME_domID());

        vnet = vnet_get_vnet();

        switch (cooperate_args->role) {
        case COOPERATE_INITIATOR:
                printk("Cooperate: Initiator %d\n", ME_domID());
                printk("Alone: %d\n", cooperate_args->alone);
                if (cooperate_args->alone){
                        vnet->connected = 0;
                        return 0;
                }

                for (i = 0; i < MAX_ME_DOMAINS; i++) {
                        if (cooperate_args->u.target_coop_slot[i].spad.valid) {

                                memcpy(me_spad_caps, cooperate_args->u.target_coop_slot[i].spad.caps, SPAD_CAPS_SIZE);

                                localinfo_data->data_ring_pfn = phys_to_pfn(virt_to_phys_pt((uint32_t)&vnet->ring_data));
                                localinfo_data->data_sring_pfn = phys_to_pfn(virt_to_phys_pt((uint32_t)vnet->ring_data.sring));
                                localinfo_data->buff_tx_pfn = phys_to_pfn(vnet_get_vbuff_tx()->data_phys);
                                localinfo_data->buff_rx_pfn = phys_to_pfn(vnet_get_vbuff_rx()->data_phys);

                                /* Collaboration ... */
                                agency_ctl_args.u.target_cooperate_args.pfns.content = phys_to_pfn(virt_to_phys_pt((uint32_t)localinfo_data));
                                /* This pattern enables the cooperation with the target ME */


                                agency_ctl_args.cmd = AG_COOPERATE;
                                agency_ctl_args.slotID = cooperate_args->u.target_coop_slot[i].slotID;
                                // TODO
                                //memcpy(agency_ctl_args.u.target_cooperate_args.spid, get_ME_desc()->spid, SPID_SIZE);
                                //memcpy(agency_ctl_args.u.target_cooperate_args.spad_caps, get_ME_desc()->spad.caps, SPAD_CAPS_SIZE);

                                /* Perform the cooperate in the target ME */
                                args->__agency_ctl(&agency_ctl_args);
                        }
                }

                set_ME_state(ME_state_killed);

                break;

        case COOPERATE_TARGET:
                printk("Cooperate: Target %d\n", ME_domID());

                DBG("SPID of the initiator: ");
                DBG_BUFFER(cooperate_args->u.initiator_coop.spid, SPID_SIZE);
                DBG("SPAD caps of the initiator: ");
                DBG_BUFFER(cooperate_args->u.initiator_coop.spad_caps, SPAD_CAPS_SIZE);

                pfn = cooperate_args->u.initiator_coop.pfns.content;
                recv_localinfo = (struct localinfo*)io_map(pfn_to_phys(pfn), PAGE_SIZE);

                lprintk("## in-cooperate received : %c\n", *((char *) recv_localinfo));

                /* We keep a pointer the the original ring so we can edit prod indexes */
                data_ring_initiator = (struct vnet_data_front_ring *)io_map(pfn_to_phys(recv_localinfo->data_ring_pfn), PAGE_SIZE);

                /* We must copy the ring to a local variable to be able to change the sring pointer without modifying the other end */
                memcpy(&data_ring, data_ring_initiator, sizeof(struct vnet_data_front_ring));
                data_ring.sring = (struct vnet_data_sring *)io_map(pfn_to_phys(recv_localinfo->data_sring_pfn), PAGE_SIZE);

                vbuff_rx = (uint8_t*)io_map(pfn_to_phys(recv_localinfo->buff_rx_pfn), VBUFF_SIZE);
                vbuff_tx = (uint8_t*)io_map(pfn_to_phys(recv_localinfo->buff_tx_pfn), VBUFF_SIZE);

                vnet_response_t *ring_res_from;
                vnet_response_t *ring_res_to;

                vnet_request_t *ring_req_from;
                vnet_request_t *ring_req_to;


                /* Responses contains incomming frames */
                int i = recv_localinfo->rx_start;
                while(vnet->connected == 0 && i < recv_localinfo->rx_end){
                        ring_res_from = RING_GET_RESPONSE(&data_ring, i++);
                        /* Frame is too old */
                        if(ring_res_from->buff.timestamp <= recv_localinfo->last_rx_timestamp){
                                continue;
                        }

                        ring_res_to = RING_GET_RESPONSE(&vnet->ring_data, vnet->ring_data.sring->rsp_prod);			\

                        void* data = vbuff_rx + ring_res_from->buff.offset;
                        vbuff_put(vnet_get_vbuff_rx(), &ring_res_to->buff, &data, ring_res_from->buff.size);

                        vnet->ring_data.sring->rsp_prod++;
                }

                i = recv_localinfo->tx_start;
                while(vnet->connected == 1 && i < recv_localinfo->tx_start){
                        ring_req_from = RING_GET_REQUEST(&data_ring, i++);

                        if((ring_req_to = vnet_data_ring_request(&vnet->ring_data)) == NULL)
                                break;

                        void* data = vbuff_tx + ring_req_from->buff.offset;
                        vbuff_put(vnet_get_vbuff_tx(), &ring_req_to->buff, &data, ring_req_from->buff.size);

                        ring_req_to->type = 0xefef;

                        vnet_data_ring_request_ready(&vnet->ring_data);
                        // TODO notify_remote_via_irq(vnet->irq);
                }

#if 0 /* Alphabet - Increment the alphabet in this case. */
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
                io_unmap((uint32_t) recv_localinfo);
                io_unmap((uint32_t) data_ring.sring);
                io_unmap((uint32_t) data_ring_initiator);
                io_unmap((uint32_t) vbuff_rx);
                io_unmap((uint32_t) vbuff_tx);

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

        return 1;
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

        return 1;
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

        return 1;
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


        localinfo_data->tx_start = localinfo_data->tx_end = 0;
        localinfo_data->rx_start = localinfo_data->rx_end = 0;

        /* We only want to receive frames received after our first init */
        localinfo_data->last_rx_timestamp = NOW() / 1000000ull;
}


