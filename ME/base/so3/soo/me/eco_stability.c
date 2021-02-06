/*
 * Copyright (C) 2018-2019 Baptiste Delporte <bonel@bonel.net>
 * Copyright (C) 2018-2019 David Truan <david.truan@heig-vd.ch>
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

#include <spinlock.h>
#include <heap.h>
#include <errno.h>

#include <soo/avz.h>
#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/soo.h>
#include <soo/console.h>
#include <soo/debug.h>

#include "eco_stability.h"

/**** Declarations for the interactions with the application ****/

/* Must be declared by the application */

extern void reset_desc(void *local_info_ptr, uint32_t index);
extern void copy_desc(void *dest_info_ptr, uint32_t local_index,
			void *src_info_ptr, uint32_t remote_index);
extern void update_desc(uint8_t *agencyUID,
			void *local_info_ptr, uint32_t local_index,
			void *recv_data_ptr, uint32_t remote_index);
extern void action_after_merge(void *local_info_ptr, uint32_t local_index, uint32_t remote_index);
extern void update_my_id(uint32_t new_id);

extern uint32_t my_id;
extern agencyUID_t origin_agencyUID;
extern bool available_devcaps;

/***** SOO Presence scheme implementation *****/

/**
 * Retrieve the ID of a Smart Object in the ecosystem, by agency UID.
 * Return 0xff if not found.
 */
static uint32_t get_id_by_agencyUID(uint8_t *agencyUID, soo_presence_data_t *presence) {
	uint32_t i;

	for (i = 0; i < MAX_DESC; i++) {
		if ((presence[i].active) && (!memcmp(presence[i].agencyUID, agencyUID, SOO_AGENCY_UID_SIZE)))
			return i;
	}

	return 0xff;
}

/**
 * Check if a Smart Object with the agencyUID passed in parameter is present in the ecosystem.
 */
static bool is_agencyUID_present(uint8_t *agencyUID, soo_presence_data_t *presence) {
	return (get_id_by_agencyUID(agencyUID, presence) != 0xff);
}

/**
 * Retrieve the current ID of this Smart Object in the ecosystem.
 */
uint32_t get_my_id(soo_presence_data_t *presence) {
	my_id = (available_devcaps) ? get_id_by_agencyUID((uint8_t *) &origin_agencyUID, presence) : 0xff;
	return my_id;
}

/**
 * Add a new Smart Object to the global SOO.* info array and prepare a new SOO.* descriptor.
 * The function returns the index of the inserted descriptor. If there is no memory available,
 * -ENOMEM is returned. If there is another error, -EIO is returned.
 * new_agencyUID: Agency UID of the Smart Object to add
 * local_presence: SOO presence data associated to the local SOO.* data
 * local_info_ptr: Pointer to the local SOO.* info encapsulated in the local SOO.* data
 * tmp_presence: SOO presence data in temporary SOO.* data buffer
 * tmp_info_ptr: Pointer to the temporary SOO.* info buffer
 * tmp_info_size: Size of the incoming SOO.* info
 */
static int add_SOO(uint8_t *new_agencyUID,
			soo_presence_data_t *local_presence, void *local_info_ptr,
			soo_presence_data_t *tmp_presence, void *tmp_info_ptr,
			size_t tmp_info_size) {
	uint32_t i;
	uint32_t j = 0;
	bool new_agencyUID_inserted = false;
	int ret = -EIO;

	DBG("Add agency UID: ");
	DBG_BUFFER(new_agencyUID, SOO_AGENCY_UID_SIZE);

	/* Save the current global SOO.* info */
	memcpy(tmp_info_ptr, local_info_ptr, tmp_info_size);

	/*
	 * All the active SOO.* descriptors are at the beginning of the global SOO.* info.
	 * The first inactive descriptor follows the last active one.
	 */
	for (i = 0; (i < MAX_DESC) && (tmp_presence[i].active); i++) {
		if ((!new_agencyUID_inserted) && (memcmp(new_agencyUID, tmp_presence[i].agencyUID, SOO_AGENCY_UID_SIZE) < 0)) {
			/* The new agency UID has to be inserted before the end of the SOO.* data array */
			DBG("Insert new agency UID in position %d\n", i);

			/* Reset the descriptor and set it as active */
			reset_desc(local_info_ptr, i);
			/* Set the agency UID for the new desc as it is reset before */
			memcpy(local_presence[i].agencyUID, new_agencyUID, SOO_AGENCY_UID_SIZE);
			local_presence[i].active = true;

			ret = i;

			/*
			 * Once the new agency UID has now been inserted, all the other agency UIDs will be
			 * copied from tmp_*_data into *_data with an offset of 1.
			 */
			j = 1;

			new_agencyUID_inserted = true;
		}

		if (i + j == MAX_DESC) {
			DBG0("Maximal number of SOO." APP_NAME " descriptors reached\n");
			return -ENOMEM;
		}

		DBG("Copy %d > %d\n", i, i + j);

		/* Restore the saved data related to this Smart Object */
		copy_desc(local_info_ptr, i + j, tmp_info_ptr, i);
	}

	/* The new agency UID has to be added after all the others (tail) */
	if (!new_agencyUID_inserted) {
		DBG("Insert new agency UID in position %d\n", i);

		reset_desc(local_info_ptr, i);
		/* Set the agency UID for the new desc as it is reseted before */
		memcpy(local_presence[i].agencyUID, new_agencyUID, SOO_AGENCY_UID_SIZE);
		local_presence[i].active = true;

		ret = i;
	}

	/* Reset all the remaining descriptors until the end */
	for (i = 0; i < MAX_DESC; i++) {
		if (!local_presence[i].active)
			reset_desc(local_info_ptr, i);
	}

	/* Update the ID of this Smart Object */
	get_my_id(local_presence);
	update_my_id(my_id);

	return ret;
}

/**
 * Remove a Smart Object from the global info array and delete its SOO.* descriptor.
 * Return the new ID of this Smart Object.
 * agencyUID: Agency UID of the Smart Object to delete
 * local_presence: SOO presence data associated to the local SOO.* data
 * local_info_ptr: Pointer to the local SOO.* info encapsulated in the local SOO.* data
 * tmp_presence: SOO presence data in temporary SOO.* data buffer
 * tmp_info_ptr: Pointer to the temporary SOO.* info buffer
 * tmp_info_size: Size of the incoming SOO.* info
 */
static uint32_t delete_SOO(uint8_t *agencyUID,
				soo_presence_data_t *local_presence, void *local_info_ptr,
				soo_presence_data_t *tmp_presence, void *tmp_info_ptr,
				size_t tmp_info_size) {
	uint32_t i;
	uint32_t j = 0;
	uint32_t last_active_position = MAX_DESC - 1;
	bool agencyUID_deleted = false;

	/* Save the current global SOO.* info */
	memcpy(tmp_info_ptr, local_info_ptr, tmp_info_size);

	for (i = 0; i < MAX_DESC; i++) {
		/*
		 * All the active SOO.* descriptors are at the beginning of the global SOO.* info.
		 * The first inactive descriptor follows the last active one.
		 */
		if (!tmp_presence[i].active) {
			last_active_position = i - 1;
			break;
		}

		if ((!agencyUID_deleted) && (!memcmp(agencyUID, local_presence[i].agencyUID, SOO_AGENCY_UID_SIZE))) {
			DBG("Delete agency UID at index %d: ", i); DBG_BUFFER(agencyUID, SOO_AGENCY_UID_SIZE);

			/*
			 * Once the agency UID to be deleted has been found, all the other agency UIDs will be
			 * copied from tmp_*_info into *_data with an offset of 1.
			 */
			j = 1;

			agencyUID_deleted = true;
		}

		if (i + j >= MAX_DESC)
			break;

		DBG("Copy %d > %d\n", i + j, i);

		/* Restore the saved data related to this Smart Object */
		copy_desc(local_info_ptr, i, tmp_info_ptr, i + j);
	}

	/* Reset all the remaining descriptors until the end */
	for (i = last_active_position; i < MAX_DESC; i++)
		reset_desc(local_info_ptr, i);

	return get_my_id(local_presence);
}

/**
 * Merge the SOO.* information from the received info buffer into the localinfo buffer.
 * This function is in charge of the insertion of the agency UIDs that are in the incoming info buffer but not
 * in the localinfo buffer, then the merge, where the entries are examined one by one.
 * This function is called in a cooperation context, on CPU 0.
 * lock: Pointer to the spinlock used to protect local SOO.* data
 * local_presence: SOO presence data associated to the local SOO.* data
 * local_info_ptr: Pointer to the local SOO.* info encapsulated in the local SOO.* data
 * incoming_presence: SOO presence data associated to the incoming SOO.* data
 * recv_data_ptr: Pointer to the incoming SOO.* data (got during cooperation)
 * tmp_presence: SOO presence data in temporary SOO.* data buffer
 * tmp_info_ptr: Pointer to the temporary SOO.* info buffer
 * info_size: Size of the local SOO.* info encapsulated in the local SOO.* data
 */
void merge_info(spinlock_t *lock,
		soo_presence_data_t *local_presence, void *local_info_ptr,
		soo_presence_data_t *incoming_presence, void *recv_data_ptr,
		soo_presence_data_t *tmp_presence, void *tmp_info_ptr,
		size_t info_size) {
	uint32_t new_index, local_index, remote_index;
	bool agencyUID_present;

	DBG("Merging information: SOO." APP_NAME " > SOO." APP_NAME "\n");

	spin_lock(lock);

	/*
	 * All the active SOO.* descriptors are at the beginning of the global SOO.* info.
	 * The first inactive descriptor follows the last active one.
	 */
	for (remote_index = 0; (remote_index < MAX_DESC) && (incoming_presence[remote_index].active); remote_index++) {
		/*
		 * All the active SOO.* descriptors are at the beginning of the global SOO.* info.
		 * The first inactive descriptor follows the last active one.
		 */
		if (!incoming_presence[remote_index].active)
			break;

		if ((agencyUID_present = is_agencyUID_present(incoming_presence[remote_index].agencyUID, local_presence))) {
			/* The originating Smart Object is in the known SOO list */

			/* Look for the index of the originating Smart Object in the local global SOO.* info */
			local_index = get_id_by_agencyUID(incoming_presence[remote_index].agencyUID, local_presence);

#if 0
			if (unlikely(local_index == 0xff)) {
				/* There are too many SOO.* Smart Objects */
				spin_unlock(lock);
				return ;
			}
#endif

			DBG("Local index=%d, age=%d, remote index=%d, age=%d\n", local_index, local_presence[local_index].age, remote_index, incoming_presence[remote_index].age);

			if (local_presence[local_index].age == incoming_presence[remote_index].age) {
				/* The remote Smart Object might be disappearing. Update the inertia. */
				DBG0("Equal ages\n");

				/* Keep the highest inertia value */
				local_presence[local_index].inertia = (local_presence[local_index].inertia > incoming_presence[remote_index].inertia) ?
						local_presence[local_index].inertia : incoming_presence[remote_index].inertia;

				if (local_presence[local_index].inertia > MAX_INERTIA) {
					delete_SOO(incoming_presence[remote_index].agencyUID,
							local_presence, local_info_ptr,
							tmp_presence, tmp_info_ptr,
							info_size);

					remote_index = 0;
					continue;
				}
			} else if (incoming_presence[remote_index].age > local_presence[local_index].age) {
				/* This is a more recent ME */
				DBG0("More recent ME\n");

				local_presence[local_index].inertia = 0;
				local_presence[local_index].age = incoming_presence[remote_index].age;
			} else {
				/* This is an older ME */
				DBG0("Older ME\n");
			}

			DBG("Remote change count=%d, local change count=%d\n", incoming_presence[remote_index].change_count, local_presence[local_index].change_count);

			/* Check if there is explicit new info to import */
			if (incoming_presence[remote_index].change_count > local_presence[local_index].change_count) {
#if 0
				if (likely((my_id = get_id_by_agencyUID((uint8_t *) &origin_agencyUID, local_presence)) != 0xff))
#endif
					update_desc(incoming_presence[remote_index].agencyUID,
							local_info_ptr, local_index,
							recv_data_ptr, remote_index);
				/* If this SOO.* changed, request a SOO.* management */
				get_my_id(local_presence);
				action_after_merge(local_info_ptr, local_index, remote_index);

#if 0
				local_presence[local_index].inertia = 0;
#endif
			}
		} else {
			/* The originating Smart Object is not in the known Smart Object list */

			if (incoming_presence[remote_index].inertia != 0) {
				/* The remote Smart Object might be disappearing. Ignore the incoming information. */
				spin_unlock(lock);
				return ;
			}

			/* As the inertia is 0, the Smart Object is living. Add it into the global ecosystem */

			if ((new_index = add_SOO(incoming_presence[remote_index].agencyUID,
							local_presence, local_info_ptr,
							tmp_presence, tmp_info_ptr,
							info_size)) < 0) {
				spin_unlock(lock);
				return ;
			}

			/* Look for the index of the originating Smart Object in the local global SOO.* info */
			local_index = get_id_by_agencyUID(incoming_presence[remote_index].agencyUID, local_presence);

#if 0
			if (unlikely(local_index == 0xff)) {
				/* There are too many SOO.* Smart Objects */
				spin_unlock(lock);
				return ;
			}

			if (likely((my_id = get_id_by_agencyUID((uint8_t *) &origin_agencyUID, local_presence)) != 0xff))
#endif
				update_desc(incoming_presence[remote_index].agencyUID,
						local_info_ptr, local_index,
						recv_data_ptr, remote_index);

			local_presence[new_index].inertia = 0;
			local_presence[new_index].age = incoming_presence[remote_index].age;
			local_presence[new_index].last_age = local_presence[new_index].age;

			remote_index = 0;
			continue;
		}
	}

	spin_unlock(lock);
}

/**
 * Increment the age counter of this Smart Object and reset its inertia counter to 0.
 * This function is called in a pre-propagate context, on CPU 0.
 * lock: Pointer to the spinlock used to protect local SOO.* data
 * presence: SOO presence data associated to the local SOO.* data
 *
 */
void inc_age_reset_inertia(spinlock_t *lock, soo_presence_data_t *presence) {
	my_id = get_my_id(presence);

#if 0
	if (unlikely(my_id == 0xff)) {
		/* There are too many Smart Objects */
		return ;
	}
#endif

	spin_lock(lock);
	/* Update the age and reset the inertia only if the ME is on its originating Smart Object */
	if (available_devcaps) {
		presence[my_id].age++;
		presence[my_id].inertia = 0;

		DBG("Age: %d, inertia=%d\n", presence[my_id].age, presence[my_id].inertia);
	}
	spin_unlock(lock);
}

/**
 * Detect inactive ages and increment the associated inertia.
 * This function is called in a pre-propagate context, on CPU 0.
 * lock: Pointer to the spinlock used to protect local SOO.* data
 * local_presence: SOO presence data associated to the local SOO.* data
 * local_info_ptr: Pointer to the local SOO.* info encapsulated in the local SOO.* data
 * tmp_presence: SOO presence data in temporary SOO.* data buffer
 * tmp_info_ptr: Pointer to the temporary SOO.* info buffer
 *
 */
void watch_ages(spinlock_t *lock,
		soo_presence_data_t *local_presence, void *local_info_ptr,
		soo_presence_data_t *tmp_presence, void *tmp_info_ptr,
		size_t info_size) {
	uint32_t i;

	spin_lock(lock);

	for (i = 0; (i < MAX_DESC) && (local_presence[i].active); i++) {
		if (local_presence[i].last_age == local_presence[i].age) {
			local_presence[i].inertia++;
			DBG("%d: inertia=%d\n", i, local_presence[i].inertia);
		}

		local_presence[i].last_age = local_presence[i].age;
	}

	for (i = 0; (i < MAX_DESC) && (local_presence[i].active); i++) {
		if (local_presence[i].inertia > MAX_INERTIA) {
			DBG("i: %d, inertia=%d > %d\n", i, local_presence[i].inertia, MAX_INERTIA);

			delete_SOO(local_presence[i].agencyUID,
					local_presence, local_info_ptr,
					tmp_presence, tmp_info_ptr,
					info_size);

			i = 0;
			continue;
		}
	}

	spin_unlock(lock);
}
