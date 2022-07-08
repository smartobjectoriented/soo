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
#include <sys/stat.h>

#include <soo/uapi/soo.h>

#include <zip.h>

int fd_core;

int initialize_migration(unsigned int slotID) {
	int rc;
	struct agency_ioctl_args args;

	args.slotID = slotID;

	rc = ioctl(fd_core, AGENCY_IOCTL_INIT_MIGRATION, &args);
	assert(rc == 0);

	return args.value;
}

/**
 * Restore the snapshot of a ME.
 */
void write_ME_snapshot(unsigned int slotID, unsigned char *ME_buffer) {
	agency_ioctl_args_t args;

	args.slotID = slotID;
	args.buffer = ME_buffer;

	ioctl(fd_core, AGENCY_IOCTL_WRITE_SNAPSHOT, &args);
}

int main(int argc, char *argv[]) {
	struct agency_ioctl_args args;
	void *buffer = NULL;
	size_t buffer_size;
	struct zip_t *zip;

	printf("*** SOO - Mobile Entity snapshot restorer ***\n");

	if (argc != 2) {
		printf("## Usage is : restoreme <filename> where <filename> is the file containing the ME snapshot.\n");
		exit(-1);
	}

	printf("** Now reading the ME snapshot.\n");

	fd_core = open("/dev/soo/core", O_RDWR);
	assert(fd_core > 0);

	/* Save the snapshot to file */
	zip = zip_open(argv[1], 0, 'r');
	if (!zip) {
		perror("");
		return -1;
	}

	zip_entry_open(zip, "me");
	zip_entry_read(zip, &buffer, &buffer_size);
	zip_entry_close(zip);

	zip_close(zip);

	printf("  ** ME memory re-implantation and resuming...\n");

	args.value = buffer_size;

	ioctl(fd_core, AGENCY_IOCTL_GET_ME_FREE_SLOT, &args);
	assert(args.slotID == 2);

	initialize_migration(2);

	write_ME_snapshot(2, buffer);

	args.slotID = 2;

	ioctl(fd_core, AGENCY_IOCTL_FINAL_MIGRATION, &args);

	close(fd_core);

	free(buffer);

	printf("  ** ME successfully restored and resumed...\n");

	return 0;
}
