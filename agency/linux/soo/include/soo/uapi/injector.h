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

#ifndef INJECTOR_H
#define INJECTOR_H

#ifdef __KERNEL__
#include <asm-generic/ioctl.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#else
#include <stdint.h>
#include <stddef.h>

#endif /* __KERNEL__ */


#define INJECTOR_DEV_NAME		"/dev/soo/injector"

#ifdef __KERNEL__

#define INJECTOR_MAJOR		122

#define INJECTOR_N_RECV_BUFFERS	10

/* Max ME size in bytes */
#define DATACOMM_ME_MAX_SIZE	(32 * 1024 * 1024)

#endif /* __KERNEL__ */

/*
 * Types of buffer; helpful to manage buffers in a seamless way.
 */
typedef enum {
	INJECTOR_BUFFER_SEND = 0,
	INJECTOR_BUFFER_RECV
} injector_buffer_direction_t;


#ifdef __KERNEL__

/*
 * - INJECTOR_BUFFER_FREE means the buffer is ready for send/receive operations.
 * - INJECTOR_BUFFER_BUSY means the buffer is reserved for a ME.
 * - INJECTOR_BUFFER_SENDING means the buffer is currently along the path for being sent out.
 *   The buffer can still be altered depending on the steps of sending (See SOOlink/Coder).
 */
typedef enum {
	INJECTOR_BUFFER_FREE = 0,
	INJECTOR_BUFFER_BUSY,
	INJECTOR_BUFFER_SENDING,
} injector_buffer_status_t;

/*
 * Buffer descriptor
 */
typedef struct {
	injector_buffer_status_t	status;

	/*
	 * Reference to the ME.
	 */
	void *ME_data;
	size_t size;

	uint32_t prio;

} injector_buffer_desc_t;

void injector_prepare(uint32_t size);
void injector_clean_ME(void);
uint32_t injector_retrieve_ME(void);

void *injector_get_ME_buffer(void);
size_t injector_get_ME_size(void);

void *injector_get_tmp_buf(void);
size_t injector_get_tmp_size(void);
bool injector_is_full(void);
void injector_set_full(bool _full);

void injector_receive_ME(void *ME, size_t size);

int inject_ME(void *buffer, size_t size);

ssize_t agency_read(struct file *fp, char *buff, size_t length, loff_t *ppos);

#endif /* __KERNEL__ */


#endif /* INJECTOR_H */
