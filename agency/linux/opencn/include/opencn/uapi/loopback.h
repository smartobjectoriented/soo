/*
 * Copyright (C) 2014-2021 Elieva Pignat <elieva.pignat@heig-vd.ch>
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
#ifndef UAPI_LOOPBACK_H
#define UAPI_LOOPBACK_H

#include <opencn/uapi/hal.h>

#define LOOPBACK_DEV_NAME	"/dev/opencn/loopback/0"
#define LOOPBACK_DEV_MAJOR  109
#define LOOPBACK_DEV_MINOR  0

/*
 * IOCTL codes
 */
#define LOOPBACK_IOCTL_CONNECT		_IOW(0x05000000, 0, char)
#define LOOPBACK_IOCTL_DISCONNECT	_IOW(0x05000000, 1, char)

#define LOOPBACK_MAX_PINS           25
#define MAX_LOOPBACKS               8

typedef struct {
	char name[HAL_NAME_LEN];
	char cfg[HAL_NAME_LEN];
} loopback_connect_args_t;

#endif /* UAPI_LOOPBACK_H */

