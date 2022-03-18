/*
 * Copyright (C) 2022 Mattia Gallacchi <mattia.gallaccchi@heig-vd.ch>
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

#ifndef VWAGOLED_H
#define VWAGOLED_H

#include <soo/ring.h>
#include <soo/grant_table.h>
#include <soo/vdevfront.h>

#define WAGOLED_PACKET_SIZE	32

#define WAGOLED_NAME		"wagoled"
#define WAGOLED_PREFIX		"[" WAGOLED_NAME "] "

typedef struct {
	char buffer[WAGOLED_PACKET_SIZE];
} wagoled_request_t;

typedef struct  {
	char buffer[WAGOLED_PACKET_SIZE];
} wagoled_response_t;

/*
 * Generate ring structures and types.
 */
DEFINE_RING_TYPES(wagoled, wagoled_request_t, wagoled_response_t);

/*
 * General structure for this virtual device (frontend side)
 */

typedef struct {
	/* Must be the first field */
	vdevfront_t vdevfront;

	wagoled_front_ring_t ring;
	unsigned int irq;

	grant_ref_t ring_ref;
	grant_handle_t handle;
	uint32_t evtchn;

} wagoled_t;


#endif /* WAGOLED_H */
