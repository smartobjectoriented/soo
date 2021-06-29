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

#include <soo/uapi/asf.h>

typedef struct {
	int val;
} hello_args_t;


int main(int argc, char *argv[])
{
	int devfd;
	hello_args_t args;
	int ret;

	devfd = open(ASF_DEV_NAME, O_RDWR);
	if (!devfd) {
		printf("Opening '%s' failed\n", ASF_DEV_NAME);
		return -1;
	}

	/* Hello World Test TA test */
	args.val = 5;
	printf("Hello World TA Test, value: %d\n", args.val);
	ret = ioctl(devfd, ASF_IOCTL_HELLO_WORLD_TEST, &args);
	if (ret != 0) {
		printf("ioctl command failed\n");
		return -1;
	}

	/* ASF TA crypto Test  */
	ioctl(devfd, ASF_IOCTL_CRYPTO_TEST, NULL);

	close(devfd);

	return 0;
}
