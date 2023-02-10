/*
 * Copyright (C) 2023-2024 A.Gabriel Catel Torres <arzur.cateltorres@heig-vd.ch>
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

#ifndef IUOC_U_H
#define IUOC_U_H

/* Declaration for SOO.blind ME */ 
typedef enum {
	STORE_UP_FULL,
	STORE_DOWN_FULL,
	STORE_UP_STEP,
	STORE_DOWN_STEP,
	STORE_TILT
} soo_blind_action_t;

typedef struct {
	soo_blind_action_t action;
} soo_blind_data_t;

/* Declaration for SOO.switch ME */ 
typedef enum {
	SWITCH_PRESSED_SHORT,
	SWITCH_PRESSED_LONG,
	SWITCH_RELEASED
} soo_switch_action_t;

typedef struct {
	soo_switch_action_t action;
} soo_switch_data_t;


/* Global communication declaration for IUOC */
typedef enum {
	IUOC_ME_BLIND,
	IUOC_ME_OUTDOOR,
	IUOC_ME_WAGOLED,
	IUOC_ME_HEAT,
	IUOC_ME_SWITCH,
	IUOC_ME_END
} me_type_t;

typedef struct {
	me_type_t me_type;
	unsigned timestamp;
	union {
		soo_blind_data_t blind_data;
		soo_switch_data_t switch_data;
	} data;
} iuoc_data_t;

#define UIOC_IOCTL_SEND_DATA _IOW('a', 'a', iuoc_data_t *)
#define UIOC_IOCTL_RECV_DATA _IOR('a', 'b', iuoc_data_t *)
#define UIOC_IOCTL_TEST      _IOW('a', 'c', iuoc_data_t *)

#endif
