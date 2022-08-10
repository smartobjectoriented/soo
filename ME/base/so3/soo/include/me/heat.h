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

#define GET_TEMP_MS		            3000
#define VAL_STOP_GET_OUTDOOR_TEMP   10
#define TARGET_TEMP_DEFAULT         20

#define MAX_MSG_LENGTH 		100
#define ID_MAX_LENGTH		20
#define ACTION_MAX_LENGTH	20

#define HEAT_MODEL "<model spid=\"002000000000000a\">\
<name>SOO.heat</name>\
<description>\"Gestion électrovanne.\"</description>\
<layout>\
    <row>\
        <col span=\"3\"><text>Consigne chauffage C°</text></col>\
    </row>\
    <row>\
        <col span=\"2\"><input id=\"setpoint-temp\" ></input></col>\
        <col span=\"2\"><button id=\"button-save\" lockable=\"false\">\"Save\"</button></col>\
    </row>\
    <row>\
        <col span=\"2\"><text>Consigne</text></col>\
        <col span=\"2\"><text>C° intérieure</text></col>\
        <col span=\"2\"><text>C° extérieure</text></col>\
        <col span=\"2\"><text>Etat valve</text></col>\
    </row>\
    <row>\
        <col span=\"2\"><text id=\"target-temp\" ></text></col>\
        <col span=\"2\"><text id=\"indoor-temp\" ></text></col>\
        <col span=\"2\"><text id=\"outdoor-temp\" ></text></col>\
        <col span=\"2\"><text id=\"status-valve\" ></text></col>\
    </row>\
</layout>\
</model>"


typedef struct {
	uint32_t    id;
    bool 		event;
    char 		temperatureOutdoor;
	char		temperatureIndoor;
    char        targetTemp;
} heat_t;

typedef struct {
    heat_t 		heat;
	int 		checkNoOutdoorTemp;
	bool 		isNewOutdoorTemp;
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

void send_temp_to_tablet(char *temp, char *id);

#endif /* HEAT_H */
