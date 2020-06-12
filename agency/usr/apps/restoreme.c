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

#include <uapi/soo.h>

int fd_core;

int initialize_migration(unsigned int ME_slotID) {
	int rc;
	struct agency_tx_args args;

	args.ME_slotID = ME_slotID;

	rc = ioctl(fd_core, AGENCY_IOCTL_INIT_MIGRATION, &args);
	assert(rc == 0);

	return args.value;
}

/**
 * Restore the snapshot of a ME.
 */
void write_ME_snapshot(unsigned int slotID, unsigned char *ME_buffer) {
	agency_tx_args_t args;

	args.ME_slotID = slotID;
	args.buffer = ME_buffer;

	ioctl(fd_core, AGENCY_IOCTL_WRITE_SNAPSHOT, &args);
}

int main(int argc, char *argv[]) {
	struct agency_tx_args args;
	int fd, ret;
	void *buffer;
	unsigned int buffer_size;
	struct stat filestat;

	printf("*** SOO - Mobile Entity snapshot restorer ***\n");

	if (argc != 2) {
		printf("## Usage is : restoreme <filename> where <filename> is the file containing the ME snapshot.\n");
		exit(-1);
	}

	printf("** Now reading the ME snapshot.\n");

	fd_core = open("/dev/soo/core", O_RDWR);
	assert(fd_core > 0);

	/* Get the size of the image */
	stat(argv[1], &filestat);
	buffer_size = filestat.st_size;

	buffer = malloc(buffer_size);
	assert(buffer != NULL);

	/* Save the snapshot to file */
	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror("");
		return -1;
	}

	ret = read(fd, buffer, buffer_size);
	if (ret != buffer_size) {
		perror("");
		return -1;
	}

	printf("  ** ME memory re-implantation and resuming...\n");

	args.value = buffer_size;
	ioctl(fd_core, AGENCY_IOCTL_GET_ME_FREE_SLOT, &args);
	assert(args.ME_slotID == 2);

	/* Set personality to target */

	args.value = SOO_PERSONALITY_TARGET;
	ioctl(fd_core, AGENCY_IOCTL_SET_PERSONALITY, &args);

	initialize_migration(2);

	write_ME_snapshot(2, buffer);

	args.ME_slotID = 2;

	ioctl(fd_core, AGENCY_IOCTL_FINAL_MIGRATION, &args);
	args.value = SOO_PERSONALITY_INITIATOR;

	ioctl(fd_core, AGENCY_IOCTL_SET_PERSONALITY, &args);

	close(fd);

	close(fd_core);

	printf("  ** ME successfully restored and resumed...\n");

	return 0;
}