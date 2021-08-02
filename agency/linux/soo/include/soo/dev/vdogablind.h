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

#ifndef VDOGABLIND_H
#define VDOGABLIND_H

#include <soo/ring.h>
#include <soo/grant_table.h>
#include <soo/vdevback.h>

#define VDOGABLIND_PACKET_SIZE	32

#define VDOGABLIND_NAME		"vdogablind"
#define VDOGABLIND_PREFIX		"[" VDOGABLIND_NAME "] "

typedef struct {
	char buffer[VDOGABLIND_PACKET_SIZE];
} vdogablind_request_t;

typedef struct  {
	char buffer[VDOGABLIND_PACKET_SIZE];
} vdogablind_response_t;

/*
 * Generate ring structures and types.
 */
DEFINE_RING_TYPES(vdogablind, vdogablind_request_t, vdogablind_response_t);

/*
 * General structure for this virtual device (backend side)
 */

typedef struct {

	/* Must be the first field */
	vdevback_t vdevback;

	vdogablind_back_ring_t ring;
	unsigned int irq;

} vdogablind_t;


/* these functions can be called by other drivers
   to control the lahoco blind (FOR TEST ONLY)*/
void vdoga_motor_enable(void);
void vdoga_motor_disable(void);
void vdoga_motor_set_percentage_speed(int speed);
void vdoga_motor_set_direction(int dir);


#endif /* VDOGABLIND_H */
