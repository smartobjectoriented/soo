
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

#ifndef VLOG_H
#define VLOG_H

#include <asm/page.h>

#include <soo/ring.h>
#include <soo/uapi/console.h>

#define VLOG_NAME		"vlog"
#define VLOG_PREFIX		"[" VLOG_NAME "] "

#define VLOG_RING_SIZE		PAGE_SIZE * 128

typedef struct {
	char line[CONSOLEIO_BUFFER_SIZE];
} vlog_request_t;

typedef struct  {
	char dummy;
} vlog_response_t;

/*
 * Generate vlog ring structures and types.
 */
DEFINE_RING_TYPES(vlog, vlog_request_t, vlog_response_t);

#endif /* VLOG_H */
