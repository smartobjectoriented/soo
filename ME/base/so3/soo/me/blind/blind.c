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

#include <me/blind.h>

static tcb_t *switch_th, *knx_th, *blind_th;

#define ENOCEAN_SWITCH_ID	0x002A3D45

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
			vbwa88pg_blind_up(&bl->blind);
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
			vbwa88pg_blind_down(&bl->blind);
			break;
		
		default:
			break;
	}
}

/**
 * @brief Blind switch released. May not apply to all blinds types
 * 
 * @param bl Blind to move
 */
void blind_released(blind_t *bl) {
	switch (bl->type) {
		case VBWA88PG:
			vbwa88pg_blind_stop_step(&bl->blind);
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
int blind_send_cmd_th(void *args) {
	blind_t *bl = (blind_t*)args;
	blind_init(bl);

	DBG(MEBLIND_PREFIX "Started: %s\n", __func__);

	while(_atomic_read(shutdown)) {
		wait_for_completion(&send_data_lock);

		switch(sh_blind->cmd) {
			case SWITCH_UP:
				blind_up(bl);
				break;

			case SWITCH_DOWN:
				blind_down(bl);
				break;

			case SWITCH_RELEASED:
				blind_released(bl);
				break;

			default:
				break;
		}	

	}

	DBG(MEBLIND_PREFIX "Stopped: %s\n", __func__);

	return 0;
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

	while (_atomic_read(shutdown)) {
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
			DBG(MEBLIND_PREFIX "KNX indication\n");
			break;
		}
	}

	DBG(MEBLIND_PREFIX "Stopped: %s\n", __func__);

	return 0;
}

/**
 * @brief Generic switch init
 * 
 * @param sw Switch to init
 */
void switch_init(switch_t *sw) {
#ifdef ENOCEAN_SWITCH
	sw->type = PT210;
#endif

	switch(sw->type) {
		case PT210:
			pt210_init(&sw->sw, ENOCEAN_SWITCH_ID);
			break;
		default: 
			break;
	}
}

/**
 * @brief Generic switch get data. Wait for an event.
 * 
 * @param sw Switch to get data from
 */
void switch_get_data(switch_t *sw) {
	switch (sw->type)
	{
	case PT210:
		pt210_wait_event(&sw->sw);
		
		if (sw->sw.event) {
			if (sw->sw.up) 
				sw->cmd = SWITCH_UP;
			else if (sw->sw.down) 
				sw->cmd = SWITCH_DOWN;
			else if (sw->sw.released)
				sw->cmd = SWITCH_RELEASED;
			else
				sw->cmd = NONE;
		} else 
			sw->cmd = NONE;
		
		pt210_reset(&sw->sw);
		break;
	
	default:
		break;
	}
} 

/**
 * @brief Thread to acquire switch events
 * 
 * @param args (switch_t *) generic struct switch
 * @return int 0
 */
int switch_wait_data_th(void *args) {
	switch_t *sw = (switch_t*)args;
	switch_init(sw);

	DBG(MEBLIND_PREFIX "Started: %s\n", __func__);

	while(_atomic_read(shutdown)) {
		switch_get_data(sw);
		if (sw->cmd != NONE) {
			sh_blind->switch_event = true;
			sh_blind->cmd = sw->cmd;
			
			/** Check if blind thread exist and if is waiting **/
			if (blind_th && blind_th->state == THREAD_STATE_WAITING) {
				complete(&send_data_lock);
			} else {
				/** migrate **/
				sh_blind->need_propagate = true;
			}
		}
	}

	DBG(MEBLIND_PREFIX "Stopped: %s\n", __func__);

	return 0;
}

int app_thread_main(void *args) {
	blind_t *bl;
	switch_t *sw;

	bl = (blind_t *)malloc(sizeof(blind_t));
	sw = (switch_t *)malloc(sizeof(switch_t));

	/* The ME can cooperate with the others. */
	spad_enable_cooperate();

	printk(MEBLIND_PREFIX "Welcome\n");

	blind_th = kernel_thread(blind_send_cmd_th, "blind_send_cmd_th", bl, THREAD_PRIO_DEFAULT);
	knx_th = kernel_thread(knx_wait_data_th, "knx_wait_data_th", bl, THREAD_PRIO_DEFAULT);
	switch_th = kernel_thread(switch_wait_data_th, "switch_wait_data_th", sw, THREAD_PRIO_DEFAULT);

	thread_join(blind_th);
	thread_join(knx_th);
	thread_join(switch_th);

	printk(MEBLIND_PREFIX "Goodbye\n");

	return 0;
}
