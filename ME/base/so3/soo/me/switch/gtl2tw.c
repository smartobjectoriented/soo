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

#include <string.h>

#include <soo/knx/gtl2tw.h>
#include <soo/dev/vknx.h>

enum {
    GTL2TW_UP_LEFT = FIRST_DP_ID,
    GTL2TW_DOWN_LEFT,
    GTL2TW_UP_RIGHT,
    GTL2TW_DOWN_RIGHT
} ids;

enum {
    OFF = 0x00,
    ON
} sts;

void gtl2tw_init(gtl2tw_t *sw) {
    memset(sw->status, 0, DP_COUNT);
    memset(sw->events, 0, DP_COUNT);
}

void gtl2tw_wait_event(gtl2tw_t *sw) {
    vknx_response_t data;
    int i;

    memset(sw->events, 0, DP_COUNT);

    if (get_knx_data(&data) < 0) {
		DBG("Failed to get knx data\n");
		return;
	} 

	DBG("Got new knx data. Type:\n");

	switch (data.event)
	{
		case KNX_RESPONSE:
			DBG("KNX response\n");
			break;
		
		case KNX_INDICATION:
			DBG("KNX indication\n");
			
            for (i = 0; i < data.dp_count; i++) {
                switch(data.datapoints[i].id) {
                    case GTL2TW_UP_LEFT:
                        DBG("UP left\n");
                        if (sw->status[0] != data.datapoints[i].data[0]) {
                            sw->events[0] = true;
                            sw->status[0] = sw->status[0] == ON ? OFF : ON;
                        }
                        break;
                    case GTL2TW_DOWN_LEFT:
                        DBG("DOWN left\n");
                        if (sw->status[1] != data.datapoints[i].data[0]) {
                            sw->events[1] = true;
                            sw->status[1] = sw->status[1] == ON ? OFF : ON;
                        }
                        break;

                    case GTL2TW_UP_RIGHT:
                        DBG("UP right\n");

                        if (sw->status[2] != data.datapoints[i].data[0]) {
                            sw->events[2] = true;
                            sw->status[2] = sw->status[2] == ON ? OFF : ON;
                        }
                        break;
                    case GTL2TW_DOWN_RIGHT:
                        DBG("DOWN right\n");
                        if (sw->status[3] != data.datapoints[i].data[0]) {
                            sw->events[3] = true;
                            sw->status[3] = sw->status[3] == ON ? OFF : ON;
                        }
                        break;
                }
            }

			break;
	}
}