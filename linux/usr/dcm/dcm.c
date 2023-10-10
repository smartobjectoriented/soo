/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2018-2019 Baptiste Delporte <bonel@bonel.net>
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <core/core.h>
#include <core/send.h>
#include <core/receive.h>
#include <core/injector.h>
#include <core/debug.h>
#include <core/types.h>

#include <dcm/dcm.h>
#include <dcm/compressor.h>
#include <dcm/datacomm.h>
#include <dcm/security.h>

int fd_dcm;

/** RX **/
/**
 * Try to retrieve a ME from the DCM and deploy it.
 *
 * A word about the (receive) buffer handling. Actually, the buffer is vmalloc'd in the Compressor functional block
 * of the DCM in order to make a fast memcpy. We retrieve the buffer address returned by the vm memory allocator, but
 * it is *strongly* forbidden to access the buffer from the user space. Indeed, the kernel memory management including page
 * fault handing enforces this memory region to be accessed from the kernel space only.
 *
 * The function returns true if an available ME has been found, false otherwise.
 */
void ME_processing_receive(void) {
	dcm_buffer_t dcm_buffer;
	int slotID;
	int count, i;

#ifdef WITH_LED_ACTIVITIES
	static uint8_t toggle = 0;
#endif

	count = ioctl(fd_dcm, DCM_IOCTL_RX_AVAILABLE_ME_N, 0);

	/* We process a number of available MEs at a certain time, but not more to avoid
	 * endless receiving loop.
	 */
	for (i = 0; i < count; i++) {

		if ((ioctl(fd_dcm, DCM_IOCTL_RECV, (unsigned long) &dcm_buffer)) < 0) {
			printf("ioctl DCM_IOCTL_RECV failed.\n");
			BUG();
		}

#ifdef WITH_LED_ACTIVITIES
		led_on(4);

		if (toggle == 0) {
			led_on(5);
			led_off(6);
		} else {
			led_on(6);
			led_off(5);
		}
		toggle = (toggle + 1) % 2;
#endif

		DBG("ME available: 0x%lx, %d\n", (unsigned long) dcm_buffer.ME_data, dcm_buffer.ME_size);

		slotID = get_ME_free_slot(dcm_buffer.ME_size);
		if (slotID > 0) {

			printf("(dcm) ** Receiving a ME -> now in slot %d\n", slotID);

			/* Tell AVZ to create a new domain context including the ME descriptor,
			 * and to prepare the ME to be implemented.
			 */

			initialize_migration(slotID);

			DBG("Write snapshot located @ 0x%lx\n", (unsigned long) (dcm_buffer.ME_data));
			write_ME_snapshot(slotID, dcm_buffer.ME_data);

			DBG0("End write snapshot.\n");

			finalize_migration(slotID);
		}

		if ((ioctl(fd_dcm, DCM_IOCTL_RELEASE, &dcm_buffer)) < 0) {
			printf("Failed to release the ME...\n");
			BUG();
		}

#ifdef WITH_LED_ACTIVITIES
		led_off(4);
#endif
	}
}

/**
 * Try to send the ME in the slot slotID.
 * The first ME is in the slot ID 2.
 * Slot ID 0 is for the hypervisor.
 * Slot ID 1 is for the agency.
 *
 * The function returns true if the ME corresponding to slotID has been sent.
 */
bool ME_processing_send(unsigned int ME_slotID) {
	ME_id_t me_id;
	dcm_buffer_t dcm_buffer;

	if ((ME_slotID < 2) || (ME_slotID > MAX_DOMAINS)) {
		printf("%s: bad ME slot ID %d\n", __func__, ME_slotID);
		BUG();
	}

	if (!get_ME_id(ME_slotID, &me_id)) {
		DBG0("ME size: 0. return.\n");
		return false;
	}

	/* Only a ME which is in the state living or dormant can be subject to be migrated. */
	if ((me_id.state != ME_state_living) && (me_id.state != ME_state_dormant)) {
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

#ifdef WITH_LED_ACTIVITIES
	led_on(3);
#endif

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

	read_ME_snapshot(ME_slotID, &dcm_buffer.ME_data, &dcm_buffer.ME_size);

	if (finalize_migration(ME_slotID)) {
		printf("%s: finalize_migration failed.\n", __func__);
		BUG();
	}

	/**
	 * Send a ME using the DCM. ME_buffer and ME_size must include the migration structure.
	 * This ioctl triggers the TX request by Datalink's side.
	 */
#warning We should improve the send path with a delay of waiting to become speaker to avoid sending ME getting too old...

	if ((ioctl(fd_dcm, DCM_IOCTL_SEND, &dcm_buffer)) < 0) {
		printf("Failed to send the ME...\n");
		BUG();
	}

	DBG0("Migration processing terminated.\n");

	return true;
out:

#ifdef WITH_LED_ACTIVITIES
	led_off(3);
#endif

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
	dcm_buffer_t dcm_buffer;

	if (opt_nosend) {
		DBG0("-nosend enabled: Skipping sending ME\n");
		return;
	}

	/* Perform a complete sending of all available MEs */
	for (i = 0; i < MAX_ME_DOMAINS; i++)
		at_least_one |= ME_processing_send(i+2);

	if (at_least_one) {
		dcm_buffer.ME_data = NULL;
		dcm_buffer.ME_size = 0;

		/* Tell the DCM that the transmission is over. */
		if ((ioctl(fd_dcm, DCM_IOCTL_SEND, &dcm_buffer)) < 0) {
			printf("Failed to send the ME...\n");
			BUG();
		}
	}

}

void sig_initiate_migration(int sig) {
	try_to_send_ME();
}

void dcm_dev_init(void) {
	if ((fd_dcm = open(DCM_DEV_NAME, O_RDWR)) < 0) {
		printf("Failed to open device: " DCM_DEV_NAME " (%d)\n", fd_dcm);
		perror("dcm_dev_init");
		BUG();
	}
}
