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

#ifndef _PT210_H_
#define _PT210_H_ 

#include <types.h>
#include <timer.h>
#include <completion.h>
#include <thread.h>
#include <asm/atomic.h>

#define PT210_SWITCH_UP             0x70
#define PT210_SWITCH_DOWN           0x50
#define PT210_SWITCH_RELEASED       0x00
#define PT210_PRESSED_TIME_MS       300

typedef unsigned char byte;

/**
 * @brief PT210 enocean switch struct
 * 
 * @param id Enocean unique ID
 * @param up Switch up pressed
 * @param down Switch down pressed
 * @param released Switch released
 * @param event Switch event. One of the above is set to true
 * 
 */
typedef struct {
    uint32_t id;
    atomic_t up;
    atomic_t down;
    atomic_t released;
    atomic_t event;

    // private
    timer_t _pressed_time;
    struct completion _wait_event;
    tcb_t *_wait_event_th;
    atomic_t _th_run;

} pt210_t;


/**
 * @brief Initialize PT210 struct members
 * 
 * @param sw PT210 switch to initialize
 * @param switch_id Enocean id. Read on the back of the device
 */
void pt210_init(pt210_t *sw, uint32_t switch_id);


void pt210_deinit(pt210_t *sw);
/**
 * @brief Wait for an event coming from the PT210 device. This call is blocking.
 * 
 * @param sw switch to wait for.
 */
void pt210_wait_event(pt210_t *sw);

/**
 * @brief Reset switch values. Not the id. After an event is received and treated this function 
 * must be called. 
 * 
 * @param sw switch to reset the values of
 */
void pt210_reset(pt210_t *sw);

#endif //_PT210_H_