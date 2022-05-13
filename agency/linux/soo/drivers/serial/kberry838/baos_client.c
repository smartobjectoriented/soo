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

#include <linux/slab.h>

#include "baos_client.h"

static byte *baos_flatten(baos_frame_t frame) {
    byte *buf;
    int buf_len = frame.length;
    int i;

    buf = kzalloc(buf_len * sizeof(byte), GFP_KERNEL);
    BUG_ON(!buf);

    buf[BAOS_MAIN_SERVICE_OFF] = BAOS_MAIN_SERVICE;
    buf[BAOS_SUBSERVICE_OFF] = frame.subservice;
    buf[BAOS_START_ITEM_OFF] = frame.first_item.bytes.msb;
    buf[BAOS_START_ITEM_OFF + 1] = frame.first_item.bytes.lsb;
    buf[BAOS_ITEM_COUNT_OFF] = frame.item_count.bytes.msb;
    buf[BAOS_ITEM_COUNT_OFF + 1] = frame.item_count.bytes.lsb;

    if (buf_len > BAOS_FRAME_MIN_SIZE) {
        // TODO read item list
    }

    return buf;
}

void baos_get_server_item(u_int16_t first_item_id, u_int16_t item_count) {
    baos_frame_t frame;
    byte *data;

    frame.subservice = BAOS_SUBSERVICE_GET_SERVER_ITEM;
    frame.first_item.val = first_item_id;
    frame.item_count.val = item_count;
    frame.length = BAOS_FRAME_MIN_SIZE;

    data = baos_flatten(frame);

    kberry838_send_data(data, frame.length);
}