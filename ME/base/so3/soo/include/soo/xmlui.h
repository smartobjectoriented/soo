/*
 * Copyright (C) 2021 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef XMLUI_H
#define XMLUI_H


/* SOO.chat widgets id */
/* ID for the different models */
#define TEXTEDIT_ID		"text-edit"
#define SCROLL_ID		"msg-history"
#define BTN_SEND_ID		"button-send"

/* SOO.heat widgets id */
#define SETPOINT_TEMP_ID    "setpoint-temp"
#define BTN_SAVE_ID         "button-save"
#define INDOOR_TEMP         "indoor-temp"
#define OUTDOOR_TEMP        "outdoor-temp"
#define STATUS_VALVE        "status-valve"
#define TARGET_TEMP         "target-temp"


/*
 * Prepare a XML message.
 * Allocation of the message has to be done by the caller
 */
void xml_prepare_message(char *buffer, char *id, char *value);

/*
 * Retrieve the action from an XML event message.
 * Allocation of the buffers has to be done by the caller
 */
void xml_parse_event(char *buffer, char *id, char *action);


/**
 * Retrieve the content of an event 
 * 
 * Allocation of the content buffer has to be done by the caller
 */
void xml_get_event_content(char *buffer, char *content);

void xml_prepare_chat(char *buffer, int sender, char *text);

#endif /* XMLUI_H */
