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

#ifndef _VBWA88PG_H_
#define _VBWA88PG_H_

#include <soo/dev/vknx.h>

#define DATAPOINT_COUNT     0x06

#define BLIND_UP            0x00
#define BLIND_DOWN          0x01

#define BLIND_STOP          0x00

/**
 * @brief BAOS datapoint command codes. See datasheet
 * 
 */
typedef enum {
    NO_COMMAND = 0b0000,
    SET_NEW_VALUE = 0b0001,
    SEND_VALUE_ON_BUS = 0b0010,
    SET_NEW_VALUE_AND_SEND_ON_BUS = 0b0011,
    READ_NEW_VALUE_VIA_BUS = 0b0100,
    CLEAR_DATA_POINT_TRANSMISSION_STATE = 0b0101
} datapoint_commands;

struct blind_vbwa88pg {
    dp_t up_down;
    dp_t inc_dec_stop;
    dp_t blind_set_pos;
    dp_t slat_set_pos;
    dp_t blind_get_pos;
    dp_t slat_get_pos;
};
typedef struct blind_vbwa88pg blind_vbwa88pg_t;

void vbwa88pg_blind_init(blind_vbwa88pg_t *blind);
void vbwa88pg_blind_update(dp_t *dps, int dp_count);
void vbwa88pg_blind_up(blind_vbwa88pg_t *blind);
void vbwa88pg_blind_down(blind_vbwa88pg_t *blind);

#endif // _VBWA88PG_H_