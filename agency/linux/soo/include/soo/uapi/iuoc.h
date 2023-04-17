/*
 * Copyright (C) 2023 A.Gabriel Catel Torres <arzur.cateltorres@heig-vd.ch>
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

#define NB_DATA_MAX 15
#define MAX_DATA_STRING_SIZE 50

/* Global communication declaration for IUOC */
/* This needs to match the declaration done in the frontend*/
typedef enum {
	IUOC_ME_BLIND,
	IUOC_ME_OUTDOOR,
	IUOC_ME_WAGOLED,
	IUOC_ME_HEAT,
	IUOC_ME_SWITCH,
	IUOC_ME_END
} me_type_t;

/* Structure used to define a single data defining any behaviour/information
 * coming from a SO. Multiple data can be sent from the SO.
 */
typedef struct {
	char name[30];
	char type[30];
	int value;
} field_data_t;

/* Generic structure used to pass any ME information between the FE and the user space */
typedef struct {
	me_type_t me_type;
	unsigned timestamp;
    unsigned data_array_size;
	field_data_t data_array[NB_DATA_MAX];
} iuoc_data_t;

#define UIOC_IOCTL_SEND_DATA _IOW('a', 'a', iuoc_data_t *)
#define UIOC_IOCTL_RECV_DATA _IOR('a', 'b', iuoc_data_t *)

/**
 * @brief Add a new data received from the FE to a queue to be sure no data is lost
 * 
 * @param data: structure containing the ME data information
 */
void add_iuoc_element_to_queue(iuoc_data_t data);

#endif
