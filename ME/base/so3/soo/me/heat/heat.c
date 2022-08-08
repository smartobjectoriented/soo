/*
 * Copyright (C) 2021 Thomas Rieder <thomas.rieder@heig-vd.ch>
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

#if 0
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
#include <soo/evtchn.h>
#include <soo/dev/vvalve.h>
#include <soo/dev/vtemp.h>
#include <me/heat.h>
#include <soo/xmlui.h>
#include <soo/dev/vuihandler.h>

#include <completion.h>
#include <device/irq.h>

#define TMP_WANTED	25


/* ID of the Smart Object on which the ME is running. 0xff means that it has not been initialized yet. */
uint32_t my_id = 0xff;

/* Name of the Smart Object on which the Entity has been injected */
char origin_soo_name[SOO_NAME_SIZE];

/* Bool telling that the ME is in a Smart Object with the expected devcaps */
bool available_devcaps = false;

/* Bool telling that a remote application is connected */
bool remote_app_connected = false;

/* Detection of a ME that has migrated on another new Smart Object */
bool has_migrated = false;

char cur_text[MAX_MSG_LENGTH];


/* Bool telling that at least 1 post-activate has been performed */
bool post_activate_done = false;

extern void *localinfo_data;

struct completion new_temp;


void heat_send_model(void) {
	printk("SENDINGGGG MODELLLLLLLLL\n");
	vuihandler_send(HEAT_MODEL, strlen(HEAT_MODEL)+1, VUIHANDLER_SELECT);
}

void heat_process_events(char *data, size_t size) {
	char id[ID_MAX_LENGTH];
	char action[ACTION_MAX_LENGTH];
	char content[MAX_MSG_LENGTH];

	memset(id, 0, ID_MAX_LENGTH);
	memset(action, 0, ACTION_MAX_LENGTH);

	xml_parse_event(data, id, action);

	if (!strcmp(action, "clickDown")) {

		if (!strcmp(id, BTN_SEND_ID)) {
			if (!strcmp(id, TEXTEDIT_ID)) {
			xml_get_event_content(data, content);
			strcpy(cur_text, content);
			}
		}

		complete(&send_data_lock);
	}
}



void *soo_heat_get_outdoor_temp(void *args)
{
	while(atomic_read(&shutdown)){
		wait_for_completion(&send_data_lock);
		sh_heat->isNewOutdoorTemp = true;
		sh_heat->checkNoOutdoorTemp = 0;
	}
}


/**
 * @brief Main thread sending command to the valve when SOO.indoor cooperate
 **/
void *soo_heat_command_valve(void *args)
{
	int valve_id;
	char actualTemp;

	while (atomic_read(&shutdown)) { 

		// DBG(MEHEAT_PREFIX "is waiting\n");
		wait_for_completion(&new_temp);
		DBG(MEHEAT_PREFIX "going to send valve cmd !!!\n");

		/* Get ID of the valve connected on the current Smart Object*/
		valve_id = vvalve_get_id();
	
		/* Compare the temperature Sensor and SOO.outdoor */
		if(sh_heat->isNewOutdoorTemp){
			DBG(MEHEAT_PREFIX "Temp outdoor : %d\n", sh_heat->heat.temperatureOutdoor);
			actualTemp = (sh_heat->heat.temperatureOutdoor + sh_heat->heat.temperatureIndoor) / 2;
		}else{
			actualTemp = sh_heat->heat.temperatureIndoor;
		}
		
		DBG(MEHEAT_PREFIX "Temp indoor  : %d\n", sh_heat->heat.temperatureIndoor);
		DBG(MEHEAT_PREFIX "Temp actual  : %d\n", actualTemp);
		DBG(MEHEAT_PREFIX "Temp target  : %d\n", TMP_WANTED);

		if(sh_heat->heat.id == valve_id) {
			DBG(MEHEAT_PREFIX "Same ID\n");
			if(actualTemp >= TMP_WANTED) {
				DBG(MEHEAT_PREFIX "Valve closed\n");
				vvalve_send_cmd(VALVE_CMD_CLOSE);

			} else {
				DBG(MEHEAT_PREFIX "Valve opened\n");
				vvalve_send_cmd(VALVE_CMD_OPEN);
			}
		} else {
			DBG(MEHEAT_PREFIX "Not the same ID\n");
			/*TODO: ME SOO.indoor should migrate on another Smart Object */
		}
	}

	return NULL;
}


void *soo_heat_get_sensor_temp(void *args){
	char *buf;

	while(atomic_read(&shutdown)){
		buf = malloc(sizeof(char));
		// DBG(MEHEAT_PREFIX "get temp data ME\n");
		vtemp_get_temp_data(buf);
		// DBG(MEHEAT_PREFIX "sensor temp %d ME\n", *buf);
		sh_heat->heat.temperatureIndoor = (char)*buf;

		if(sh_heat->checkNoOutdoorTemp >= VAL_STOP_GET_OUTDOOR_TEMP){
			sh_heat->isNewOutdoorTemp = false;
		}else{
			sh_heat->checkNoOutdoorTemp++;
		}
		complete(&new_temp);
		msleep(GET_TEMP_MS);
	}
}


irq_return_t evt_interrupt(int irq, void *dev_id) {

	DBG("## got evt interrupt (irq %d)\n", irq);

	return IRQ_COMPLETED;
}


/*
 * The main application of the ME is executed right after the bootstrap. It may be empty since activities can be triggered
 * by external events based on frontend activities.
 */
void *app_thread_main(void *args) {
	tcb_t *heat_cmd_valve_th;
	tcb_t *sensorTemp_th;
	tcb_t *outdoorTemp_th;
	heat_t *heat;

	heat = (heat_t *)malloc(sizeof(heat_t));
	/* The ME can cooperate with the others. */
	spad_enable_cooperate();

	init_completion(&new_temp);

	vuihandler_register_callbacks(heat_send_model, heat_process_events);

	DBG(MEWEATHERSTATION_PREFIX "Welcome\n");
	heat_cmd_valve_th = kernel_thread(soo_heat_command_valve, "soo_heat_command_valve", heat, THREAD_PRIO_DEFAULT);
	sensorTemp_th = kernel_thread(soo_heat_get_sensor_temp, "soo_heat_get_sensor_temp", heat, THREAD_PRIO_DEFAULT);
	outdoorTemp_th = kernel_thread(soo_heat_get_outdoor_temp, "soo_heat_get_outdoor_temp", heat, THREAD_PRIO_DEFAULT);

	if(!heat_cmd_valve_th){
		DBG(MEHEAT_PREFIX "Failed to start heat_cmd_valve thread\n");
		kernel_panic();
	}else if(!sensorTemp_th){
		DBG(MEHEAT_PREFIX "Failed to start sensorTemp thread\n");
		kernel_panic();
	}else if(!outdoorTemp_th){
		DBG(MEHEAT_PREFIX "Failed to start otudoorTemp thread\n");
		kernel_panic();
	}
	//init_timer(&timer, timer_fn, NULL);
	DBG("SOO.heat Mobile Entity -- Copyright (c) 2016-2021 REDS Institute (HEIG-VD)\n\n");

	return NULL;
}
