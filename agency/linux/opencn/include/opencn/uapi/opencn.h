
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

#ifndef UAPI_OPENCN_H
#define UAPI_OPENCN_H

#define OPENCN_LOGFILE_NAME	"/root/opencn.log"

#define OPENCN_CORE_DEV_NAME	"/dev/opencn/core"
#define OPENCN_CORE_DEV_MAJOR	100

/* Streamer component */
#define OPENCN_CORE_DEV_MINOR	1

/*
 * IOCTL codes
 */
#define OPENCN_IOCTL_LOGFILE_ON		_IOW(0x05000000, 0, char)
#define OPENCN_IOCTL_LOGFILE_OFF	_IOW(0x05000000, 1, char)

#endif /* UAPI_OPENCN_H */
