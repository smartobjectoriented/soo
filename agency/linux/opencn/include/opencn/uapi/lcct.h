/********************************************************************
 *  Copyright (C) 2019  Peter Lichard  <peter.lichard@heig-vd.ch>
 *  Copyright (C) 2019 Jean-Pierre Miceli Miceli <jean-pierre.miceli@heig-vd.ch>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 ********************************************************************/

#ifndef UAPI_LCCT_H
#define UAPI_LCCT_H

#define LCCT_DEV_NAME    "/dev/opencn/lcct/0"
#define LCCT_DEV_MAJOR   107

#include <opencn/uapi/hal.h>

/*
 * IOCTL codes
 */
#define LCCT_IOCTL_CONNECT       _IOW(0x05000000, 0, char)
#define LCCT_IOCTL_DISCONNECT    _IOW(0x05000000, 1, char)

typedef struct {
	char name[HAL_NAME_LEN];
} lcct_connect_args_t;



#endif /* UAPI_LCCT_H */
