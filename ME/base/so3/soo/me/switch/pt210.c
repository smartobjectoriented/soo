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

#if 1
#define DEBUG
#endif

#include <soo/enocean/pt210.h>
#include <soo/enocean/enocean.h>
#include <soo/debug.h>
#include <heap.h>

void pressed_time_cb(void *args) {
    pt210_t *sw = (pt210_t*)args;

    DBG("Timer event\n");

    complete(&sw->_wait_event);
}

int wait_event_th(void *args) {
    pt210_t *sw = (pt210_t*)args;
    enocean_telegram_t *tel;

    while(atomic_read(&sw->_th_run)) {
        tel = enocean_get_data();
        if (!tel)
            continue;

        if (tel->rorg != RPS) {
            free(tel);
            continue;
        }

        DBG("%s: got new enocean data: 0x%02X\n", __func__, tel->data[0]);

        if (tel->sender_id.val == sw->id) {
            switch ((int)tel->data[0]) {
                case PT210_SWITCH_UP:
                    atomic_set(&sw->up, 1);
                    break;
                case PT210_SWITCH_DOWN:
                    atomic_set(&sw->down, 1);
                    break;
                case PT210_SWITCH_RELEASED:
                    atomic_set(&sw->released, 1);
                    complete(&sw->_wait_event);
                    stop_timer(&sw->_pressed_time);
                    continue;
                default:
                    continue;
            }
            atomic_set(&sw->event, 1);
            set_timer(&sw->_pressed_time, NOW() + MILLISECS(PT210_PRESSED_TIME_MS));
        } 

        free(tel);
    }

    return 0;
}

void pt210_deinit(pt210_t *sw) {
    atomic_set(&sw->_th_run, 0);
    thread_join(sw->_wait_event_th);
}

void pt210_reset(pt210_t *sw) {
    atomic_set(&sw->up, 0);
    atomic_set(&sw->down, 0);
    atomic_set(&sw->released, 0);
    atomic_set(&sw->event, 0);
}

void pt210_init(pt210_t *sw, uint32_t switch_id) {
    sw->id = switch_id;
    pt210_reset(sw);

    init_timer(&sw->_pressed_time, pressed_time_cb, sw);
    init_completion(&sw->_wait_event);

    atomic_set(&sw->_th_run, 1);
    sw->_wait_event_th = kernel_thread(wait_event_th, "wait_event_th", sw, THREAD_PRIO_DEFAULT);
}

// void pt210_wait_event() {
        
//     /** Second case either we get an release or the timer expires **/
//     wait_for_completion(&sw->_wait_event);
// }