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


#define MAX_MSG_LENGTH 		200 
#define ID_MAX_LENGTH		20
#define ACTION_MAX_LENGTH	20

/* Contains the current chat message. */
char cur_text[MAX_MSG_LENGTH];




/**
 *
 * @param args - To be compliant... Actually not used.
 * @return
 */
void process_events(char * data, size_t size) {

	// printk("GOT AN EVENT TO PARSE\n");
	char id[ID_MAX_LENGTH];
	char action[ACTION_MAX_LENGTH];
	char content[MAX_MSG_LENGTH];
	char msg[MAX_MSG_LENGTH];

	memset(id, 0, ID_MAX_LENGTH);
	memset(action, 0, ACTION_MAX_LENGTH);
	memset(msg, 0, MAX_MSG_LENGTH);
	memset(content, 0, MAX_MSG_LENGTH);

	xml_parse_event(data, id, action);

	/* If it is a tex-edit event, it means the user typed something
	so we save it in the temporary buffer */
	if (!strcmp(id, TEXTEDIT_ID)) {

		xml_get_event_content(data, content);
		strncpy(cur_text, content, strlen(content)+1);
	} else if (!strcmp(id, BTN_SEND_ID) && !strcmp(action, "clickDown")) {
			
		/* We don't send empty text */	
		if (!strcmp(cur_text, "")) return;
		// TODO replace 0 by slotID 
		/* Pepare an send the chat message */
		xml_prepare_chat(msg, 0, cur_text);
		vuihandler_send(msg, strlen(msg)+1);
		
		/* Notify the text-edit widget that it must clear its text */
		memset(msg, 0, MAX_MSG_LENGTH);	
		xml_prepare_message(msg, TEXTEDIT_ID, "");
		vuihandler_send(msg, strlen(msg)+1);
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

	/* register our process_event callback to the vuihandler */ 
	vuihandler_register_callback(NULL, process_events);

	while(1);

	return 0;
}
