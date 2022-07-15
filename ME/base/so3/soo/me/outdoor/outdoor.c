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

#include <me/outdoor.h>


/**
 * @brief Generic weatherstation init
 * 
 * @param ws Weatherstation to init
 */
void weatherstation_init(weatherstation_t *ws){
	p04_init(&ws->ws, ENOCEAN_OUTDOOR_ID);
}

/**
 * @brief Generic weatherstation get data. Wait for an event.
 * 
 * @param ws weatherstation to get data from
 */
void weatherstation_get_data(weatherstation_t *ws){
	p04_wait_event(&ws->ws);

	if(ws->ws.event){
		sh_weatherstation->ws.ws.lightSensor	= ws->ws.lightSensor;
#if 1 // test
	if(sh_weatherstation->ws.ws.outdoorTemp == 15){
		sh_weatherstation->ws.ws.outdoorTemp 	= 30;
	}else{
		sh_weatherstation->ws.ws.outdoorTemp 	= 15;
	}
#else
		sh_weatherstation->ws.ws.outdoorTemp 	= ws->ws.outdoorTemp;
#endif
		sh_weatherstation->ws.ws.windSpeed 		= ws->ws.windSpeed;
		sh_weatherstation->ws.ws.identifier 	= ws->ws.identifier;
		sh_weatherstation->ws.ws.LRNBit 		= ws->ws.LRNBit;
		sh_weatherstation->ws.ws.day0_night1 	= ws->ws.day0_night1;
		sh_weatherstation->ws.ws.rain 			= ws->ws.rain;
		sh_weatherstation->weatherstation_event = true;
	}
	DBG("[ Ligth sensor ] : %d\n", sh_weatherstation->ws.ws.lightSensor);
	DBG("[ Outdoor temp ] : %d\n", sh_weatherstation->ws.ws.outdoorTemp);
	DBG("[ Wind speed   ] : %d\n", sh_weatherstation->ws.ws.windSpeed);
	DBG("[ Identifier   ] : %d\n", sh_weatherstation->ws.ws.identifier);
	DBG("[ LRNBit       ] : %d\n", sh_weatherstation->ws.ws.LRNBit);
	DBG("[ Day0_Night1  ] : %d\n", sh_weatherstation->ws.ws.day0_night1);
	DBG("[ Rain         ] : %d\n", sh_weatherstation->ws.ws.rain);

	p04_reset(&ws->ws);
}

/**
 * @brief Thread to acquire weatherstation events
 * 
 * @param args (weatherstation_t *) generic struct weatherstation
 * @return int 0
 */
void *weatherstation_wait_data_th(void *args){
	weatherstation_t *ws = (weatherstation_t*)args;
	weatherstation_init(ws);

	DBG(MEWEATHERSTATION_PREFIX "Started: %s\n", __func__);
	while(atomic_read(&shutdown)){
		weatherstation_get_data(ws);

		if(sh_weatherstation->weatherstation_event){
			sh_weatherstation->timestamp++;

			spin_lock(&propagate_lock);
			sh_weatherstation->need_propagate = true;
			spin_unlock(&propagate_lock);

			DBG(MEWEATHERSTATION_PREFIX "New weatherstation event.\n");
			sh_weatherstation->weatherstation_event = false;
		}else{
			DBG(MEWEATHERSTATION_PREFIX "No weatherstation event\n");
		}
	}

		DBG(MEWEATHERSTATION_PREFIX "Stopped: %s\n", __func__);

		return NULL;
}

void *app_thread_main(void *args){
	tcb_t *weatherstation_th;
	weatherstation_t *ws;

	ws = (weatherstation_t *) malloc(sizeof(weatherstation_t));
	if (!ws) {
		DBG(MEWEATHERSTATION_PREFIX "Failed to allocate weatherstation_t\n");
		kernel_panic();
	}

	/* The ME can cooperate with the others. */
	spad_enable_cooperate();

	DBG(MEWEATHERSTATION_PREFIX "Welcome\n");

	weatherstation_th = kernel_thread(weatherstation_wait_data_th, "weatherstation_wait_data_th", ws, THREAD_PRIO_DEFAULT);
	if (!weatherstation_th) {
		DBG(MEWEATHERSTATION_PREFIX "Failed to start weatherstation thread\n");
		kernel_panic();
	}

	DBG("SOO.outdoor Mobile Entity -- Copyright (c) 2016-2021 REDS Institute (HEIG-VD)\n\n");

	return NULL;
}
