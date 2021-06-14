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

#if 0
#define DEBUG
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <core/core.h>
#include <core/inject.h>
#include <core/debug.h>
#include <core/types.h>
#include <core/device_access.h>

#include <uapi/dcm.h>

#include <injector/core.h>

/**
 * Inject a ME.
 * @ME_buffer: the ITB file of the ME.
 */
int inject_ME(void *ME_buffer) {
	int rc;
	struct agency_tx_args args;

	args.buffer = ME_buffer;

	if ((rc = ioctl(fd_core, AGENCY_IOCTL_INJECT_ME, &args)) < 0) {
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


void agency_inject_ME_from_memory(void) {
}

void sig_inject_ME_from_memory(int sig) {
	agency_inject_ME_from_memory();
}

/**
 * Look for MEs in the SOO_ME_DIRECTORY directory and inject the MEs one by one.
 * The SOO_ME_DIRECTORY can be a mount point (mounted on a dedicated storage partition,
 * this is the default method) or a directory integrated into the agency's rootfs.
 */
void inject_MEs_from_filesystem(void) {
	DIR *directory;
	int fd;
	struct dirent *ent;
	char filename[FILENAME_MAX_LEN];
	int nread, ME_size;
	unsigned char *ME_buffer = NULL;
	struct stat filestat;

	if (opt_noinject) {
		DBG0("-noinject enabled: Skipping auto injection\n");
		return;
	}

	directory = opendir(SOO_ME_DIRECTORY);
	if (!directory) {
		DBG0("The " SOO_ME_DIRECTORY "does not exist\n");

		return;
	}

	while ((ent = readdir(directory)) != NULL) {

		/* Ignore . and .. */
		if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..") || !strcmp(ent->d_name, "lost+found"))
			continue;

		printf("Found new ME to inject : %s\n", ent->d_name);

		strcpy(filename, SOO_ME_DIRECTORY);
		strcat(filename, "/");
		strcat(filename, ent->d_name);

		stat(filename, &filestat);

		fd = open(filename, O_RDONLY);

		if (fd < 0) {
			perror(filename);
			printf("%s not found.\n", filename);
			exit(-1);
		}

		ME_size = filestat.st_size;

		/* Allocate the ME buffer */
		ME_buffer = malloc(ME_size);
		if (!ME_buffer) {
			printf("%s: failure during ME malloc...\n", __func__);
			BUG();
		}

		DBG("agency_core: size to read from sd : %d, buffer address : 0x%08x\n", ME_size, (unsigned int) ME_buffer);

		/* Read the ME content  */
		nread = read(fd, ME_buffer, ME_size);
		if (nread != ME_size) {
			printf("Error when reading the ME\n");
			BUG();
		}

		/* Inject the ME */
		ME_inject(ME_buffer);

		close(fd);

		free(ME_buffer);

	}

	closedir(directory);
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
			if (!ME) {
				printf("%s: failure during malloc...\n", __func__);
				BUG();
			}

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
