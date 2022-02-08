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

#ifndef UAPI_LCEC_H
#define UAPI_LCEC_H

#include <opencn/uapi/hal.h>
#include <opencn/uapi/lcec_conf.h>

#define LCEC_DEV_NAME    "/dev/opencn/lcec/0"
#define LCEC_DEV_MAJOR   105

/*
 * IOCTL codes
 */
#define LCEC_IOCTL_CONNECT       _IOW(0x05000000, 0, char)
#define LCEC_IOCTL_DISCONNECT    _IOW(0x05000000, 1, char)

typedef struct {
	char *name;
	lcec_master_t *config;
	int debug;
} lcec_connect_args_t;

#endif /* UAPI_LCEC_H */
