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

#ifndef _ENOCEAN_H_
#define _ENOCEAN_H_

#include <types.h>

#define ENOCEAN_SENDER_ID_SIZE      0x04
#define ENOCEAN_MAX_DATA_SIZE       14

#define ENOCEAN_TELEGRAM_RORG_OFF   0x00
#define ENOCEAN_TELEGRAM_DATA_OFF   0x01

#define ENOCEAN_RSP_DATA_SIZE       0x01
#define ENOCEAN_1BS_DATA_SIZE       0x01



/** Enocean Radio telegram types, 
 * see https://www.enocean-alliance.org/wp-content/uploads/2020/07/EnOcean-Equipment-Profiles-3-1.pdf
 */
typedef enum {
    RPS = 0xF6,
    BS_1 = 0xD5
} RORG; 

typedef unsigned char byte;

struct sender_id {
    union
    {
        uint32_t val;
        byte bytes[ENOCEAN_SENDER_ID_SIZE];
    };
    
};
typedef struct sender_id sender_id_t;

struct enocean_telegram {
    RORG rorg;
    int data_len;
    byte data[ENOCEAN_MAX_DATA_SIZE];
    sender_id_t sender_id;
    byte status;
};
typedef struct enocean_telegram enocean_telegram_t;

enocean_telegram_t * enocean_buffer_to_telegram(byte *buf, int len);
void enocean_print_telegram(enocean_telegram_t *tel);

#endif
