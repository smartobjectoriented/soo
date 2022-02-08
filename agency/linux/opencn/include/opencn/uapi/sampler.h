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

#ifndef UAPI_SAMPLER_H
#define UAPI_SAMPLER_H

#include <opencn/uapi/hal.h>

#define SAMPLER_DEV_NAME	"/dev/opencn/sampler/0"
#define SAMPLER_DEV_MAJOR	103

/* Streamer component */
#define SAMPLER_DEV_MINOR	0

#define MAX_SAMPLERS		8
#define SAMPLER_MAX_PINS    25

/*
 * IOCTL codes
 */
#define SAMPLER_IOCTL_CONNECT		_IOW(0x05000000, 0, char)
#define SAMPLER_IOCTL_DISCONNECT	_IOW(0x05000000, 1, char)

typedef struct {
	char name[HAL_NAME_LEN];
	int channel;
	char cfg[HAL_NAME_LEN];
	int depth;

} sampler_connect_args_t;

typedef struct {
	hal_type_t type;
	union {
	double f;
	uint32_t u;
	int32_t s;
	bool b;
	};
} sampler_value_t;

typedef struct {
	int n_pins;
	sampler_value_t pins[SAMPLER_MAX_PINS];
} sampler_sample_t;

#endif /* UAPI_SAMPLER_H */
