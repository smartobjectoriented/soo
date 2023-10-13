/*
 * Copyright (C) 2016-2018 Baptiste Delporte <bonel@bonel.net>
 * Copyright (C) 2018-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

 	venocean {
		compatible = "venocean,backend";
		status = "disabled";
	};
 
 To activate add to bcm2711-<platform>.dts:
	
	&agency {
		backends {
			...

			venocean {
				status = "ok";
			};

			...
		};
	};
 
 */


#ifndef VENOCEAN_H
#define VENOCEAN_H

#include <soo/ring.h>
#include <soo/grant_table.h>

#include <linux/vt_kern.h>

#define VENOCEAN_NAME	"venocean"
#define VENOCEAN_PREFIX	" [" VENOCEAN_NAME " backend] "

/*** For now it should be enough ***/
#define BUFFER_SIZE		256

typedef struct {
	unsigned char buffer[BUFFER_SIZE];
	/* Buffer len */
	int len;
} venocean_request_t;

typedef struct {
	unsigned char buffer[BUFFER_SIZE];
	int len;
} venocean_response_t;

DEFINE_RING_TYPES(venocean, venocean_request_t, venocean_response_t);

typedef struct {
    struct list_head list;
    int32_t id;
} domid_priv_t;

/*
 * General structure for this virtual device (backend side)
 */

typedef struct {
	vdevback_t vdevback;

	spinlock_t ring_lock;
	venocean_back_ring_t ring;
	unsigned int irq;

} venocean_t;

#endif /* VENOCEAN_H */
