/*
 * Copyright (C) 2018-2019 Thomas Rieder <thomas.rieder@heig-vd.ch>
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


#ifndef INDOOR_H
#define INDOOR_H

#include <completion.h>
#include <spinlock.h>
#include <printk.h>
#include <completion.h>
#include <asm/atomic.h>

#include <me/common.h>

#define TEMPSENSOR_NAME		"ME indoor"
#define TEMPSENSOR_PREFIX	    "[ " TEMPSENSOR_NAME " ] "

#define TEMPSENSOR_INDOOR_ID    1

typedef struct {
    int indoorTemp;
    int id;
    bool need_propagate;
} tempSensor_t;

typedef struct {
	tempSensor_t ts;
	bool tempSensor_event;
	bool need_propagate;
	bool delivered;
	uint64_t timestamp;
    uint64_t originUID;
	/*
	 * MUST BE the last field, since it contains a field at the end which is used
	 * as "payload" for a concatened list of hosts.
	 */
	me_common_t me_common;
} sh_tempSensor_t;

void *app_thread_main(void *args);

extern sh_tempSensor_t *sh_weatherstation;

extern spinlock_t propagate_lock;

extern struct completion send_data_lock;

extern atomic_t shutdown;


#endif /* INDOOR_H */
