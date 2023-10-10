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

int main(int argc, char *argv[]) {
	int fd_core;
	agency_ioctl_args_t agency_ioctl_args;

	printf("*** SOO - Mobile Entity shutdown ***\n");

	if (argc != 2) {
		printf("## Usage is : shutdownme <ME ID (1-5)>\n");
		exit(-1);
	}

	printf("** Perform a shutdown of ME #%d (slotID %d)...", atoi(argv[1]), atoi(argv[1])+1);
	fflush(stdout);

	fd_core = open("/dev/soo/core", O_RDWR);
	assert(fd_core > 0);

	/* Prepare to terminate the running ME (dom #2) */

	agency_ioctl_args.slotID = atoi(argv[1]) + 1;

	ioctl(fd_core, AGENCY_IOCTL_FORCE_TERMINATE, &agency_ioctl_args);

	printf("done.\n");

	close(fd_core);

	return 0;
}
