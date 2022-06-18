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

#include <soo/dev/vknx.h>
#include <soo/debug.h>
#include <timer.h>
#include <delay.h>

#include <me/blind.h>

/**
 * @brief Generic blind initialization
 * 
 * @param bl Blind to init
 */
void blind_init(blind_t *bl)
{
#ifdef BLIND_VBWA88PG
	bl->type = VBWA88PG;
#endif

	switch (bl->type)
	{
	case VBWA88PG:
		vbwa88pg_blind_init(&bl->blind, 0x01);
		break;
	
	default:
		break;
	}
}

/**
 * @brief Generic blind up
 * 
 * @param bl Blind to move up
 */
void blind_up(blind_t *bl) {
	DBG(MEBLIND_PREFIX "%s\n", __func__);

	switch(bl->type) {
		case VBWA88PG:
			if (sh_blind->sw_press == PRESS_SHORT) {
				bl->blind.dps[INC_DEC_STOP].data[0] = VBWA88PG_BLIND_INC;
				vbwa88pg_blind_inc_dec_stop(&bl->blind);
			} else if (sh_blind->sw_press == PRESS_LONG) {
				bl->blind.dps[UP_DOWN].data[0] = VBWA88PG_BLIND_UP;
				vbwa88pg_blind_up_down(&bl->blind);
			}
			break;
		default:
			break;
	}
}

/**
 * @brief Generic blind down
 * 
 * @param bl Blind to move down
 */
void blind_down(blind_t *bl) {
	DBG(MEBLIND_PREFIX "%s\n", __func__);

	switch(bl->type) {
		case VBWA88PG:
			if (sh_blind->sw_press == PRESS_SHORT) {
				bl->blind.dps[INC_DEC_STOP].data[0] = VBWA88PG_BLIND_DEC;
				vbwa88pg_blind_inc_dec_stop(&bl->blind);
			} else if (sh_blind->sw_press == PRESS_LONG) {
				bl->blind.dps[UP_DOWN].data[0] = VBWA88PG_BLIND_DOWN;
				vbwa88pg_blind_up_down(&bl->blind);
			}
			break;
		
		default:
			break;
	}
}

/**
 * @brief Thread used to send command to a blind
 * 
 * @param args (blind_t *) generic struct blind 
 * @return int 0
 */
void *blind_send_cmd_th(void *args) {
	blind_t *bl = (blind_t*)args;
	blind_init(bl);

	DBG(MEBLIND_PREFIX "Started: %s\n", __func__);

	while (atomic_read(&shutdown)) {
		wait_for_completion(&send_data_lock);

		// msleep(300);

		switch(sh_blind->sw_pos) {
			case POS_LEFT_UP:
				blind_up(bl);
				break;

			case POS_LEFT_DOWN:
				blind_down(bl);
				break;

			default:
				break;
		}	

	}

	DBG(MEBLIND_PREFIX "Stopped: %s\n", __func__);

	return NULL;
}

/**
 * @brief Wait KNX data. May be the result of a request or an event
 * 
 * @param args (blind_t *) generic struct blind 
 * @return int 0
 */
int knx_wait_data_th(void *args) {
	blind_t *bl = (blind_t *)args;
	vknx_response_t data;

	DBG(MEBLIND_PREFIX "Started: %s\n", __func__);

	while (atomic_read(&shutdown)) {
		if (get_knx_data(&data) < 0) {
			DBG(MEBLIND_PREFIX "Failed to get knx data\n");
			continue;
		} 

		DBG(MEBLIND_PREFIX "Got new knx data. Type:\n");

		switch (data.event)
		{
		case KNX_RESPONSE:
			DBG(MEBLIND_PREFIX "KNX response\n");
			vbwa88pg_blind_update(&bl->blind, data.datapoints, data.dp_count);
			break;
		
		case KNX_INDICATION:
			/** Not used yet **/
			DBG(MEBLIND_PREFIX "KNX indication\n");
			break;
		}
	}

	DBG(MEBLIND_PREFIX "Stopped: %s\n", __func__);
	return 0;
}

void *app_thread_main(void *args) {
	tcb_t *knx_th, *blind_th;
	blind_t *bl;

	bl = (blind_t *)malloc(sizeof(blind_t));

	/* The ME can cooperate with the others. */
	spad_enable_cooperate();

	printk(MEBLIND_PREFIX "Welcome\n");

	blind_th = kernel_thread(blind_send_cmd_th, "blind_send_cmd_th", bl, THREAD_PRIO_DEFAULT);
	// knx_th = kernel_thread(knx_wait_data_th, "knx_wait_data_th", bl, THREAD_PRIO_DEFAULT);

	// thread_join(knx_th);

	return NULL;
}
