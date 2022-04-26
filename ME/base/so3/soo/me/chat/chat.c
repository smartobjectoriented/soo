/*
 * Copyright (C) 2016-2020 Daniel Rossier <daniel.rossier@soo.tech>
 * Copyright (C) 2016-2019 Baptiste Delporte <bonel@bonel.net>
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

#if 0
#define DEBUG
#endif

#include <mutex.h>
#include <delay.h>
#include <timer.h>
#include <heap.h>
#include <memory.h>

#include <soo/avz.h>
#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/soo.h>
#include <soo/console.h>
#include <soo/debug.h>
#include <soo/debug/dbgvar.h>
#include <soo/debug/logbool.h>
#include <soo/evtchn.h>

#include <soo/dev/vuihandler.h>

#include <me/ledctrl.h>

#include <soo/xmlui.h>


#define MAX_MSG_LENGTH 200 

/* Contains the current chat message. */
char cur_text[MAX_MSG_LENGTH];

/**
 *
 * @param args - To be compliant... Actually not used.
 * @return
 */
void process_events(char * data, size_t size) {

	// printk("GOT AN EVENT TO PARSE\n");
	char id[50];
	char value[50];
	char content[MAX_MSG_LENGTH];

	memset(id, 0, 50);
	memset(value, 0, 50);

	xml_parse_event(data, id, value);

	printk("Event: ID: %s, Value: %s\n", id, value);


	if (!strcmp(id, "text-edit")) {
		xml_get_event_content(data, content);
		strncpy(cur_text, value, strlen(value)+1);
	} else if (!strcmp(id, "button-send")) {
		printk("WE ARE SENDING THE MSG: %s\n", cur_text);
	}

}

/*
 * The main application of the ME is executed right after the bootstrap. It may be empty since activities can be triggered
 * by external events based on frontend activities.
 */
int app_thread_main(void *args) {

	/* The ME can cooperate with the others. */
	spad_enable_cooperate();

	printk("Enjoy the SOO.chat ME !\n");

	vuihandler_register_callback(NULL, process_events);

	while(1);

	return 0;
}
