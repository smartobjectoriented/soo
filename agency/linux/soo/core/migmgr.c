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
#include <soo/uapi/me_access.h>
#include <soo/uapi/debug.h>

#include <soo/core/core.h>
#include <soo/core/migmgr.h>

#include <soo/vbus.h>
#include <soo/paging.h>

/*
 * Used to store ioctl args/buffers
 * Cannot be stored on the local stack since it is to big.
 */
static uint8_t buffer[32 * 1024]; /* 32 Ko */

/**
 * Initialize the migration process of a ME.
 *
 * The process starts with the execution of pre_propagate, which might decide to kill (remove) the ME.
 * This is useful to control the propagation of the ME when necessary.
 * Furthermore, if the ME is dormant, pre_propagate is still called, but the rest of the process is skipped.
 * To let the user space aware of the state, a return value is put in args.value as follows:
 * - (-1) means the ME is dead (has disappeared) or can not pursue its propagation.
 * - (0) means the ME is dormant
 * - (1) means the ME is ready be activated
 */
int ioctl_initialize_migration(unsigned long arg) {
	int rc;
	agency_tx_args_t args;
	int propagate = 0;
	ME_state_t ME_state;

	if ((rc = copy_from_user(&args, (void *) arg, sizeof(agency_tx_args_t))) != 0) {
		lprintk("Agency: %s:%d Failed to retrieve args from userspace\n", __func__, __LINE__);
		BUG();
	}

	ME_state = get_ME_state(args.ME_slotID);

	BUG_ON(!((ME_state == ME_state_living) || (ME_state == ME_state_dormant) || (ME_state == ME_state_migrating)));

	if (!((ME_state == ME_state_booting) || (ME_state == ME_state_migrating))) {

		if ((rc = soo_hypercall(AVZ_MIG_PRE_PROPAGATE, NULL, NULL, &args.ME_slotID, &propagate)) != 0) {
			lprintk("Agency: %s:%d Failed to trigger pre-propagate callback (%d)\n", __func__, __LINE__, rc);
			BUG();
		}

		/* Set a return value so that the caller can decide what to do. */
		if (get_ME_state(args.ME_slotID) == ME_state_dead) {

			/* Just make sure that the ME has not triggered any vbstore entry creation. */
			/* Remove all associated entries. */

#warning Remove all vbstore entries....

			args.value = -1;
			goto out;
		}

#ifdef VERBOSE
		lprintk("%s: returned propagate value = %d\n", __func__, propagate);
#endif

		if (!propagate) {
			args.value = -1;
			goto out;
		}

		if (get_ME_state(args.ME_slotID) == ME_state_dormant) {
			args.value = 0;
			goto out;
		}

		do_sync_dom(args.ME_slotID, DC_PRE_SUSPEND);

		/* Set the ME in suspended state */
		set_ME_state(args.ME_slotID, ME_state_suspended);

		vbus_suspend_devices(args.ME_slotID);

		do_sync_dom(args.ME_slotID, DC_SUSPEND);
	}

	if ((rc = soo_hypercall(AVZ_MIG_INIT, NULL, NULL, &args.ME_slotID, NULL)) != 0) {
		lprintk("Agency: %s:%d Failed to initialize migration (%d)\n", __func__, __LINE__, rc);
		return rc;
	}

	/* Ready to be migrated */
	args.value = 0;
out:
	if ((rc = copy_to_user((void *) arg, &args, sizeof(agency_tx_args_t))) != 0) {
		lprintk("Agency: %s:%d Failed to copy args to userspace\n", __func__, __LINE__);
		BUG();
	}

	return 0;
}

int ioctl_write_snapshot(unsigned long arg) {
	ME_desc_t ME_desc;
	agency_tx_args_t args;
	void *target;
	ME_info_transfer_t *ME_info_transfer;

	if ((copy_from_user(&args, (void *) arg, sizeof(agency_tx_args_t))) != 0) {
		lprintk("Agency: %s:%d Failed to retrieve args from userspace\n", __func__, __LINE__);
		BUG();
	}

	/* Get the ME descriptor corresponding to this slotID. */
	get_ME_desc(args.ME_slotID, &ME_desc);

	/* Beginning of the ME_buffer */
	ME_info_transfer = (ME_info_transfer_t *) args.buffer;

	/* Retrieve the info related to the migration structure */
	memcpy(buffer, args.buffer + sizeof(ME_info_transfer_t), ME_info_transfer->size_mig_structure);

	if (soo_hypercall(AVZ_MIG_WRITE_MIGRATION_STRUCT, buffer, NULL, NULL, NULL) < 0) {
		lprintk("Agency: %s:%d Failed to write migration struct.\n", __func__, __LINE__);
		BUG();
	}

	/* We got the pfn of the local destination for this ME, therefore... */

	target = paging_remap(ME_desc.pfn << PAGE_SHIFT, ME_desc.size);
	BUG_ON(target == NULL);

	/* Finally, perform the copy */
	memcpy(target, (void *) (args.buffer + sizeof(ME_info_transfer_t) + ME_info_transfer->size_mig_structure), ME_desc.size);

	/* Relase the map used to copy the ME to its final location */
	iounmap(target);

	/* Release the buffer */

	return 0;
}

/*
 * Retrieve a valid (user space) address to the ME snapshot which has been previously
 * read with the READ_SNAPSOT ioctl.
 *
 * In tx_args_t <args>, the following fields are used as follows:
 *
 * @buffer:	pointer to the vmalloc'd memory (not be used in the user space, but will be used in following calls
 * @ME_slotID: 	the *size* of the contents to be copied.
 * @value: 	the target user space address the ME has to be copied to
 */
void ioctl_get_ME_snapshot(unsigned long arg) {
	agency_tx_args_t args;

	if ((copy_from_user(&args, (void *) arg, sizeof(agency_tx_args_t))) != 0) {
		lprintk("Agency: %s:%d Failed to retrieve args from userspace\n", __func__, __LINE__);
		BUG();
	}

	/* Awful usage of these fields, the name will have to evolve... */
	memcpy((void *) args.value, args.buffer, args.ME_slotID);
}

/*
 * Read a ME snapshot for migration or saving.
 * Currently, the ME is read and stored in a vmalloc'd memory area.
 * It will be preferable in the future to have a memory allocated and handled by
 * the user space application (avoiding restriction access and so on).
 *
 * In tx_args_t <args>, the following fields are used as follows:
 *
 * @buffer:	pointer to the vmalloc'd memory (not be used in the user space, but will be used in following calls
 * @ME_slotID: 	the slotID which leads to retrieving the ME descriptor
 * @value: 	the size including ME size + additional transfer information
 */
int ioctl_read_snapshot(unsigned long arg) {
	ME_desc_t ME_desc;
	agency_tx_args_t args;
	ME_info_transfer_t *ME_info_transfer;
	void *ME_buffer;
	void *source;

	if ((copy_from_user(&args, (void *) arg, sizeof(agency_tx_args_t))) != 0) {
		lprintk("Agency: %s:%d Failed to retrieve args from userspace\n", __func__, __LINE__);
		BUG();
	}

	/* Get the ME descriptor corresponding to this slotID. */
	get_ME_desc(args.ME_slotID, &ME_desc);

	/*
	 * Prepare a buffer to store the ME and additional header information like migration structure and transfer information.
	 * The buffer must be free'd once it has been sent out (by the DCM).
	 */

	ME_buffer = __vmalloc(ME_desc.size + ME_EXTRA_BUFFER_SIZE, GFP_HIGHUSER | __GFP_ZERO, PAGE_KERNEL);
	BUG_ON(ME_buffer == NULL);

	/* Beginning of the ME buffer to transmit - We start with the information transfer. */
	ME_info_transfer = (ME_info_transfer_t *) ME_buffer;
	ME_info_transfer->ME_size = ME_desc.size;

	if ((soo_hypercall(AVZ_MIG_READ_MIGRATION_STRUCT, buffer, NULL, &args.ME_slotID, &args.value)) < 0) {
		lprintk("Agency: %s:%d Failed to read migration struct.\n", __func__, __LINE__);
		BUG();
	}

	/* Store the migration structure within the ME buffer */
	memcpy(ME_buffer + sizeof(ME_info_transfer_t), buffer, args.value);

	/* Keep the size of migration structure */
	ME_info_transfer->size_mig_structure = args.value;

	/* Finally, store the ME in this buffer. */
	source = ioremap(ME_desc.pfn << PAGE_SHIFT, ME_desc.size);
	BUG_ON(source == NULL);

	memcpy(ME_buffer + sizeof(ME_info_transfer_t) + ME_info_transfer->size_mig_structure, source, ME_desc.size);

	iounmap(source);

	args.buffer = ME_buffer;
	args.value = sizeof(ME_info_transfer_t) + ME_info_transfer->size_mig_structure + ME_desc.size;

	if ((copy_to_user((void *) arg, &args, sizeof(agency_tx_args_t))) != 0) {
		lprintk("Agency: %s:%d Failed to retrieve args from userspace\n", __func__, __LINE__);
		BUG();
	}

	return 0;
}

/**
 * Initiate the last stage of the migration process of a ME, so called "migration
 * finalization".
 */
int ioctl_finalize_migration(unsigned long arg) {
	int rc;
	agency_tx_args_t args;
	int ME_slotID, ME_state;

	if ((rc = copy_from_user(&args, (void *) arg, sizeof(agency_tx_args_t))) != 0) {
		lprintk("Agency: %s:%d Failed to retrieve args from userspace\n", __func__, __LINE__);
		return rc;
	}

	if (get_ME_state(args.ME_slotID) == ME_state_booting) {

		ME_slotID = args.ME_slotID;

		DBG("Unpause the ME (slot %d)...\n", ME_slotID);

		/*
		 * During the unpause operation, we take the opportunity to pass the pfn of the shared page used for exchange
		 * between the ME and VBstore.
		 */
		avz_ME_unpause(ME_slotID, virt_to_pfn((unsigned long) __vbstore_vaddr[ME_slotID]));

		/*
		 * Now, we must wait for the ME to set its state to ME_state_preparing to pause it. We'll then be able
		 * to perform a pre-activate callback on it.
		 */
		while (1) {
			schedule();

			if (get_ME_state(ME_slotID) == ME_state_preparing) {
				DBG("ME now paused, continuing...\n");
				break;
			}
		}

		/* Pause the ME */
		avz_ME_pause(ME_slotID);

		/*
		 * Now we pursue with a call to pre-activate callback to
		 * see if the Smart Object has the necessary devcaps. To do that,
		 * we ask the ME to decide.
		 */
		if ((rc = soo_hypercall(AVZ_MIG_PRE_ACTIVATE, NULL, NULL, &ME_slotID, NULL)) != 0) {
			lprintk("Agency: %s:%d Failed to trigger pre-activate callback (%d)\n", __func__, __LINE__, rc);
			BUG();
		}

		/* Check if the pre-activate callback has changed the ME state */
		if ((get_ME_state(ME_slotID) == ME_state_dead) || (get_ME_state(ME_slotID) == ME_state_dormant))
			return 0;

		if ((rc = soo_hypercall(AVZ_MIG_FINAL, NULL, NULL, &args.ME_slotID, NULL)) < 0) {
			lprintk("Agency: %s:%d Failed to finalize migration (%d)\n", __func__, __LINE__, rc);
			BUG();
		}

		ME_state = get_ME_state(args.ME_slotID);

		if ((ME_state != ME_state_dead) && (ME_state != ME_state_dormant)) {

			/* Tell the ME that it can go further */
			set_ME_state(ME_slotID, ME_state_booting);

			DBG("Unpause the ME and waiting boot completion...\n");

			/* Unpause the ME */
			avz_ME_unpause(ME_slotID, virt_to_pfn((unsigned long) __vbstore_vaddr[ME_slotID]));

			/* Wait for all backend/frontend initialized. */
			wait_for_completion(&backend_initialized);

			DBG("The ME is now living, continuing the injection...\n");

			ME_state = get_ME_state(args.ME_slotID);

			DBG("Putting ME domid %d in state living...\n", args.ME_slotID);
			set_ME_state(args.ME_slotID, ME_state_living);

		}

	} else {

		ME_state = get_ME_state(args.ME_slotID);
		BUG_ON(!((ME_state == ME_state_migrating) || (ME_state == ME_state_suspended) || (ME_state == ME_state_dormant)));

		DBG0("SOO migration subsys: Entering post migration tasks...\n");

		if ((ME_state != ME_state_dormant) &&
			((rc = soo_hypercall(AVZ_MIG_FINAL, NULL, NULL, &args.ME_slotID, NULL)) < 0)) {
				lprintk("Agency: %s:%d Failed to finalize migration (%d)\n", __func__, __LINE__, rc);
				BUG();
		}

		DBG0("Call to AVZ_MIG_FINAL terminated\n");

		ME_state = get_ME_state(args.ME_slotID);

		if (!((ME_state == ME_state_dead) || (ME_state == ME_state_dormant))) {
			DBG0("Pinging ME for DC_RESUME...\n");
			do_sync_dom(args.ME_slotID, DC_RESUME);

			DBG("Resuming all devices (resuming from backend devices) on domain %d...\n", args.ME_slotID);
			vbus_resume_devices(args.ME_slotID);

			DBG("Pinging ME %d for DC_POST_ACTIVATE...\n", args.ME_slotID);
			do_sync_dom(args.ME_slotID, DC_POST_ACTIVATE);

			DBG("Putting ME domid %d in state living...\n", args.ME_slotID);
			set_ME_state(args.ME_slotID, ME_state_living);

		}
	}

	return 0;
}


