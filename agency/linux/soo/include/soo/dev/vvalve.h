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

#ifndef VVALVE_H
#define VVALVE_H

#include <soo/ring.h>
#include <soo/grant_table.h>
#include <soo/vdevback.h>

#define VVALVE_PACKET_SIZE	32

#define VVALVE_NAME		"vvalve"
#define VVALVE_PREFIX		"[" VVALVE_NAME "] "

#define VVALVE_UART1_DEV "ttyS0"

#define CMD_DATA_SIZE 8

#define DEV_ID_BLOCK_SIZE 4
#define DEV_TYPE_BLOCK_SIZE 1


typedef struct {
	int temp;
	uint32_t dev_id;
	uint8_t dev_type;
	char cmd_valve[CMD_DATA_SIZE+2];
} vvalve_data_t;

typedef struct {
	char buffer[CMD_DATA_SIZE+2];
} vvalve_request_t;

typedef struct  {
	/* EMPTY */
} vvalve_response_t;

/*
 * Generate ring structures and types.
 */
DEFINE_RING_TYPES(vvalve, vvalve_request_t, vvalve_response_t);

/*
 * General structure for this virtual device (backend side)
 */

typedef struct {

	/* Must be the first field */
	vdevback_t vdevback;

	vvalve_back_ring_t ring;
	unsigned int irq;

} vvalve_t;

#endif /* VVALVE_H */
