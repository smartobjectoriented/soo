/*
 * Copyright (C) 2021 David Truan <david.truan@heig-vd.ch>
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

#ifndef CHAT_H
#define CHAT_H

#include <completion.h>
#include <spinlock.h>

#include <me/common.h>


#define MAX_MSG_LENGTH 		200 
#define ID_MAX_LENGTH		20
#define ACTION_MAX_LENGTH	20


#define CHAT_MODEL  "<model spid=\"00000200000000000000000000000004\">\
        <name>SOO.chat</name>\
        <description>\"SOO.chat permet de participer à un live chat entre Smart Objects\".</description>\
        <layout>\
            <row>\
                <col span=\"2\">\
                    <text>SOO.chat app</text>\
                </col>\
            </row>\
            <row>\
                <col span=\"8\">\
                    <scroll id=\"msg-history\"></scroll>\
                </col>\
            </row>\
            <row>\
                <col span=\"3\">\
                    <input id=\"text-edit\" ></input>\
                </col>\
                <col span=\"2\">\
                    <button id=\"button-send\" lockable=\"false\">\"Send\"</button>\
                </col>\
            </row>\
        </layout>\
    </model>"



/**
 * This is used to keep an history of the different 
 * messages we received from the MEs.
 * It stores the originUID, the message and the 
*/
typedef struct {
    uint64_t originUID;
    size_t stamp;
    char text[MAX_MSG_LENGTH];
} chat_entry_t;

typedef struct {
    struct list_head list;
    chat_entry_t chat_entry;
} chat_t;

/*
 * Never use lock (completion, spinlock, etc.) in the shared page since
 * the use of ldrex/strex instructions will fail with cache disabled.
 */
typedef struct {

    chat_entry_t cur_chat;

    bool has_a_new_msg;

	/* To determine if the ME needs to be propagated.
	 * If it is the same state, no need to be propagated.
	 */
	bool need_propagate;

    /* UID of the initiator SOO, on which this ME was deployed at boot time */
	uint64_t initiator;

	/*
	 * MUST BE the last field, since it contains a field at the end which is used
	 * as "payload" for a concatened list of hosts.
	 */
	me_common_t me_common;

} sh_chat_t;




/* Protecting variables between domcalls and the active context */
extern spinlock_t propagate_lock;

extern sh_chat_t *sh_chat;


/* Bellow are the functions used to manage the message history */
bool sender_is_in_history(uint64_t senderUID);
chat_entry_t *find_chat_in_history(uint64_t senderUID);
void add_chat_in_history(chat_entry_t *new_chat);
bool update_chat_in_history(chat_entry_t *updated_chat);
bool is_chat_in_history(chat_entry_t *chat);


void send_chat_to_tablet(uint64_t senderUID, char* text);

#endif /* CHAT_H */


