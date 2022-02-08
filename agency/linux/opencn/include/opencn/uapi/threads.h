/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef UAPI_THREADS_H
#define UAPI_THREADS_H

#include <opencn/uapi/hal.h>

#define THREADS_DEV_NAME	"/dev/opencn/threads/0"
#define THREADS_DEV_MAJOR	104

/* Streamer component */
#define THREADS_DEV_MINOR	0

#define MAX_THREADS		8

/*
 * IOCTL codes
 */
#define THREADS_IOCTL_CONNECT		_IOW(0x05000000, 0, char)
#define THREADS_IOCTL_DISCONNECT	_IOW(0x05000000, 1, char)

typedef struct {
	char name1[HAL_NAME_LEN], name2[HAL_NAME_LEN], name3[HAL_NAME_LEN];
	unsigned long period1, period2, period3;
	int fp1, fp2, fp3;
} threads_connect_args_t;

#endif /* UAPI_THREADS_H */
