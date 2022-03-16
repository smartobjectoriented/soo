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
	ME_id_t id_array[MAX_ME_DOMAINS];
	agency_ioctl_args_t agency_ioctl_args;

	printf("*** SOO - Mobile Entity ID Retrieval ***\n\n");

	fd_core = open("/dev/soo/core", O_RDWR);
	assert(fd_core > 0);

	/* Prepare to terminate the running ME (dom #2) */
	printf("*** List of residing Mobile Entities: \n");

	agency_ioctl_args.buffer = &id_array;
	ioctl(fd_core, AGENCY_IOCTL_GET_ME_ID_ARRAY, (unsigned long) &agency_ioctl_args);

	for (i = 0; i < MAX_ME_DOMAINS; i++) {
		if (id_array[i].state == ME_state_dead)
			printf("  slot %d -> empty\n", i+2);
		else {
			printf("  slot %d -> spid: %llx       name: %s\n", i+2, id_array[i].spid, id_array[i].name);
			printf("             Short description: %s\n", id_array[i].shortdesc);
		}
	}

	printf("done.\n");

	close(fd_core);

	return 0;
}
