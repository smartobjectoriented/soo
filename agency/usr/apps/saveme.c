/*
 * Copyright (C) 2014-2020 Daniel Rossier <daniel.rossier@soo.tech>
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

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <stdio.h>

#include <sys/ioctl.h>

#include <soo/uapi/soo.h>

#include <zip.h>

int fd_core;

int initialize_migration(unsigned int ME_slotID) {
	int rc;
	struct agency_ioctl_args args;

	args.slotID = ME_slotID;

	rc = ioctl(fd_core, AGENCY_IOCTL_INIT_MIGRATION, &args);
	assert(rc == 0);

	return args.value;
}

void read_ME_snapshot(unsigned int slotID, void **buffer, size_t *buffer_size) {
	struct agency_ioctl_args args;
	int ret;

	args.slotID = slotID;

	ret = ioctl(fd_core, AGENCY_IOCTL_READ_SNAPSHOT, &args);
	assert(ret == 0);

	*buffer = args.buffer;
	*buffer_size = args.value;
}


/*
 * Read a valid (user space) address to the ME snapshot.
 */
void *get_ME_snapshot_user(void *origin, size_t size) {

	struct agency_ioctl_args args;
	int ret;

	args.buffer = origin;
	args.slotID = size;

	args.value = (unsigned long) malloc(size);
	assert(args.value != 0);

	ret = ioctl(fd_core, AGENCY_IOCTL_GET_ME_SNAPSHOT, &args);
	assert(ret == 0);

	return (void *) args.value;

}

void finalize_migration(unsigned int slotID) {
	int rc;
	struct agency_ioctl_args args;

	args.slotID = slotID;

	rc = ioctl(fd_core, AGENCY_IOCTL_FINAL_MIGRATION, &args);
	assert(rc == 0);
}

int main(int argc, char *argv[]) {
	void *buffer;
	size_t buffer_size;
	int ret;
	struct zip_t *zip;

	printf("*** SOO - Mobile Entity snapshot saver ***\n");

	if (argc != 2) {
		printf("## Usage is : saveme <filename> where <filename> is the file containing the ME snapshot.\n");
		exit(-1);
	}

	printf("** Now taking the memory snapshot.\n");

	fd_core = open("/dev/soo/core", O_RDWR);
	assert(fd_core > 0);

	/* Prepare to suspend */

	ret = initialize_migration(2);
	if (ret) {
		printf("## No possibility to read the snapshot...\n");
		return -1;
	}

	/* Get the snapshot */
	read_ME_snapshot(2, &buffer, &buffer_size);

	buffer = get_ME_snapshot_user(buffer, buffer_size);

	printf("  * Got a ME buffer of %ld bytes.\n", buffer_size);

	finalize_migration(2);

	printf("  * Saving to the file...");
	fflush(stdout);

	/* Save the snapshot to file */
	zip = zip_open(argv[1], ZIP_DEFAULT_COMPRESSION_LEVEL, 'w');

	zip_entry_open(zip, "me");
	zip_entry_write(zip, buffer, buffer_size);
	zip_entry_close(zip);

	zip_close(zip);

	close(fd_core);

	printf("done.\n\n");
	printf("  * snapshot saved successfully\n");

	return 0;
}
