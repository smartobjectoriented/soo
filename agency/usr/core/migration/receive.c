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

#include <kconfig.h>

#include <core/core.h>
#include <core/receive.h>
#include <core/debug.h>
#include <core/types.h>

#include <dcm/core.h>

#include <uapi/debug.h>

#if defined(CONFIG_LEDS)
#include <leds/leds.h>
#endif /* CONFIG_LEDS */

/**
 * Try to retrieve a ME from the DCM and deploy it.
 *
 * A word about the (receive) buffer handling. Actually, the buffer is vmalloc'd in the Compressor functional block
 * of the DCM in order to make a fast memcpy. We retrieve the buffer address returned by the vm memory allocator, but
 * it is *strongly* forbidden to access the buffer from the user space. Indeed, the kernel memory management including page
 * fault handing enforces this memory region to be accessed from the kernel space only.
 * The virtual address of the buffer is used as a *key* to identify the buffer in all operations from the user space.
 *
 * The function returns true if an available ME has been found, false otherwise.
 */
bool ME_processing_receive(void) {
	unsigned char *ME_buffer;
	size_t ME_size;
	int slotID;

#if defined(CONFIG_LEDS)
	static uint8_t toggle = 0;
#endif /* CONFIG_LEDS */

	DBG0("Recv ME\n");
	/* Perform receive processing as long as there are available MEs */

	ME_size = dcm_recv_ME(&ME_buffer);
	if (!ME_size) {
		DBG0("No ME available. Return\n");
		return false;
	}
	
#if defined(CONFIG_LEDS)
	led_on(4);

	if (toggle == 0) {
		led_on(5);
		led_off(6);
	}
	else {
		led_on(6);
		led_off(5);
	}
	toggle = (toggle + 1) % 2;
#endif /* CONFIG_LEDS */

	DBG("ME available: 0x%08x, %d\n", (unsigned int) ME_buffer, ME_size);

	slotID = get_ME_free_slot(ME_size);
	if (slotID > 0) {

		/* Set the personality to TARGET only and only if the incoming personality is INITIATOR.
		 * If the personality is SELFREFERENT, the injection must proceed.
		 */
		if (get_personality() == SOO_PERSONALITY_INITIATOR)
			set_personality_target();


		printf("** Receiving a ME -> now in slot %d\n", slotID);

		/* Tell AVZ to create a new domain context including the ME descriptor,
		 * and to prepare the ME to be implemented.
		 */
		if ((initialize_migration(slotID))) {
			force_print("%s: initialize_migration failed.\n", __func__);
			BUG();
		}

		DBG("Write snapshot located @ 0x%08x\n", (unsigned int) (ME_buffer));
		write_ME_snapshot(slotID, ME_buffer);

		DBG0("End write snapshot.\n");

		if ((finalize_migration(slotID))) {
			force_print("%s: finalize_migration failed.\n", __func__);
			BUG();
		}

		/* Be ready for future migration */
		set_personality_initiator();

	}

	dcm_release_ME(ME_buffer);

#if defined(CONFIG_LEDS)
	led_off(4);
#endif /* CONFIG_LEDS */

	return true;
}
