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

#ifndef UAPI_STREAMER_H
#define UAPI_STREAMER_H

#include <opencn/uapi/hal.h>

#define STREAMER_DEV_NAME	"/dev/opencn/streamer/0"
#define STREAMER_DEV_MAJOR	102

/* Streamer component */
#define STREAMER_DEV_MINOR	0

#define MAX_STREAMERS		8

/*
 * IOCTL codes
 */
#define STREAMER_IOCTL_CONNECT		_IOW(0x05000000, 0, char)
#define STREAMER_IOCTL_DISCONNECT	_IOW(0x05000000, 1, char)
#define STREAMER_IOCTL_CLEAR_FIFO	_IOW(0x05000000, 2, char)


typedef struct {
	char name[HAL_NAME_LEN];
	int channel;
	char cfg[HAL_NAME_LEN];
	int depth;

} streamer_connect_args_t;

#endif /* UAPI_STREAMER_H */
