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

#if 0
#define DEBUG
#endif

#if 1
#define BLIND_RELEASED_MODE
#endif

#include <soo/vbwa88pg.h>
#include <timer.h>
#include <soo/debug.h>

#define MS_DELAY    500

void vbwa88pg_blind_init(blind_vbwa88pg_t *blind, uint16_t first_dp_id) {
    int i;

    blind->blind_id = first_dp_id;

    for (i = 0; i < DATAPOINT_COUNT; i++) {
        blind->dps[i].id = first_dp_id + i;
        blind->dps[i].cmd = SET_NEW_VALUE_AND_SEND_ON_BUS;
        blind->dps[i].data_len = 1;
        blind->dps[i].data[0] = 0x00;
    }

    // vknx_get_dp_value(blind->blind_id, DATAPOINT_COUNT);
}

void vbwa88pg_blind_update(blind_vbwa88pg_t *blind, dp_t *dps, int dp_count) {
    int i, j;

    for (i = 0; i < dp_count; i++) {
        for (j = 0; j < DATAPOINT_COUNT; j++) {
            if (dps[i].id == blind->dps[j].id) {
                memcpy(blind->dps[j].data, dps[i].data, VKNX_DATAPOINT_DATA_MAX_SIZE);
            } 
        }
    }
#ifdef DEBUG
    vknx_print_dps(dps, dp_count);
#endif
}

/**
 * @brief Effectively send data to KNX frontend. Move up or down
 * 
 * @param blind Blind to control
 */
void vbwa88pg_blind_up_down(blind_vbwa88pg_t *blind) {
    dp_t dps[1];
    dps[0] = blind->dps[UP_DOWN];
    vknx_set_dp_value(dps, 1);
}

/**
 * @brief Effectively send data to KNX frontend. Increase or decrease by one step or stop the blind
 * 
 * @param blind Blind to control
 */
void vbwa88pg_blind_inc_dec_stop(blind_vbwa88pg_t *blind) {
    dp_t dps[1];
    dps[0] = blind->dps[INC_DEC_STOP];
    vknx_set_dp_value(dps, 1);
}

#if 0
void vbwa88pg_blind_up(blind_vbwa88pg_t *blind) {
#ifdef BLIND_RELEASED_MODE
    blind->time = NOW();
    blind->prev_cmd = BLIND_UP;
#else
    dp_t dps[1];

    if (blind->dps[UP_DOWN].data[0] != BLIND_UP) {
        blind->dps[UP_DOWN].data[0] = BLIND_UP;
        dps[0] = blind->dps[UP_DOWN];
        vknx_set_dp_value(dps, 1);
    } else {
        blind->dps[INC_DEC_STOP].data[0] = BLIND_STOP;
        dps[0] = blind->dps[INC_DEC_STOP];
        vknx_set_dp_value(dps, 1);
    }
#endif
}

void vbwa88pg_blind_down(blind_vbwa88pg_t *blind) {
#ifdef BLIND_RELEASED_MODE
    blind->time = NOW();
    blind->prev_cmd = BLIND_DOWN;
#else
    dp_t dps[1];
    
    if (blind->dps[UP_DOWN].data[0] != BLIND_DOWN) {
        blind->dps[UP_DOWN].data[0] = BLIND_DOWN;
        dps[0] = blind->dps[UP_DOWN];
        vknx_set_dp_value(dps, 1);
    } else {
        blind->dps[INC_DEC_STOP].data[0] = BLIND_STOP;
        dps[0] = blind->dps[INC_DEC_STOP];
        vknx_set_dp_value(dps, 1);
    }
#endif
}

void vbwa88pg_blind_stop_step(blind_vbwa88pg_t *blind) {
#ifdef BLIND_RELEASED_MODE
    uint64_t delay_ms = NS_TO_MS(NOW() - blind->time);

    DBG("delay %llu ms\n", delay_ms);

    if (delay_ms > MS_DELAY) {
        blind->dps[UP_DOWN].data[0] = blind->prev_cmd;
        vbwa88pg_send_data_up_down(blind);
    } else {
        if (blind->prev_cmd == BLIND_UP)
            blind->dps[INC_DEC_STOP].data[0] = BLIND_INC;
        else if (blind->prev_cmd == BLIND_DOWN)
            blind->dps[INC_DEC_STOP].data[0] = BLIND_DEC;
        else
            return;     
        vbwa88pg_send_data_inc_dec_stop(blind);
    }
#else
    return;
#endif
}
#endif