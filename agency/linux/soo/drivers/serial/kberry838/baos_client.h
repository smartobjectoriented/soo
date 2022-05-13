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

#ifndef _LINUX_BAOS_CLIENT_H
#define _LINUX_BAOS_CLIENT_H

#include <linux/types.h>
#include <linux/list.h>

#include <soo/device/kberry838.h>

#define BAOS_MAIN_SERVICE                           0xF0
#define BAOS_START_ITEM_SIZE                        0x02
#define BAOS_COUNT_ITEM_SIZE                        0x02
#define BAOS_ITEM_ID_SIZE                           0x02
#define BAOS_FRAME_MIN_SIZE                         0x06

/** BAOS Frame offsets **/
#define BAOS_MAIN_SERVICE_OFF                       0x00
#define BAOS_SUBSERVICE_OFF                         0x01
#define BAOS_START_ITEM_OFF                         0x02
#define BAOS_ITEM_COUNT_OFF                         0x04
#define BAOS_FIRST_ITEM_OFF                         0x06

/** BAOS subservice codes **/
#define BAOS_SUBSERVICE_GET_SERVER_ITEM             0x01

/** BAOS items id **/
#define BAOS_ID_SERIAL_NUM                          0x08                              

typedef union {
    u_int16_t val;
    struct {
        u_int8_t lsb;
        u_int8_t msb;
    } bytes;
} item_id_t;

typedef union {
    u_int16_t val;
    struct {
        u_int8_t lsb;
        u_int8_t msb;
    } bytes;
} item_count_t;

struct baos_item {
    item_id_t id;
    byte *data;
    byte length;
};

struct baos_frame {
    byte subservice;
    item_id_t first_item;
    item_count_t item_count;

    struct list_head items;
    byte length;
};
typedef struct baos_frame baos_frame_t;

void baos_get_server_item(u_int16_t first_item, u_int16_t item_count);

#endif