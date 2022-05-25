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

	vknx {
		compatible = "vknx,backend";
		status = "disabled";
	};

To activate add to bcm2711-<platform>.dts:
	
	&agency {
		backends {
			...

			vknx {
				status = "ok";
			};

			...
		};
	};

 */

#ifndef VKNX_H
#define VKNX_H

#include <soo/ring.h>
#include <soo/grant_table.h>
#include <soo/vdevback.h>
#include <linux/list.h>
#include <linux/mutex.h>

#define VKNX_NAME		                    "vknx"
#define VKNX_PREFIX		                    "[" VKNX_NAME " backend] "

#define VKNX_DATAPOINT_DATA_MAX_SIZE        14
#define VKNX_MAX_DATAPOINTS                 10
#define VKNX_RING_PAGES_NUMBER              8
#define VKNX_MIN_DOMIDS_NUMBER              10

typedef enum {
    KNX_RESPONSE = 0,
    KNX_INDICATION
} event_type;

typedef enum {
    GET_DP_VALUE = 0,
    SET_DP_VALUE
} request_type;

struct dp {
    uint16_t id;
    union {
        uint8_t state;
        uint8_t cmd;
    };
    uint8_t data_len;
    uint8_t data[VKNX_DATAPOINT_DATA_MAX_SIZE];
};
typedef struct dp dp_t;

typedef struct {
    request_type type;
    uint16_t dp_count;
    dp_t datapoints[VKNX_MAX_DATAPOINTS];
} vknx_request_t;

typedef struct  {
    event_type event;
    uint16_t dp_count;
    dp_t datapoints[VKNX_MAX_DATAPOINTS];

} vknx_response_t;

/*
 * Generate ring structures and types.
 */
DEFINE_RING_TYPES(vknx, vknx_request_t, vknx_response_t);

typedef struct {
    struct list_head list;
    int32_t id;
} domid_priv_t;

/*
 * General structure for this virtual device (backend side)
 */
typedef struct {

	/* Must be the first field */
	vdevback_t vdevback;

	vknx_back_ring_t ring;
	unsigned int irq;

    int32_t otherend_id;

    uint64_t spid;

} vknx_t;

#endif /* VKNX_H */
