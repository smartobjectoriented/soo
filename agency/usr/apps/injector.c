/*
 * Copyright (C) 2018 Baptiste Delporte <bonel@bonel.net>
 * Copyright (C) 2019-2021 Daniel Rossier <daniel.rossier@heig-vd.ch>
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
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdbool.h>

#include <soo/uapi/soo.h>

#include <core/core.h>
#include <core/debug.h>
#include <core/types.h>
#include <core/injector.h>

int fd_migration;

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

	if ((rc = ioctl(fd_migration, AGENCY_IOCTL_GET_ME_ID, &args)) < 0) {
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

	if ((ioctl(fd_migration, AGENCY_IOCTL_READ_SNAPSHOT, &args)) < 0) {
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

	if (ioctl(fd_migration, AGENCY_IOCTL_WRITE_SNAPSHOT, &args) < 0) {
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

	if ((rc = ioctl(fd_migration, AGENCY_IOCTL_FINAL_MIGRATION, &args)) < 0) {
		printf("Failed to initialize migration (%d)\n", rc);
		BUG();
	}

	return 0;
}

/**
 * Inject a ME.
 * @ME_buffer: the ITB file of the ME.
 */
int inject_ME(void *ME_buffer, size_t size) {
	int rc;
	struct agency_ioctl_args args;

	args.buffer = ME_buffer;
	args.value = size;

	if ((rc = ioctl(fd_migration, AGENCY_IOCTL_INJECT_ME, &args)) < 0) {
		printf("Failed to inject ME (%d)\n", rc);
		BUG();
	}

	return args.slotID;
}

/**
 * Try to retrieve a ME from the DCM and deploy it.
 */
void ME_inject(unsigned char *ME_buffer, uint32_t size) {
	int slotID;

	slotID = inject_ME(ME_buffer, size);
	if (slotID == -1) {
		printf("No available ME slot further...\n");
		return;
	}

	/* Finalization of the injected ME involving the callback sequence */
	if (finalize_migration(slotID)) {
		printf("%s: finalize_migration failed.\n", __func__);
		BUG();
	}
}

/**
 * Look for MEs in the SOO_ME_DIRECTORY directory and inject the MEs one by one.
 * The SOO_ME_DIRECTORY can be a mount point (mounted on a dedicated storage partition,
 * this is the default method) or a directory integrated into the agency's rootfs.
 */
void inject_MEs_from_filesystem(char *filename) {
	int fd;
	int nread, ME_size;
	unsigned char *ME_buffer;
	struct stat filestat;

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

	DBG("agency_core: size to read from sd : %d, buffer address : 0x%08x\n", ME_size, (unsigned int) ME_buffer);

	/* Read the ME content  */
	nread = read(fd, ME_buffer, ME_size);

	if (nread < 0) {
		printf("Error when reading the ME\n");
		BUG();

		goto close_free;
	}

	/* Inject the ME */
	ME_inject(ME_buffer, ME_size);

close_free:
	close(fd);

	free(ME_buffer);

}

/**
 * Initialization of the Injector functional block of the Core subsystem.
 */
void injector_init(void) {
	/* Open the migration SOO device */
	if ((fd_migration = open(SOO_CORE_DEVICE, O_RDWR)) < 0) {
		printf("Failed to open device: " SOO_CORE_DEVICE " (%d)\n", fd_migration);
		BUG();
	}
}

/**
 * Main entry point of the Agency core subsystem.
 */
int main(int argc, char *argv[]) {

	printf("SOO ME injector (Smart Object Oriented based virtualization framework).\n");
	printf("Version: %s\n", AGENCY_CORE_VERSION);

	injector_init();

	inject_MEs_from_filesystem(argv[1]);

	return 0;
}
