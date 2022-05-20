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

#ifndef VKNX_H
#define VKNX_H

#include <soo/ring.h>
#include <soo/grant_table.h>
#include <soo/vdevfront.h>


#define VKNX_NAME		                    "vknx"
#define VKNX_PREFIX		                    "[" VKNX_NAME " frontend] "

#define VKNX_DATAPOINT_DATA_MAX_SIZE        14
#define VKNX_MAX_DATAPOINTS                 10
#define VKNX_RING_PAGES_NUMBER              8


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

/*
 * General structure for this virtual device (frontend side)
 */

typedef struct {
	/* Must be the first field */
	vdevfront_t vdevfront;

	vknx_front_ring_t ring;
	unsigned int irq;

	grant_ref_t ring_ref;
	grant_handle_t handle;
	uint32_t evtchn;

} vknx_t;

int get_knx_data(vknx_response_t *data);
void vknx_set_dp_value(dp_t* dps, int dp_count);
void vknx_get_dp_value(uint16_t first_dp, int dp_count);
void vknx_print_dps(dp_t *dps, int dp_count);

#endif /* BLIND_H */
