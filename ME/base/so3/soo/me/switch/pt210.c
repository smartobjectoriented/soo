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

#include <soo/enocean/pt210.h>
#include <soo/enocean/enocean.h>
#include <soo/debug.h>
#include <timer.h>

void pt210_reset(pt210_t *sw) {
    sw->up = false;
    sw->down = false;
    sw->released = false;
    sw->event = false;
}

void pt210_init(pt210_t *sw, uint32_t switch_id) {
    sw->id = switch_id;
    pt210_reset(sw);
}

void pt210_wait_event(pt210_t *sw) {
    enocean_telegram_t *tel;

    tel = enocean_get_data();
    if (!tel) {
        sw->event = false;
        return;
    }

    DBG("%s: got new enocean data: 0x%02X\n", __func__, tel->data[0]);

    if (tel->rorg != RPS)
        return;

    if (tel->sender_id.val == sw->id) {
        switch ((int)tel->data[0]) {
            case PT210_SWITCH_UP:
                sw->up = true;
                sw->press_time = NOW();
                break;
            case PT210_SWITCH_DOWN:
                sw->down = true;
                sw->press_time = NOW();
                break;
            case PT210_SWITCH_RELEASED:
                sw->released = true;
                sw->released_time = NOW();
                break;
            default:
                return;
        }

        sw->event = true;
    }
}