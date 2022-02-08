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

#ifndef OPENCN_LOGFILE_H
#define OPENCN_LOGFILE_H

#include <linux/types.h>

#include <soo/ring.h>
#include <soo/uapi/console.h>

#include <asm/page.h>

#define LOG_RING_SIZE		PAGE_SIZE * 128

typedef struct {
	char line[CONSOLEIO_BUFFER_SIZE];
} rtapi_log_request_t;

typedef struct  {
	char dummy;
} rtapi_log_response_t;

/*
 * Generate rtapi msg print ring structures and types.
 */
DEFINE_RING_TYPES(rtapi_log, rtapi_log_request_t, rtapi_log_response_t);

extern rtapi_log_sring_t *rtapi_log_sring;

void logfile_init(void);
void logfile_write(char *s);
void logfile_close(void);

bool logfile_enabled(void);

extern bool *p_rtapi_log_enabled;


#endif /* OPENCN_LOGFILE_H */

