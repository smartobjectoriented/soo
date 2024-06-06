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

#ifndef VVEXT_H
#define VVEXT_H

#include <soo/ring.h>
#include <soo/grant_table.h>
#include <soo/vdevfront.h>

#include <asm/mmu.h>

#define VVEXT_PACKET_SIZE	32

#define VVEXT_NAME		"vvext"
#define VVEXT_PREFIX		"[" VVEXT_NAME "] "

typedef struct {
	char buffer[VVEXT_PACKET_SIZE];
} vvext_request_t;

typedef struct  {
	//char buffer[VVEXT_PACKET_SIZE];
	uint16_t type;
	uint16_t code;
	int value;
} vvext_response_t;

/*
 * Generate blkif ring structures and types.
 */
DEFINE_RING_TYPES(vvext, vvext_request_t, vvext_response_t);

typedef struct {
	vdevfront_t vdevfront;

	vvext_front_ring_t ring;
	grant_ref_t ring_ref;
	grant_handle_t handle;
	uint32_t evtchn;
	uint32_t irq;

} vvext_t;


#endif /* VVEXT_H */
