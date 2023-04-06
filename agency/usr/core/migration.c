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
#include <core/injector.h>

#include <dcm/dcm.h>

#ifdef WITH_LED_ACTIVITIES
#include <leds/leds.h>
#endif

int fd_core;

/**
 * Initiate the migration process of a ME.
 * In any case, the function can't fail. As a principle, any error leads to BUG().
 */
/**
 * Initiate the migration process of a ME.
 * In any case, the function can't fail. As a principle, any error leads to BUG().
 *
 * @param slotID
 * @return false in case the ME is dead during the pre_propagate callback
 */
int initialize_migration(unsigned int slotID) {
	int rc;
	struct agency_ioctl_args args;

	args.slotID = slotID;

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
	struct agency_ioctl_args args;

	args.value = ME_size;

	if ((rc = ioctl(fd_core, AGENCY_IOCTL_GET_ME_FREE_SLOT, &args)) < 0) {
		printf("Failed to get ME slot (%d)\n", rc);
		BUG();
	}

	return args.slotID;
}

/**
 * Retrieve the ME identity including the SPID, the state and the SPAD.
 * If no ME is present in the specified slot, the size of the ME id structure is set to 0.
 * If the commands succeeds, it returns 0, otherwise the error code.
 */
bool get_ME_id(uint32_t slotID, ME_id_t *me_id) {
	int rc;
	struct agency_ioctl_args args;

	args.slotID = slotID;
	args.buffer = me_id;

	if ((rc = ioctl(fd_core, AGENCY_IOCTL_GET_ME_ID, &args)) < 0) {
		printf("Failed to get ME ID (%d)\n", rc);
		BUG();
	}

	return ((args.value == 0) ? true : false);
}

/**
 * Make a snapshot of the ME.
 */
void read_ME_snapshot(unsigned int slotID, void **buffer, uint32_t *buffer_size) {
	struct agency_ioctl_args args;

	args.slotID = slotID;

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
	agency_ioctl_args_t args;

	args.slotID = slotID;
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
	struct agency_ioctl_args args;

	args.slotID = slotID;

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

#ifdef WITH_LED_ACTIVITIES
	uint32_t i;
#endif

	while (!ag_cycle_interrupted) {
		DBG("* Migration Cycle %d *\n", mig_count);

		DBG0("Receiving ME...\n");

		/* Try to receive a ME and deploy it */
		ME_processing_receive();

		DBG0("Send ME\n");

		/* Send the next available ME */
		try_to_send_ME();

		DBG("* End of Cycle %d *\n\n", mig_count);

		/* Update the migration counter */
		mig_count++;
		usleep(cycle_period * 1000);
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
