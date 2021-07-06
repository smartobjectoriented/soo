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

#ifndef VSENSEJ_H
#define VSENSEJ_H

#include <soo/ring.h>
#include <soo/grant_table.h>
#include <soo/vdevback.h>

#define VSENSEJ_PACKET_SIZE	32

#define VSENSEJ_NAME		"vsensej"
#define VSENSEJ_PREFIX		"[" VSENSEJ_NAME "] "

typedef struct {
	/* nothing */
} vsensej_request_t;

typedef struct  {

	uint16_t type;
	uint16_t code;
	int value;

} vsensej_response_t;

/*
 * Generate ring structures and types.
 */
DEFINE_RING_TYPES(vsensej, vsensej_request_t, vsensej_response_t);

/*
 * General structure for this virtual device (backend side)
 */

typedef struct {

	/* Must be the first field */
	vdevback_t vdevback;

	vsensej_back_ring_t ring;
	unsigned int irq;

} vsensej_t;

#endif /* VSENSEJ_H */
