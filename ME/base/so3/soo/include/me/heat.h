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

#ifndef HEAT_H
#define HEAT_H

#include <me/common.h>
#include <completion.h>

#include <me/outdoor.h>

#define MEHEAT_NAME		"ME heat"
#define MEHEAT_PREFIX	    "[ " MEHEAT_NAME " ] "

typedef struct {
	uint32_t    id;
    bool 		event;
    char 		temperature;
} heat_t;

typedef struct {
    heat_t 		heat;
	bool 		heat_event;
	bool 		need_propagate;
	bool 		delivered;
	uint64_t	timestamp;
    uint64_t 	originUID;
	/*
	 * MUST BE the last field, since it contains a field at the end which is used
	 * as "payload" for a concatened list of hosts.
	 */
	me_common_t me_common;
} sh_heat_t;

/* Export the reference to the shared content structure */
extern sh_heat_t *sh_heat;

extern struct completion send_data_lock;

extern atomic_t shutdown;

#endif /* HEAT_H */
