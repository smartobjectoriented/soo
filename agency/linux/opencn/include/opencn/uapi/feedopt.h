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

#ifndef UAPI_FEEDOPT_H
#define UAPI_FEEDOPT_H

#include <opencn/uapi/hal.h>

#define FEEDOPT_DEV_NAME "/dev/opencn/feedopt/0"
#define FEEDOPT_DEV_MAJOR 108

#define FEEDOPT_SHMEM_NAME "feedopt"
#define FEEDOPT_SHMEM_KEY_CONFIG 65
#define FEEDOPT_SHMEM_KEY_CURV 66

/* Feedopt component */
#define FEEDOPT_DEV_MINOR 0

/*
 * IOCTL codes
 */
#define FEEDOPT_IOCTL_CONNECT 		_IOW(0x05000000, 0, char)
#define FEEDOPT_IOCTL_DISCONNECT 	_IOW(0x05000000, 1, char)
#define FEEDOPT_IOCTL_RESET _IOW(0x05000000, 3, char)

#define FEEDOPT_RESAMPLE_FREQ 10000

/* Use a queue long enough for 0.1 seconds of runtime */
#define FEEDOPT_RT_QUEUE_SIZE (FEEDOPT_RESAMPLE_FREQ / 10)


typedef struct {
	char name[HAL_NAME_LEN];
	int channel;
	int depth;
} feedopt_connect_args_t;

typedef struct {
	double axis_position[3];
    double spindle_speed;
	int end_flag;
	int index;
    int gcode_line;
} feedopt_sample_t;

typedef enum {
	PushStatus_Success,
	PushStatus_TryAgain,
} PushStatus;

#endif /* FEEDOPT_H */
