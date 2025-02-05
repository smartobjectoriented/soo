/*
 * Copyright (C) 2016-2022 Daniel Rossier <daniel.rossier@heig-vd.ch>
 * Copyright (C) January 2018 Baptiste Delporte <bonel@bonel.net>
 * Copyright (C) 2019-2022 David Truan <david.truan@heig-vd.ch>
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <core/core.h>
#include <core/debug.h>
#include <core/types.h>
#include <core/injector.h>

/**
 * Main entry point of the Agency core subsystem.
 */
int main(int argc, char *argv[]) {
        int fd_migration, fd, ret;
        int nread, ME_size;
        void *ME_buffer;
	struct stat filestat;
	struct agency_ioctl_args args;

	printf("SOO ME injector (Smart Object Oriented based virtualization framework).\n");
	printf("Version: %s\n", AGENCY_CORE_VERSION);

        fd_migration = open(SOO_CORE_DEVICE, O_RDWR);
        if (fd_migration < 0) {
                printf("Failed to open device: " SOO_CORE_DEVICE " (%d)\n", fd_migration);
		exit(EXIT_FAILURE);
        }

        stat(argv[1], &filestat);

	fd = open(argv[1], O_RDONLY);

	if (fd < 0) {
		perror(argv[1]);
		printf("%s not found.\n", argv[1]);
		exit(EXIT_FAILURE);
	}

	ME_size = filestat.st_size;

	/* Allocate the ME buffer */
	ME_buffer = malloc(ME_size);

	DBG("agency_core: size to read from sd : %d, buffer address : 0x%08lx\n", ME_size, (unsigned long) ME_buffer);

	/* Read the ME content  */
	nread = read(fd, ME_buffer, ME_size);

	if (nread < 0) {
		printf("Error when reading the ME\n");
		exit(EXIT_FAILURE);
	}

	/* Inject the ME */
	
	args.buffer = ME_buffer;
	args.value = ME_size;

	if ((ret = ioctl(fd_migration, AGENCY_IOCTL_INJECT_ME, &args)) < 0) {
		printf("Failed to inject ME (%d)\n", ret);
		exit(EXIT_FAILURE);
	}

	if (args.slotID == -1) {
		printf("No available ME slot further...\n");
		exit(EXIT_FAILURE);
	}
	
	close(fd);
	free(ME_buffer);

	return 0;
}
