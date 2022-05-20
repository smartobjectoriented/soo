/*
 * Copyright (C) 2022 Mattia Gallacchi <mattia.gallaccchi@heig-vd.ch>
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

#if 1
#define DEBUG
#endif

#include <thread.h>
#include <heap.h>

#include <soo/dev/venocean.h>
#include <soo/dev/vknx.h>

#include <me/blind.h>
#include <soo/enocean.h>

#include "vbwa88pg.h"

#define ENOCEAN_SWITCH_ID	0x002A3D45
#define SWITCH_UP			0x70
#define SWITCH_DOWN			0x50
#define SWITCH_RELEASED		0x00


int knx_wait_data_th(void *args) {
	vknx_response_t data;

	DBG(MEBLIND_PREFIX "Started: %s\n", __func__);

	while (1) {
		if (get_knx_data(&data) < 0) {
			DBG(MEBLIND_PREFIX "Failed to get knx data\n");
			continue;
		} 

		DBG(MEBLIND_PREFIX "Got new knx data. Type:\n");

		switch (data.event)
		{
		case KNX_RESPONSE:
			DBG(MEBLIND_PREFIX "KNX response\n");
			vbwa88pg_blind_update(data.datapoints, data.dp_count);
			break;
		
		case KNX_INDICATION:
			DBG(MEBLIND_PREFIX "KNX indication\n");
			break;
		}
	}

	return 0;
}

int enocean_wait_data_th(void *args) {
	char data[100];
	int data_len;
	enocean_telegram_t *tel;
	blind_vbwa88pg_t blind = {
		.up_down = { 	
			.id = 0x01, 
			.cmd = SET_NEW_VALUE_AND_SEND_ON_BUS, 
			.data = { 0x00 }, 
			.data_len = 1
		},
		.inc_dec_stop = { 
			.id = 0x02, 
			.cmd = SET_NEW_VALUE_AND_SEND_ON_BUS, 
			.data = { 0x00 }, 
			.data_len = 1
		}
	};

	vbwa88pg_blind_init(&blind);

	DBG(MEBLIND_PREFIX "Started: %s\n", __func__);

	while(1) {
		if ((data_len =  venocean_get_data(data)) < 0) {
			DBG(MEBLIND_PREFIX "Failed to get enOcean data\n");
			continue;
		}
		tel = enocean_buffer_to_telegram(data, data_len);

		if (!tel) 
			continue;
		
		// enocean_print_telegram(tel);

		if (tel->sender_id.val == ENOCEAN_SWITCH_ID) {
			if (tel->data[0] == SWITCH_UP) 
				vbwa88pg_blind_up(&blind);
			
			if (tel->data[0] == SWITCH_DOWN)
				vbwa88pg_blind_down(&blind);
		
		}

		free(tel);
	}

	return 0;
}

int app_thread_main(void *args) {
	tcb_t *switch_th, *knx_th;
	/* The ME can cooperate with the others. */
	spad_enable_cooperate();

	printk("Welcome to blind ME\n");

	knx_th = kernel_thread(knx_wait_data_th, "knx_wait_data_th", NULL, THREAD_PRIO_DEFAULT);
	switch_th = kernel_thread(enocean_wait_data_th, "enocean_wait_data_th", NULL, THREAD_PRIO_DEFAULT);

	thread_join(knx_th);
	thread_join(switch_th);

	return 0;
}
