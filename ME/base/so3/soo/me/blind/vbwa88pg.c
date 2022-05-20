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

#include "vbwa88pg.h"

void vbwa88pg_blind_init(blind_vbwa88pg_t *blind) {
    vknx_get_dp_value(blind->up_down.id, 6);
}

void vbwa88pg_blind_update(dp_t *dps, int dp_count) {
    vknx_print_dps(dps, dp_count);
}

void vbwa88pg_blind_up(blind_vbwa88pg_t *blind) {
    dp_t dps[1];
    if (blind->up_down.data[0] != BLIND_UP) {
        blind->up_down.data[0] = BLIND_UP;
        dps[0] = blind->up_down;
        vknx_set_dp_value(dps, 1);
    } else {
        blind->inc_dec_stop.data[0] = BLIND_STOP;
        dps[0] = blind->inc_dec_stop;
        vknx_set_dp_value(dps, 1);
    }
}
void vbwa88pg_blind_down(blind_vbwa88pg_t *blind) {
    dp_t dps[1];
    
    if (blind->up_down.data[0] != BLIND_DOWN) {
        blind->up_down.data[0] = BLIND_DOWN;
        dps[0] = blind->up_down;
        vknx_set_dp_value(dps, 1);
    } else {
        blind->inc_dec_stop.data[0] = BLIND_STOP;
        dps[0] = blind->inc_dec_stop;
        vknx_set_dp_value(dps, 1);
    }
}