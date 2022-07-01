/*
 * Copyright (C) 2021 David Truan david.truan@heig-vd.ch
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


/**
 * Main entry point of the Agency core subsystem.
 */
int main(int argc, char *argv[]) {
	int i, fd_core;
	agency_ioctl_args_t agency_ioctl_args;


	if (argc != 2) {
		printf("Usage: %s <SOO-to-blacklist>\n", argv[0]);
		return -1;
	}

	fd_core = open("/dev/soo/core", O_RDWR);
	assert(fd_core > 0);

	agency_ioctl_args.buffer = argv[1];
	ioctl(fd_core, AGENCY_IOCTL_BLACKLIST_SOO, (unsigned long) &agency_ioctl_args);

	printf("The SOO %s was blacklisted.\n", argv[1]);

	close(fd_core);

	return 0;
}