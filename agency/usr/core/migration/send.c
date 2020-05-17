/*
 * Copyright (C) 2016-2019 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) January-April 2018 Baptiste Delporte <bonel@bonel.net>
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

#define _GNU_SOURCE

#include <kconfig.h>

#include <sys/ioctl.h>

#include <core/core.h>
#include <core/send.h>
#include <core/debug.h>
#include <core/types.h>
#include <core/debug.h>
#include <core/types.h>

#include <dcm/core.h>

#if defined(CONFIG_LEDS)
#include <leds/leds.h>
#endif /* CONFIG_LEDS */


/**
 * Try to send the ME in the slot slotID.
 * The first ME is in the slot ID 2.
 * Slot ID 0 is for the hypervisor.
 * Slot ID 1 is for the agency.
 *
 * The function returns true if the ME corresponding to slotID has been sent.
 */
bool ME_processing_send(unsigned int ME_slotID) {
	ME_desc_t ME_desc;
	void *buffer;
	size_t buffer_size;

	if ((ME_slotID < 2) || (ME_slotID > MAX_DOMAINS)) {
		printf("%s: bad ME slot ID %d\n", __func__, ME_slotID);
		BUG();
	}

	if (get_ME_desc(ME_slotID, &ME_desc)) {
		printf("%s: get_ME_desc failed.\n", __func__);
		BUG();
	}

	if (!ME_desc.size) {
		DBG0("ME size: 0. return.\n");
		return false;
	}

	/* Only a ME which is in the state living or dormant can be subject to be migrated. */
	if ((ME_desc.state != ME_state_living) && (ME_desc.state != ME_state_dormant)) {
		DBG0("Slot state: not living nor dormant. Return.\n");
		return false;
	}

	/* We ask the DCM if there is some smart object in the neighborhood.
	 * Otherwise, no interest to take a ME snapshot or whatever...
	 */

	if (ioctl(fd_dcm, DCM_IOCTL_NEIGHBOUR_COUNT, 0) == 0)
		/* Nothing to send */
		return false;

	DBG("Send ME %d, slotID=%d\n", ME_slotID, ME_slotID);

#if defined(CONFIG_LEDS)
	led_on(3);
#endif /* CONFIG_LEDS */

	/* Perform initialization sequence, and checks if the ME is ready to be migrated,
	 * i.e. the ME did not get killed.
	 */
	if (initialize_migration(ME_slotID) < 0)
		goto out;

	/*
	 * Read the ME snapshot.
	 * We give the ME_EXTRA_BUFFER_SIZE to perform a kernel memory allocation of the entire buffer
	 * containing transfer and migration struct information.
	 *
	 */

	DBG("Reading the ME snapshot at slotID %d...\n", ME_slotID);

	read_ME_snapshot(ME_slotID, &buffer, &buffer_size);

	if (finalize_migration(ME_slotID)) {
		printf("%s: finalize_migration failed.\n", __func__);
		BUG();
	}

	DBG("Sending to the DCM %i bytes to send\n", buffer_size);

	/* Perform the DCM sending operation */
	dcm_send_ME(buffer, buffer_size, 0);

	DBG0("Migration processing terminated.\n");

	return true;
out:
#if defined(CONFIG_LEDS)
	led_off(3);
#endif /* CONFIG_LEDS */

	return false;
}

/**
 * Try to send the ME in the next populated slot, if any. We give as much opportunities
 * to each ME to be propagated. The slot ID of the last sent ME is saved.
 *
 * Regarding the slot ID:
 * 0 is for the domain ID 0 (non-RT)
 * 1 is for the agency (RT)
 * 2 is for the first ME
 * A ME n will have a slotID equal to the domain_id.
 */
void try_to_send_ME(void) {
	unsigned int i;
	bool at_least_one = false;

	if (opt_nosend) {
		DBG0("-nosend enabled: Skipping sending ME\n");
		return;
	}

	/* Perform a complete sending of all available MEs */
	for (i = 0; i < MAX_ME_DOMAINS; i++)
		at_least_one |= ME_processing_send(i+2);

	if (at_least_one)
		/* Tell the DCM that the transmission is over. */
		dcm_send_ME(NULL, 0, 0);

}

void sig_initiate_migration(int sig) {
	try_to_send_ME();
}
