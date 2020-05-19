/*
 * Copyright (C) 2016-2019 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) January 2018 Baptiste Delporte <bonel@bonel.net>
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

#if 1
#define DEBUG
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <kconfig.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <core/core.h>
#include <core/inject.h>
#include <core/debug.h>
#include <core/types.h>

#include <uapi/dcm.h>

#include <injector/core.h>


static int fd_core;


/**
 * Inject a ME.
 * @ME_buffer: the ITB file of the ME.
 */
int inject_ME(void *ME_buffer) {
	int rc;
	struct agency_tx_args args;

	args.buffer = ME_buffer;

	if ((rc = ioctl(fd_migration, AGENCY_IOCTL_INJECT_ME, &args)) < 0) {
		printf("Failed to inject ME (%d)\n", rc);
		BUG();
	}

	return args.ME_slotID;
}

/**
 * Try to retrieve a ME from the DCM and deploy it.
 */
void ME_inject(unsigned char *ME_buffer) {
	int slotID;

	slotID = inject_ME(ME_buffer);
	if (slotID == -1) {
		printf("No available ME slot further...\n");
		return;
	}

	/* Set the personality to "selfreferent" so that the final migration path will be slightly different
	 * than a "target" personality.
	 */
	set_personality_selfreferent();

	/* Finalization of the injected ME involving the callback sequence */
	if (finalize_migration(slotID)) {
		printf("%s: finalize_migration failed.\n", __func__);
		BUG();
	}

	/* Be ready for future migration */
	set_personality_initiator();
}


void save_itb(void *ME_buffer, size_t size) {
	FILE *f;

	int i;
	
	f = fopen("/root/test_ME.itb", "wb");

	for (i = 0; i < size; ++i) {
		fwrite(ME_buffer+i, sizeof(char), 1, f);
	}

	fclose(f);
}


void *ME_retrieve_fn(void *dummy) {

	injector_ioctl_recv_args_t args;

	void *ME;
	int br = 0;
	int current_size = 0;
	int chunk = 2000;

	memset(&args, 0, sizeof(injector_ioctl_recv_args_t));

	printf("Injector: ME retrieve thread started\n");
	while(1) {

		usleep(500 * 1000);

		if ((ioctl(fd_core, INJECTOR_IOCTL_RETRIEVE_ME, &args)) < 0) {
			DBG("ioctl INJECTOR_IOCTL_RETRIEVE_ME failed.\n");
			BUG();
		}
		
		if (args.size != 0) {

			printf("Injector: An ME is ready to be retrieved (%d B)\n", args.size);
			ME = malloc(args.size);


			while (current_size != args.size) {
				br = read(fd_core, ME+current_size, chunk);
				current_size += br;

			}

			ME_inject(ME);

			if ((ioctl(fd_core, INJECTOR_IOCTL_CLEAN_ME, NULL)) < 0) {
				DBG("ioctl INJECTOR_IOCTL_RETRIEVE_ME failed.\n");
				BUG();
			}
			
			return NULL;

		}
	}
	
	return NULL;
}

void injector_dev_init(void) {
	if ((fd_core = open(SOO_CORE_DEVICE, O_RDWR)) < 0) {
		printf("Failed to open device: " INJECTOR_DEV_NAME " (%d)\n", fd_core);
		perror("injector_dev_init");
		BUG();
	}
}


void injector_init(void) {

	pthread_t injection_thread;

	injector_dev_init();

	pthread_create(&injection_thread, NULL, ME_retrieve_fn, NULL);
}