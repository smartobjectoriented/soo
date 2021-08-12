/*
 * Copyright (C) 2014-2019 Daniel Rossier <daniel.rossier@heig-vd.ch>
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

#ifndef VENOCEANSW_H
#define VENOCEANSW_H

#include <soo/ring.h>
#include <soo/grant_table.h>
#include <soo/vdevback.h>

#define ENOCEAN_UART5_DEV "ttyAMA5"

#define VENOCEANSW_PACKET_SIZE	32

#define VENOCEANSW_FRAME_SIZE 	20
#define VENOCEANSW_INIT_SIZE 		5

//different states for the switchs (single channel)
//Interpreted from the frame during successive tests (could not find documentation about the data encription)
#define SWITCH_IS_RELEASED 	0x00 
#define SWITCH_IS_UP 		0x70
#define SWITCH_IS_DOWN 		0x50

#define ENOCEAN_MODE_BLIND 0
#define ENOCEAN_MODE_VALVE 1


#define VENOCEANSW_NAME		"venoceansw"
#define VENOCEANSW_PREFIX		"[" VENOCEANSW_NAME "] "

typedef struct {
	/* empty */
} venoceansw_request_t;

typedef struct  {
	int sw_cmd;
	int sw_id;
} venoceansw_response_t;


/*
 * Generate ring structures and types.
 */
DEFINE_RING_TYPES(venoceansw, venoceansw_request_t, venoceansw_response_t);

/*
 * General structure for this virtual device (backend side)
 */

typedef struct {

	/* Must be the first field */
	vdevback_t vdevback;

	venoceansw_back_ring_t ring;
	unsigned int irq;

} venoceansw_t;

/* ASCII data coming from the enocean module*/
typedef struct {
	char	frame_begin;	/* 0x55 */
	char	data_length[2];	/* lenght for the main datas*/
	char	optionnal_length;	/* length for the optionnal datas */
	char	packet_type;	/* 0x01 = ERP Radio Telegramm */
	char	crc_h;	/* CRC 8 */
	char	command_code;	/* command code */
	char	switch_data;	/* switch state at telegramm reception*/
	char	switch_ID[4];	/* enocean device ID */
	char	secondary_datas[7];	/* secondary datas from the switch */
	char	crc_l;	/* CRC 8*/
} venoceansw_ascii_data_t;



#endif /* VENOCEANSW_H */
