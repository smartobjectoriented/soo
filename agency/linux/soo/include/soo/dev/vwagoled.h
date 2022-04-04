/*
 * Copyright (C) 2022 Mattia Gallacchi <mattia.gallacchi@heig-vd.ch>
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

/* ============================== DTS =================================== */
/*
soo.dtsi:

	vwagoled {
		compatible = "vwagoled,backend";
		status = "disabled";
	};

To activate add to bcm2711-<platform>.dts:
	
	&agency {
		backends {
			...

			vwagoled {
				status = "ok";
			};

			...
		};
	};

 */

#ifndef VWAGOLED_H
#define VWAGOLED_H

#include <soo/ring.h>
#include <soo/grant_table.h>
#include <soo/vdevback.h>

#define VWAGOLED_PACKET_SIZE	64

#define VWAGOLED_NAME		"vwagoled"
#define VWAGOLED_PREFIX		"[" VWAGOLED_NAME " backend] "


typedef struct {
	uint8_t cmd;
	uint8_t dim_value;
	int ids[VWAGOLED_PACKET_SIZE];
	uint8_t ids_count;
} vwagoled_request_t;

typedef struct  {
	uint8_t cmd;
	uint8_t dim_value;
	int ids[VWAGOLED_PACKET_SIZE];
	uint8_t ids_count;
} vwagoled_response_t;

/*
 * Generate ring structures and types.
 */
DEFINE_RING_TYPES(vwagoled, vwagoled_request_t, vwagoled_response_t);

/*
 * General structure for this virtual device (backend side)
 */

typedef struct {

	/* Must be the first field */
	vdevback_t vdevback;

	vwagoled_back_ring_t ring;
	unsigned int irq;

} vwagoled_t;

#endif /* VWAGOLED_H */
