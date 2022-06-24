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

#ifndef _P04_H_
#define _P04_H_ 

#include <types.h>

typedef unsigned char byte;

/**
 * @brief 
 * 
 */
typedef struct {
    uint32_t id;
    byte dawnSensor;    // 8 bits
    byte outdoorTemp;   // 8 bits
    byte windSpeed;     // 8 bits
    byte identifier;    // 4 bits
    bool LRNBit;
    bool day0_night1;
    bool rain;
} p04_t;

/**
 * @brief 
 * 
 * @param sw 
 * @param switch_id 
 */
void p04_init(p04_t *ws, uint32_t weatherstation_id);

/**
 * @brief 
 * 
 * @param ws 
 */
void p04_wait_event(p04_t *ws);

/**
 * @brief 
 * 
 * @param ws 
 */
void p04_reset(p04_t *ws);

#endif //_P04_H_