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
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>

#include <core/core.h>
#include <core/upgrader.h>
#include <core/send.h>
#include <core/receive.h>
#include <core/debug.h>
#include <core/types.h>
#include <core/uevent.h>

#include <dcm/core.h>
#include <injector/core.h>

#ifdef WITH_LED_ACTIVITIES
#include <leds/leds.h>
#endif

int fd_core;

/**
 * Set the personality.
 */
static int set_personality(soo_personality_t personality) {
	int rc;
	struct agency_tx_args args;

	args.value = (int) personality;

	if ((rc = ioctl(fd_core, AGENCY_IOCTL_SET_PERSONALITY, &args)) < 0) {
		printf("Failed to set personality (%d)\n", rc);
		BUG();
	}

	return 0;
}

int get_personality(void) {
	struct agency_tx_args args;

	if (ioctl(fd_core, AGENCY_IOCTL_GET_PERSONALITY, &args) < 0) {
		printf("Failed to set personality.\n");
		BUG();
	}

	return args.value;
}

int set_personality_initiator(void) {
	return set_personality(SOO_PERSONALITY_INITIATOR);
}

int set_personality_target(void) {
	return set_personality(SOO_PERSONALITY_TARGET);
}

int set_personality_selfreferent(void) {
	return set_personality(SOO_PERSONALITY_SELFREFERENT);
}

/**
 * Initiate the migration process of a ME.
 * In any case, the function can't fail. As a principle, any error leads to BUG().
 */
int initialize_migration(unsigned int ME_slotID) {
	int rc;
	struct agency_tx_args args;

	args.ME_slotID = ME_slotID;

	if ((rc = ioctl(fd_core, AGENCY_IOCTL_INIT_MIGRATION, &args)) < 0) {
		printf("Failed to initialize migration (%d)\n", rc);
		BUG();
	}

	return args.value;
}

/**
 * Get an available ME slot from the hypervisor.
 */
int get_ME_free_slot(size_t ME_size) {
	int rc;
	struct agency_tx_args args;

	args.value = ME_size;

	if ((rc = ioctl(fd_core, AGENCY_IOCTL_GET_ME_FREE_SLOT, &args)) < 0) {
		printf("Failed to get ME slot (%d)\n", rc);
		BUG();
	}

	return args.ME_slotID;
}

/**
 * Retrieve the ME descriptor including the SPID, the state and the SPAD.
 * If no ME is present in the specified slot, the size of the ME descriptor is set to 0.
 * If the commands succeeds, it returns 0, otherwise the error code.
 */
int get_ME_desc(unsigned int ME_slotID, ME_desc_t *ME_desc) {
	int rc;
	struct agency_tx_args args;

	args.ME_slotID = ME_slotID;
	args.buffer = (unsigned char *) ME_desc;

	if ((rc = ioctl(fd_core, AGENCY_IOCTL_GET_ME_DESC, &args)) < 0) {
		printf("Failed to get ME desc (%d)\n", rc);
		BUG();
	}

	if (ME_desc->size != 0)
		DBG("ME %d (size %d) has the state %d\n", ME_slotID, ME_desc->size, ME_desc->state);

	return 0;
}

/**
 * Make a snapshot of the ME.
 */
void read_ME_snapshot(unsigned int slotID, void **buffer, size_t *buffer_size) {
	struct agency_tx_args args;

	args.ME_slotID = slotID;

	if ((ioctl(fd_core, AGENCY_IOCTL_READ_SNAPSHOT, &args)) < 0) {
		printf("%s: (ioctl) Failed to read the ME snapshot.\n", __func__);
		BUG();
	}

	*buffer = args.buffer;
	*buffer_size = args.value;

	/* The ME snapshot ready to be sent it in args.buffer */
	DBG0("Read snapshot done.\n");

}

/**
 * Restore the snapshot of a ME.
 */
void write_ME_snapshot(unsigned int slotID, unsigned char *ME_buffer) {
	agency_tx_args_t args;

	args.ME_slotID = slotID;
	args.buffer = ME_buffer;

	if (ioctl(fd_core, AGENCY_IOCTL_WRITE_SNAPSHOT, &args) < 0) {
		printf("%s: (ioctl) failed to write snapshot.\n", __func__);
		BUG();
	}

	DBG0("Write snapshot done.\n");
}

/**
 * Initiate the last stage of the migration process of a ME, so called "migration
 * finalization".
 */
int finalize_migration(unsigned int slotID) {
	int rc;
	struct agency_tx_args args;

	args.ME_slotID = slotID;

	if ((rc = ioctl(fd_core, AGENCY_IOCTL_FINAL_MIGRATION, &args)) < 0) {
		printf("Failed to initialize migration (%d)\n", rc);
		BUG();
	}

	return 0;
}

/**
 * Get the current system time.
 */
long long get_system_time(void) {
	struct timeval tv;

	gettimeofday(&tv, NULL);

	return ((tv.tv_sec) * 1000 + (tv.tv_usec) / 1000);
}

/**
 * main_loop() implements the main SOO activity sequence in terms of broadcasting and receiving MEs.
 * The sequence has to be synchronous over the activities.
 * cycle_period is the cycle period in ms.
 */
void main_loop(int cycle_period) {
	static unsigned int mig_count = 1;
	bool available_ME = false;
#ifdef WITH_LED_ACTIVITIES
	uint32_t i;
#endif

	while (!ag_cycle_interrupted) {
		DBG("* Migration Cycle %d *\n", mig_count);

		/* We start waiting a fixed delay between a send/receive lifecycle */
		DBG0("Waiting between propagation...\n");

		/* --> transform this into a ioctl for waiting OR triggering a cycle upon receival of MEs */
		usleep(cycle_period * 1000);

		do {
			/* Try to receive a ME and deploy it */
			available_ME = ME_processing_receive();
			if (available_ME)
				/* Check for pending uevents */
				check_for_uevent();

		} while (available_ME);

		DBG0("Send ME\n");

		/* Send the next available ME */
		try_to_send_ME();

		DBG("* End of Cycle %d *\n\n", mig_count);

		/* Update the migration counter */
		mig_count++;
	}

#ifdef WITH_LED_ACTIVITIES
	for (i = 0 ; i < SOO_N_LEDS ; i++)
		led_off(i + 1);
#endif

	printf("SOO Agency core application: end\n");

	exit(0);
}

/**
 * Initialization of the Migration Manager functional block of the Core subsystem.
 */
void migration_init(void) {
	/* Open the migration SOO device */
	if ((fd_core = open(SOO_CORE_DEVICE, O_RDWR)) < 0) {
		printf("Failed to open device: " SOO_CORE_DEVICE " (%d)\n", fd_core);
		BUG();
	}
}
