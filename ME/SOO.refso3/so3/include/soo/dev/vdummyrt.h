/*
 * Copyright (C) 2016-2018 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef VDUMMYRT_H
#define VDUMMYRT_H

#include <soo/ring.h>
#include <soo/grant_table.h>

#define VDUMMYRT_PACKET_SIZE	32

#define VDUMMYRT_NAME		"vdummyrt"
#define VDUMMYRT_PREFIX		"[" VDUMMYRT_NAME "] "

typedef struct {
	char buffer[VDUMMYRT_PACKET_SIZE];
} vdummyrt_request_t;

typedef struct  {
	char buffer[VDUMMYRT_PACKET_SIZE];
} vdummyrt_response_t;

/*
 * Generate vdummy ring structures and types.
 */
DEFINE_RING_TYPES(vdummyrt, vdummyrt_request_t, vdummyrt_response_t);

#endif /* VDUMMYRT_H */
