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


/**
 * @brief XML model sending
 * 
 */
void heat_send_model(void)
{
	vuihandler_send(HEAT_MODEL, strlen(HEAT_MODEL)+1, VUIHANDLER_SELECT);
}


/**
 * @brief Callback events from GUI
 * 
 * @param data char*
 * @param size size_t
 */
void heat_process_events(char *data, size_t size)
{
	char id[ID_MAX_LENGTH];
	char action[ACTION_MAX_LENGTH];
	char content[MAX_MSG_LENGTH];

	memset(id, 0, ID_MAX_LENGTH);
	memset(action, 0, ACTION_MAX_LENGTH);
	memset(content, 0, MAX_MSG_LENGTH);

	xml_parse_event(data, id, action);

	if (!strcmp(action, "clickUp")) {
		if (!strcmp(id, BTN_SAVE_ID)) {
			DBG(MEHEAT_PREFIX "[0] %d     [1] %d", cur_text[0], cur_text[1]);

			// Check if the user input is in range 0 to 9
			if(cur_text[0] >= ASCII_0 && cur_text[0] <= ASCII_9 && cur_text[1] >= ASCII_0 && cur_text[1] <= ASCII_9){
				sh_heat->heat.targetTemp = ((cur_text[0] - ASCII_0)*10) + (cur_text[1] - ASCII_0);

			}else if(cur_text[0] >= ASCII_0 && cur_text[0] <= ASCII_9){
				sh_heat->heat.targetTemp = (cur_text[0] - ASCII_0);
			}	

			DBG(MEHEAT_PREFIX "sh_heat->heat.targetTemp = %s\n", cur_text);
		}
	}else if (!strcmp(id, SETPOINT_TEMP_ID)) {
		xml_get_event_content(data, content);
		strcpy(cur_text, content);
	}

	prepar_temp_to_send(sh_heat->heat.targetTemp, TARGET_TEMP);
}


/**
 * @brief Getter outdoor temperatur. Wait for a event
 * 
 * @param args void*
 * @return void* 
 */
void *soo_heat_get_outdoor_temp(void *args)
{	
	while(atomic_read(&shutdown)){
		wait_for_completion(&send_data_lock);
		sh_heat->isNewOutdoorTemp = true;
		sh_heat->checkNoOutdoorTemp = 0;
	}
	return NULL;
}


/**
 * @brief Main thread sending command to the valve when there is new indoor temp. Wait for a event
 **/
void *soo_heat_command_valve(void *args)
{
	int valve_id;
	char actualTemp;

	while (atomic_read(&shutdown)) { 

		wait_for_completion(&new_temp);

		/* Get ID of the valve connected on the current Smart Object*/
		valve_id = vvalve_get_id();
	
		// Compare the temperature Sensor and SOO.outdoor if there is outdoor temperatur
		if(sh_heat->isNewOutdoorTemp){
			DBG(MEHEAT_PREFIX "Temp outdoor : %d\n", sh_heat->heat.temperatureOutdoor);
			actualTemp = (sh_heat->heat.temperatureOutdoor + sh_heat->heat.temperatureIndoor) / 2;
			prepar_temp_to_send(sh_heat->heat.temperatureOutdoor, OUTDOOR_TEMP);
		}else{
			actualTemp = sh_heat->heat.temperatureIndoor;
			send_temp_to_tablet("-", OUTDOOR_TEMP);
		}

		prepar_temp_to_send(sh_heat->heat.temperatureIndoor, INDOOR_TEMP);
		
		DBG(MEHEAT_PREFIX "Temp indoor  : %d\n", sh_heat->heat.temperatureIndoor);
		DBG(MEHEAT_PREFIX "Temp actual  : %d\n", actualTemp);
		DBG(MEHEAT_PREFIX "Temp target  : %d\n", sh_heat->heat.targetTemp);

		if(sh_heat->heat.id == valve_id) {
			// DBG(MEHEAT_PREFIX "Same ID\n");
			if(actualTemp >= sh_heat->heat.targetTemp) {
				DBG(MEHEAT_PREFIX "Valve closed\n");
				vvalve_send_cmd(VALVE_CMD_CLOSE);
				send_temp_to_tablet("Fermée", STATUS_VALVE);
			} else {
				DBG(MEHEAT_PREFIX "Valve opened\n");
				vvalve_send_cmd(VALVE_CMD_OPEN);
				send_temp_to_tablet("Ouverte", STATUS_VALVE);
			}
		} else {
			DBG(MEHEAT_PREFIX "Not the same ID\n");
			/*TODO: ME SOO.indoor should migrate on another Smart Object */
		}
	}

	return NULL;
}


/**
 * @brief Func to prepar char temperatur to char*
 * 
 * @param temp char
 * @param id char*
 */
void prepar_temp_to_send(char temp, char *id)
{
	char buf[MAX_MSG_LENGTH];

	memset(buf, 0, MAX_MSG_LENGTH);
	sprintf(buf, "%d", temp);
	send_temp_to_tablet(buf, id);
}


/**
 * @brief Func to get indoor temperatur from Sense Hat sensor
 * 
 * @param args 
 * @return void* 
 */
void *soo_heat_get_sensor_temp(void *args)
{
	char *buf;

	while(atomic_read(&shutdown)){
		buf = malloc(sizeof(char));
		vtemp_get_temp_data(buf);
		sh_heat->heat.temperatureIndoor = (char)*buf;

		//Check if there is still outdoor temperatur comming
		if(sh_heat->checkNoOutdoorTemp >= VAL_STOP_GET_OUTDOOR_TEMP){
			sh_heat->isNewOutdoorTemp = false;
		}else{
			sh_heat->checkNoOutdoorTemp++;
		}
		complete(&new_temp);
		msleep(GET_TEMP_MS);
	}
	return NULL;
}


/**
 * @brief Func to send temperatur to app
 * 
 */
void send_temp_to_tablet(char *temp, char *id)
{
	char msg[MAX_MSG_LENGTH];

	memset(msg, 0, MAX_MSG_LENGTH);
	DBG("BUFFER send to tablet %s\n", temp);
	xml_prepare_message(msg, id, temp);

	vuihandler_send(msg, strlen(msg)+1, VUIHANDLER_POST);
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

	sh_heat->heat.targetTemp = TARGET_TEMP_DEFAULT;

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
