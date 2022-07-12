/*
 * Copyright (C) 2018-2019 Baptiste Delporte <bonel@bonel.net>
 * Copyright (C) 2018-2019 David Truan <david.truan@heig-vd.ch>
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

#ifndef OUTDOOR_H
#define OUTDOOR_H

#include <completion.h>
#include <spinlock.h>
#include <printk.h>
#include <completion.h>
#include <asm/atomic.h>

#include <me/common.h>
#include <soo/enocean/p04.h>

#define MEWEATHERSTATION_NAME		"ME outdoor"
#define MEWEATHERSTATION_PREFIX	    "[ " MEWEATHERSTATION_NAME " ] "

#define ENOCEAN_OUTDOOR_ID 0x018C8B75


typedef struct {
	p04_t ws;
} weatherstation_t;

typedef enum {
	P04 = 0
} weatherstation_type;

typedef struct {
	weatherstation_t ws;
	bool weatherstation_event;
	bool need_propagate;
	bool delivered;
	uint64_t timestamp;
    uint64_t originUID;
	weatherstation_type type;
	/*
	 * MUST BE the last field, since it contains a field at the end which is used
	 * as "payload" for a concatened list of hosts.
	 */
	me_common_t me_common;
} sh_weatherstation_t;

void weatherstation_init(weatherstation_t *ws);

void weatherstation_get_data(weatherstation_t *ws);

void *weatherstation_wait_data_th(void *args);

void *app_thread_main(void *args);

extern sh_weatherstation_t *sh_weatherstation;

extern spinlock_t propagate_lock;

extern struct completion send_data_lock;

extern atomic_t shutdown;

#endif /* OUTDOOR_H */
