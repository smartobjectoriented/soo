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

#ifndef VTEMP_H
#define VTEMP_H

#include <soo/ring.h>
#include <soo/grant_table.h>
#include <soo/vdevback.h>

#define VTEMP_PACKET_SIZE	32

#define VTEMP_NAME		"vtemp"
#define VTEMP_PREFIX		"[" VTEMP_NAME "] "

#define VTEMP_UART1_DEV "ttyS0"

#define TEMP_DATA_SIZE 8

#define TEMP_DEV_ID		1


typedef struct {
	/* EMPTY */
} vtemp_request_t;

typedef struct  {
	int temp;
	uint32_t dev_id;
} vtemp_response_t;

/*
 * Generate ring structures and types.
 */
DEFINE_RING_TYPES(vtemp, vtemp_request_t, vtemp_response_t);

/*
 * General structure for this virtual device (backend side)
 */

typedef struct {

	/* Must be the first field */
	vdevback_t vdevback;

	vtemp_back_ring_t ring;
	unsigned int irq;

} vtemp_t;

#endif /* VTEMP_H */
