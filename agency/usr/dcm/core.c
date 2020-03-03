/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2018-2019 Baptiste Delporte <bonel@bonel.net>
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

#define _GNU_SOURCE

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <kconfig.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <core/core.h>
#include <core/send.h>
#include <core/receive.h>
#include <core/inject.h>
#include <core/debug.h>
#include <core/types.h>

#include <dcm/core.h>
#include <dcm/compressor.h>
#include <dcm/datacomm.h>
#include <dcm/security.h>

int fd_dcm;

/**
 * Send a ME using the DCM. ME_buffer and ME_size must include the migration structure.
 * This function triggers the TX request by Datalink's side.
 * To indicate the end of propagation to the DCM, the NULL value is passed as ME_buffer.
 */
void dcm_send_ME(void *ME_buffer, size_t ME_size, uint32_t prio) {
	int rc;
	dcm_ioctl_send_args_t args;

	args.ME_data = ME_buffer;
	args.size = ME_size;
	args.prio = prio;

	DBG("Sending ME: 0x%08x, %d\n", (unsigned int) ME_buffer, ME_size);

	if ((rc = ioctl(fd_dcm, DCM_IOCTL_SEND, &args)) < 0) {
		printf("Failed to send the ME (%d)\n", rc);
		BUG();
	}
}

/**
 * Try to retrieve a ME from the DCM. If a ME is available, the function returns its size.
 * Otherwise, a value of 0 is returned. The caller function must consume the ME then it has to
 * call the dcm_release_ME function with the buffer pointer and the size returned by this
 * function, as parameters.
 *
 * The size of the ME buffer includes the various header structure as described in the Agency Core specification.
 */
size_t dcm_recv_ME(unsigned char **ME_buffer) {
	dcm_ioctl_recv_args_t args;

	DBG("Trying to receive a ME\n");

	if ((ioctl(fd_dcm, DCM_IOCTL_RECV, &args)) < 0) {
		DBG0("ioctl DCM_IOCTL_RECV failed.\n");
		BUG();
	}

	if (args.ME_data == NULL) {
		DBG0("No ME\n");
		return 0;
	}

	*ME_buffer = (unsigned char *) args.ME_data;

	return args.size;
}

/**
 * Free the ME and release the ME slot in the DCM. The parameters must be the ones returned
 * by the dcm_recv_ME function.
 */
void dcm_release_ME(void *ME_buffer) {
	int rc;

	if (!ME_buffer) {
		printf("No ME is available\n: 0x%08x", (unsigned int) ME_buffer);
		BUG();
	}

	DBG("Release ME: 0x%08x\n", (unsigned int) ME_buffer);

	if ((rc = ioctl(fd_dcm, DCM_IOCTL_RELEASE, ME_buffer)) < 0) {
		printf("Failed to release the ME (%d)\n", rc);
		BUG();
	}
}

void dcm_dev_init(void) {
	if ((fd_dcm = open(DCM_DEV_NAME, O_RDWR)) < 0) {
		printf("Failed to open device: " DCM_DEV_NAME " (%d)\n", fd_dcm);
		perror("dcm_dev_init");
		BUG();
	}
}

void dcm_init(void) {
	int rc;

	if ((rc = ioctl(fd_dcm, DCM_IOCTL_INIT, 0)) < 0) {
		printf("Failed to init the DCM (%d)\n", rc);
		BUG();
	}
}
