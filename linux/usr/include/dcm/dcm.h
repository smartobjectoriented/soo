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

#ifndef DCM_CORE_H
#define DCM_CORE_H

#include <stdint.h>
#include <stddef.h>

#include <core/core.h>

#include <soo/uapi/dcm.h>

/* Max ME size is 64 MBytes */
#define	ME_BUFFER_MAXSIZE	8*1024*1024

/* Global DCM file descriptor to access the kernel space */
extern int dcm_fd;

/*
 * Header of a DCM frame as defined in the DCM specification
 */
typedef struct {
	uint32_t max_seq;
	uint32_t seq_id;
	uint32_t crc;
	uint32_t len;
} dcm_frame_hdr_t;


void dcm_dev_init(void);

bool dcm_is_send_buffer_available(unsigned int ME_slotID);
bool dcm_same_slotID_available(unsigned int ME_slotID);

void dcm_prepare_send_ME(unsigned int ME_slotID, void *ME, size_t size, uint32_t prio);
void dcm_send_ME(void *ME, size_t size, uint32_t prio);
void dcm_prepare_update_ME(unsigned int ME_slotID, void *ME, size_t size, uint32_t prio);
void dcm_update_ME(unsigned int ME_slotID, void *ME_buffer, size_t ME_size, uint32_t prio);

void dcm_recv_ME(unsigned char **ME_buffer, size_t *buffer_size, size_t *ME_size);

void dcm_release_ME(void *ME_buffer);

#endif /* DCM_CORE_H */

