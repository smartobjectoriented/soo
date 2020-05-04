/*
 * Copyright (C) 2020 Jean-Pierre Miceli <jean-pierre.miceli@heig-vd.ch>
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

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define ASF_TA_DEV_NAME  			"/dev/soo/asf"
#define ASF_TA_IOCTL_HELLO_WORLD	0

typedef struct {
	int val;
} hello_args_t;


int main(int argc, char *argv[])
{
	int devfd;
	hello_args_t args;
	int ret;

	devfd = open(ASF_TA_DEV_NAME, O_RDWR);
	if (!devfd) {
		printf("Opening '%s' failed\n", ASF_TA_DEV_NAME);
		return -1;
	}

	args.val = 42;
	ret = ioctl(devfd, ASF_TA_IOCTL_HELLO_WORLD, &args);
	if (ret != 0) {
		printf("ioctl command failed\n");
		return -1;
	}

	args.val = 5;
	ret = ioctl(devfd, ASF_TA_IOCTL_HELLO_WORLD, &args);
	if (ret != 0) {
		printf("ioctl command failed\n");
		return -1;
	}

	close(devfd);

	return 0;
}