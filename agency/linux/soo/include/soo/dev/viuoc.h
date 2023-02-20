/*
 * Copyright (C) 2023 A.Gabriel Catel Torres <arzur.cateltorres@heig-vd.ch>
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

#ifndef VIUOC_H
#define VIUOC_H

#include <soo/ring.h>
#include <soo/grant_table.h>
#include <soo/vdevback.h>
#include <soo/uapi/iuoc.h>


#define VIUOC_NAME			"viuoc"
#define VIUOC_PREFIX		"[" VIUOC_NAME " backend] "
#define NB_DATA_MAX 			15

// /* Global communication declaration for IUOC */
// /* This needs to match the declaration done in the frontend*/
// typedef enum {
// 	IUOC_ME_BLIND,
// 	IUOC_ME_OUTDOOR,
// 	IUOC_ME_WAGOLED,
// 	IUOC_ME_HEAT,
// 	IUOC_ME_SWITCH,
// 	IUOC_ME_END
// } me_type_t;

// typedef struct {
// 	char name[30];
// 	char type[30];
// 	int value;
// } field_data_t;

typedef struct {
	iuoc_data_t me_data;
} viuoc_request_t;

typedef struct  {
	iuoc_data_t me_data;
} viuoc_response_t;

/*
 * Generate ring structures and types.
 */
DEFINE_RING_TYPES(viuoc, viuoc_request_t, viuoc_response_t);

/*
 * General structure for this virtual device (backend side)
 */
typedef struct {

	/* Must be the first field */
	vdevback_t vdevback;

	viuoc_back_ring_t ring;
	unsigned int irq;

} viuoc_t;

#endif /* VIUOC_H */
