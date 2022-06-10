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
#include <timer.h>

#include <soo/hypervisor.h>
#include <soo/debug.h>

#include <me/switch.h>

#define ENOCEAN_SWITCH_ID	0x002A3D45

/**
 * @brief Generic switch init
 * 
 * @param sw Switch to init
 */
void switch_init(switch_t *sw) {

#ifdef ENOCEAN
	pt210_init(&sw->sw, ENOCEAN_SWITCH_ID);
#endif

#ifdef KNX
	gtl2tw_init(&sw->sw);
#endif

	sh_switch->timestamp = 0;
}

/**
 * @brief Generic switch get data. Wait for an event.
 * 
 * @param sw Switch to get data from
 */
void switch_get_data(switch_t *sw) {
#ifdef ENOCEAN
	uint64_t pressed_time;

	pt210_wait_event(&sw->sw);
	if (sw->sw.event) {
		if (sw->sw.up) 
			sh_switch->pos = POS_LEFT_UP;
		else if (sw->sw.down) 
			sh_switch->pos = POS_LEFT_DOWN;
		else if (sw->sw.released) {
			pressed_time = NS_TO_MS(sw->sw.released_time - sw->sw.press_time);
			sh_switch->press = pressed_time > PT210_PRESSED_TIME_MS ? PRESS_LONG : PRESS_SHORT;
			sh_switch->switch_event= true;
		}
	} 
			
	pt210_reset(&sw->sw);
#endif
		
#ifdef KNX
	gtl2tw_wait_event(&sw->sw);

	if (sw->sw.events[POS_LEFT_UP]) {
		sh_switch->pos = POS_LEFT_UP;
		sh_switch->status = sw->sw.status[POS_LEFT_UP];
		sh_switch->switch_event = true;
	}

	if (sw->sw.events[POS_RIGHT_UP]) {
		sh_switch->pos = POS_RIGHT_UP;
		sh_switch->status = sw->sw.status[POS_RIGHT_UP];
		sh_switch->switch_event = true;
	}
	
#endif

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

			spin_lock(&propagate_lock);
			sh_switch->need_propagate = true;
			spin_unlock(&propagate_lock);

			DBG(MESWITCH_PREFIX "New switch event. pos: %d, press: %d, status %d\n", sh_switch->pos, sh_switch->press,
				sh_switch->status);
			sh_switch->switch_event = false;
		} else {
			DBG(MESWITCH_PREFIX "No switch event\n");
		}
	}

	DBG(MESWITCH_PREFIX "Stopped: %s\n", __func__);

	return 0;
}

int app_thread_main(void *args) {
	tcb_t *switch_th;
	switch_t *sw;

	sw = (switch_t *)malloc(sizeof(switch_t));
	if (!sw) {
		DBG(MESWITCH_PREFIX "Failed to allocate switch_t\n");
		kernel_panic();
	}

	/* The ME can cooperate with the others. */
	spad_enable_cooperate();

	printk(MESWITCH_PREFIX "Welcome\n");

	switch_th = kernel_thread(switch_wait_data_th, "switch_wait_data_th", sw, THREAD_PRIO_DEFAULT);
	if (!switch_th) {
		DBG(MESWITCH_PREFIX "Failed to start switch thread\n");
		kernel_panic();
	}
	thread_join(switch_th);

	printk(MESWITCH_PREFIX "Goodbye\n");

	return 0;
}
