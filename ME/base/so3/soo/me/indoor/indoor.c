/*
 * Copyright (C) 2019 Baptiste Delporte <bonel@bonel.net>
 * Copyright (C) 2020 David Truan <david.truan@heig-vd.ch>
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

#include <mutex.h>
#include <delay.h>
#include <timer.h>
#include <heap.h>
#include <memory.h>

#include <soo/avz.h>
#include <soo/gnttab.h>
#include <soo/hypervisor.h>
#include <soo/vbus.h>
#include <soo/soo.h>
#include <soo/console.h>
#include <soo/debug.h>
#include <soo/debug/dbgvar.h>
#include <soo/debug/logbool.h>

#include <soo/dev/vuihandler.h>

#include <me/indoor.h>


/**
 * @brief Generic tempSensor init
 * 
 * @param ts tempSensor to init
 */
void tempSensor_init(tempSensor_t *ts){
	ts->id = TEMPSENSOR_INDOOR_ID;
	ts->indoorTemp = 0;
}

/**
 * @brief Generic tempSensor get data. Wait for an event.
 * 
 * @param ts tempSensor to get data from
 */
void tempSensor_get_data(tempSensor_t *ts){
	
}

/**
 * @brief Thread to acquire tempSensor events
 * 
 * @param args (tempSensor_t *) generic struct tempSensor
 * @return int 0
 */
void *tempSensor_wait_data_th(void *args){
	tempSensor_t *ts = (tempSensor_t*)args;
	tempSensor_init(ts);

	DBG(TEMPSENSOR_PREFIX "Started: %s\n", __func__);
	while(atomic_read(&shutdown)){
		tempSensor_get_data(ts);

		if(sh_tempSensor->tempSensor_event){
			sh_tempSensor->timestamp++;

			spin_lock(&propagate_lock);
			sh_tempSensor->need_propagate = true;
			spin_unlock(&propagate_lock);

			DBG(TEMPSENSOR_PREFIX "New tempSensor event.\n");
			sh_tempSensor->tempSensor_event = false;
		}else{
			DBG(TEMPSENSOR_PREFIX "No tempSensor event\n");
		}
	}

		DBG(TEMPSENSOR_PREFIX "Stopped: %s\n", __func__);

		return NULL;
}

void *app_thread_main(void *args){
	tcb_t *tempSensor_th;
	tempSensor_t ts;

	ts = (tempSensor_t *) malloc(sizeof(tempSensor_t));
	if (!ts) {
		DBG(TEMPSENSOR_PREFIX "Failed to allocate tempSensor_t\n");
		kernel_panic();
	}

	/* The ME can cooperate with the others. */
	spad_enable_cooperate();

	DBG(TEMPSENSOR_PREFIX "Welcome\n");

	tempSensor_th = kernel_thread(tempSensor_wait_data_th, "tempSensor_wait_data_th", ts, THREAD_PRIO_DEFAULT);
	if (!tempSensor_th) {
		DBG(TEMPSENSOR_PREFIX "Failed to start tempSensor thread\n");
		kernel_panic();
	}

	DBG("SOO.indoor Mobile Entity -- Copyright (c) 2016-2021 REDS Institute (HEIG-VD)\n\n");

	return NULL;
}
