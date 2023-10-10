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
static uint8_t __buffer[32 * 1024]; /* 32 Ko */

/**
 * Initialize the migration process of a ME.
 *
 * The process starts with the execution of pre_propagate, which might decide to kill (remove) the ME.
 * This is useful to control the propagation of the ME when necessary.
 * Furthermore, if the ME is dormant, pre_propagate is still called, but the rest of the process is skipped.
 *
 * @param slotID
 * @return true if the ME can proceed with the migration, false otherwise.
 */
bool initialize_migration(uint32_t slotID) {
	int propagate = 0;
	ME_state_t ME_state;

	ME_state = get_ME_state(slotID);

	BUG_ON(!((ME_state == ME_state_living) || (ME_state == ME_state_dormant) || (ME_state == ME_state_migrating)));

	if (!(ME_state == ME_state_migrating)) {

		soo_hypercall(AVZ_MIG_PRE_PROPAGATE, NULL, NULL, &slotID, &propagate);

		/* Set a return value so that the caller can decide what to do. */
		if (get_ME_state(slotID) == ME_state_dead) {

			/* Just make sure that the ME has not triggered any vbstore entry creation. */
			/* Remove all associated entries. */

#warning Remove all vbstore entries....

			return false;
		}

		if (!propagate)
			return false;

		/* If dormant, the ME will not be resumed. */
		if (get_ME_state(slotID) == ME_state_dormant)
			return true;

		do_sync_dom(slotID, DC_PRE_SUSPEND);

		/* Set the ME in suspended state */
		set_ME_state(slotID, ME_state_suspended);

		vbus_suspend_devices(slotID);

		do_sync_dom(slotID, DC_SUSPEND);
	}

	soo_hypercall(AVZ_MIG_INIT, NULL, NULL, &slotID, NULL);

	/* Ready to be migrated */
	return true;
}

/**
 * Write a ME snapshot provided a ME_info_transfer_t descriptor.
 *
 * @param slotID
 * @param buffer  Adresse of a buffer of ME_info_transfert_t
 */
void write_snapshot(uint32_t slotID, void *buffer) {
	ME_desc_t ME_desc;
	void *target;
	ME_info_transfer_t *ME_info_transfer;

	/* Get the ME descriptor corresponding to this slotID. */
	get_ME_desc(slotID, &ME_desc);

	/* Beginning of the ME_buffer */
	ME_info_transfer = (ME_info_transfer_t *) buffer;

	/* Retrieve the info related to the migration structure */
	memcpy(__buffer, buffer + sizeof(ME_info_transfer_t), ME_info_transfer->size_mig_structure);

	soo_hypercall(AVZ_MIG_WRITE_MIGRATION_STRUCT, __buffer, NULL, NULL, NULL);

	/* We got the pfn of the local destination for this ME, therefore... */

	target = paging_remap(ME_desc.pfn << PAGE_SHIFT, ME_desc.size);
	BUG_ON(target == NULL);

	/* Finally, perform the copy */
	memcpy(target, (void *) (buffer + sizeof(ME_info_transfer_t) + ME_info_transfer->size_mig_structure), ME_desc.size);

	/* Relase the map used to copy the ME to its final location */
	iounmap(target);
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
/**
 * Kernel address returned by vmalloc() cannot be used directly from the user space.
 * The user space can provide a valid address (resulting from malloc() for example) and this
 * function will copy the snapshot.
 *
 * @param ME_snapshot
 * @param user_addr
 * @param size
 */
void copy_ME_snapshot_to_user(void *ME_snapshot, void *user_addr, uint32_t size) {

	/* Awful usage of these fields, the name will have to evolve... */
	memcpy(user_addr, ME_snapshot, size);
}

/**
 * Read a ME snapshot for migration or saving.
 * The ME is read and stored in a vmalloc'd memory area.
 *
 * @param slotID
 * @param buffer pointer to the ME buffer. The address is returned by vmalloc().
 * @return size of the buffer
 */
int read_snapshot(uint32_t slotID, void **buffer) {
	ME_desc_t ME_desc;
	ME_info_transfer_t *ME_info_transfer;
	void *source;
	int size;

	/* Get the ME descriptor corresponding to this slotID. */
	get_ME_desc(slotID, &ME_desc);

	/*
	 * Prepare a buffer to store the ME and additional header information like migration structure and transfer information.
	 * The buffer must be free'd once it has been sent out (by the DCM).
	 */

	*buffer = __vmalloc(ME_desc.size + ME_EXTRA_BUFFER_SIZE, GFP_HIGHUSER | __GFP_ZERO);
	BUG_ON(*buffer == NULL);

	/* Beginning of the ME buffer to transmit - We start with the information transfer. */
	ME_info_transfer = (ME_info_transfer_t *) *buffer;
	ME_info_transfer->ME_size = ME_desc.size;

	soo_hypercall(AVZ_MIG_READ_MIGRATION_STRUCT, __buffer, NULL, &slotID, &size);

	/* Store the migration structure within the ME buffer */
	memcpy(*buffer + sizeof(ME_info_transfer_t), __buffer, size);

	/* Keep the size of migration structure */
	ME_info_transfer->size_mig_structure = size;

	/* Finally, store the ME in this buffer. */
	source = ioremap(ME_desc.pfn << PAGE_SHIFT, ME_desc.size);
	BUG_ON(source == NULL);

	memcpy(*buffer + sizeof(ME_info_transfer_t) + ME_info_transfer->size_mig_structure, source, ME_desc.size);

	iounmap(source);

	return sizeof(ME_info_transfer_t) + ME_info_transfer->size_mig_structure + ME_desc.size;

}

/**
 * Initiate the last stage of the migration process of a ME, so called "migration
 * finalization".
 */
void finalize_migration(uint32_t slotID) {

	int ME_state;

	if (get_ME_state(slotID) == ME_state_booting) {


		DBG("Unpause the ME (slot %d)...\n", slotID);

		/*
		 * During the unpause operation, we take the opportunity to pass the pfn of the shared page used for exchange
		 * between the ME and VBstore.
		 */
		avz_ME_unpause(slotID, virt_to_pfn((unsigned long) __vbstore_vaddr[slotID]));

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

		/*
		 * Now we pursue with a call to pre-activate callback to
		 * see if the Smart Object has the necessary devcaps. To do that,
		 * we ask the ME to decide.
		 */
		soo_hypercall(AVZ_MIG_PRE_ACTIVATE, NULL, NULL, &slotID, NULL);

		/* Check if the pre-activate callback has changed the ME state */
		if ((get_ME_state(slotID) == ME_state_dead) || (get_ME_state(slotID) == ME_state_dormant))
			return ;

		soo_hypercall(AVZ_MIG_FINAL, NULL, NULL, &slotID, NULL);

		/* Check for ME which have been terminated during the cooperate callback. */
		check_terminated_ME();

		ME_state = get_ME_state(slotID);

		if ((ME_state != ME_state_dead) && (ME_state != ME_state_dormant)) {

			/* Tell the ME that it can go further */
			set_ME_state(slotID, ME_state_booting);

			DBG("Unpause the ME and waiting boot completion...\n");

			/* Unpause the ME */
			avz_ME_unpause(slotID, virt_to_pfn((unsigned long) __vbstore_vaddr[slotID]));

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
			soo_hypercall(AVZ_MIG_FINAL, NULL, NULL, &slotID, NULL);

			DBG0("Call to AVZ_MIG_FINAL terminated\n");

			/* Check for ME which have been terminated during the cooperate callback. */
			check_terminated_ME();
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


