/*
 * Copyright (C) 2016 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2017,2018 Baptiste Delporte <bonel@bonel.net>
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

#ifndef DCM_H
#define DCM_H

#ifdef __KERNEL__

#include <linux/list.h>
#include <asm-generic/ioctl.h>

#else /* !__KERNEL__ */

#include <stdint.h>
#include <stddef.h>

#endif /* __KERNEL__ */

#define DCM_DEV_NAME		"/dev/soo/dcm"

#ifdef __KERNEL__

#define DCM_MAJOR		127

#define DCM_N_RECV_BUFFERS	1

#endif /* __KERNEL__ */

/* IOCTL codes exposed to the user space side */
#define DCM_IOCTL_NEIGHBOUR_COUNT		_IOWR(0x5000DC30, 0, char)
#define DCM_IOCTL_SEND				_IOWR(0x5000DC30, 1, char)
#define DCM_IOCTL_RECV				_IOWR(0x5000DC30, 2, char)
#define DCM_IOCTL_RELEASE			_IOWR(0x5000DC30, 3, char)
#define DCM_IOCTL_DUMP_NEIGHBOURHOOD		_IOWR(0x5000DC30, 4, char)
#define DCM_IOCTL_SET_AGENCY_UID		_IOWR(0x5000DC30, 5, char)
#define DCM_IOCTL_RX_AVAILABLE_ME_N	 	_IOWR(0x5000DC30, 6, char)
/*
 * Buffer descriptor
 */
typedef struct {

	/* ME_data refers to the buffer containing the ME *and* its ME_info_transfer header */
	void *ME_data;
	uint32_t ME_size;

	/* For future usage */
	uint32_t prio;

} dcm_buffer_t;

#ifdef __KERNEL__

typedef struct {
	struct list_head list;

	dcm_buffer_t dcm_buffer;
} dcm_buffer_entry_t;


/* Called from SOOlink */
int dcm_ME_rx(void *ME_buffer, uint32_t size);

#endif /* __KERNEL__ */

#endif /* DCM_H */
