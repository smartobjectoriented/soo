/*
 * Copyright (C) 2021 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#ifndef CONFIG_ARM64
#include <asm/mach/map.h>
#endif

#include <soo/uapi/console.h>
#include <soo/uapi/soo.h>
#include <soo/uapi/debug.h>

#include <soo/soo.h>
#include <soo/core/core.h>
#include <soo/core/migmgr.h>

#include <soo/vbus.h>
#include <soo/paging.h>

/**
 * Initialize the migration process of a ME.
 *
 * @param slotID
 * @return true if the ME can proceed with the migration, false otherwise.
 */
bool initialize_migration(uint32_t slotID) {
	ME_state_t ME_state;
        avz_hyp_t args;

        ME_state = get_ME_state(slotID);

        BUG_ON(!((ME_state == ME_state_living) || (ME_state == ME_state_dormant)));
 
	do_sync_dom(slotID, DC_PRE_SUSPEND);

	/* Set the ME in suspended state */
	set_ME_state(slotID, ME_state_suspended);

	vbus_suspend_devices(slotID);

	do_sync_dom(slotID, DC_SUSPEND);

        args.cmd = AVZ_MIG_INIT;
        args.u.avz_mig_init_args.slotID = slotID;

        avz_hypercall(&args);

        /* Ready to be migrated */
	return true;
}


/**
 * Initiate the last stage of the migration process of a ME, so called "migration
 * finalization".
 */
void finalize_migration(uint32_t slotID) {
        avz_hyp_t args;
        int ME_state;

        if (get_ME_state(slotID) == ME_state_booting) {


		DBG("Unpause the ME (slot %d)...\n", slotID);

		/*
		 * During the unpause operation, we take the opportunity to pass the pfn of the shared page used for exchange
		 * between the ME and VBstore.
		 */
		avz_ME_unpause(slotID, vbstore_grant_ref[slotID]);

		/*
		 * Now, we must wait for the ME to set its state to ME_state_preparing to pause it. We'll then be able
		 * to perform a pre-activate callback on it.
		 */
		while (1) {
			schedule();

			if (get_ME_state(slotID) == ME_state_preparing) {
				DBG("ME now paused, continuing...\n");
				break;
			}
		}

		/* Pause the ME */
		avz_ME_pause(slotID);

                args.cmd = AVZ_MIG_FINAL;
                args.u.avz_mig_final_args.slotID = slotID;
                
		avz_hypercall(&args);

                ME_state = get_ME_state(slotID);

		if ((ME_state != ME_state_dead) && (ME_state != ME_state_dormant)) {

			/* Trigger the container that can go further */
			set_ME_state(slotID, ME_state_booting);

			DBG("Unpause the ME and waiting boot completion...\n");

			/* Unpause the ME */
			avz_ME_unpause(slotID, vbstore_grant_ref[slotID]);

			/* Wait for all backend/frontend initialized. */
			wait_for_completion(&backend_initialized);

			DBG("The ME is now living, continuing the injection...\n");

			ME_state = get_ME_state(slotID);

			DBG("Putting ME domid %d in state living...\n", slotID);
			set_ME_state(slotID, ME_state_living);
		}

	} else {

		ME_state = get_ME_state(slotID);
		BUG_ON(!((ME_state == ME_state_migrating) || (ME_state == ME_state_suspended) || (ME_state == ME_state_dormant)));

		DBG0("SOO migration subsys: Entering post migration tasks...\n");

		if (ME_state != ME_state_dormant) {

			args.cmd = AVZ_MIG_FINAL;
                	args.u.avz_mig_final_args.slotID = slotID;
                
			avz_hypercall(&args);

			DBG0("Call to AVZ_MIG_FINAL terminated\n");
		}

		ME_state = get_ME_state(slotID);

		if (!((ME_state == ME_state_dead) || (ME_state == ME_state_dormant))) {
			DBG("Pinging ME %d for DC_RESUME...\n", slotID);
			do_sync_dom(slotID, DC_RESUME);

			DBG("Resuming all devices (resuming from backend devices) on domain %d...\n", slotID);
			vbus_resume_devices(slotID);

			DBG("Pinging ME %d for DC_POST_ACTIVATE...\n", slotID);
			do_sync_dom(slotID, DC_POST_ACTIVATE);

			DBG("Putting ME domid %d in state living...\n", slotID);
			set_ME_state(slotID, ME_state_living);
		}
	}
}


