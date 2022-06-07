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

// #include <soo/vdevfront.h>
#include <soo/debug.h>
#include <timer.h>


#if 1
#define ENOCEAN_SWITCH
#include <soo/enocean/pt210.h>
#include <soo/dev/venocean.h>
#define ENOCEAN_SWITCH_ID	0x002A3D45
#endif

#include <me/switch.h>

#ifdef ENOCEAN_SWITCH
static tcb_t *switch_th;
#elif KNX_SWITCH
static tcb_t *knx_th;
#endif

/**
 * @brief Wait KNX data. May be the result of a request or an event
 * 
 * @param args (blind_t *) generic struct blind 
 * @return int 0
 */
int knx_wait_data_th(void *args) {
	// blind_t *bl = (blind_t *)args;
	// vknx_response_t data;

	// DBG(MESWITCH_PREFIX "Started: %s\n", __func__);

	// while (_atomic_read(shutdown)) {
	// 	if (get_knx_data(&data) < 0) {
	// 		DBG(MESWITCH_PREFIX "Failed to get knx data\n");
	// 		continue;
	// 	} 

	// 	DBG(MESWITCH_PREFIX "Got new knx data. Type:\n");

	// 	switch (data.event)
	// 	{
	// 	case KNX_RESPONSE:
	// 		DBG(MESWITCH_PREFIX "KNX response\n");
	// 		vbwa88pg_blind_update(&bl->blind, data.datapoints, data.dp_count);
	// 		break;
		
	// 	case KNX_INDICATION:
	// 		DBG(MESWITCH_PREFIX "KNX indication\n");
	// 		break;
	// 	}
	// }

	// DBG(MESWITCH_PREFIX "Stopped: %s\n", __func__);

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

	sh_switch->timestamp = 0;
}

/**
 * @brief Generic switch get data. Wait for an event.
 * 
 * @param sw Switch to get data from
 */
void switch_get_data(switch_t *sw) {
	uint64_t pressed_time;
	switch (sw->type)
	{
		case PT210:
			pt210_wait_event(&sw->sw);
			if (sw->sw.event) {
				if (sw->sw.up) 
					sh_switch->cmd = SWITCH_UP;
				else if (sw->sw.down) 
					sh_switch->cmd = SWITCH_DOWN;
				else if (sw->sw.released) {
					pressed_time = NS_TO_MS(sw->sw.released_time - sw->sw.press_time);
					sh_switch->press = pressed_time > PT210_PRESSED_TIME_MS ? LONG_PRESS : SHORT_PRESS;
					sh_switch->switch_event= true;
				}
			} 
			
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

	DBG(MESWITCH_PREFIX "Started: %s\n", __func__);
	while(_atomic_read(shutdown)) {
		
		switch_get_data(sw);

		if (sh_switch->switch_event) {		
			/** migrate **/
			sh_switch->timestamp++;
			sh_switch->need_propagate = true;
			DBG("New switch event. cmd: %d, press: %d\n", sh_switch->cmd, sh_switch->press);
			sh_switch->switch_event = false;
		}
	}

	DBG(MESWITCH_PREFIX "Stopped: %s\n", __func__);

	return 0;
}

int app_thread_main(void *args) {
	switch_t *sw;

	sw = (switch_t *)malloc(sizeof(switch_t));
	if (!sw) {
		DBG(MESWITCH_PREFIX "Failed to allocate switch_t\n");
		kernel_panic();
	}

	/* The ME can cooperate with the others. */
	spad_enable_cooperate();

	printk(MESWITCH_PREFIX "Welcome\n");

#ifdef ENOCEAN_SWITCH
	switch_th = kernel_thread(switch_wait_data_th, "switch_wait_data_th", sw, THREAD_PRIO_DEFAULT);
	if (!switch_th) {
		DBG(MESWITCH_PREFIX "Failed to start switch thread\n");
		kernel_panic();
	}
	thread_join(switch_th);
#elif KNX_SWITCH
	knx_th = kernel_thread(knx_wait_data_th, "knx_wait_data_th", bl, THREAD_PRIO_DEFAULT);
	thread_join(knx_th);
#endif


	printk(MESWITCH_PREFIX "Goodbye\n");

	return 0;
}
